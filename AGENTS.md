# AGENTS.md

> 中文版 / Chinese version: [AGENTS.zh-CN.md](./AGENTS.zh-CN.md). Keep both files in sync when editing.

This file tells agents how to generate **correct build commands** for `posix-bsp-perf`. Read it fully before generating any build command.

The full list of reference commands is in [reference_build_cmd.sh](./reference_build_cmd.sh). It is a **command cheat sheet**, not a script that can be run top to bottom (see "Common mistakes" below).

## 1. Ground rules

1. **Docker is the recommended build environment.** By default, generate commands that run inside the Docker build container.
   - Dependencies live inside the container at `/opt/cross_env/<platform>/install`; they are normally not present on the host.
   - If you are not inside the container (e.g. `/opt/cross_env` does not exist), tell the user to create and enter the container first (section 2). **Do not** run `cmake` directly on the host.
   - Users may also set up a local Ubuntu environment and build there (not recommended, see "Local environment" in section 2). If the user explicitly chooses a local build, respect that choice; do not force Docker.
2. **Always confirm the target platform before building.** The main supported platforms are `jetson` and `rk3588s`. The recommended way is to **cross-compile** for them on an x86 host (inside the Docker container). `x86` as a *target* platform is **not recommended** (most features are missing and there may be compile errors). If the user does not name a target platform, ask. Do not guess, and do not default to x86.
3. **The build directory is always `build/` under the project root.** Every `cmake` command is `cmake ..`, so you must `mkdir -p build && cd build` first.
4. Pick exactly **one** `CMAKE_BUILD_TYPE` per build. Never run several `cmake` commands back to back.
5. Do not modify anything under `/opt/cross_env/...`, and do not modify the environment files in `tools_conf/`, unless the user explicitly asks.

## 2. Container environment

### 2.1 Build the image and start the container (recommended)

See [get_build_env_docker.sh](./get_build_env_docker.sh) for the full command set. Day to day you only need the two commands below, run **from the project root**:

```bash
# 1) Build the image (based on zczjx/bsp-perf-build-env-x86-base:latest, using the Dockerfile in the project root)
docker build --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) --build-arg DIR_SRC_PATH=tools_conf -t bsp-perf-build-env-x86:v0.0.1 -f Dockerfile .

# 2) Start the container and open a shell
docker run --network=host --gpus all --privileged --user $(id -u):$(id -g) -it -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v /build:/build -v /dev/bus/usb:/dev/bus/usb bsp-perf-build-env-x86:v0.0.1 /bin/bash
```

- If the image `bsp-perf-build-env-x86:v0.0.1` already exists, skip step 1.
- You must mount the source directory with `-v /build:/build`. Drop `--gpus all` if the machine has no GPU.
- Do not change the parameters of these two commands on your own, especially `USER_ID` / `GROUP_ID` (they make files created in the container owned by the host user).
- To enter an existing container, use `docker exec -it <container-id-or-name> /bin/bash`. Ask the user for the container name (**TBD**: whether there is a fixed container name).
- The user inside the container is `builder`, with passwordless sudo.
- The project path is the same inside the container and on the host (both under `/build/...`).

Every new terminal inside the container must load the environment for the target platform before building; see the next section.

### 2.2 Local environment (not recommended)

Users may set up a local Ubuntu environment and build there:

- For dependency installation see [local_env_pkgs_intall.sh](./local_env_pkgs_intall.sh) (`libspdlog-dev`, CLI11, etc.).
- A local environment has no `/opt/cross_env/...`, so the `BSP_*` paths and `source /opt/tools_conf/...` in sections 3 and 4 must be adapted by the user to their machine. Do not invent paths; ask the user where the dependency libraries are installed.
- If a local build runs into problems, suggest that the user open a GitHub issue: <https://github.com/zczjx/posix-bsp-perf/issues>.

## 3. Build flow template

### 3.1 Initialize the container environment (first step after entering the container)

