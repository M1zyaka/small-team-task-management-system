file(MAKE_DIRECTORY "${TEST_DIR}")
set(board "${TEST_DIR}/course board.db")
file(REMOVE "${board}")

function(run expected message)
    execute_process(
        COMMAND "${APP}" --data "${board}" --today 2026-12-02 ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    set(combined "${output}${error}")
    string(FIND "${combined}" "${message}" match)
    if(NOT "${result}" STREQUAL "${expected}" OR match EQUAL -1)
        message(FATAL_ERROR "Command [${ARGN}] returned ${result}. Output: ${combined}")
    endif()
endfunction()

run(0 "No matching work items." task list)
if(EXISTS "${board}")
    message(FATAL_ERROR "A read-only command created a board.")
endif()
run(0 "Added member #1." member add "Avery Chen")
run(0 "Added task #1." task add "Prepare release" high 2026-12-01 "Check the build.")
run(0 "Added bug #2." bug add "Export fails" medium 2026-12-02 critical)
run(0 "Updated assignment for item #1." task assign 1 1)
file(SHA256 "${board}" before)
run(1 "Cannot move from todo to done." task move 1 done)
run(1 "Unknown member ID: 2" task assign 1 2)
run(2 "Unknown or repeated list option:" task list --status todo --status done)
file(SHA256 "${board}" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "A rejected command changed the snapshot.")
endif()

run(0 "Moved item #1 to in-progress." task move 1 in-progress)
run(0 "Overdue: 1" summary)
run(0 "#1 [task] Prepare release" task list --status in-progress --assignee 1)
run(0 "Check the build." task show 1)
run(0 "Updated item #2." task edit 2 "Export test" high - "Run with a large report.")
run(0 "high | critical | due -" task show 2)
run(0 "Moved item #1 to done." task move 1 done)
run(0 "Overdue: 0" summary)
run(0 "Moved item #1 to todo." task move 1 todo)
run(0 "Updated assignment for item #1." task assign 1 none)
run(0 "Unassigned open: 2" summary)
run(0 "Removed item #2." task remove 2)
run(1 "Unknown item ID: 2" task show 2)
run(0 "Added task #3." task add "Retest" low -)
run(0 "No matching work items." task list --status done)

set(invalid_snapshots
    "TEAM_BOARD 99 1 1\nEND\n"
    "TEAM_BOARD 1 1 1\n"
    "TEAM_BOARD 1 1 2\nTASK 1 todo medium - 100 \"Orphan\" \"\"\nEND\n"
    "TEAM_BOARD 1 2 1\nMEMBER 1 \"First\"\nMEMBER 1 \"Duplicate\"\nEND\n"
    "TEAM_BOARD 1 1 1\nEND trailing\n")
foreach(snapshot IN LISTS invalid_snapshots)
    file(WRITE "${board}" "${snapshot}")
    run(1 "Invalid board file" member add "Another Member")
    file(READ "${board}" actual)
    if(NOT actual STREQUAL snapshot)
        message(FATAL_ERROR "An invalid snapshot was changed.")
    endif()
endforeach()
file(REMOVE "${board}")
