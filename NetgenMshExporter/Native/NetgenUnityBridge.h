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
    struct NetgenUnityMeshingParameters
    {
        int useLocalMeshSize;
        double maximumMeshSize;
        double minimumMeshSize;
        double fineness;
        double grading;
        double elementsPerEdge;
        double elementsPerCurve;
        int closeEdgeEnable;
        double closeEdgeFactor;
        int minimumEdgeLengthEnable;
        double minimumEdgeLength;
        int secondOrder;
        int quadDominated;
        int optimizeSurfaceMesh;
        int optimizeVolumeMesh;
        int optimizeSteps2D;
        int optimizeSteps3D;
        int invertTetrahedra;
        int invertTriangles;
        int checkOverlap;
        int checkOverlappingBoundary;
    };

    NETGEN_UNITY_BRIDGE_API int GenerateTetrahedralMshFromUnitySurfaceMesh(
        const float* vertices,
        int vertexCount,
        const int* triangleIndices,
        int indexCount,
        const char* outputPath,
        char* errorBuffer,
        int errorBufferSize);

    NETGEN_UNITY_BRIDGE_API int GenerateTetrahedralMshFromUnitySurfaceMeshWithParameters(
        const float* vertices,
        int vertexCount,
        const int* triangleIndices,
        int indexCount,
        const char* outputPath,
        const NetgenUnityMeshingParameters* parameters,
        char* errorBuffer,
        int errorBufferSize);

    NETGEN_UNITY_BRIDGE_API int GetNetgenUnityBridgeVersion(
        char* buffer,
        int bufferSize);
}
