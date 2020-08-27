cmake_minimum_required(VERSION 3.0.0)
project(fiber_bm_cpp VERSION 0.1.0)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 17)

include_directories(../boost_1_74_0/)
link_directories(../boost_1_74_0/stage/lib)

link_libraries(boost_context)
link_libraries(pthread)
link_libraries(boost_fiber)

if (APPLE)
    add_definitions(-DMACOS)
else (LINUX)
    add_definitions(-DLINUX)
endif()

add_executable(pipeline_test pipeline_test.cpp facility.cpp)
add_executable(thread_test select/thread_test.cpp facility.cpp)
add_executable(thread_test_mm select/thread_test_mm.cpp facility.cpp)
add_executable(fork_test select/fork_test.cpp facility.cpp)
add_executable(fiber_test select/fiber_test.cpp facility.cpp)
add_executable(thread_test_opt select/thread_test_opt.cpp facility.cpp)
add_executable(thread_test_k kevent/thread_test_k.cpp facility.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
