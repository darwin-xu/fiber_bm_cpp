# fiber_bm_cpp

## Platform
- macOS
- ubuntu
- FreeBSD
- OneFS

## Dependency
- CMake 3.19.2, (Supporting of C++17 is mandatory)
- Boost 1.73 or later (earlier versions might work but I haven't tested it), build it with: b2 --with-fiber

## Building Instructions
- mkdir build
- cd build
- cmake ..
- cmake --build . -j24

## Override compile
```
export CC=clang-6.0
export CXX=clang++-6.0
```

## TODO list
- [x] Remove all the words of "master". (Not because of racism, but because it cannot indicate the correct concept)
- [x] Change the words of "worker" in the program arguments into something more meaningful.
- [x] Change the name from name_name into nameName.
- [x] The release version was optimized too much, couldn't get a reasonable result. (no issue, caused by wrong argument)
- [x] fiber_test.cpp cores at `./fiber_test 500 10`. (no fix, FD_SETSIZE is 1024)
- [x] Visual studio code seems doesn't support passing variables to CMake by using "cmake.buildEnvironment", use "Configure Args" instead.
- [ ] fiber_test.cpp and fiber_test_k.cpp isn't balance.
- [ ] batchesNumber of thread_test_mm.cpp is not processed correctly.
- [ ] Support more accurate time point for each thread.
- [ ] Use fork to move fibers into dedicated process.
- [x] Only copy when it is write in operate(), check data when it is read in operate().
