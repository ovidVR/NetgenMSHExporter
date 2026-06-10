using System.Runtime.InteropServices;
using System.Text;

namespace NetgenMshExporter
{
    internal static class NetgenNativeBindings
    {
        private const string DllName = "NetgenUnityBridge";

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativeMeshingParameters
        {
            public int UseLocalMeshSize;
            public double MaximumMeshSize;
            public double MinimumMeshSize;
            public double Fineness;
            public double Grading;
            public double ElementsPerEdge;
            public double ElementsPerCurve;
            public int CloseEdgeEnable;
            public double CloseEdgeFactor;
            public int MinimumEdgeLengthEnable;
            public double MinimumEdgeLength;
            public int SecondOrder;
            public int QuadDominated;
            public int OptimizeSurfaceMesh;
            public int OptimizeVolumeMesh;
            public int OptimizeSteps2D;
            public int OptimizeSteps3D;
            public int InvertTetrahedra;
            public int InvertTriangles;
            public int CheckOverlap;
            public int CheckOverlappingBoundary;
        }

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int GenerateTetrahedralMshFromUnitySurfaceMesh(
            [In] float[] vertices,
            int vertexCount,
            [In] int[] triangleIndices,
            int indexCount,
            [MarshalAs(UnmanagedType.LPStr)] string outputPath,
            StringBuilder errorBuffer,
            int errorBufferSize);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int GenerateTetrahedralMshFromUnitySurfaceMeshWithParameters(
            [In] float[] vertices,
            int vertexCount,
            [In] int[] triangleIndices,
            int indexCount,
            [MarshalAs(UnmanagedType.LPStr)] string outputPath,
            ref NativeMeshingParameters parameters,
            StringBuilder errorBuffer,
            int errorBufferSize);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int GetNetgenUnityBridgeVersion(
            StringBuilder buffer,
            int bufferSize);
    }
}
