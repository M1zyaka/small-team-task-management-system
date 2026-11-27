# Small Team Task Management System

**A C++20 command-line board for tracking tasks and bugs in a small team.**

Register team members, assign work, and see which items need attention. The board is stored in a local file and is available immediately when you run the next command. The application can build and run offline using the C++ standard library and native Windows argument handling.

[Quick start](#quick-start) · [Commands](#commands) · [Tests](#tests) · [Design](docs/design.md) · [Coursework guide](docs/coursework.md)

[![Build and test](https://github.com/M1zyaka/small-team-task-management-system/actions/workflows/ci.yml/badge.svg)](https://github.com/M1zyaka/small-team-task-management-system/actions)

## Quick start

Install CMake 3.20 or newer and a C++20 compiler with calendar support. GitHub Actions builds with the compiler supplied by each Linux, Windows, and macOS runner.

```sh
git clone https://github.com/M1zyaka/small-team-task-management-system.git
cd small-team-task-management-system
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

| Toolchain | Executable path |
| --- | --- |
| Linux or macOS with a single-configuration generator | `./build/team-board` |
| Windows with the Visual Studio generator | `.\build\Release\team-board.exe` |
| Windows with MinGW Makefiles | `.\build\team-board.exe` |

For MinGW, configure a fresh build directory with `-G "MinGW Makefiles"` and run the same build command. On Windows, the application accepts Unicode arguments and writes UTF-8. Configure your terminal for UTF-8 if accented or Cyrillic characters display incorrectly.

### Walk through a release

Run the commands from the repository root with a fresh `build/demo.db`. In PowerShell with the Visual Studio build, start with `$app = '.\build\Release\team-board.exe'` and use `& $app` in place of `./build/team-board`.

```sh
./build/team-board --data build/demo.db member add "Avery Chen"
./build/team-board --data build/demo.db task add "Prepare release notes" high 2026-12-01
./build/team-board --data build/demo.db bug add "CSV export fails" medium 2026-12-02 critical
./build/team-board --data build/demo.db task assign 1 1
./build/team-board --data build/demo.db task move 1 in-progress
./build/team-board --data build/demo.db task list
./build/team-board --data build/demo.db --today 2026-12-02 summary
```

```text
#1 [task] Prepare release notes
  in-progress | high | due 2026-12-01 | assigned to #1 Avery Chen
#2 [bug] CSV export fails
  todo | medium | critical | due 2026-12-02 | unassigned
Board summary for 2026-12-02 (UTC)
  Todo: 1
  In progress: 1
  Done: 0
  Overdue: 1
  Unassigned open: 1
```

## Commands

| Command | Effect |
| --- | --- |
| `member add NAME` / `member list` | Register or list team members |
| `task add TITLE PRIORITY DUE [DESCRIPTION]` | Create a task in `todo` |
| `bug add TITLE PRIORITY DUE SEVERITY [DESCRIPTION]` | Create a bug in `todo` with severity |
| `task list [--status STATUS] [--assignee ID\|none]` | List tasks and bugs. The filters can be combined |
| `task show ID` | Show one item with its description |
| `task edit ID TITLE PRIORITY DUE [DESCRIPTION]` | Replace the common details of a task or bug. Omitting the description clears it |
| `task assign ID MEMBER_ID\|none` | Assign or unassign an item |
| `task move ID STATUS` | Start, return, complete, or reopen an item |
| `task remove ID` | Permanently delete an item from the board |
| `summary` | Count items by status and report overdue and unassigned open work |

Priorities are `low`, `medium`, and `high`. Bug severity is `minor`, `major`, or `critical`. Use `YYYY-MM-DD` for a deadline or `-` to leave it open. In the workflow, `todo` can move to `in-progress`, `in-progress` can move to `todo` or `done`, and `done` can reopen into `todo`.

The queue puts open items first. Within each group it sorts by earliest deadline, highest urgency, then lowest item ID. Undated items follow dated items in the same group. Open work is overdue when its deadline is strictly earlier than the reporting date.

Pass `--data FILE` and `--today YYYY-MM-DD` before the command. The defaults are `team-board.db` in the working directory and the current UTC date. Each successful change saves automatically. A local board has one active process at a time and its members represent assignees. See [storage notes](docs/design.md#storage) for recovery after an interrupted process.

## Tests

`ctest --test-dir build -C Release --output-on-failure` runs seven cases covering dates, all nine status pairs, data validation, sorting and filtering, counters, storage locking and round trips, and a multi-process CLI scenario. The CLI test also checks that failed commands and malformed snapshots leave the board file intact.

## Repository layout

| Path | Contents |
| --- | --- |
| [`include/team_board/`](include/team_board/) | Public domain and storage interfaces |
| [`src/`](src/) | Domain rules, snapshot storage, and command-line entry point |
| [`tests/`](tests/) | Domain, storage, and command-line scenarios |
| [`docs/`](docs/) | Design, coursework requirements and defense plan, and reading list |
| [`.github/workflows/ci.yml`](.github/workflows/ci.yml) | Automated builds and tests across three operating systems |

Fundamentals of Object-Oriented Programming · Novosibirsk State University
