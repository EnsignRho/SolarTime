cmake_minimum_required(VERSION 3.10)

# Compilers must be chosen before project().  clang everywhere (house rule):  the IDE's
# Error List parses "file:row:col: error:" lines and clang's -fdiagnostics-absolute-paths
# makes the file part a full path.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
endif()

# BUG_FIX_PLAN_2026-10-05.md FIX-05 — fixed 2026-10-05 16:48:37 EDT (UTC-04:00).
project(soltime LANGUAGES C CXX)
include(CTest)

add_compile_options(-fdiagnostics-absolute-paths)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


##########
# SolTime library
#####
    add_library(${PROJECT_NAME} SHARED soltime.cpp soltime.h)
    target_link_libraries(${PROJECT_NAME} PRIVATE m)


##########
# Tests (unity build over soltime.cpp -- sees the iSolTime_* internals)
#####
    add_executable(soltime_tests test_soltime.cpp)
    target_link_libraries(soltime_tests PRIVATE m)
    add_test(NAME soltime COMMAND soltime_tests)
    add_executable(soltime_calendar_tests test_calendar.cpp)
    target_link_libraries(soltime_calendar_tests PRIVATE m)
    target_compile_options(soltime_calendar_tests PRIVATE -Wall -O2 -Wno-misleading-indentation)
    add_test(NAME calendar_and_travel COMMAND soltime_calendar_tests)
    # BUG_FIX_PLAN_2026-10-05.md FIX-01/FIX-03 — fixed 2026-10-05 16:43:53 EDT (UTC-04:00).
    add_executable(soltime_review_tests tests/test_review.cpp)
    target_link_libraries(soltime_review_tests PRIVATE m)
    target_compile_options(soltime_review_tests PRIVATE -Wall -O2 -Wno-misleading-indentation)
    add_test(NAME review_regressions COMMAND soltime_review_tests)
    # BUG_FIX_PLAN_2026-10-05.md FIX-05 — fixed 2026-10-05 16:48:37 EDT (UTC-04:00).
    # Compile a C client against an isolated copy of the installed-style public header.
    configure_file(soltime.h public_api/soltime.h COPYONLY)
    add_executable(soltime_c_api_tests tests/test_c_api.c)
    set_target_properties(soltime_c_api_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    target_include_directories(soltime_c_api_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/public_api)
    target_compile_options(soltime_c_api_tests PRIVATE -Wall -Wextra -pedantic-errors -UNDEBUG)
    target_link_libraries(soltime_c_api_tests PRIVATE soltime m)
    add_test(NAME c_public_api COMMAND soltime_c_api_tests)
    find_package(Java COMPONENTS Development QUIET)
    if(Java_FOUND)
        add_test(NAME android_location COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_java_tests.sh)
    endif()


##########
# The zone registry publisher (unity build over soltime.cpp;  -O2:  it builds the whole registry)
#####
    add_executable(soltime_zonetool zonetool.cpp)
    target_link_libraries(soltime_zonetool PRIVATE m)
    target_compile_options(soltime_zonetool PRIVATE -Wall -O2 -Wno-misleading-indentation)
    find_program(SOLTIME_PYTHON_EXECUTABLE NAMES python3)
    if(SOLTIME_PYTHON_EXECUTABLE)
        add_test(NAME publisher_recalculation COMMAND ${SOLTIME_PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_publisher.py $<TARGET_FILE:soltime_zonetool>)
    endif()


##########
# The display (soltime_display.cpp) on the TUI library (../tui, built here as a subdirectory;
# its test programs are left out).  Both must be present:  the evaluation package leaves out the
# display's source.
#####
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../tui/CMakeLists.txt AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/soltime_display.cpp)
        if(NOT TARGET tui)
            add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tui ${CMAKE_BINARY_DIR}/tui EXCLUDE_FROM_ALL)
        endif()
        find_package(Threads REQUIRED)
        add_executable(soltime_display soltime_display.cpp)
        target_include_directories(soltime_display PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../tui ${CMAKE_CURRENT_SOURCE_DIR}/../common)
        target_compile_definitions(soltime_display PRIVATE SOLTIME_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
        target_link_libraries(soltime_display PRIVATE ${PROJECT_NAME} tui Threads::Threads m)
        target_compile_options(soltime_display PRIVATE -Wall -O0 -Wno-misleading-indentation)
    endif()


##########
# Compiler warnings
#####
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -O0 -Wno-misleading-indentation)
    target_compile_options(soltime_tests   PRIVATE -Wall -O0 -Wno-misleading-indentation)
