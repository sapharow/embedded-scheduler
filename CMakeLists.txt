cmake_minimum_required(VERSION 3.10)
project(Scheduler)

enable_testing()

find_package(GTest REQUIRED)

add_executable(SchedulerTests
        Tests/SchedulerTests.cpp)

target_include_directories(SchedulerTests PRIVATE include)
target_link_libraries(SchedulerTests PRIVATE GTest::gtest GTest::Main GTest::gmock)
target_compile_features(SchedulerTests PRIVATE cxx_std_20)

add_test(NAME AllTests COMMAND SchedulerTests)