`/opt/tools_conf/` is the shell configuration directory baked into the image (it corresponds to [tools_conf/](./tools_conf) in this repo). It contains a generic `.bashrc` plus per-platform files such as `.bashrc.rk3588s` and `.bashrc.jetson`. After entering the container, first copy these configs to `~/`, then `source` the file for the target platform:

```bash
cd /opt/tools_conf/
ls -A | grep '^\.' | xargs -I {} cp -r {} ~/     # copy all dotfiles/dot-directories to ~/
source /opt/tools_conf/<BASHRC_FILE>              # see the platform table in section 4
```

Notes:

- **Copy** (line 2): copies `.bashrc`, `.profile`, `.vimrc`, `.gitconfig`, etc. to `~/`, overwriting files of the same name. Do it once per container; skip it if it was already done.
- **`source`**: must be run in **every new terminal**, with the file that matches the target platform (x86 uses `.bashrc`, rk3588s uses `.bashrc.rk3588s`, jetson uses `.bashrc.jetson`). It sets environment variables such as the cross compiler `CC/CXX`, and only affects the current terminal.
- **Never `source` configs for different platforms in the same terminal**; the environment variables will contaminate each other. Open a new terminal when switching platforms.
- Running these commands outside the container is pointless (`/opt/tools_conf` only exists inside the container).

### 3.2 cmake configure and make build

```bash
# 1) Go back to the project root, create and enter the build directory
cd /build/clarencez/posix-bsp-perf            # adjust to the actual project path
mkdir -p build && cd build

# 2) Configure (platform parameters: section 4; pick one CMAKE_BUILD_TYPE: section 5)
cmake .. <platform parameters> -DCMAKE_BUILD_TYPE=<NoOptimize|Debug|Release>

# 3) Build and install (install directory is build/install)
make -j8 install
```

**make rules:**

- Always use **`make -j8 install`**: `-j8` builds in parallel, `install` installs the artifacts into `build/install`.
- Do not drop `install`, and do not replace it with plain `make` or `make -j` (unbounded parallelism can exhaust memory). The exception is when the user explicitly asks for a different job count.
- Use the same form when building third-party libraries (see section 7).

## 4. Platform support and parameter reference

### 4.1 Platform support

| Platform | Status | How to build |
| --- | --- | --- |
| jetson | **Mainly supported** | Cross-compile inside the Docker container on an x86 host (`aarch64-linux-gnu-*`) |
| rk3588s | **Mainly supported** | Cross-compile inside the Docker container on an x86 host (`aarch64-linux-gnu-*`) |
| x86 | **Not recommended** | Most features are missing (`bsp_dnn`, `bsp_g2d`, `bsp_codec`, `bsp_egl` are off by default) and there may be compile errors. Only generate x86 commands when the user explicitly asks, and mention these risks |

### 4.2 Recommended cmake configuration

- **jetson**: see the jetson example in 4.4 (`NoOptimize`, with `BUILD_APP_DATA_RECORDER` enabled).
- **rk3588s**: see the rk3588s example in 4.4 (`NoOptimize`, with `BUILD_APP_DATA_RECORDER` enabled).
- On top of the recommended configuration, **only change `CMAKE_BUILD_TYPE`**; keep every other parameter as is:
  - For gdb debugging: `-DCMAKE_BUILD_TYPE=Debug`
  - For `-O2` or higher optimization: `-DCMAKE_BUILD_TYPE=Release`

### 4.3 Parameter reference

| Platform | File to `source` | Platform switch | Other required parameters |
| --- | --- | --- | --- |
| jetson | `/opt/tools_conf/.bashrc.jetson` (sets `CC/CXX=aarch64-linux-gnu-*`) | `-DBUILD_PLATFORM_JETSON=ON` | `-DBSP_PKG_CONFIG_PATH=...` `-DBSP_INCLUDE_PATH=...` `-DBSP_LIB_PATH=...` `-DBSP_CUDA_PATH=...` |
| rk3588s | `/opt/tools_conf/.bashrc.rk3588s` (sets `CC/CXX=aarch64-linux-gnu-*`) | `-DBUILD_PLATFORM_RK35XX=ON` | `-DBSP_PKG_CONFIG_PATH=...` `-DBSP_LIB_PATH=...` |
| x86 (not recommended) | `/opt/tools_conf/.bashrc`, plus extra `export`s below | none (do not pass any `BUILD_PLATFORM_*`) | `-DBSP_PKG_CONFIG_PATH=...` |

