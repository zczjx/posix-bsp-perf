# AGENTS.zh-CN.md

> English version: [AGENTS.md](./AGENTS.md)（工具会自动读取英文版）。修改时请两个文件保持同步。

本文件指导 Agent 为 `posix-bsp-perf` 生成**正确的构建命令**。生成任何构建命令前，请先通读本文件。

命令的完整参考清单见 [reference_build_cmd.sh](./reference_build_cmd.sh)。该文件是**命令清单**，不是可以直接顺序执行的脚本（见下文"常见错误"）。

## 1. 基本规则

1. **推荐使用 Docker 构建环境**，默认按 Docker 容器内的流程生成构建命令。
  - 依赖库位于容器内的 `/opt/cross_env/<platform>/install`，宿主机上通常没有。
  - 如果当前不在容器内（例如没有 `/opt/cross_env`），先提示用户按第 2 节创建并进入容器，**不要**直接在宿主机上执行 `cmake`。
  - 用户也可以选择在本地 Ubuntu 上自行搭建环境并构建（不推荐，见第 2 节"本地环境"）。用户明确选择本地构建时，尊重其选择，不要强行改成 Docker。
2. **构建前必须确认目标平台**。目前主要支持 `jetson` 和 `rk3588s`，推荐在 x86 主机（Docker 容器）上**交叉编译**到这两个平台。`x86` 作为目标平台**不推荐**（大部分功能缺失，还可能有编译问题）。用户没有说明目标平台时先询问，不要猜，也不要默认选 x86。
3. **构建目录固定为项目根目录下的** `build/`，所有 `cmake` 命令都是 `cmake ..`，所以必须先 `mkdir -p build && cd build`。
4. 一次构建只选**一个** `CMAKE_BUILD_TYPE`，不要连续执行多条 `cmake`。
5. 不要修改 `/opt/cross_env/...` 下的内容，也不要修改 `tools_conf/` 里的环境文件，除非用户明确要求。



## 2. 容器环境



### 2.1 构建镜像并启动容器（推荐）

完整命令见 [get_build_env_docker.sh](./get_build_env_docker.sh)，日常只需要下面两条，**在项目根目录**执行：

```bash
# 1) 构建镜像（基于 zczjx/bsp-perf-build-env-x86-base:latest，使用根目录的 Dockerfile）
docker build --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) --build-arg DIR_SRC_PATH=tools_conf -t bsp-perf-build-env-x86:v0.0.1 -f Dockerfile .

# 2) 启动容器并进入 shell
docker run --network=host --gpus all --privileged --user $(id -u):$(id -g) -it -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v /build:/build -v /dev/bus/usb:/dev/bus/usb bsp-perf-build-env-x86:v0.0.1 /bin/bash
```

- 镜像 `bsp-perf-build-env-x86:v0.0.1` 已存在时，跳过第 1 条。
- 启动时必须用 `-v /build:/build` 把源码目录映射进容器。没有 GPU 时可去掉 `--gpus all`。
- 不要擅自修改这两条命令里的参数，尤其是 `USER_ID` / `GROUP_ID`（用于让容器内生成的文件属主与宿主机用户一致）。
- 已有容器用 `docker exec -it <container-id-or-name> /bin/bash` 进入。容器名需向用户确认（**待确认**：是否有固定容器名）。
- 容器内用户为 `builder`，有免密 sudo。
- 工程路径在容器内与宿主机一致（均在 `/build/...` 下）。

在容器内进入新终端后，每次构建前都要加载对应平台的环境，步骤见下一节。

### 2.2 本地环境（不推荐）

用户可以自行在本地 Ubuntu 上搭建环境并构建：

