#!/usr/bin/env python3
"""One-command build for NetgenMshExporter.

Examples:
  python build.py --target macos
  python build.py --target macos --arch universal
  python build.py --target win32
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys


NETGEN_URL = "https://github.com/NGSolve/netgen"
ZLIB_URL = "https://github.com/madler/zlib"


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(1)


def run(command: list[str], cwd: Path | None = None) -> None:
    where = f" ({cwd})" if cwd else ""
    print(f"+{where} {' '.join(command)}")
    try:
        subprocess.run(command, cwd=cwd, check=True)
    except subprocess.CalledProcessError as exc:
        fail(f"command failed with exit code {exc.returncode}")


def output(command: list[str]) -> str:
    try:
        result = subprocess.run(
            command,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except subprocess.CalledProcessError as exc:
        detail = exc.stderr.strip() or exc.stdout.strip()
        fail(f"command failed: {' '.join(command)}\n{detail}")
    return result.stdout.strip()


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        fail(f"required tool not found on PATH: {name}")


def script_dir() -> Path:
    return Path(__file__).resolve().parent


def project_root() -> Path:
    return script_dir()


def project_path(path: Path) -> Path:
    return path if path.is_absolute() else project_root() / path


def cmake_arch(arch: str) -> str:
    return "arm64;x86_64" if arch == "universal" else arch


def default_arch(target: str) -> str:
    if target == "win32":
        return "x86_64"
    if sys.platform == "darwin":
        machine = output(["uname", "-m"])
        return "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    return "arm64"


def default_generator(target: str) -> str:
    if target == "macos" and shutil.which("ninja"):
        return "Ninja"
    return "Unix Makefiles" if target == "macos" else ""


def validate_tools(target: str, generator: str) -> None:
    require_tool("git")
    require_tool("cmake")
    if generator == "Ninja":
        require_tool("ninja")

    if target == "macos":
        if sys.platform != "darwin":
            print("warning: --target macos is intended to run on macOS")
        else:
            require_tool("xcrun")
            output(["xcrun", "--find", "clang++"])

    if target == "win32" and sys.platform != "win32":
        print("warning: --target win32 is intended to run on Windows")


def is_git_repo(path: Path) -> bool:
    return (path / ".git").exists()


def clone_or_update(name: str, url: str, path: Path, ref: str, recursive: bool) -> None:
    if path.exists() and not is_git_repo(path):
        fail(f"{name} exists but is not a git repository: {path}")

    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        command = ["git", "clone"]
        if recursive:
            command.append("--recursive")
        command.extend([url, str(path)])
        run(command)
    else:
        run(["git", "-C", str(path), "fetch", "--tags", "--prune", "origin"])

    if remote_branch_exists(path, ref):
        run(["git", "-C", str(path), "checkout", "-B", ref, f"origin/{ref}"])
    else:
        run(["git", "-C", str(path), "checkout", ref])

    if recursive:
        run(["git", "-C", str(path), "submodule", "sync", "--recursive"])
        run(["git", "-C", str(path), "submodule", "update", "--init", "--recursive"])


def remote_branch_exists(path: Path, ref: str) -> bool:
    result = subprocess.run(
        ["git", "-C", str(path), "rev-parse", "--verify", f"refs/remotes/origin/{ref}"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    return result.returncode == 0


def cmake_configure_args(
    source: Path,
    build: Path,
    generator: str,
    configuration: str,
    target: str,
    arch: str,
) -> list[str]:
    args = [
        "cmake",
        "-S",
        str(source),
        "-B",
        str(build),
        f"-DCMAKE_BUILD_TYPE={configuration}",
    ]
    if generator:
        args.extend(["-G", generator])
    if target == "macos":
        args.append(f"-DCMAKE_OSX_ARCHITECTURES={cmake_arch(arch)}")
    return args


def build_zlib(
    source: Path,
    build: Path,
    install: Path,
    generator: str,
    configuration: str,
    target: str,
    arch: str,
) -> tuple[Path, Path]:
    install.mkdir(parents=True, exist_ok=True)
    args = cmake_configure_args(source, build, generator, configuration, target, arch)
    args.extend([
        f"-DCMAKE_INSTALL_PREFIX={install}",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DZLIB_BUILD_EXAMPLES=OFF",
    ])
    run(args)
    run(["cmake", "--build", str(build), "--config", configuration])
    run(["cmake", "--install", str(build), "--config", configuration])

    include = install / "include"
    library = find_zlib_library(install)
    if not include.exists():
        fail(f"zlib include directory was not created: {include}")
    return include, library


def find_zlib_library(install: Path) -> Path:
    candidates = [
        install / "lib" / "libz.a",
        install / "lib64" / "libz.a",
        install / "lib" / "zlibstatic.lib",
        install / "lib" / "zlib.lib",
        install / "lib64" / "zlibstatic.lib",
        install / "lib64" / "zlib.lib",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    found = sorted(install.rglob("*.a")) + sorted(install.rglob("*.lib"))
    if found:
        return found[0]
    fail(f"could not find built zlib library under {install}")


def build_plugin(
    native_root: Path,
    build_dir: Path,
    output_dir: Path,
    netgen_source: Path,
    zlib_include: Path,
    zlib_library: Path,
    zlib_install: Path,
    generator: str,
    configuration: str,
    target: str,
    arch: str,
) -> Path:
    args = cmake_configure_args(native_root, build_dir, generator, configuration, target, arch)
    args.extend([
        f"-DNETGEN_SOURCE_DIR={netgen_source}",
        f"-DCMAKE_PREFIX_PATH={zlib_install}",
        f"-DZLIB_INCLUDE_DIRS={zlib_include}",
        f"-DZLIB_LIBRARIES={zlib_library}",
    ])
    run(args)
    run(["cmake", "--build", str(build_dir), "--config", configuration, "--target", "NetgenUnityBridge"])

    plugin_subdir = "Plugins/macOS" if target == "macos" else "Plugins/Windows/x86_64"
    package_plugin_dir = script_dir() / plugin_subdir
    output_plugin_dir = output_dir / plugin_subdir
    package_plugin_dir.mkdir(parents=True, exist_ok=True)
    output_plugin_dir.mkdir(parents=True, exist_ok=True)

    copy_package_files(output_dir)
    copy_native_libraries(build_dir, zlib_install, package_plugin_dir, output_plugin_dir, target, configuration)
    return package_plugin_dir


def copy_package_files(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for name in ("Editor", "Runtime", "Documentation"):
        source = script_dir() / name
        destination = output_dir / name
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(source, destination)


def copy_native_libraries(
    build_dir: Path,
    zlib_install: Path,
    package_plugin_dir: Path,
    output_plugin_dir: Path,
    target: str,
    configuration: str,
) -> None:
    if target == "macos":
        names = {"libNetgenUnityBridge.dylib", "NetgenUnityBridge.bundle", "libnglib.dylib", "libngcore.dylib"}
        patterns = ("*.dylib", "*.bundle")
        bridge_names = {"libNetgenUnityBridge.dylib", "NetgenUnityBridge.bundle"}
    else:
        names = {"NetgenUnityBridge.dll", "nglib.dll", "ngcore.dll", "z.dll", "libz.dll"}
        patterns = ("*.dll",)
        bridge_names = {"NetgenUnityBridge.dll"}

    copied_bridge = False
    for search_root in (build_dir, zlib_install):
        for pattern in patterns:
            for library in search_root.rglob(pattern):
                if library.name not in names and configuration not in str(library.parent):
                    continue
                for destination_dir in (package_plugin_dir, output_plugin_dir):
                    shutil.copy2(library, destination_dir / library.name)
                if library.name in bridge_names:
                    copied_bridge = True

    if not copied_bridge:
        fail(f"NetgenUnityBridge library was not found under {build_dir}")


def write_dependency_json(path: Path, values: dict[str, str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(values, indent=2) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build NetgenMshExporter from local GitHub dependencies.")
    parser.add_argument("--target", choices=("macos", "win32"), required=True)
    parser.add_argument("--arch", choices=("arm64", "x86_64", "universal"), default=None)
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--generator", default=None)
    parser.add_argument("--external-dir", type=Path, default=project_root() / "External")
    parser.add_argument("--build-root", type=Path, default=project_root() / "Build" / "NetgenMshExporter")
    parser.add_argument("--output-root", type=Path, default=project_root() / "BuildOutput" / "NetgenMshExporter")
    parser.add_argument("--netgen-ref", default="master")
    parser.add_argument("--zlib-ref", default="develop")
    parser.add_argument("--skip-deps", action="store_true", help="Use already prepared External/netgen and External/install/zlib.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    target = args.target
    arch = args.arch or default_arch(target)
    generator = args.generator if args.generator is not None else default_generator(target)
    validate_tools(target, generator)

    external = project_path(args.external_dir).resolve()
    netgen_source = external / "netgen"
    zlib_source = external / "zlib"
    zlib_install = external / "install" / "zlib"
    zlib_build = external / "build" / f"zlib-{target}-{arch}"
    plugin_build = project_path(args.build_root).resolve() / f"{target}-{arch}"
    output_root = project_path(args.output_root).resolve()

    if not args.skip_deps:
        clone_or_update("Netgen", NETGEN_URL, netgen_source, args.netgen_ref, recursive=True)
        clone_or_update("zlib", ZLIB_URL, zlib_source, args.zlib_ref, recursive=False)
        zlib_include, zlib_library = build_zlib(
            zlib_source,
            zlib_build,
            zlib_install,
            generator,
            args.configuration,
            target,
            arch,
        )
    else:
        zlib_include = zlib_install / "include"
        zlib_library = find_zlib_library(zlib_install)
        if not netgen_source.exists():
            fail(f"missing Netgen source: {netgen_source}")

    values = {
        "NetgenSource": str(netgen_source),
        "ZlibIncludeDir": str(zlib_include),
        "ZlibLibrary": str(zlib_library),
        "CMakePrefixPath": str(zlib_install),
    }
    write_dependency_json(external / "dependency_paths.json", values)

    plugin_dir = build_plugin(
        native_root=script_dir() / "Native",
        build_dir=plugin_build,
        output_dir=output_root,
        netgen_source=netgen_source,
        zlib_include=zlib_include,
        zlib_library=zlib_library,
        zlib_install=zlib_install,
        generator=generator,
        configuration=args.configuration,
        target=target,
        arch=arch,
    )

    print("\nDone.")
    print(f"Target:          {target}")
    print(f"Arch:            {arch}")
    print(f"NetgenSource:    {values['NetgenSource']}")
    print(f"ZlibIncludeDir:  {values['ZlibIncludeDir']}")
    print(f"ZlibLibrary:     {values['ZlibLibrary']}")
    print(f"CMakePrefixPath: {values['CMakePrefixPath']}")
    print(f"PluginDir:       {plugin_dir}")
    print(f"OutputRoot:      {output_root}")


if __name__ == "__main__":
    main()
