#!/usr/bin/env python3
"""Sync selected RK3588 AArch64 libraries into a Docker cross sysroot."""

from __future__ import annotations

import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


DEFAULT_STAGING = "/tmp/rk3588-aarch64-sync"


def load_config(path: Path) -> dict[str, Any]:
    try:
        import yaml  # type: ignore
    except ImportError:
        return load_config_without_pyyaml(path)

    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError(f"Config must be a mapping: {path}")
    return data


def load_config_without_pyyaml(path: Path) -> dict[str, Any]:
    """Parse the small YAML subset used by config.example.yaml."""
    data: dict[str, Any] = {}
    current_section: str | None = None
    current_library: dict[str, Any] | None = None
    current_list_key: str | None = None
    current_nested_key: str | None = None

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("#", 1)[0].rstrip()
        if not line:
            continue

        indent = len(line) - len(line.lstrip(" "))
        stripped = line.strip()

        if indent == 0 and stripped.endswith(":"):
            current_section = stripped[:-1]
            data[current_section] = [] if current_section == "libraries" else {}
            current_library = None
            current_list_key = None
            current_nested_key = None
            continue

        if current_section is None:
            raise ValueError(f"Invalid config line before section: {raw_line}")

        if current_section == "libraries":
            if indent == 2 and stripped.startswith("- "):
                current_library = {}
                data["libraries"].append(current_library)
                key, value = parse_key_value(stripped[2:])
                current_library[key] = value
                current_list_key = None
            elif indent == 4 and current_library is not None:
                key, value = parse_key_value(stripped)
                if value is None:
                    current_library[key] = []
                    current_list_key = key
                else:
                    current_library[key] = value
                    current_list_key = None
            elif indent == 6 and stripped.startswith("- ") and current_library is not None:
                if current_list_key is None:
                    raise ValueError(f"List item without list key: {raw_line}")
                current_library[current_list_key].append(parse_scalar(stripped[2:]))
            else:
                raise ValueError(f"Unsupported libraries YAML line: {raw_line}")
            continue

        section = data[current_section]
        if not isinstance(section, dict):
            raise ValueError(f"Section must be a mapping: {current_section}")

        if indent == 2:
            key, value = parse_key_value(stripped)
            if value is None:
                if key == "auth":
                    section[key] = {}
                    current_nested_key = key
                    current_list_key = None
                else:
                    section[key] = []
                    current_list_key = key
                    current_nested_key = None
            else:
                section[key] = value
                current_list_key = None
                current_nested_key = None
        elif indent == 4 and stripped.startswith("- "):
            if current_list_key is None:
                raise ValueError(f"List item without list key: {raw_line}")
            section[current_list_key].append(parse_scalar(stripped[2:]))
        elif indent == 4 and current_nested_key is not None:
            nested = section[current_nested_key]
            if not isinstance(nested, dict):
                raise ValueError(f"Nested key must be a mapping: {current_nested_key}")
            key, value = parse_key_value(stripped)
            nested[key] = value
        else:
            raise ValueError(f"Unsupported YAML line: {raw_line}")

    return data


def parse_key_value(text: str) -> tuple[str, Any]:
    if ":" not in text:
        raise ValueError(f"Expected key/value: {text}")
    key, value = text.split(":", 1)
    value = value.strip()
    return key.strip(), parse_scalar(value) if value else None


def parse_scalar(value: str) -> Any:
    value = value.strip()
    if value in {"true", "True"}:
        return True
    if value in {"false", "False"}:
        return False
    if value.isdigit():
        return int(value)
    return value.strip("\"'")


def shell_join(command: list[str]) -> str:
    return shlex.join(command)


def run(
    command: list[str],
    *,
    dry_run: bool,
    env: dict[str, str] | None = None,
    display_env: dict[str, str] | None = None,
) -> None:
    if display_env:
        env_prefix = " ".join(f"{key}={value}" for key, value in display_env.items())
        print(f"+ {env_prefix} {shell_join(command)}")
    else:
        print(f"+ {shell_join(command)}")

    if dry_run:
        return
    run_env = os.environ.copy()
    if env:
        run_env.update(env)
    subprocess.run(command, check=True, env=run_env)


def require_mapping(config: dict[str, Any], key: str) -> dict[str, Any]:
    value = config.get(key)
    if not isinstance(value, dict):
        raise ValueError(f"Config section `{key}` must be a mapping")
    return value


def require_libraries(config: dict[str, Any]) -> list[dict[str, Any]]:
    libraries = config.get("libraries")
    if not isinstance(libraries, list):
        raise ValueError("Config section `libraries` must be a list")
    for item in libraries:
        if not isinstance(item, dict) or "name" not in item:
            raise ValueError("Each library entry must contain `name`")
    return libraries


