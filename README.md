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
