#include "team_board/model.hpp"
#include "team_board/storage.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace {
using namespace team_board;

void check(bool passed, std::string_view explanation)
{
    if (!passed) {
        throw std::runtime_error(std::string(explanation));
    }
}

template <typename Exception, typename Operation>
void expect_failure(Operation operation, std::string_view explanation)
{
    try {
        operation();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(std::string(explanation));
}

Details details(std::string title = "Prepare release", Priority priority = Priority::medium,
                std::optional<Date> due = std::nullopt)
{
    return {std::move(title), "Check the test report.", priority, due};
}

void calendar()
{
    check(format_date(parse_date("2024-02-29")) == "2024-02-29", "Leap day must round-trip.");
    check(format_date(parse_date("0001-01-01")) == "0001-01-01", "Minimum date must round-trip.");
    check(format_date(parse_date("9999-12-31")) == "9999-12-31", "Maximum date must round-trip.");
    check(!parse_due("-"), "The dash denotes no deadline.");
    for (const auto invalid : {"2025-02-29", "1900-02-29", "0000-12-31", "2025-4-30", "2025-04-31"}) {
        expect_failure<std::invalid_argument>([&] { (void)parse_date(invalid); }, invalid);
    }
}

void workflow()
{
    using enum Status;
    constexpr std::array cases{
        std::tuple{todo, todo, false}, std::tuple{todo, in_progress, true},
        std::tuple{todo, done, false}, std::tuple{in_progress, todo, true},
        std::tuple{in_progress, in_progress, false}, std::tuple{in_progress, done, true},
        std::tuple{done, todo, true}, std::tuple{done, in_progress, false},
        std::tuple{done, done, false},
    };
    for (const auto& [from, to, allowed] : cases) {
        TeamBoard board;
        const Id id = board.add_task(details());
        if (from != todo) {
            board.move(id, in_progress);
        }
        if (from == done) {
            board.move(id, done);
        }
        if (allowed) {
            board.move(id, to);
            check(board.item(id).status() == to, "Allowed move must update the status.");
        } else {
            expect_failure<std::invalid_argument>([&] { board.move(id, to); },
                                                  "Invalid move must be rejected.");
            check(board.item(id).status() == from, "Invalid move must preserve the status.");
        }
    }
}

void validation()
{
    TeamBoard board;
    const Id member = board.add_member("Avery Chen");
    expect_failure<std::invalid_argument>([&] { board.add_member(" "); }, "Blank name accepted.");
    expect_failure<std::invalid_argument>([&] { board.add_task(details("")); }, "Blank title accepted.");
    const Id item = board.add_task(details());
    check(item == 1, "A rejected addition advanced the ID sequence.");
    board.assign(item, member);
    expect_failure<std::invalid_argument>([&] { board.assign(item, 99); }, "Unknown assignee accepted.");
    check(board.item(item).assignee() == member, "Rejected assignment changed the owner.");
    expect_failure<std::invalid_argument>([&] { board.edit(item, details("bad\ntext")); },
                                          "A newline in a title was accepted.");
    check(board.item(item).details().title == "Prepare release", "The bad edit changed the title.");
    board.assign(item, std::nullopt);
    check(!board.item(item).assignee(), "Unassign should clear the member.");
    board.remove(item);
    expect_failure<std::invalid_argument>([&] { (void)board.item(item); }, "Deleted item still exists.");
    check(board.add_task(details()) == 2, "A deleted ID was reused.");

    TeamBoard exhausted{1, std::numeric_limits<Id>::max(), {}, {}};
    expect_failure<std::overflow_error>([&] { exhausted.add_task(details()); }, "Item ID overflowed.");
}

void ordering()
{
    TeamBoard board;
    const auto day = parse_date("2026-12-02");
    const auto earlier = parse_date("2026-12-01");
    const Id member = board.add_member("Avery Chen");
    const Id high = board.add_task(details("High priority", Priority::high, day));
    const Id critical = board.add_bug(details("Critical bug", Priority::medium, day), Severity::critical);
    const Id sooner = board.add_task(details("Earlier deadline", Priority::low, earlier));
    const Id later_id = board.add_task(details("Same deadline", Priority::high, day));
    const Id undated = board.add_task(details("No deadline", Priority::high));
    const Id closed = board.add_task(details("Finished", Priority::high, earlier));
    board.assign(high, member);
    board.move(closed, Status::in_progress);
    board.move(closed, Status::done);

    const auto items = board.list();
    const std::array expected{sooner, critical, high, later_id, undated, closed};
    check(items.size() == expected.size(), "Listing lost items.");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        check(items[i]->id() == expected[i], "Unexpected queue ordering.");
    }
    const auto focused = board.list({Status::todo, Assignment::member, member});
    check(focused.size() == 1 && focused.front()->id() == high, "Combined filters failed.");
    check(board.list({Status::todo, Assignment::unassigned}).size() == 4,
          "Unassigned filtering failed.");
}

void summary()
{
    TeamBoard board;
    const auto day = parse_date("2026-12-02");
    const auto yesterday = day - std::chrono::days{1};
    const Id member = board.add_member("Avery Chen");
    const Id overdue = board.add_task(details("Overdue", Priority::medium, yesterday));
    board.add_task(details("Due today", Priority::medium, day));
    const Id closed = board.add_task(details("Finished", Priority::medium, yesterday));
    board.add_task(details("No deadline"));
    board.assign(overdue, member);
    board.move(overdue, Status::in_progress);
    board.move(closed, Status::in_progress);
    board.move(closed, Status::done);

    const auto result = board.summary(day);
    check(result.todo == 2 && result.in_progress == 1 && result.done == 1, "Status counts are wrong.");
    check(result.overdue == 1, "Due today, undated and completed items should not be overdue.");
    check(result.unassigned_open == 2, "Unassigned open count is wrong.");
}

void storage()
{
    const std::filesystem::path path{"storage-test.db"};
    {
        const BoardFile first{path};
        expect_failure<std::runtime_error>([&] { const BoardFile second{path}; },
                                           "A second writer acquired the same file lock.");
        TeamBoard board;
        const Id member = board.add_member("Avery Chen");
        const Id bug = board.add_bug(details(R"(Reproduce "missing\file")"), Severity::critical);
        const Id removed = board.add_task(details());
        board.remove(removed);
        board.assign(bug, member);
        first.save(board);
    }
    const BoardFile second{path};
    const auto restored = second.load();
    check(restored.next_item() == 3, "Storage lost the next item ID.");
    check(restored.item(1).details().title == R"(Reproduce "missing\file")",
          "Quoted text did not survive saving.");
    check(restored.item(1).kind() == "bug" && restored.item(1).severity() == Severity::critical,
          "Storage lost the concrete item type or severity.");
    check(restored.item(1).assignee() == 1, "Storage lost the assignee.");
    std::filesystem::remove(path);
}

}  // namespace

int main(int argc, char* argv[])
{
    try {
        if (argc != 2) {
            throw std::invalid_argument("Pass one test case name.");
        }
        using Entry = std::pair<std::string_view, void (*)()>;
        constexpr std::array cases{
            Entry{"calendar", calendar}, Entry{"workflow", workflow},
            Entry{"validation", validation}, Entry{"ordering", ordering},
            Entry{"summary", summary}, Entry{"storage", storage},
        };
        for (const auto& [name, test] : cases) {
            if (name == argv[1]) {
                test();
                return 0;
            }
        }
        throw std::invalid_argument("Unknown test case.");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