Root path per platform:

| Platform | `<ROOT>` |
| --- | --- |
| jetson | `/opt/cross_env/nvidia/install` (note: the directory is named `nvidia`, not `jetson`) |
| rk3588s | `/opt/cross_env/rk3588s/install` |
| x86 (not recommended) | `/opt/cross_env/x86/install` |

Parameter values:

- `BSP_PKG_CONFIG_PATH` = `<ROOT>/lib/pkgconfig:<ROOT>/share/pkgconfig`
- `BSP_LIB_PATH` = `<ROOT>/lib`
- `BSP_INCLUDE_PATH` = `<ROOT>/include` (jetson only)
- `BSP_CUDA_PATH` = `<ROOT>/cuda-12.6` (jetson only)

Extra environment variables for x86 (not recommended); the x86 bashrc does not set them:

```bash
export LD_LIBRARY_PATH=/opt/cross_env/x86/install/lib:$LD_LIBRARY_PATH
export PATH=/opt/cross_env/x86/install/bin:$PATH
```

### 4.4 Complete examples (recommended configurations)

jetson (after `source /opt/tools_conf/.bashrc.jetson`):

```bash
cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/nvidia/install/lib/pkgconfig:/opt/cross_env/nvidia/install/share/pkgconfig \
    -DBSP_INCLUDE_PATH=/opt/cross_env/nvidia/install/include \
    -DBSP_LIB_PATH=/opt/cross_env/nvidia/install/lib \
    -DBSP_CUDA_PATH=/opt/cross_env/nvidia/install/cuda-12.6 \
    -DBUILD_PLATFORM_JETSON=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=NoOptimize
```

rk3588s (after `source /opt/tools_conf/.bashrc.rk3588s`):

```bash
cmake .. -DBSP_PKG_CONFIG_PATH=/opt/cross_env/rk3588s/install/lib/pkgconfig:/opt/cross_env/rk3588s/install/share/pkgconfig \
    -DBSP_LIB_PATH=/opt/cross_env/rk3588s/install/lib \
    -DBUILD_PLATFORM_RK35XX=ON \
    -DBUILD_APP_DATA_RECORDER=ON \
    -DCMAKE_BUILD_TYPE=NoOptimize
```

Both are the recommended configurations as is. For gdb debugging change the last line to `-DCMAKE_BUILD_TYPE=Debug`; for `-O2` optimization change it to `-DCMAKE_BUILD_TYPE=Release`.

x86 (not recommended; only when the user explicitly asks):

```bash
cmake .. \
    -DBSP_PKG_CONFIG_PATH=/opt/cross_env/x86/install/lib/pkgconfig:/opt/cross_env/x86/install/share/pkgconfig \
    -DCMAKE_BUILD_TYPE=Debug
```

## 5. `CMAKE_BUILD_TYPE`

The root [CMakeLists.txt](./CMakeLists.txt) only recognizes the three values below. Any other value (including `RelWithDebInfo` and `MinSizeRel`) adds no compile flags at all:

| Value | Compile flags | Purpose |
| --- | --- | --- |
| `NoOptimize` | `-O0 -Wall -Wextra -fPIC` | **Recommended default**; no optimization, no `-g` |
| `Debug` | `-O0 -rdynamic -g -Wall -Wextra` | Use when gdb debugging is needed |
| `Release` | `-O2 -Wall -Wextra` | Use when `-O2` or higher optimization is needed (e.g. performance tests) |

If the user does not specify one, use `NoOptimize` (the recommended configuration). If the user mentions "gdb debugging", use `Debug`; if they mention "optimization / performance", use `Release`.

## 6. Optional build switches