def selected_libraries(config: dict[str, Any], names: str | None) -> list[dict[str, Any]]:
    libraries = require_libraries(config)
    if not names:
        return libraries
    wanted = {name.strip() for name in names.split(",") if name.strip()}
    selected = [lib for lib in libraries if str(lib["name"]) in wanted]
    missing = wanted - {str(lib["name"]) for lib in selected}
    if missing:
        raise ValueError(f"Unknown libraries requested: {', '.join(sorted(missing))}")
    return selected


def board_ssh(board: dict[str, Any]) -> tuple[str, int, str | None]:
    host = str(board.get("host", "")).strip()
    user = str(board.get("user", "")).strip()
    port = int(board.get("port", 22))
    identity_file = board.get("identity_file")
    if not host:
        raise ValueError("board.host is required")
    target = f"{user}@{host}" if user else host
    return target, port, str(identity_file) if identity_file else None


def ssh_command(port: int, identity_file: str | None) -> str:
    command = ["ssh", "-p", str(port)]
    if identity_file:
        command.extend(["-i", str(Path(identity_file).expanduser())])
    return shell_join(command)


def rsync_auth_prefix(
    board: dict[str, Any],
    *,
    dry_run: bool,
) -> tuple[list[str], dict[str, str] | None, dict[str, str] | None]:
    auth = board.get("auth")
    if auth is None:
        return [], None, None
    if not isinstance(auth, dict):
        raise ValueError("board.auth must be a mapping")

    method = str(auth.get("method", "")).strip()
    if method != "password_env":
        raise ValueError(f"Unsupported board.auth.method: {method}")

    password_env = str(auth.get("password_env", "RK3588_SSH_PASSWORD")).strip()
    if not password_env:
        raise ValueError("board.auth.password_env is required for password_env auth")
    if not dry_run and password_env not in os.environ:
        raise ValueError(
            f"Environment variable {password_env} is not set. "
            f"Run: read -s {password_env}; export {password_env}"
        )
    if not dry_run and shutil.which("sshpass") is None:
        raise ValueError("sshpass is required for board.auth.method=password_env")

    return (
        ["sshpass", "-e"],
        {"SSHPASS": os.environ.get(password_env, "")},
        {"SSHPASS": f"<from {password_env}>"},
    )


def docker_paths(docker: dict[str, Any]) -> dict[str, str]:
    prefix = str(docker.get("prefix") or "").rstrip("/")

    if not prefix:
        sysroot = str(docker.get("sysroot") or "").rstrip("/")
        if not sysroot:
            raise ValueError("docker.prefix is required")
        prefix = f"{sysroot}/usr"

    default_libdir = f"{prefix}/{str(docker.get('libdir_suffix', 'lib')).strip('/')}"
    return {
        "prefix": prefix,
        "libdir": str(docker.get("libdir") or default_libdir).rstrip("/"),
        "includedir": str(docker.get("includedir") or f"{prefix}/include").rstrip("/"),
    }


def staging_dest_for_board_path(board_path: str, staging: Path) -> Path:
    path = board_path.strip()
    if "/pkgconfig/" in path:
        return staging / "lib" / "pkgconfig"
    if "/cmake/" in path and (path.startswith("/usr/lib/") or path.startswith("/lib/")):
        return staging / "lib" / "cmake"
    if path.startswith("/usr/lib/") or path.startswith("/lib/"):
        return staging / "lib"
    if path.startswith("/usr/include/"):
        return staging / "include"
    if path.startswith("/usr/share/pkgconfig/"):
        return staging / "share" / "pkgconfig"
    return staging


def cleanup_patterns_for_libraries(libraries: list[dict[str, Any]]) -> list[str]:
    patterns: set[str] = set()
    for lib in libraries:
        for board_path in lib.get("board_paths", []):
            path = str(board_path).strip()
            if not (path.startswith("/usr/lib/") or path.startswith("/lib/")):
                continue
            pattern = Path(path).name
            if not is_library_cleanup_pattern(pattern):
                continue
            patterns.add(pattern)
    return sorted(patterns)


def is_library_cleanup_pattern(pattern: str) -> bool:
    allowed = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-*+")
    if not pattern or any(char not in allowed for char in pattern):
        raise ValueError(f"Unsafe cleanup pattern: {pattern}")
    return pattern.startswith("lib") and any(suffix in pattern for suffix in (".so", ".a", ".la"))


