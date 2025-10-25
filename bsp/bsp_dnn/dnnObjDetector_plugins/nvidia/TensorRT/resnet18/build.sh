#!/bin/bash
#
# ResNet18 TrafficCamNet Plugin 编译脚本
#

set -e  # 遇到错误立即退出

echo "====================================="
echo "ResNet18 TrafficCamNet Plugin 编译"
echo "====================================="
echo ""

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# 创建构建目录
if [ -d "build" ]; then
    echo "清理旧的构建目录..."
    rm -rf build
fi

echo "创建构建目录..."
mkdir -p build
cd build

# 运行CMake
echo ""
echo "运行CMake配置..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=../install

# 编译
echo ""
echo "开始编译..."
make -j$(nproc)

# 安装
echo ""
echo "安装插件..."
make install

echo ""
echo "====================================="
echo "编译完成！"
echo "====================================="
echo ""
echo "生成的文件:"
echo "  共享库: build/libresnet18_trafficcamnet.so"
echo "  安装路径: install/lib/libresnet18_trafficcamnet.so"
echo ""
echo "使用方法:"
echo "  1. 将 libresnet18_trafficcamnet.so 复制到应用程序目录"
echo "  2. 使用 dlopen() 加载插件"
echo "  3. 参考 example_usage.cpp 了解详细用法"
echo ""

