#include "team_board/model.hpp"
#include "team_board/storage.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
using namespace team_board;
using Args = std::span<const std::string_view>;

constexpr std::string_view usage = R"(Small Team Task Management System

Usage: team-board [--data FILE] [--today YYYY-MM-DD] COMMAND

  member add NAME
  member list
  task add TITLE PRIORITY DUE [DESCRIPTION]
  bug add TITLE PRIORITY DUE SEVERITY [DESCRIPTION]
  task list [--status STATUS] [--assignee ID|none]
  task show ID
  task edit ID TITLE PRIORITY DUE [DESCRIPTION]
  task assign ID MEMBER_ID|none
  task move ID STATUS
  task remove ID
  summary

PRIORITY: low, medium, high.  SEVERITY: minor, major, critical.
STATUS: todo, in-progress, done.  DUE: YYYY-MM-DD or - for no deadline.
Default file: team-board.db. The summary uses the current UTC date.
Use --help for this command reference.
)";

class UsageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void require_count(Args args, std::size_t min, std::size_t max, const char* syntax)
{
    if (args.size() < min || args.size() > max) {
        throw UsageError(syntax);
    }
}

Details parse_details(Args args, std::size_t description_index)
{
    return {std::string(args[0]),
            args.size() > description_index ? std::string(args[description_index]) : "",
            parse_priority(args[1]), parse_due(args[2])};
}

Filter parse_filter(Args args)
{
    if (args.size() % 2 != 0) {
        throw UsageError("task list expects a value for each option.");
    }
    Filter result;
    for (std::size_t i = 0; i < args.size(); i += 2) {
        const auto option = args[i];
        const auto value = args[i + 1];
        if (option == "--status" && !result.status) {
            result.status = parse_status(value);
        } else if (option == "--assignee" && result.assignment == Assignment::any) {
            if (value == "none") {
                result.assignment = Assignment::unassigned;
            } else {
                result.assignment = Assignment::member;
                result.member = parse_id(value);
            }
        } else {
            throw UsageError("Unknown or repeated list option: " + std::string(option));
        }
    }
    return result;
}

void print_item(const TeamBoard& board, const WorkItem& item, bool show_description,
                std::ostream& out)
{
    const auto& details = item.details();
    out << '#' << item.id() << " [" << item.kind() << "] " << details.title << '\n'
        << "  " << name(item.status()) << " | " << name(details.priority);
    if (const auto severity = item.severity()) {
        out << " | " << name(*severity);
    }
    out << " | due ";
    if (details.due) {
        out << format_date(*details.due);
    } else {
        out << '-';
    }
    out << " | ";
    if (const auto id = item.assignee()) {
        out << "assigned to #" << *id << ' ' << board.members().at(*id).name;
    } else {
        out << "unassigned";
    }
    out << '\n';
    if (show_description && !details.description.empty()) {
        out << "  " << details.description << '\n';
    }
}

bool member_command(TeamBoard& board, Args args, std::ostream& out)
{
    if (args.size() == 2 && args.front() == "add") {
        const auto id = board.add_member(std::string(args[1]));
        out << "Added member #" << id << ".\n";
        return true;
    }
    if (args.size() == 1 && args.front() == "list") {
        if (board.members().empty()) {
            out << "No team members.\n";
        }
        for (const auto& [id, member] : board.members()) {
            out << '#' << id << ' ' << member.name << '\n';
        }
        return false;
    }
    throw UsageError("Expected: member add NAME or member list.");
}

