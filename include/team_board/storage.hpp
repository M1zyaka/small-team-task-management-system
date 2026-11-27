#pragma once

#include "team_board/model.hpp"

#include <filesystem>

namespace team_board {

class BoardFile {
public:
    explicit BoardFile(std::filesystem::path path);
    ~BoardFile() noexcept;

    BoardFile(const BoardFile&) = delete;
    BoardFile& operator=(const BoardFile&) = delete;

    [[nodiscard]] TeamBoard load() const;
    void save(const TeamBoard& board) const;

private:
    std::filesystem::path path_;
    std::filesystem::path lock_;
    std::filesystem::path staging_;
};

}  // namespace team_board
