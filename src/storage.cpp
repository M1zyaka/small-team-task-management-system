#include "team_board/storage.hpp"

#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace team_board {
namespace {

namespace fs = std::filesystem;

std::string display_path(const fs::path& path)
{
    const auto utf8 = path.u8string();
    return {utf8.begin(), utf8.end()};
}

std::string read_word(std::istream& record)
{
    std::string word;
    if (!(record >> word)) {
        throw std::invalid_argument("A field is missing.");
    }
    return word;
}

std::string read_text(std::istream& record)
{
    record >> std::ws;
    std::string value;
    if (record.peek() != '"' || !(record >> std::quoted(value))) {
        throw std::invalid_argument("Expected a quoted text field.");
    }
    return value;
}

void require_end(std::istream& record)
{
    if (record >> std::ws && record.peek() != std::char_traits<char>::eof()) {
        throw std::invalid_argument("Unexpected field after the record.");
    }
}

std::unique_ptr<WorkItem> read_item(std::istream& record, std::string_view kind)
{
    const Id id = parse_id(read_word(record));
    const Status status = parse_status(read_word(record));
    const Priority priority = parse_priority(read_word(record));
    const auto due = parse_due(read_word(record));
    const auto assignee_field = read_word(record);
    const auto assignee = assignee_field == "-"
                              ? std::nullopt
                              : std::optional{parse_id(assignee_field)};
    Details details{read_text(record), read_text(record), priority, due};
    if (kind == "BUG") {
        const Severity severity = parse_severity(read_word(record));
        return std::make_unique<Bug>(id, std::move(details), severity, status, assignee);
    }
    return std::make_unique<Task>(id, std::move(details), status, assignee);
}

TeamBoard read_board(std::istream& input)
{
    std::string line;
    if (!std::getline(input, line)) {
        throw std::invalid_argument("Snapshot header is missing.");
    }
    std::istringstream header{line};
    if (read_word(header) != "TEAM_BOARD" || read_word(header) != "1") {
        throw std::invalid_argument("Unsupported snapshot format or version.");
    }
    const Id next_member = parse_id(read_word(header));
    const Id next_item = parse_id(read_word(header));
    require_end(header);

    std::vector<Member> members;
    std::vector<std::unique_ptr<WorkItem>> items;
    std::size_t line_number = 1;
    bool ended = false;
    while (std::getline(input, line)) {
        ++line_number;
        std::istringstream record{line};
        try {
            if (ended) {
                throw std::invalid_argument("Unexpected record after END.");
            }
            const std::string kind = read_word(record);
            if (kind == "MEMBER") {
                members.push_back({parse_id(read_word(record)), read_text(record)});
            } else if (kind == "TASK" || kind == "BUG") {
                items.push_back(read_item(record, kind));
            } else if (kind == "END") {
                ended = true;
            } else {
                throw std::invalid_argument("Unknown record type.");
            }
            require_end(record);
        } catch (const std::invalid_argument& error) {
            throw std::invalid_argument("Line " + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (input.bad() || !input.eof()) {
        throw std::runtime_error("Could not read the complete snapshot.");
    }
    // A snapshot truncated between complete records still fails this check.
    if (!ended) {
        throw std::invalid_argument("Snapshot has no END record.");
    }
    return TeamBoard{next_member, next_item, std::move(members), std::move(items)};
}

void write_board(std::ostream& output, const TeamBoard& board)
{
    output << "TEAM_BOARD 1 " << board.next_member() << ' ' << board.next_item() << '\n';
    for (const auto& [id, member] : board.members()) {
        output << "MEMBER " << id << ' ' << std::quoted(member.name) << '\n';
    }
    for (const auto* item : board.list()) {
        const auto& details = item->details();
        const auto assignee = item->assignee();
        const auto severity = item->severity();
        output << (severity ? "BUG" : "TASK") << ' ' << item->id() << ' ' << name(item->status())
               << ' ' << name(details.priority) << ' '
               << (details.due ? format_date(*details.due) : "-") << ' '
               << (assignee ? std::to_string(*assignee) : "-") << ' '
               << std::quoted(details.title) << ' ' << std::quoted(details.description);
        if (severity) {
            output << ' ' << name(*severity);
        }
        output << '\n';
    }
    output << "END\n";
}

}  // namespace

BoardFile::BoardFile(fs::path path) : path_(std::move(path))
{
    if (path_.filename().empty() || path_.filename() == "." || path_.filename() == "..") {
        throw std::invalid_argument("The board path must name a file.");
    }
    lock_ = path_;
    lock_ += ".lock";
    staging_ = lock_ / "pending.db";
    // Creating the directory acquires the lock in one filesystem operation.
    if (!fs::create_directory(lock_)) {
        throw std::runtime_error("Board is locked: " + display_path(lock_));
    }
}

BoardFile::~BoardFile() noexcept
{
    std::error_code ignored;
    fs::remove(staging_, ignored);
    fs::remove(lock_, ignored);
}

TeamBoard BoardFile::load() const
{
    const auto status = fs::symlink_status(path_);
    if (!fs::exists(status)) {
        return {};
    }
    if (!fs::is_regular_file(status)) {
        throw std::runtime_error("The board path must be a regular file: " + display_path(path_));
    }
    std::ifstream input{path_};
    if (!input) {
        throw std::runtime_error("Cannot open the board file: " + display_path(path_));
    }
    input.imbue(std::locale::classic());
    try {
        return read_board(input);
    } catch (const std::invalid_argument& error) {
        throw std::runtime_error("Invalid board file " + display_path(path_) + ": " + error.what());
    }
}

void BoardFile::save(const TeamBoard& board) const
{
    std::ofstream output{staging_};
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.imbue(std::locale::classic());
    write_board(output, board);
    output.close();
    // Replace the previous snapshot only after buffered writes have succeeded.
    fs::rename(staging_, path_);
}

}  // namespace team_board