def sync_from_board(
    config: dict[str, Any],
    libraries: list[dict[str, Any]],
    staging: Path,
    *,
    dry_run: bool,
) -> None:
    board = require_mapping(config, "board")
    target, port, identity_file = board_ssh(board)
    auth_prefix, auth_env, display_env = rsync_auth_prefix(board, dry_run=dry_run)
    if not dry_run:
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir(parents=True, exist_ok=True)

    for lib in libraries:
        for board_path in lib.get("board_paths", []):
            dest = staging_dest_for_board_path(str(board_path), staging)
            if not dry_run:
                dest.mkdir(parents=True, exist_ok=True)
            remote = f"{target}:{board_path}"
            command = [
                *auth_prefix,
                "rsync",
                "-avz",
                "-e",
                ssh_command(port, identity_file),
                remote,
                str(dest),
            ]
            run(command, dry_run=dry_run, env=auth_env, display_env=display_env)


def rewrite_pc_files(config: dict[str, Any], staging: Path, *, dry_run: bool) -> None:
    docker = require_mapping(config, "docker")
    paths = docker_paths(docker)
    replacements = {
        "prefix": paths["prefix"],
        "exec_prefix": "${prefix}",
        "libdir": paths["libdir"].replace(paths["prefix"], "${prefix}", 1),
        "includedir": paths["includedir"].replace(paths["prefix"], "${prefix}", 1),
    }

    if dry_run:
        print("* dry-run: .pc files copied into staging will be rewritten")
        return

    pc_files = sorted(staging.rglob("*.pc"))
    if not pc_files:
        print(f"No .pc files found under {staging}")
        return

    for pc_file in pc_files:
        original = pc_file.read_text(encoding="utf-8")
        rewritten = rewrite_pc_text(original, replacements, paths)
        if original == rewritten:
            print(f"= {pc_file} unchanged")
            continue

        print(f"* rewrite {pc_file}")
        if dry_run:
            continue

        backup = pc_file.with_suffix(pc_file.suffix + ".bak")
        if not backup.exists():
            shutil.copy2(pc_file, backup)
        pc_file.write_text(rewritten, encoding="utf-8")


def rewrite_pc_text(text: str, replacements: dict[str, str], paths: dict[str, str]) -> str:
    lines: list[str] = []
    for line in text.splitlines(keepends=True):
        newline = "\n" if line.endswith("\n") else ""
        body = line[:-1] if newline else line
        key = body.split("=", 1)[0].strip() if "=" in body else ""

        if key in replacements and not body.lstrip().startswith("#"):
            lines.append(f"{key}={replacements[key]}{newline}")
            continue

        if body.startswith(("Libs:", "Cflags:")):
            body = map_absolute_paths(body, paths)
        lines.append(f"{body}{newline}")
    return "".join(lines)


def map_absolute_paths(text: str, paths: dict[str, str]) -> str:
    return (
        text.replace("-L/usr/lib/aarch64-linux-gnu", f"-L{paths['libdir']}")
        .replace("-L/usr/lib", f"-L{paths['libdir']}")
        .replace("-I/usr/include", f"-I{paths['includedir']}")
    )


def rewrite_cmake_files(config: dict[str, Any], staging: Path, *, dry_run: bool) -> None:
    docker = require_mapping(config, "docker")
    paths = docker_paths(docker)
    prefix_parent = str(Path(paths["prefix"]).parent).rstrip("/")

    if dry_run:
        print("* dry-run: .cmake files copied into staging will be path-rewritten")
        return

    cmake_files = sorted(staging.rglob("*.cmake"))
    if not cmake_files:
        print(f"No .cmake files found under {staging}")
        return

    for cmake_file in cmake_files:
        original = cmake_file.read_text(encoding="utf-8")
        rewritten = rewrite_cmake_text(original, paths, prefix_parent)
        if original == rewritten:
            print(f"= {cmake_file} unchanged")
            continue

        print(f"* rewrite {cmake_file}")
        backup = cmake_file.with_suffix(cmake_file.suffix + ".bak")
        if not backup.exists():
            shutil.copy2(cmake_file, backup)
        cmake_file.write_text(rewritten, encoding="utf-8")


def rewrite_cmake_text(text: str, paths: dict[str, str], prefix_parent: str) -> str:
    replacements = {
        'get_filename_component(OpenCV_INSTALL_PATH "${OpenCV_CONFIG_PATH}/../../../../" REALPATH)': (
            f'set(OpenCV_INSTALL_PATH "{paths["prefix"]}")'
        ),
        "${_IMPORT_PREFIX}/lib/aarch64-linux-gnu": "${_IMPORT_PREFIX}/lib",
        f"{prefix_parent}/include/opencv4": f"{paths['includedir']}/opencv4",
        f"{prefix_parent}/include": paths["includedir"],
        f"{prefix_parent}/lib/aarch64-linux-gnu": paths["libdir"],
        f"{prefix_parent}/lib": paths["libdir"],
        "/usr/include/opencv4": f"{paths['includedir']}/opencv4",
        "/usr/include": paths["includedir"],
        "/usr/lib/aarch64-linux-gnu": paths["libdir"],
        "/usr/lib": paths["libdir"],
    }

    rewritten = text
    for source, target in replacements.items():
        rewritten = rewritten.replace(source, target)
    rewritten = rewrite_import_prefix_parent_walk(rewritten)
    return rewritten


