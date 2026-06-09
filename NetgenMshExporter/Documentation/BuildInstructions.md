# Build Instructions

`build.py` is the simple entry point for building the NetgenMshExporter Unity native plugin.

It does three things:

1. Clones or updates Netgen and zlib from GitHub into `External/`.
2. Builds zlib locally.
3. Builds `NetgenUnityBridge` and copies the plugin libraries into Unity's plugin folder.

The script is safe to rerun. Existing Git repositories are fetched and checked out to the requested refs.

## Requirements

- Python 3
- Git
- CMake
- Windows: Visual Studio C++ build tools
- macOS: Xcode Command Line Tools
- macOS optional: Ninja

## Quick Start

From `Assets/NetgenMshExporter`:

```bash
python3 build.py --target macos
```

On Windows:

```powershell
python build.py --target win32
```

## macOS Options

Build for the current Mac architecture:

```bash
python3 build.py --target macos
```

Build Apple Silicon:

```bash
python3 build.py --target macos --arch arm64
```

Build Intel:

```bash
python3 build.py --target macos --arch x86_64
```

Build universal:

```bash
python3 build.py --target macos --arch universal
```

The universal option passes:

```text
CMAKE_OSX_ARCHITECTURES=arm64;x86_64
```

## Windows Options

Use CMake's default Windows generator:

```powershell
python build.py --target win32
```

Use an explicit generator:

```powershell
python build.py --target win32 --generator "Visual Studio 17 2022"
```

## Common Options

Use pinned dependency refs:

```bash
python3 build.py --target macos --netgen-ref master --zlib-ref develop
```

Reuse already prepared dependencies:

```bash
python3 build.py --target macos --skip-deps
```

Use a different dependency folder:

```bash
python3 build.py --target macos --external-dir ../../External
```

## Output

The script writes dependency paths to:

```text
External/dependency_paths.json
```

Example:

```json
{
  "NetgenSource": "External/netgen",
  "ZlibIncludeDir": "External/install/zlib/include",
  "ZlibLibrary": "External/install/zlib/lib/libz.a",
  "CMakePrefixPath": "External/install/zlib"
}
```

Built Unity plugin libraries are copied into:

```text
Assets/NetgenMshExporter/Plugins/macOS/
Assets/NetgenMshExporter/Plugins/Windows/x86_64/
```

Portable package output is copied into:

```text
BuildOutput/NetgenMshExporter/
```

## Notes

- `build.py` uses list-based `subprocess.run(...)` calls, not shell command strings, so paths with spaces are handled safely.
- zlib is built static by default to keep Unity plugin loading simpler.
- Netgen is built through the Unity bridge CMake project by passing `NETGEN_SOURCE_DIR=External/netgen`.
- The old PowerShell and Bash wrappers were removed to keep the build surface small.
