#!/bin/bash

# Install gstreamer
sudo apt install gstreamer

# Install perf
sudo apt install perf

# Install bpf
sudo apt install bpf

# Install other third-party packages
sudo apt install package1 package2 package3

# Add any additional packages you want to install here

echo "Package installation complete."

# build 3rd party packages

# build spdlog
cmake .. -DCMAKE_INSTALL_PREFIX=./install -DSPDLOG_BUILD_BENCH=ON -DSPDLOG_BUILD_SHARED=ON
make -j8 install

# build cpp-tbox
cmake .. -DCMAKE_INSTALL_PREFIX=./install
make -j8 install