def rewrite_import_prefix_parent_walk(text: str) -> str:
    four_parent_walk = "\n".join(
        ['get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)'] * 4
    )
    three_parent_walk = "\n".join(
        ['get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)'] * 3
    )
    return text.replace(four_parent_walk, three_parent_walk)


def copy_to_docker(config: dict[str, Any], staging: Path, *, dry_run: bool) -> None:
    docker = require_mapping(config, "docker")
    container = str(docker.get("container", "")).strip()
    prefix = docker_paths(docker)["prefix"]
    if not container:
        raise ValueError("docker.container is required")

    run(["docker", "exec", container, "mkdir", "-p", prefix], dry_run=dry_run)
    run(["docker", "cp", f"{staging}/.", f"{container}:{prefix}/"], dry_run=dry_run)


def clean_old_libraries(
    config: dict[str, Any],
    libraries: list[dict[str, Any]],
    *,
    dry_run: bool,
) -> None:
    docker = require_mapping(config, "docker")
    container = str(docker.get("container", "")).strip()
    libdir = docker_paths(docker)["libdir"]
    if not container:
        raise ValueError("docker.container is required")

    patterns = cleanup_patterns_for_libraries(libraries)
    if not patterns:
        print("No old library cleanup patterns inferred")
        return

    rm_targets = " ".join(f"{shlex.quote(libdir)}/{pattern}" for pattern in patterns)
    run(["docker", "exec", container, "sh", "-lc", f"rm -f -- {rm_targets}"], dry_run=dry_run)


def verify_pkg_config(config: dict[str, Any], libraries: list[dict[str, Any]], *, dry_run: bool) -> None:
    docker = require_mapping(config, "docker")
    container = str(docker.get("container", "")).strip()
    paths = docker_paths(docker)
    pkgconfig_dirs = docker.get("pkgconfig_dirs", [])
    if not container:
        raise ValueError("docker.container is required")
    if not isinstance(pkgconfig_dirs, list) or not pkgconfig_dirs:
        pkgconfig_dirs = [
            f"{paths['libdir']}/pkgconfig",
            f"{paths['prefix']}/share/pkgconfig",
        ]

    env = [f"PKG_CONFIG_LIBDIR={':'.join(str(item) for item in pkgconfig_dirs)}"]
    pkg_config_sysroot_dir = docker.get("pkg_config_sysroot_dir")
    if pkg_config_sysroot_dir:
        env.append(f"PKG_CONFIG_SYSROOT_DIR={pkg_config_sysroot_dir}")

    for lib in libraries:
        names = lib.get("pkgconfig_names") or [lib["name"]]
        for pkg_name in names:
            command = [
                "docker",
                "exec",
                *sum((["-e", item] for item in env), []),
                container,
                "sh",
                "-lc",
                f"pkg-config --modversion {shlex.quote(str(pkg_name))} && "
                f"pkg-config --cflags --libs {shlex.quote(str(pkg_name))}",
            ]
            run(command, dry_run=dry_run)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, type=Path, help="Path to aarch64-libs.yaml")
    parser.add_argument("--libs", help="Comma-separated library names. Defaults to all libraries.")
    parser.add_argument("--staging", type=Path, default=Path(DEFAULT_STAGING), help="Local staging directory")
    parser.add_argument(
        "--clean-old",
        action="store_true",
        help="Remove matching old library files from docker.libdir before docker cp",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="Print planned commands and rewrites")
    mode.add_argument("--apply", action="store_true", help="Sync, rewrite .pc files, and copy into Docker")
    mode.add_argument("--verify", action="store_true", help="Run pkg-config verification inside Docker")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    config = load_config(args.config)
    libraries = selected_libraries(config, args.libs)
    dry_run = args.dry_run or not args.apply

    if args.verify:
        verify_pkg_config(config, libraries, dry_run=False)
        return 0

    print(f"Config: {args.config}")
    print(f"Staging: {args.staging}")
    print("Libraries: " + ", ".join(str(lib["name"]) for lib in libraries))
    print("Mode: " + ("apply" if args.apply else "dry-run"))

    sync_from_board(config, libraries, args.staging, dry_run=dry_run)
    rewrite_pc_files(config, args.staging, dry_run=dry_run)
    rewrite_cmake_files(config, args.staging, dry_run=dry_run)
    if args.clean_old:
        clean_old_libraries(config, libraries, dry_run=dry_run)
    copy_to_docker(config, args.staging, dry_run=dry_run)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except (subprocess.CalledProcessError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
