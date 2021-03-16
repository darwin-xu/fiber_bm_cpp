# fiber_bm_cpp

## Platform
- macOS
- ubuntu

## Dependency
- Boost 1.73 or later (earlier versions might work but I haven't tested it), build it with: b2 --with-fiber

## Building Instructions
- mkdir build
- cd build
- cmake ..
- cmake --build . -j24

## Override compile
export CC=clang-6.0
export CXX=clang++-6.0

## TODO list
- [ ] Remove all the words of "master".
- [ ] Change the words of "worker" into something more meaningful.
