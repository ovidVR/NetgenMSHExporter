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