- 依赖安装可参考 [local_env_pkgs_intall.sh](./local_env_pkgs_intall.sh)（`libspdlog-dev`、CLI11 等）。
- 本地环境没有 `/opt/cross_env/...`，第 3、4 节中的 `BSP_*` 路径、`source /opt/tools_conf/...` 等需要由用户按本机实际路径调整。Agent 不要臆造路径，应先向用户确认依赖库的安装位置。
- 本地构建遇到问题，建议用户到 GitHub 提 issue：[https://github.com/zczjx/posix-bsp-perf/issues](https://github.com/zczjx/posix-bsp-perf/issues)。



## 3. 构建流程模板



### 3.1 初始化容器环境（进入容器后的第一步）

`/opt/tools_conf/` 是镜像内置的 shell 配置目录（与仓库中的 [tools_conf/](./tools_conf) 对应），里面有通用的 `.bashrc`，以及各平台的 `.bashrc.rk3588s`、`.bashrc.jetson` 等。进入容器后，先把这些配置拷贝到 `~/` 初始化环境，再 `source` 目标平台的配置：

```bash
cd /opt/tools_conf/
ls -A | grep '^\.' | xargs -I {} cp -r {} ~/     # 把所有以 . 开头的配置文件/目录拷贝到 ~/
source /opt/tools_conf/<BASHRC_FILE>              # 见 4 节平台表
```

要点：

- **拷贝**（第 2 行）：把 `.bashrc`、`.profile`、`.vimrc`、`.gitconfig` 等拷到 `~/`，会覆盖 `~/` 下同名文件。同一个容器里做一次即可，已经做过就不用重复。
- `source`：必须在**每个新开的终端**里执行，且选对目标平台的文件（x86 用 `.bashrc`，rk3588s 用 `.bashrc.rk3588s`，jetson 用 `.bashrc.jetson`）。它设置交叉编译器 `CC/CXX` 等环境变量，只对当前终端有效。
- **不要在同一个终端里先后** `source` **不同平台的配置**，环境变量会互相污染。切换平台时，新开一个终端。
- 在容器外的终端执行这些命令没有意义（`/opt/tools_conf` 只在容器内存在）。



### 3.2 cmake 配置与 make 编译

```bash
# 1) 回到工程根目录，创建并进入 build 目录
cd /build/clarencez/posix-bsp-perf            # 以实际工程路径为准
mkdir -p build && cd build

# 2) 配置（平台参数见第 4 节，三选一的 CMAKE_BUILD_TYPE 见第 5 节）
cmake .. <平台参数> -DCMAKE_BUILD_TYPE=<NoOptimize|Debug|Release>

# 3) 编译并安装（安装目录为 build/install）
make -j8 install
```

**make 命令规则**：

- 统一使用 `make -j8 install`：`-j8` 并行编译，`install` 把产物安装到 `build/install`。
- 不要省略 `install`，也不要改成裸 `make` 或 `make -j`（不限制并行数可能耗尽内存）。用户明确指定其他并行数时除外。
- 同样的写法也用于第三方库的构建（见第 7 节）。



## 4. 平台支持与参数对照表



### 4.1 平台支持情况


| 平台      | 支持状态     | 构建方式                                                                                       |
| ------- | -------- | ------------------------------------------------------------------------------------------ |
| jetson  | **主要支持** | 在 x86 主机的 Docker 容器内交叉编译（`aarch64-linux-gnu-`*）                                            |
| rk3588s | **主要支持** | 在 x86 主机的 Docker 容器内交叉编译（`aarch64-linux-gnu-`*）                                            |
| x86     | **不推荐**  | 大部分功能缺失（`bsp_dnn`、`bsp_g2d`、`bsp_codec`、`bsp_egl` 默认关闭），可能存在编译问题。用户明确要求时才生成 x86 命令，并提示以上风险 |




### 4.2 推荐的 cmake 配置

- **jetson**：推荐配置见 4.4 的 jetson 示例（`NoOptimize`，并开启 `BUILD_APP_DATA_RECORDER`）。
- **rk3588s**：推荐配置见 4.4 的 rk3588s 示例（`NoOptimize`，并开启 `BUILD_APP_DATA_RECORDER`）。
- 在推荐配置的基础上**只替换** `CMAKE_BUILD_TYPE`，其余参数保持不变：
  - 需要 gdb 调试：`-DCMAKE_BUILD_TYPE=Debug`
  - 需要 `-O2` 及以上优化：`-DCMAKE_BUILD_TYPE=Release`



### 4.3 参数对照表


| 平台       | `source` 的文件                                                       | 平台开关                         | 其他必需参数                                                                                          |
| -------- | ------------------------------------------------------------------ | ---------------------------- | ----------------------------------------------------------------------------------------------- |
| jetson   | `/opt/tools_conf/.bashrc.jetson`（设置 `CC/CXX=aarch64-linux-gnu-*`）  | `-DBUILD_PLATFORM_JETSON=ON` | `-DBSP_PKG_CONFIG_PATH=...` `-DBSP_INCLUDE_PATH=...` `-DBSP_LIB_PATH=...` `-DBSP_CUDA_PATH=...` |
| rk3588s  | `/opt/tools_conf/.bashrc.rk3588s`（设置 `CC/CXX=aarch64-linux-gnu-*`） | `-DBUILD_PLATFORM_RK35XX=ON` | `-DBSP_PKG_CONFIG_PATH=...` `-DBSP_LIB_PATH=...`                                                |
| x86（不推荐） | `/opt/tools_conf/.bashrc`，并额外 `export` 见下方                         | 无（不加任何 `BUILD_PLATFORM_*`）   | `-DBSP_PKG_CONFIG_PATH=...`                                                                     |


各平台根路径：


| 平台       | `<ROOT>`                                                     |
| -------- | ------------------------------------------------------------ |
| jetson   | `/opt/cross_env/nvidia/install`（注意目录名是 `nvidia`，不是 `jetson`） |
| rk3588s  | `/opt/cross_env/rk3588s/install`                             |
| x86（不推荐） | `/opt/cross_env/x86/install`                                 |


通用参数的取值：

- `BSP_PKG_CONFIG_PATH` = `<ROOT>/lib/pkgconfig:<ROOT>/share/pkgconfig`
- `BSP_LIB_PATH` = `<ROOT>/lib`
- `BSP_INCLUDE_PATH` = `<ROOT>/include`（仅 jetson）
- `BSP_CUDA_PATH` = `<ROOT>/cuda-12.6`（仅 jetson）

x86（不推荐）额外要导出的环境变量（x86 的 bashrc 不会设置它们）：

```bash
export LD_LIBRARY_PATH=/opt/cross_env/x86/install/lib:$LD_LIBRARY_PATH
export PATH=/opt/cross_env/x86/install/bin:$PATH
```



### 4.4 完整示例（推荐配置）

jetson（`source /opt/tools_conf/.bashrc.jetson` 之后）：

```bash
cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/nvidia/install/lib/pkgconfig:/opt/cross_env/nvidia/install/share/pkgconfig \
    -DBSP_INCLUDE_PATH=/opt/cross_env/nvidia/install/include \
    -DBSP_LIB_PATH=/opt/cross_env/nvidia/install/lib \
    -DBSP_CUDA_PATH=/opt/cross_env/nvidia/install/cuda-12.6 \
    -DBUILD_PLATFORM_JETSON=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=NoOptimize
```

rk3588s（`source /opt/tools_conf/.bashrc.rk3588s` 之后）：

```bash
cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rk3588s/install/lib/pkgconfig:/opt/cross_env/rk3588s/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/rk3588s/install/lib \
    -DBUILD_PLATFORM_RK35XX=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=NoOptimize
```

以上两条都是原样的推荐配置，gdb 调试时把末行改成 `-DCMAKE_BUILD_TYPE=Debug`，需要 `-O2` 优化时改成 `-DCMAKE_BUILD_TYPE=Release`。

x86（不推荐，仅用户明确要求时使用）：

```bash
cmake .. \
    -DBSP_PKG_CONFIG_PATH=/opt/cross_env/x86/install/lib/pkgconfig:/opt/cross_env/x86/install/share/pkgconfig \
    -DCMAKE_BUILD_TYPE=Debug
```



## 5. `CMAKE_BUILD_TYPE`

根 [CMakeLists.txt](./CMakeLists.txt) 只识别下面三个值，用别的值（包括 `RelWithDebInfo`、`MinSizeRel`）不会添加任何编译选项：


| 值            | 编译选项                             | 用途                       |
| ------------ | -------------------------------- | ------------------------ |
| `NoOptimize` | `-O0 -Wall -Wextra -fPIC`        | **推荐的默认配置**，无优化、无 `-g`   |
| `Debug`      | `-O0 -rdynamic -g -Wall -Wextra` | 需要 gdb 调试时使用             |
| `Release`    | `-O2 -Wall -Wextra`              | 需要 `-O2` 及以上优化时使用（如性能测试） |


用户没有指定时，使用 `NoOptimize`（即推荐配置）。用户提到"gdb 调试"选 `Debug`，提到"优化 / 性能"选 `Release`。

## 6. 可选的构建开关

默认值依赖平台，以各级 `CMakeLists.txt` 里的 `option()` 为准。


| 开关                                                                                                           | 位置                 | 默认                            | 说明                        |
| ------------------------------------------------------------------------------------------------------------ | ------------------ | ----------------------------- | ------------------------- |
| `BUILD_PLATFORM_RK35XX` / `BUILD_PLATFORM_JETSON`                                                            | 根 `CMakeLists.txt` | OFF                           | 平台开关，同一次构建只开一个            |
| `BUILD_APP_DATA_RECORDER`                                                                                    | `apps/`            | OFF                           | 数据录制应用，rk3588s / jetson 用 |
| `BUILD_SRC_BSP_DNN` / `_G2D` / `_CODEC`                                                                      | `bsp/`             | rk3588s、jetson 为 ON，x86 为 OFF | 依赖硬件加速库                   |
| `BUILD_SRC_BSP_EGL`                                                                                          | `bsp/`             | rk3588s、jetson 为 ON           | x86 分支里没有定义该选项            |
| `BUILD_SRC_FRAMEWORK` / `_SHARED` / `_IMAGE` / `_PROFILER` / `_BSP_SOCKETS` / `_BSP_CONTAINER` / `_PROTOCOL` | `bsp/`             | ON                            | 基础模块                      |
| `BUILD_SRC_CORE`                                                                                             | `bsp/`             | OFF                           |                           |
| `BUILD_FEATURE_SVS`                                                                                          | `features/`        | ON                            | 环视功能                      |
| `BUILD_MIDWARE_ZMQ_IPC`                                                                                      | `midware/`         | ON                            |                           |
| `BUILD_SAMPLES_BSP` / `_MIDWARE` / `_FEATURES`                                                               | `samples/`         | ON                            | 示例程序                      |


只构建部分模块时，用 `-DBUILD_xxx=OFF` 关闭不需要的模块，不要手改 `CMakeLists.txt`。

## 7. 第三方库构建（未全部验证）

`reference_build_cmd.sh` 里 spdlog、cpp-tbox、cli11 的构建只给出了配置参数，**并非全部验证过**：

- 容器内已预装 spdlog 和 CLI11，通常不需要重新构建。
- 需要重新构建时，使用 `-DCMAKE_INSTALL_PREFIX=/opt/cross_env/<x86|rk3588s>/install`，再 `make -j8 install`。
- 构建前同样要先 `source` 对应平台的 bashrc。



## 8. 常见错误（生成命令时必须避免）

1. **漏掉** `mkdir -p build && cd build`：所有命令都是 `cmake ..`，在根目录执行会污染源码树。
2. **把** `reference_build_cmd.sh` **里同一平台的三条** `cmake`**（NoOptimize/Debug/Release）连续执行**：它们是三选一。
3. **忘记** `source` **平台 bashrc**：交叉编译器（`aarch64-linux-gnu-gcc/g++`）依赖它设置 `CC/CXX`。
4. **x86 构建时加了** `BUILD_PLATFORM_`*，或 rk3588s / jetson 构建时漏了。
  - 另外，用户没说平台时不要默认生成 x86 命令（x86 不推荐，功能缺失）。
5. **jetson 的路径写成** `/opt/cross_env/jetson/...`：实际目录是 `/opt/cross_env/nvidia/...`。
6. **多行命令漏写行尾 `\`**：除最后一行外，每行都必须以 `\` 结尾；从 `reference_build_cmd.sh` 复制时要检查。
7. **切换平台或构建类型时复用旧的** `build/`：`CMakeCache.txt` 会缓存 `BUILD_PLATFORM_`*、`BSP_*` 和编译器。切换前应删除 `build/CMakeCache.txt`（或清空 `build/`），并**先向用户确认**。
8. **在宿主机执行构建命令**（宿主机通常没有 `/opt/cross_env`），除非用户明确选择了本地环境构建。



## 9. 构建产物

- 安装目录由根 `CMakeLists.txt` 固定为 `<build>/install`（即 `build/install`），不要用 `-DCMAKE_INSTALL_PREFIX` 覆盖。
- 清理：`make clean-all` 会执行 `clean` 并删除 `build/install`。



## 10. 待补充

以下信息尚未提供，请维护者补充后删除本节：

- [ ] 构建容器的固定名称（用于 `docker exec`）
- [ ] 构建成功的判定方式、运行测试 / 示例程序的方法
- [ ] 其他项目约定（代码风格、目录结构、提交规范等）