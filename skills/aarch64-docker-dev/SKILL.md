---
name: aarch64-docker-dev
description: Update and maintain AArch64 third-party libraries in an x86 Ubuntu Docker cross-compilation environment or local install prefix. Use when syncing RK3588 board libraries, refreshing Docker sysroots, copying raw board metadata locally, or rewriting pkg-config .pc paths for cross builds.
---

# AArch64 Docker Dev

## Purpose

Maintain AArch64 dependency install prefixes by copying selected libraries from an RK3588 Ubuntu board into a Docker cross-compilation environment and, when requested, into a local Ubuntu directory. Docker metadata is rewritten for Docker paths; local `metadata: raw` copies keep board `.pc` and `.cmake` contents unchanged.

## When To Use

Use this skill when the user asks to:

- Update Docker AArch64 cross-compilation libraries.
- Sync third-party libraries from an RK3588 board.
- Copy board libraries into a Docker-maintained sysroot.
- Copy board libraries into a local install prefix.
- Fix `.pc` pkg-config paths after copying AArch64 libraries.
- Verify `pkg-config --cflags --libs` in the cross build container.

## Required Inputs

Before applying changes, identify:

1. Config file path, usually `aarch64-libs.yaml`.
2. RK3588 SSH target, for example `ubuntu@192.168.1.100`.
3. Docker container name, for example `cross-build`.
4. Docker install prefix, for example `/opt/cross_env/rk3588s/install`.
5. Optional local install prefix, for example `/build/opt/cross_env/rk3588s/install`.
6. Target: `docker`, `local`, or `both`.
7. Library names to sync, for example `opencv4,drm,rga,mpp`.

If any value is missing, ask for it or use the matching value from the config file.

## Board Authentication

Prefer SSH key authentication when available. If the user wants to enter the board password once at runtime, use `password_env`:

```yaml
board:
  host: 192.168.2.119
  user: orange
  port: 22
  auth:
    method: password_env
    password_env: RK3588_SSH_PASSWORD
```

Before `--apply`, ask the user to set the password environment variable:

```bash
read -s RK3588_SSH_PASSWORD
export RK3588_SSH_PASSWORD
```

The script uses `sshpass -e` and passes the password as `SSHPASS` internally. Do not write plaintext passwords into YAML or command arguments.

## Workflow

1. Read the config file and determine the requested libraries.
2. Run a dry run first:

   ```bash
   python ~/.cursor/skills/aarch64-docker-dev/scripts/sync_aarch64_libs.py \
     --config aarch64-libs.yaml \
     --libs opencv4,drm \
     --staging /tmp/rk3588-aarch64-sync \
     --dry-run
   ```

3. Summarize the files to sync, Docker target, and `.pc` rewrite plan.
4. Only run `--apply` after the user confirms or explicitly asks to proceed:

   ```bash
   python ~/.cursor/skills/aarch64-docker-dev/scripts/sync_aarch64_libs.py \
     --config aarch64-libs.yaml \
     --libs opencv4,drm \
     --staging /tmp/rk3588-aarch64-sync \
     --apply
   ```

5. Choose the install target with `--target`:

   - `--target docker`: default; sync into Docker and rewrite `.pc/.cmake` for Docker paths.
   - `--target local`: sync into `local.prefix`; `metadata: raw` keeps `.pc/.cmake` unchanged.
   - `--target both`: copy raw metadata locally first, then rewrite staging for Docker.

6. If the user asks to remove old same-name libraries before installing, add `--clean-old`:

   ```bash
   python ~/.cursor/skills/aarch64-docker-dev/scripts/sync_aarch64_libs.py \
     --config aarch64-libs.yaml \
     --libs opencv4,drm \
     --target both \
     --staging /tmp/rk3588-aarch64-sync \
     --apply \
     --clean-old
   ```

