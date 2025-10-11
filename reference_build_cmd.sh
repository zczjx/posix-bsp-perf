#!/bin/bash

# build posix-bsp-perf in docker container

    ## for X86

cd /opt/tools_conf/
ls -A | grep '^\.' | xargs -I {} cp -r {} ~/
source /opt/tools_conf/.bashrc

export LD_LIBRARY_PATH=/opt/cross_env/x86/install/lib:$LD_LIBRARY_PATH
export PATH=/opt/cross_env/x86/install/bin:$PATH

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/x86/install/lib/pkgconfig:/opt/cross_env/x86/install/share/pkgconfig \
    -DCMAKE_BUILD_TYPE=NoOptimize

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/x86/install/lib/pkgconfig:/opt/cross_env/x86/install/share/pkgconfig \
    -DCMAKE_BUILD_TYPE=Debug

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/x86/install/lib/pkgconfig:/opt/cross_env/x86/install/share/pkgconfig \
    -DCMAKE_BUILD_TYPE=Release


make -j8 install

    ## for rk3588s

cd /opt/tools_conf/
ls -A | grep '^\.' | xargs -I {} cp -r {} ~/
source /opt/tools_conf/.bashrc.rk3588s

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rk3588s/install/lib/pkgconfig:/opt/cross_env/rk3588s/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/rk3588s/install/lib \
    -DBUILD_PLATFORM_RK35XX=ON \
    -DENABLE_RK_MPP=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=NoOptimize

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rk3588s/install/lib/pkgconfig:/opt/cross_env/rk3588s/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/rk3588s/install/lib \
    -DBUILD_PLATFORM_RK35XX=ON \
    -DENABLE_RK_MPP=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=Debug

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rk3588s/install/lib/pkgconfig:/opt/cross_env/rk3588s/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/rk3588s/install/lib \
    -DBUILD_PLATFORM_RK35XX=ON \
    -DENABLE_RK_MPP=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=Release

    ## for Jetson

cd /opt/tools_conf/
ls -A | grep '^\.' | xargs -I {} cp -r {} ~/
source /opt/tools_conf/.bashrc.jetson

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/nvidia/install/lib/pkgconfig:/opt/cross_env/nvidia/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/nvidia/install/lib \
    -DBUILD_PLATFORM_JETSON=ON \
    -DCMAKE_BUILD_TYPE=NoOptimize

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/nvidia/install/lib/pkgconfig:/opt/cross_env/nvidia/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/nvidia/install/lib \
    -DBUILD_PLATFORM_JETSON=ON \
    -DCMAKE_BUILD_TYPE=NoOptimize

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/nvidia/install/lib/pkgconfig:/opt/cross_env/nvidia/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/nvidia/install/lib \
    -DBUILD_PLATFORM_JETSON=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=Debug

cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/nvidia/install/lib/pkgconfig:/opt/cross_env/nvidia/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/nvidia/install/lib \
    -DBUILD_PLATFORM_JETSON=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=Release


# 3rdparty build, not all validated
## build spdlog
    ### for x86
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/x86/install -DSPDLOG_BUILD_BENCH=ON -DSPDLOG_BUILD_SHARED=ON

    ### for rk3588s
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/rk3588s/install -DSPDLOG_BUILD_BENCH=ON -DSPDLOG_BUILD_SHARED=ON

make -j8 install

## build cpp-tbox
    ### for x86
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/x86/install

    ### for rk3588s
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/rk3588s/install

make -j8 install

# build cli11

    ### for x86
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/x86/install
    ###  for rk3588s
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cross_env/rk3588s/install

make -j8 install