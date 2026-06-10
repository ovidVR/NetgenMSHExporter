# Netgen Tetrahedral MSH Exporter for Unity 6

This package adds a Unity Editor tool that generates tetrahedral volume `.msh` files from readable Unity `Mesh` assets.

The Unity mesh is not exported as a surface-only triangle mesh. Its triangles are treated as the closed surface boundary of a solid volume, Netgen tetrahedralizes that volume, and the saved `.msh` is rejected unless it contains tetrahedral volume elements.

Open the tool from:

```text
Tools > Netgen > MSH Exporter
```

## Usage

1. Assign a readable `Mesh` asset directly.
2. Make sure the assigned mesh is a closed watertight manifold surface.
3. Adjust meshing parameters if needed.
4. Choose an output folder inside the Unity project's `Assets` folder.
5. Enter a `.msh` file name.
6. Click `Generate Tetrahedral MSH`.

The exporter reads:

```csharp
Vector3[] vertices = mesh.vertices;
int[] triangles = mesh.triangles;
```

Those triangles are interpreted only as the boundary of the volume to tetrahedralize. Normals, UVs, tangents, colors, and bone data are not used for tetrahedral meshing.

The editor tool intentionally accepts only `Mesh` references. It does not resolve
`GameObject`, `MeshFilter`, `SkinnedMeshRenderer`, prefab, or scene selection input.

## Meshing Parameters

The C# API exposes an interop-safe wrapper for the scalar fields of
`nglib::Ng_Meshing_Parameters`. The native `meshsize_filename` pointer is not
exposed. Defaults preserve the previous exporter behavior where it differed from
raw Netgen defaults.

- `UseLocalMeshSize`: default `true`; enables Netgen local mesh size modifiers.
- `MaximumMeshSize`: default `0`; `0` means automatic sizing from the input mesh bounds.
- `MinimumMeshSize`: default `0`; global lower mesh size limit.
- `Fineness`: default `0.4`; mesh density from `0` coarse to `1` fine.
- `Grading`: default `0.3`; mesh size transition aggressiveness from `0` to `1`.
- `ElementsPerEdge`: default `2`; target elements per geometry edge.
- `ElementsPerCurve`: default `2`; target elements per curvature radius.
- `CloseEdgeEnable`: default `false`; enables close-edge refinement.
- `CloseEdgeFactor`: default `2`; refinement factor for close edges.
- `MinimumEdgeLengthEnable`: default `false`; enables explicit edge subdivision minimum.
- `MinimumEdgeLength`: default `1e-4`; minimum edge length when enabled.
- `SecondOrder`: default `false`; must remain `false` because the importer expects linear tetrahedra.
- `QuadDominated`: default `false`; must remain `false` for this tetrahedral exporter path.
- `OptimizeSurfaceMesh`: default `true`; enables surface mesh optimization.
- `OptimizeVolumeMesh`: default `true`; enables volume mesh optimization.
- `OptimizeSteps2D`: default `3`; surface optimization step count.
- `OptimizeSteps3D`: default `3`; volume optimization step count.
- `InvertTetrahedra`: default `false`; passes Netgen's volume inversion flag.
- `InvertTriangles`: default `false`; passes Netgen's surface triangle inversion flag.
- `CheckOverlap`: default `true`; checks overlapping surfaces during surface meshing.
- `CheckOverlappingBoundary`: default `true`; checks overlapping surface elements before volume meshing.

## Valid Input

Use a closed solid test mesh such as:

- A cube with all six faces closed.
- A closed sphere-like mesh.
- An imported closed watertight manifold mesh.

These inputs must fail because they do not define a closed volume:

- A single triangle.
- A quad or plane.
- Any open surface.
- Any mesh with non-manifold edges.
- Any mesh with degenerate triangles.
- Any self-intersecting surface that Netgen detects during surface or volume meshing.

## Known Limitations

- The written file content is Gmsh 2.2 ASCII, not Netgen native VOL text.
- Output coordinates are written in centimeters so the importer's default `ConvertUnits` setting restores Unity-space scale.
- This tool generates tetrahedral volume meshes. Surface-only `.msh` export is not acceptable and is treated as failure.
- Multiple Unity submeshes are combined through `mesh.triangles`.
- Meshes must be readable in Unity import settings.
- Unity-side validation is intentionally lightweight; authoritative watertight, manifold, overlap, and tetrahedralization validation is performed by the native Netgen bridge.
- Native paths are passed to the bridge as ANSI strings; keep output paths ASCII-only for maximum Windows compatibility.

See `BuildInstructions.md`, `NativeApi.md`, and `ResearchSummary.md` for rebuild, interop, and Netgen API details.

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