Defaults depend on the platform; the `option()` calls in the various `CMakeLists.txt` files are authoritative.

| Switch | Location | Default | Notes |
| --- | --- | --- | --- |
| `BUILD_PLATFORM_RK35XX` / `BUILD_PLATFORM_JETSON` | root `CMakeLists.txt` | OFF | Platform switch; enable at most one per build |
| `BUILD_APP_DATA_RECORDER` | `apps/` | OFF | Data recorder app; used on rk3588s / jetson |
| `BUILD_SRC_BSP_DNN` / `_G2D` / `_CODEC` | `bsp/` | ON for rk3588s and jetson, OFF for x86 | Depend on hardware acceleration libraries |
| `BUILD_SRC_BSP_EGL` | `bsp/` | ON for rk3588s and jetson | Not defined in the x86 branch |
| `BUILD_SRC_FRAMEWORK` / `_SHARED` / `_IMAGE` / `_PROFILER` / `_BSP_SOCKETS` / `_BSP_CONTAINER` / `_PROTOCOL` | `bsp/` | ON | Base modules |
| `BUILD_SRC_CORE` | `bsp/` | OFF | |
| `BUILD_FEATURE_SVS` | `features/` | ON | Surround view feature |
| `BUILD_MIDWARE_ZMQ_IPC` | `midware/` | ON | |
| `BUILD_SAMPLES_BSP` / `_MIDWARE` / `_FEATURES` | `samples/` | ON | Sample programs |

To build only some modules, turn off the unneeded ones with `-DBUILD_xxx=OFF`. Do not edit `CMakeLists.txt` by hand.

## 7. Third-party library builds (not all validated)

The spdlog, cpp-tbox and cli11 builds in `reference_build_cmd.sh` only list configure parameters and are **not all validated**:

- spdlog and CLI11 are preinstalled in the container, so there is normally no need to rebuild them.
- To rebuild, use `-DCMAKE_INSTALL_PREFIX=/opt/cross_env/<x86|rk3588s>/install`, then `make -j8 install`.
- As before, `source` the matching platform bashrc first.

## 8. Common mistakes (avoid these when generating commands)

1. **Forgetting `mkdir -p build && cd build`**: every command is `cmake ..`; running it in the project root pollutes the source tree.
2. **Running the three `cmake` commands (NoOptimize/Debug/Release) of one platform in `reference_build_cmd.sh` back to back**: they are alternatives, pick one.
3. **Forgetting to `source` the platform bashrc**: the cross compiler (`aarch64-linux-gnu-gcc/g++`) depends on it to set `CC/CXX`.
4. **Passing `BUILD_PLATFORM_*` for x86, or omitting it for rk3588s / jetson.**
   - Also, if the user does not name a platform, do not default to generating x86 commands (x86 is not recommended and lacks features).
5. **Writing the jetson path as `/opt/cross_env/jetson/...`**: the real directory is `/opt/cross_env/nvidia/...`.
6. **Missing trailing `\` in multi-line commands**: every line except the last must end with `\`; check this when copying from `reference_build_cmd.sh`.
7. **Reusing an old `build/` when switching platform or build type**: `CMakeCache.txt` caches `BUILD_PLATFORM_*`, `BSP_*` and the compiler. Delete `build/CMakeCache.txt` (or empty `build/`) before switching, and **ask the user first**.
8. **Running build commands on the host** (the host normally has no `/opt/cross_env`), unless the user explicitly chose a local-environment build.

## 9. Build output

- The install directory is fixed to `<build>/install` (i.e. `build/install`) by the root `CMakeLists.txt`. Do not override it with `-DCMAKE_INSTALL_PREFIX`.
- Cleaning: `make clean-all` runs `clean` and deletes `build/install`.

## 10. TODO

The following information has not been provided yet. Maintainers, please fill it in and then delete this section:

- [ ] Fixed name of the build container (for `docker exec`)
- [ ] How to tell a build succeeded, and how to run tests / sample programs
- [ ] Other project conventions (code style, directory layout, commit conventions, etc.)
