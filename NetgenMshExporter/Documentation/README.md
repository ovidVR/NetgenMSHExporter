# Netgen Tetrahedral MSH Exporter for Unity 6

This package adds a Unity Editor tool that generates tetrahedral volume `.msh` files from readable Unity `Mesh` assets.

The Unity mesh is not exported as a surface-only triangle mesh. Its triangles are treated as the closed surface boundary of a solid volume, Netgen tetrahedralizes that volume, and the saved `.msh` is rejected unless it contains tetrahedral volume elements.

Open the tool from:

```text
Tools > Netgen > MSH Exporter
```

## Usage

1. Assign a `Mesh`, `MeshFilter`, or `GameObject` with a `MeshFilter`.
2. Make sure the assigned mesh is a closed watertight manifold surface.
3. Choose an output folder inside the Unity project's `Assets` folder.
4. Enter a `.msh` file name.
5. Click `Generate Tetrahedral MSH`.

The exporter reads:

```csharp
Vector3[] vertices = mesh.vertices;
int[] triangles = mesh.triangles;
```

Those triangles are interpreted only as the boundary of the volume to tetrahedralize. Normals, UVs, tangents, colors, and bone data are not used for tetrahedral meshing.

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

## Output Requirement

The output `.msh` file must contain tetrahedral volume elements in Gmsh 2.2 ASCII text format. It is written specifically for `TetrahedralMeshImporter`, so `$Elements` contains linear tetrahedra only.

After Netgen reports meshing success, the bridge verifies that the in-memory mesh has linear tetrahedral volume elements and then parses the saved `.msh` file to confirm the importer-compatible Gmsh structure. If the file is surface-only or structurally incompatible, generation fails with:

```text
MSH generation failed: output does not contain tetrahedral volume elements.
```

## Runtime Files

For normal Unity usage, keep:

```text
Assets/NetgenMshExporter/
  Editor/
  Runtime/
  Plugins/Windows/x86_64/
  Documentation/
```

The original `Assets/netgen` source tree is only needed to rebuild native DLLs. The final Unity folder remains plug-and-play after the Netgen source folder is removed, as long as the built DLLs remain in `Plugins/Windows/x86_64`.

## Platform

Currently supported:

```text
Windows x86_64 Unity Editor
```

## Known Limitations

- The written file content is Gmsh 2.2 ASCII, not Netgen native VOL text.
- Output coordinates are written in centimeters so the importer's default `ConvertUnits` setting restores Unity-space scale.
- This tool generates tetrahedral volume meshes. Surface-only `.msh` export is not acceptable and is treated as failure.
- Multiple Unity submeshes are combined through `mesh.triangles`.
- Meshes must be readable in Unity import settings.
- Unity-side validation is intentionally lightweight; authoritative watertight, manifold, overlap, and tetrahedralization validation is performed by the native Netgen bridge.
- Native paths are passed to the bridge as ANSI strings; keep output paths ASCII-only for maximum Windows compatibility.

See `BuildInstructions.md`, `NativeApi.md`, and `ResearchSummary.md` for rebuild, interop, and Netgen API details.
