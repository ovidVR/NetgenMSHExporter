using System;
using System.IO;
using System.Text;
using UnityEngine;

namespace NetgenMshExporter
{
    public static class NetgenMshExporter
    {
        private const int ErrorBufferSize = 4096;
        private const float DegenerateTriangleAreaTolerance = 1e-20f;

        public static bool GenerateTetrahedralMshFromSurfaceMesh(Mesh mesh, string outputPath, out string errorMessage)
        {
            return GenerateTetrahedralMshFromSurfaceMesh(
                mesh,
                outputPath,
                NetgenMeshingParameters.CreateDefault(),
                out errorMessage);
        }

        public static bool GenerateTetrahedralMshFromSurfaceMesh(
            Mesh mesh,
            string outputPath,
            NetgenMeshingParameters meshingParameters,
            out string errorMessage)
        {
            if (!ValidateReadableMeshReference(mesh, out errorMessage))
            {
                return false;
            }

            if (!ValidateOutputPath(outputPath, out errorMessage))
            {
                return false;
            }

            if (meshingParameters == null)
            {
                errorMessage = "Meshing parameters are required.";
                return false;
            }

            if (!meshingParameters.Validate(out errorMessage))
            {
                return false;
            }

            Vector3[] meshVertices;
            int[] triangleIndices;

            try
            {
                meshVertices = mesh.vertices;
                triangleIndices = mesh.triangles;
            }
            catch (Exception exception)
            {
                errorMessage = $"Failed to read mesh data. Make sure the mesh has Read/Write enabled. {exception.Message}";
                return false;
            }

            if (!ValidateSurfaceMeshData(meshVertices, triangleIndices, out errorMessage))
            {
                return false;
            }

            var vertices = new float[meshVertices.Length * 3];
            for (var i = 0; i < meshVertices.Length; i++)
            {
                var vertex = meshVertices[i];
                var baseIndex = i * 3;
                vertices[baseIndex] = vertex.x;
                vertices[baseIndex + 1] = vertex.y;
                vertices[baseIndex + 2] = vertex.z;
            }

            var nativeParameters = meshingParameters.ToNative();
            var errorBuffer = new StringBuilder(ErrorBufferSize);
            var result = NetgenNativeBindings.GenerateTetrahedralMshFromUnitySurfaceMeshWithParameters(
                vertices,
                meshVertices.Length,
                triangleIndices,
                triangleIndices.Length,
                Path.GetFullPath(outputPath),
                ref nativeParameters,
                errorBuffer,
                errorBuffer.Capacity);

            errorMessage = errorBuffer.ToString();
            if (result == 0)
            {
                return true;
            }

            if (string.IsNullOrWhiteSpace(errorMessage))
            {
                errorMessage = $"Native exporter failed with error code {result}.";
            }

            return false;
        }

        public static bool TryGetNativeVersion(out string version, out string errorMessage)
        {
            var buffer = new StringBuilder(256);

            try
            {
                var result = NetgenNativeBindings.GetNetgenUnityBridgeVersion(buffer, buffer.Capacity);
                version = buffer.ToString();
                errorMessage = result == 0 ? string.Empty : $"Native bridge returned {result}.";
                return result == 0;
            }
            catch (Exception exception)
            {
                version = string.Empty;
                errorMessage = exception.Message;
                return false;
            }
        }

        private static bool ValidateReadableMeshReference(Mesh mesh, out string errorMessage)
        {
            if (mesh == null)
            {
                errorMessage = "Assign a mesh before generating a tetrahedral MSH file.";
                return false;
            }

            if (!mesh.isReadable)
            {
                errorMessage = "Mesh is not readable. Enable Read/Write on the mesh import settings.";
                return false;
            }

            if (mesh.vertexCount <= 0)
            {
                errorMessage = "Mesh is empty: it has no vertices.";
                return false;
            }

            errorMessage = string.Empty;
            return true;
        }

        private static bool ValidateSurfaceMeshData(Vector3[] vertices, int[] triangleIndices, out string errorMessage)
        {
            if (vertices == null || vertices.Length == 0)
            {
                errorMessage = "Mesh is empty: it has no vertices.";
                return false;
            }

            if (triangleIndices == null || triangleIndices.Length == 0)
            {
                errorMessage = "Mesh is empty: it has no triangle surface boundary.";
                return false;
            }

            if ((triangleIndices.Length % 3) != 0)
            {
                errorMessage = "Mesh triangle index count is not divisible by 3.";
                return false;
            }

            for (var i = 0; i < triangleIndices.Length; i++)
            {
                var index = triangleIndices[i];
                if (index < 0 || index >= vertices.Length)
                {
                    errorMessage = $"Triangle index {i} has invalid vertex index {index} for {vertices.Length} vertices.";
                    return false;
                }
            }

            for (var i = 0; i < vertices.Length; i++)
            {
                var vertex = vertices[i];
                if (!IsFinite(vertex.x) || !IsFinite(vertex.y) || !IsFinite(vertex.z))
                {
                    errorMessage = $"Vertex {i} contains a non-finite coordinate.";
                    return false;
                }
            }

            for (var i = 0; i < triangleIndices.Length; i += 3)
            {
                var i0 = triangleIndices[i];
                var i1 = triangleIndices[i + 1];
                var i2 = triangleIndices[i + 2];

                if (i0 == i1 || i1 == i2 || i2 == i0)
                {
                    errorMessage = $"Triangle {i / 3} is degenerate because it repeats a vertex index.";
                    return false;
                }

                var areaVector = Vector3.Cross(vertices[i1] - vertices[i0], vertices[i2] - vertices[i0]);
                if (areaVector.sqrMagnitude <= DegenerateTriangleAreaTolerance)
                {
                    errorMessage = $"Triangle {i / 3} is degenerate or has near-zero area.";
                    return false;
                }
            }

            errorMessage = string.Empty;
            return true;
        }

        private static bool IsFinite(float value)
        {
            return !float.IsNaN(value) && !float.IsInfinity(value);
        }

        private static bool ValidateOutputPath(string outputPath, out string errorMessage)
        {
            if (string.IsNullOrWhiteSpace(outputPath))
            {
                errorMessage = "Choose an output path.";
                return false;
            }

            var extension = Path.GetExtension(outputPath);
            if (!string.Equals(extension, ".msh", StringComparison.OrdinalIgnoreCase))
            {
                errorMessage = "Output file must use the .msh extension.";
                return false;
            }

            var directory = Path.GetDirectoryName(Path.GetFullPath(outputPath));
            if (string.IsNullOrEmpty(directory))
            {
                errorMessage = "Output directory is invalid.";
                return false;
            }

            errorMessage = string.Empty;
            return true;
        }
    }
}