bool task_command(TeamBoard& board, std::string_view action, Args args, std::ostream& out)
{
    if (action == "list") {
        const auto items = board.list(parse_filter(args));
        if (items.empty()) {
            out << "No matching work items.\n";
        }
        for (const auto* item : items) {
            print_item(board, *item, false, out);
        }
        return false;
    }
    if (action == "add") {
        require_count(args, 3, 4, "Expected: task add TITLE PRIORITY DUE [DESCRIPTION].");
        const auto id = board.add_task(parse_details(args, 3));
        out << "Added task #" << id << ".\n";
        return true;
    }
    if (action == "show") {
        require_count(args, 1, 1, "Expected: task show ID.");
        print_item(board, board.item(parse_id(args.front())), true, out);
        return false;
    }
    if (action == "edit") {
        require_count(args, 4, 5, "Expected: task edit ID TITLE PRIORITY DUE [DESCRIPTION].");
        const auto id = parse_id(args.front());
        board.edit(id, parse_details(args.subspan(1), 3));
        out << "Updated item #" << id << ".\n";
        return true;
    }
    if (action == "assign") {
        require_count(args, 2, 2, "Expected: task assign ID MEMBER_ID|none.");
        const auto id = parse_id(args.front());
        const auto member = args[1] == "none" ? std::nullopt : std::optional{parse_id(args[1])};
        board.assign(id, member);
        out << "Updated assignment for item #" << id << ".\n";
        return true;
    }
    if (action == "move") {
        require_count(args, 2, 2, "Expected: task move ID STATUS.");
        const auto id = parse_id(args.front());
        board.move(id, parse_status(args[1]));
        out << "Moved item #" << id << " to " << args[1] << ".\n";
        return true;
    }
    if (action == "remove") {
        require_count(args, 1, 1, "Expected: task remove ID.");
        const auto id = parse_id(args.front());
        board.remove(id);
        out << "Removed item #" << id << ".\n";
        return true;
    }
    throw UsageError("Unknown task action: " + std::string(action));
}

bool execute(TeamBoard& board, Args args, Date today, std::ostream& out)
{
    if (args.front() == "member") {
        return member_command(board, args.subspan(1), out);
    }
    if (args.front() == "task" && args.size() >= 2) {
        return task_command(board, args[1], args.subspan(2), out);
    }
    if (args.front() == "bug" && args.size() >= 2 && args[1] == "add") {
        const auto fields = args.subspan(2);
        require_count(fields, 4, 5, "Expected: bug add TITLE PRIORITY DUE SEVERITY [DESCRIPTION].");
        const auto id = board.add_bug(parse_details(fields, 4), parse_severity(fields[3]));
        out << "Added bug #" << id << ".\n";
        return true;
    }
    if (args.front() == "summary" && args.size() == 1) {
        const auto counts = board.summary(today);
        out << "Board summary for " << format_date(today) << " (UTC)\n"
            << "  Todo: " << counts.todo << '\n'
            << "  In progress: " << counts.in_progress << '\n'
            << "  Done: " << counts.done << '\n'
            << "  Overdue: " << counts.overdue << '\n'
            << "  Unassigned open: " << counts.unassigned_open << '\n';
        return false;
    }
    throw UsageError("Unknown command or unexpected arguments.");
}

int run(Args args)
{
    std::filesystem::path path = "team-board.db";
    bool has_data = false;
    std::optional<Date> today;
    while (!args.empty()) {
        const auto option = args.front();
        if (option == "--help" || option == "-h") {
            std::cout << usage;
            return 0;
        }
        if (option != "--data" && option != "--today") {
            break;
        }
        if (args.size() < 2 || args[1].empty()) {
            throw UsageError("Missing value for " + std::string(option) + ".");
        }
        if (option == "--data" && !has_data) {
            path = std::filesystem::path{std::u8string{args[1].begin(), args[1].end()}};
            has_data = true;
        } else if (option == "--today" && !today) {
            today = parse_date(args[1]);
        } else {
            throw UsageError("Repeated option: " + std::string(option));
        }
        args = args.subspan(2);
    }
    if (args.empty()) {
        std::cout << usage;
        return 0;
    }

    const BoardFile file{std::move(path)};
    auto board = file.load();
    std::ostringstream output;
    const Date report_date = today.value_or(std::chrono::floor<std::chrono::days>(
        std::chrono::system_clock::now()));
    if (execute(board, args, report_date, output)) {
        file.save(board);
    }
    std::cout << output.str();
    return 0;
}

#ifdef _WIN32
std::string to_utf8(const wchar_t* value)
{
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0,
                                         nullptr, nullptr);
    if (size == 0) {
        throw std::invalid_argument("An argument contains invalid Unicode.");
    }
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, bytes.data(), size,
                            nullptr, nullptr) == 0) {
        throw std::invalid_argument("Could not decode a command-line argument.");
    }
    bytes.pop_back();
    return bytes;
}
#endif

}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    try {
        std::vector<std::string_view> args;
#ifdef _WIN32
        std::vector<std::string> converted;
        for (int index = 1; index < argc; ++index) {
            converted.push_back(to_utf8(argv[index]));
        }
        args.assign(converted.begin(), converted.end());
#else
        args.assign(argv + 1, argv + argc);
#endif
        return run(args);
    } catch (const UsageError& error) {
        std::cerr << "Error: " << error.what() << "\nRun team-board --help for usage.\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
