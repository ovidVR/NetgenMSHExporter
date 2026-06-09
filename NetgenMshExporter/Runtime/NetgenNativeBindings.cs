using System.Runtime.InteropServices;
using System.Text;

namespace NetgenMshExporter
{
    internal static class NetgenNativeBindings
    {
        private const string DllName = "NetgenUnityBridge";

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
        internal static extern int GetNetgenUnityBridgeVersion(
            StringBuilder buffer,
            int bufferSize);
    }
}
