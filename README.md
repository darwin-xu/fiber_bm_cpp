# fiber_bm_cpp

## Platform
- macOS
- ubuntu

## Dependency
- Boost 1.73 or later (ealier versions might work but I haven't tested)

## Building Instructions
- mkdir build
- cd build
- cmake ..
- cmake --build . -j24

## Override compile
export CC=clang-6.0
export CXX=clang++-6.0
