cmake_minimum_required(VERSION 3.20)

project(SmallTeamTaskManagementSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(team_board STATIC src/model.cpp src/storage.cpp)
target_include_directories(team_board PUBLIC include)
target_compile_features(team_board PUBLIC cxx_std_20)
enable_project_warnings(team_board)

add_executable(team-board src/main.cpp)
target_link_libraries(team-board PRIVATE team_board)
enable_project_warnings(team-board)
if(MINGW)
    # Select the MinGW startup routine for the wide-character Windows entry point.
    target_link_options(team-board PRIVATE -municode)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(team-board-tests tests/model_tests.cpp)
    target_link_libraries(team-board-tests PRIVATE team_board)
    enable_project_warnings(team-board-tests)

    foreach(case IN ITEMS calendar workflow validation ordering summary storage)
        add_test(NAME ${case} COMMAND team-board-tests ${case})
    endforeach()
    add_test(NAME cli_round_trip
        COMMAND ${CMAKE_COMMAND}
            "-DAPP=$<TARGET_FILE:team-board>"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli test data"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_round_trip.cmake")
endif()
