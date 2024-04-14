#!/bin/bash

mkdir -p build && cd build
export PATH=/path/to/the/gcc-linaro-xxxx-x86_64_aarch64-linux-gnu/bin:$PATH
export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
cmake ..
make install
