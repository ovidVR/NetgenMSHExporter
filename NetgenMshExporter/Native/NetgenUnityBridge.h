#pragma once

#if defined(_WIN32)
  #if defined(NETGEN_UNITY_BRIDGE_EXPORTS)
    #define NETGEN_UNITY_BRIDGE_API __declspec(dllexport)
  #else
    #define NETGEN_UNITY_BRIDGE_API __declspec(dllimport)
  #endif
#else
  #define NETGEN_UNITY_BRIDGE_API __attribute__((visibility("default")))
#endif

extern "C"
{
    NETGEN_UNITY_BRIDGE_API int GenerateTetrahedralMshFromUnitySurfaceMesh(
        const float* vertices,
        int vertexCount,
        const int* triangleIndices,
        int indexCount,
        const char* outputPath,
        char* errorBuffer,
        int errorBufferSize);

    NETGEN_UNITY_BRIDGE_API int GetNetgenUnityBridgeVersion(
        char* buffer,
        int bufferSize);
}
