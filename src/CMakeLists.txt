cmake_minimum_required(VERSION 3.10)
project(assignment_code C)

set(CMAKE_C_STANDARD 99)

#set(GCC_COVERAGE_COMPILE_FLAGS "-lpthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(rexd rexd.c jobs.c jobs.h semaphore.c semaphore.h job_scheduler.c job_scheduler.h)

add_executable(rex rex.c)