#include "team_board/model.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace team_board {
namespace {

using namespace std::string_view_literals;

constexpr std::array status_names{
    std::pair{Status::todo, "todo"sv},
    std::pair{Status::in_progress, "in-progress"sv},
    std::pair{Status::done, "done"sv},
};
constexpr std::array priority_names{
    std::pair{Priority::low, "low"sv},
    std::pair{Priority::medium, "medium"sv},
    std::pair{Priority::high, "high"sv},
};
constexpr std::array severity_names{
    std::pair{Severity::minor, "minor"sv},
    std::pair{Severity::major, "major"sv},
    std::pair{Severity::critical, "critical"sv},
};

template <typename Entries>
auto parse_name(std::string_view value, const Entries& entries, std::string_view field)
{
    for (const auto& [entry, label] : entries) {
        if (label == value) {
            return entry;
        }
    }
    throw std::invalid_argument("Invalid " + std::string(field) + ": " + std::string(value));
}

template <typename Enum, typename Entries>
std::string_view lookup_name(Enum value, const Entries& entries)
{
    for (const auto& [entry, label] : entries) {
        if (entry == value) {
            return label;
        }
    }
    throw std::invalid_argument("Invalid enumeration value.");
}

bool digits_only(std::string_view value)
{
    return !value.empty() && std::ranges::all_of(value, [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

int decimal(std::string_view value)
{
    int result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument("Invalid number: " + std::string(value));
    }
    return result;
}

void validate_text(std::string_view value, std::string_view field, std::size_t max_bytes,
                   bool required = true)
{
    if (value.size() > max_bytes) {
        throw std::invalid_argument(std::string(field) + " exceeds " + std::to_string(max_bytes)
                                    + " bytes.");
    }
    if (required && value.find_first_not_of(' ') == std::string_view::npos) {
        throw std::invalid_argument(std::string(field) + " cannot be blank.");
    }
    for (const unsigned char character : value) {
        if (character < 0x20 || character == 0x7f) {
            throw std::invalid_argument(std::string(field) + " contains a control character.");
        }
    }
}

void validate_details(const Details& details)
{
    validate_text(details.title, "Title", 120);
    validate_text(details.description, "Description", 2000, false);
    (void)name(details.priority);
    if (details.due) {
        (void)format_date(*details.due);
    }
}

void require_available_id(Id next)
{
    if (next == std::numeric_limits<Id>::max()) {
        throw std::overflow_error("No more IDs are available.");
    }
}

auto ordering_key(const WorkItem& item)
{
    const auto due = item.details().due;
    return std::tuple{item.status() == Status::done, !due.has_value(), due.value_or(Date{}),
                      -item.urgency(), item.id()};
}

}  // namespace

Id parse_id(std::string_view value)
{
    if (!digits_only(value) || value.front() == '0') {
        throw std::invalid_argument("Expected a positive ID: " + std::string(value));
    }
    return decimal(value);
}

Date parse_date(std::string_view value)
{
    if (value.size() == 10 && value[4] == '-' && value[7] == '-'
        && digits_only(value.substr(0, 4)) && digits_only(value.substr(5, 2))
        && digits_only(value.substr(8, 2))) {
        const std::chrono::year_month_day date{
            std::chrono::year{decimal(value.substr(0, 4))},
            std::chrono::month{static_cast<unsigned>(decimal(value.substr(5, 2)))},
            std::chrono::day{static_cast<unsigned>(decimal(value.substr(8, 2)))},
        };
        if (int(date.year()) >= 1 && date.ok()) {
            return Date{date};
        }
    }
    throw std::invalid_argument("Expected a valid date in YYYY-MM-DD format: " + std::string(value));
}

std::optional<Date> parse_due(std::string_view value)
{
    if (value == "-") {
        return std::nullopt;
    }
    return parse_date(value);
}

std::string format_date(Date date)
{
    constexpr Date first = std::chrono::year{1} / 1 / 1;
    constexpr Date last = std::chrono::year{9999} / 12 / 31;
    if (date < first || date > last) {
        throw std::invalid_argument("A date must be between years 0001 and 9999.");
    }

    const std::chrono::year_month_day value{date};
    const std::string year = std::to_string(int(value.year()));
    const std::string month = std::to_string(unsigned(value.month()));
    const std::string day = std::to_string(unsigned(value.day()));
    return std::string(4 - year.size(), '0') + year + '-'
           + std::string(2 - month.size(), '0') + month + '-'
           + std::string(2 - day.size(), '0') + day;
}

Status parse_status(std::string_view value) { return parse_name(value, status_names, "status"); }
Priority parse_priority(std::string_view value) { return parse_name(value, priority_names, "priority"); }
Severity parse_severity(std::string_view value) { return parse_name(value, severity_names, "severity"); }
std::string_view name(Status status) { return lookup_name(status, status_names); }
std::string_view name(Priority priority) { return lookup_name(priority, priority_names); }
std::string_view name(Severity severity) { return lookup_name(severity, severity_names); }

WorkItem::WorkItem(Id id, Details details, Status status, std::optional<Id> assignee)
    : id_(id), details_(std::move(details)), status_(status), assignee_(assignee)
{
    if (id_ < 1 || (assignee_ && *assignee_ < 1)) {
        throw std::invalid_argument("Item and member IDs must be positive.");
    }
    validate_details(details_);
    (void)name(status_);
}

std::optional<Severity> WorkItem::severity() const noexcept
{
    return std::nullopt;
}

int WorkItem::urgency() const noexcept
{
    return static_cast<int>(details_.priority) + urgency_bonus();
}

void WorkItem::edit(Details details)
{
    validate_details(details);
    details_ = std::move(details);
}

void WorkItem::move_to(Status target)
{
    constexpr std::array transitions{
        std::pair{Status::todo, Status::in_progress},
        std::pair{Status::in_progress, Status::todo},
        std::pair{Status::in_progress, Status::done},
        std::pair{Status::done, Status::todo},
    };
    if (std::ranges::find(transitions, std::pair{status_, target}) == transitions.end()) {
        throw std::invalid_argument("Cannot move from " + std::string(name(status_)) + " to "
                                    + std::string(name(target)) + ".");
    }
    status_ = target;
}

Task::Task(Id id, Details details, Status status, std::optional<Id> assignee)
    : WorkItem(id, std::move(details), status, assignee)
{
}

Bug::Bug(Id id, Details details, Severity severity, Status status, std::optional<Id> assignee)
    : WorkItem(id, std::move(details), status, assignee), severity_(severity)
{
    (void)name(severity_);
}

int Bug::urgency_bonus() const noexcept
{
    return static_cast<int>(severity_);
}

TeamBoard::TeamBoard(Id next_member, Id next_item, std::vector<Member> members,
                     std::vector<std::unique_ptr<WorkItem>> items)
    : next_member_(next_member), next_item_(next_item)
{
    if (next_member_ < 1 || next_item_ < 1) {
        throw std::invalid_argument("Invalid next ID in snapshot.");
    }
    for (auto& member : members) {
        const Id id = member.id;
        validate_text(member.name, "Member name", 80);
        if (id < 1 || id >= next_member_ || !members_.try_emplace(id, std::move(member)).second) {
            throw std::invalid_argument("Invalid or duplicate member ID in snapshot.");
        }
    }
    for (auto& item : items) {
        if (!item) {
            throw std::invalid_argument("Missing work item in snapshot.");
        }
        const Id id = item->id();
        if (const auto member = item->assignee()) {
            require_member(*member);
        }
        if (id >= next_item_ || !items_.try_emplace(id, std::move(item)).second) {
            throw std::invalid_argument("Invalid or duplicate item ID in snapshot.");
        }
    }
}

Id TeamBoard::add_member(std::string member_name)
{
    validate_text(member_name, "Member name", 80);
    require_available_id(next_member_);
    members_.emplace(next_member_, Member{next_member_, std::move(member_name)});
    return next_member_++;
}

Id TeamBoard::add_task(Details details)
{
    require_available_id(next_item_);
    items_.emplace(next_item_, std::make_unique<Task>(next_item_, std::move(details)));
    return next_item_++;
}

Id TeamBoard::add_bug(Details details, Severity severity)
{
    require_available_id(next_item_);
    items_.emplace(next_item_, std::make_unique<Bug>(next_item_, std::move(details), severity));
    return next_item_++;
}

void TeamBoard::edit(Id id, Details details)
{
    find_item(id).edit(std::move(details));
}

void TeamBoard::assign(Id id, std::optional<Id> member)
{
    auto& target = find_item(id);
    if (member) {
        require_member(*member);
    }
    target.assignee_ = member;
}

void TeamBoard::move(Id id, Status target)
{
    find_item(id).move_to(target);
}

void TeamBoard::remove(Id id)
{
    if (items_.erase(id) == 0) {
        throw std::invalid_argument("Unknown item ID: " + std::to_string(id));
    }
}

void TeamBoard::require_member(Id id) const
{
    if (!members_.contains(id)) {
        throw std::invalid_argument("Unknown member ID: " + std::to_string(id));
    }
}

WorkItem& TeamBoard::find_item(Id id) const
{
    const auto found = items_.find(id);
    if (found == items_.end()) {
        throw std::invalid_argument("Unknown item ID: " + std::to_string(id));
    }
    return *found->second;
}

const WorkItem& TeamBoard::item(Id id) const
{
    return find_item(id);
}

std::vector<const WorkItem*> TeamBoard::list(Filter filter) const
{
    if (filter.assignment == Assignment::member) {
        require_member(filter.member);
    }
    std::vector<const WorkItem*> result;
    for (const auto& [id, item] : items_) {
        if (filter.status && item->status() != filter.status) {
            continue;
        }
        if (filter.assignment == Assignment::unassigned && item->assignee()) {
            continue;
        }
        if (filter.assignment == Assignment::member && item->assignee() != filter.member) {
            continue;
        }
        result.push_back(item.get());
    }
    std::ranges::sort(result, std::less<>{}, [](const WorkItem* item) {
        return ordering_key(*item);
    });
    return result;
}

Summary TeamBoard::summary(Date today) const
{
    Summary result;
    for (const auto& [id, item] : items_) {
        switch (item->status()) {
        case Status::todo:
            ++result.todo;
            break;
        case Status::in_progress:
            ++result.in_progress;
            break;
        case Status::done:
            ++result.done;
            continue;
        }
        const auto due = item->details().due;
        if (due && *due < today) {
            ++result.overdue;
        }
        if (!item->assignee()) {
            ++result.unassigned_open;
        }
    }
    return result;
}

}  // namespace team_board
