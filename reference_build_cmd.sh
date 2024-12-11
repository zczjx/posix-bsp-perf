#!/bin/bash

# build spdlog
    ## for x86
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/x86/install -DSPDLOG_BUILD_BENCH=ON -DSPDLOG_BUILD_SHARED=ON

    ## for rk3588s
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/rk3588s/install -DSPDLOG_BUILD_BENCH=ON -DSPDLOG_BUILD_SHARED=ON

make -j8 install

# build cpp-tbox
    ## for x86
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/x86/install

    ## for rk3588s
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/rk3588s/install

make -j8 install

# build cli11

    ## for x86
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/x86/install
    ## for rk3588s
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/rk3588s/install

make -j8 install

# build posix-bsp-perf in docker container

    ## for X86

cd /opt/tools_conf/
ls -A | grep '^\.' | xargs -I {} cp -r {} ~/
source /opt/tools_conf/.bashrc

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/x86/install/lib/pkgconfig:/opt/cross_env/x86/install/share/pkgconfig -DCMAKE_BUILD_TYPE=NoOptimize

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/x86/install/lib/pkgconfig:/opt/cross_env/x86/install/share/pkgconfig -DCMAKE_BUILD_TYPE=Debug

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/x86/install/lib/pkgconfig:/opt/cross_env/x86/install/share/pkgconfig -DCMAKE_BUILD_TYPE=Release


make -j8 install

    ## for rk3588s

cd /opt/tools_conf/
ls -A | grep '^\.' | xargs -I {} cp -r {} ~/
source /opt/tools_conf/.bashrc.rk3588s

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rk3588s/install/lib/pkgconfig:/opt/cross_env/rk3588s/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/rk3588s/install/lib \
    -DCMAKE_BUILD_TYPE=NoOptimize \
    -DCMAKE_PREFIX_PATH=/home/builder/temp/aarch64

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rk3588s/install/lib/pkgconfig:/opt/cross_env/rk3588s/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/rk3588s/install/lib \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=/home/builder/temp/aarch64

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rk3588s/install/lib/pkgconfig:/opt/cross_env/rk3588s/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/rk3588s/install/lib \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/home/builder/temp/aarch64

    ## for rpi

cd /opt/tools_conf/
ls -A | grep '^\.' | xargs -I {} cp -r {} ~/
source /opt/tools_conf/.bashrc.rpi

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rpi/install/lib/pkgconfig:/opt/cross_env/rpi/install/share/pkgconfig -DCMAKE_BUILD_TYPE=NoOptimize

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rpi/install/lib/pkgconfig:/opt/cross_env/rpi/install/share/pkgconfig -DCMAKE_BUILD_TYPE=Debug

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rpi/install/lib/pkgconfig:/opt/cross_env/rpi/install/share/pkgconfig -DCMAKE_BUILD_TYPE=Release