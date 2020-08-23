cmake_minimum_required(VERSION 3.0.0)
project(fiber_bm_cpp VERSION 0.1.0)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/local/Cellar/boost/1.73.0)
link_directories(/usr/local/Cellar/boost/1.73.0/lib)

link_libraries(boost_context-mt)

add_executable(thread_test thread_test.cpp)
add_executable(thread_test_mm thread_test_mm.cpp)
add_executable(fork_test fork_test.cpp)
add_executable(fiber_test fiber_test.cpp)
add_executable(pipeline_test pipeline_test.cpp facility.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
