#!/bin/bash

## deb pkgs install
apt install -y libspdlog-dev


## source code install
# CLI11
git clone https://github.com/CLIUtils/CLI11.git
cd CLI11
mkdir build && cd build
cmake ..
make
make install