7. Verify pkg-config inside Docker:

   ```bash
   python ~/.cursor/skills/aarch64-docker-dev/scripts/sync_aarch64_libs.py \
     --config aarch64-libs.yaml \
     --libs opencv4,drm \
     --verify
   ```

## Metadata Rewrite Rules

Rewrite only these pkg-config assignment keys unless the config explicitly requests path mapping:

- `prefix=`
- `exec_prefix=`
- `libdir=`
- `includedir=`

Use Docker install prefix paths:

- `prefix`: `<docker.prefix>`
- `exec_prefix`: `${prefix}`
- `libdir`: `<docker.libdir>`, usually `${prefix}/lib`
- `includedir`: `<docker.includedir>`, usually `${prefix}/include`

Preserve `Name`, `Description`, `Version`, `Requires`, `Libs`, and `Cflags` content. If `Libs:` or `Cflags:` contains board absolute paths, map `/usr/lib` entries to `<docker.libdir>` and `/usr/include` entries to `<docker.includedir>`.

For local targets:

- `metadata: raw`: copy `.pc` and `.cmake` unchanged from the board.
- `metadata: rewrite`: rewrite metadata to `<local.prefix>`, `<local.libdir>`, and `<local.includedir>`.

For CMake package files such as OpenCV's `OpenCVConfig.cmake` and `OpenCVModules.cmake`, rewrite board absolute paths to Docker install paths:

- `<prefix parent>/include` to `<docker.includedir>`
- `<prefix parent>/lib/aarch64-linux-gnu` to `<docker.libdir>`
- `/usr/include` to `<docker.includedir>`
- `/usr/lib/aarch64-linux-gnu` to `<docker.libdir>`

## Board To Install Path Mapping

When syncing from the RK3588 board:

- `/usr/lib/aarch64-linux-gnu/*` goes to `<docker.prefix>/lib/`.
- `/usr/include/*` goes to `<docker.prefix>/include/`.
- `/usr/lib/aarch64-linux-gnu/pkgconfig/*.pc` goes to `<docker.prefix>/lib/pkgconfig/`.
- `/usr/lib/aarch64-linux-gnu/cmake/*` goes to `<docker.prefix>/lib/cmake/`.

The same staging layout is copied to `<local.prefix>` when `--target local` or `--target both` is used.

## Cleaning Old Libraries

Use `--clean-old` only when the user explicitly asks to remove old same-name libraries before installing new ones.

The script infers cleanup patterns from selected library `board_paths`:

- `/usr/lib/aarch64-linux-gnu/libdrm*.so*` removes `<docker.libdir>/libdrm*.so*`.
- `/usr/lib/aarch64-linux-gnu/libopencv*.so*` removes `<docker.libdir>/libopencv*.so*`.

It only cleans library-like patterns under the selected target `libdir` values and does not delete include directories or unrelated files.

For this repository, a typical Docker section is:

```yaml
docker:
  container: funny_jepsen
  prefix: /opt/cross_env/rk3588s/install
  libdir: /opt/cross_env/rk3588s/install/lib
  includedir: /opt/cross_env/rk3588s/install/include
  pkgconfig_dirs:
    - /opt/cross_env/rk3588s/install/lib/pkgconfig
    - /opt/cross_env/rk3588s/install/share/pkgconfig

local:
  prefix: /build/opt/cross_env/rk3588s/install
  libdir: /build/opt/cross_env/rk3588s/install/lib
  includedir: /build/opt/cross_env/rk3588s/install/include
  metadata: raw
```

## Safety Rules

- Start with `--dry-run` unless the user explicitly requested applying changes.
- Do not delete existing Docker files unless the user explicitly asks; use `--clean-old` for same-name library cleanup.
- Keep `.pc.bak` backups when rewriting pkg-config files.
- Report failed `rsync`, `docker cp`, `docker exec`, and `pkg-config` commands with the exact command and error.
- Prefer updating only the requested libraries.

## Supporting Files

- Example config: `examples/config.example.yaml`
- Sync utility: `scripts/sync_aarch64_libs.py`
