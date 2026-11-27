#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace team_board {

using Id = int;
using Date = std::chrono::sys_days;

enum class Status { todo, in_progress, done };
enum class Priority { low = 1, medium, high };
enum class Severity { minor = 1, major, critical };

[[nodiscard]] Id parse_id(std::string_view value);
[[nodiscard]] Date parse_date(std::string_view value);
[[nodiscard]] std::optional<Date> parse_due(std::string_view value);
[[nodiscard]] Status parse_status(std::string_view value);
[[nodiscard]] Priority parse_priority(std::string_view value);
[[nodiscard]] Severity parse_severity(std::string_view value);
[[nodiscard]] std::string format_date(Date date);
[[nodiscard]] std::string_view name(Status status);
[[nodiscard]] std::string_view name(Priority priority);
[[nodiscard]] std::string_view name(Severity severity);

struct Member {
    Id id;
    std::string name;
};

struct Details {
    std::string title;
    std::string description;
    Priority priority;
    std::optional<Date> due;
};

class WorkItem {
public:
    virtual ~WorkItem() = default;

    [[nodiscard]] Id id() const noexcept { return id_; }
    [[nodiscard]] Status status() const noexcept { return status_; }
    [[nodiscard]] std::optional<Id> assignee() const noexcept { return assignee_; }
    [[nodiscard]] const Details& details() const noexcept { return details_; }
    [[nodiscard]] virtual std::string_view kind() const noexcept = 0;
    [[nodiscard]] int urgency() const noexcept;
    [[nodiscard]] virtual std::optional<Severity> severity() const noexcept;

protected:
    WorkItem(Id id, Details details, Status status, std::optional<Id> assignee);

private:
    friend class TeamBoard;

    [[nodiscard]] virtual int urgency_bonus() const noexcept = 0;
    void edit(Details details);
    void move_to(Status target);

    Id id_;
    Details details_;
    Status status_;
    std::optional<Id> assignee_;
};

class Task final : public WorkItem {
public:
    Task(Id id, Details details, Status status = Status::todo,
         std::optional<Id> assignee = std::nullopt);

    [[nodiscard]] std::string_view kind() const noexcept override { return "task"; }

private:
    [[nodiscard]] int urgency_bonus() const noexcept override { return 0; }
};

class Bug final : public WorkItem {
public:
    Bug(Id id, Details details, Severity severity, Status status = Status::todo,
        std::optional<Id> assignee = std::nullopt);

    [[nodiscard]] std::string_view kind() const noexcept override { return "bug"; }
    [[nodiscard]] std::optional<Severity> severity() const noexcept override { return severity_; }

private:
    [[nodiscard]] int urgency_bonus() const noexcept override;

    Severity severity_;
};

enum class Assignment { any, unassigned, member };

struct Filter {
    std::optional<Status> status;
    Assignment assignment = Assignment::any;
    Id member = 0;
};

struct Summary {
    std::size_t todo = 0;
    std::size_t in_progress = 0;
    std::size_t done = 0;
    std::size_t overdue = 0;
    std::size_t unassigned_open = 0;
};

class TeamBoard {
public:
    TeamBoard() = default;
    TeamBoard(Id next_member, Id next_item, std::vector<Member> members,
              std::vector<std::unique_ptr<WorkItem>> items);

    Id add_member(std::string member_name);
    Id add_task(Details details);
    Id add_bug(Details details, Severity severity);
    void edit(Id item, Details details);
    void assign(Id item, std::optional<Id> member);
    void move(Id item, Status target);
    void remove(Id item);

    [[nodiscard]] Id next_member() const noexcept { return next_member_; }
    [[nodiscard]] Id next_item() const noexcept { return next_item_; }
    [[nodiscard]] const std::map<Id, Member>& members() const noexcept { return members_; }
    [[nodiscard]] const WorkItem& item(Id id) const;
    [[nodiscard]] std::vector<const WorkItem*> list(Filter filter = {}) const;
    [[nodiscard]] Summary summary(Date today) const;

private:
    void require_member(Id id) const;
    [[nodiscard]] WorkItem& find_item(Id id) const;

    Id next_member_ = 1;
    Id next_item_ = 1;
    std::map<Id, Member> members_;
    std::map<Id, std::unique_ptr<WorkItem>> items_;
};

}  // namespace team_board
