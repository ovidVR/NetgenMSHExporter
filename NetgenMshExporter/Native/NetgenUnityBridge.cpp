#include "NetgenUnityBridge.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <meshing.hpp>
#include <stlgeom.hpp>

namespace nglib
{
#include <nglib.h>
}

namespace
{
    constexpr int kSuccess = 0;
    constexpr int kInvalidArgument = 1;
    constexpr int kInvalidMesh = 2;
    constexpr int kFileError = 3;
    constexpr int kNetgenError = 4;
    constexpr int kUnexpectedError = 5;

    struct Vec3
    {
        double x;
        double y;
        double z;
    };

    struct VertexKey
    {
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t z;

        bool operator==(const VertexKey& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct VertexKeyHash
    {
        std::size_t operator()(const VertexKey& key) const
        {
            std::size_t hash = key.x;
            hash ^= static_cast<std::size_t>(key.y) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
            hash ^= static_cast<std::size_t>(key.z) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    struct EdgeKey
    {
        int a;
        int b;

        bool operator==(const EdgeKey& other) const
        {
            return a == other.a && b == other.b;
        }
    };

    struct EdgeKeyHash
    {
        std::size_t operator()(const EdgeKey& key) const
        {
            return (static_cast<std::size_t>(key.a) << 32) ^ static_cast<std::size_t>(key.b);
        }
    };

    struct EdgeUse
    {
        int count = 0;
        int orientationBalance = 0;
    };

    struct MeshDeleter
    {
        void operator()(nglib::Ng_Mesh* mesh) const
        {
            if (mesh != nullptr)
            {
                nglib::Ng_DeleteMesh(mesh);
            }
        }
    };

    struct StlGeometryDeleter
    {
        void operator()(nglib::Ng_STL_Geometry* geometry) const
        {
            delete reinterpret_cast<netgen::STLGeometry*>(geometry);
        }
    };

    void WriteMessage(char* buffer, int bufferSize, const std::string& message)
    {
        if (buffer == nullptr || bufferSize <= 0)
        {
            return;
        }

        const auto count = std::min<std::size_t>(
            static_cast<std::size_t>(bufferSize - 1),
            message.size());

        std::copy_n(message.data(), count, buffer);
        buffer[count] = '\0';
    }

    int Fail(int code, char* errorBuffer, int errorBufferSize, const std::string& message)
    {
        WriteMessage(errorBuffer, errorBufferSize, message);
        return code;
    }

    bool IsFinite(float value)
    {
        return std::isfinite(static_cast<double>(value));
    }

    bool IsFinite(double value)
    {
        return std::isfinite(value);
    }

    bool IsBooleanFlag(int value)
    {
        return value == 0 || value == 1;
    }

    NetgenUnityMeshingParameters CreateDefaultUnityMeshingParameters()
    {
        return NetgenUnityMeshingParameters
        {
            1,      // useLocalMeshSize
            0.0,    // maximumMeshSize: 0 means auto-size from input bounds.
            0.0,    // minimumMeshSize
            0.4,    // fineness, preserves the previous exporter default.
            0.3,    // grading
            2.0,    // elementsPerEdge
            2.0,    // elementsPerCurve
            0,      // closeEdgeEnable
            2.0,    // closeEdgeFactor
            0,      // minimumEdgeLengthEnable
            1.0e-4, // minimumEdgeLength
            0,      // secondOrder: importer supports linear tetrahedra only.
            0,      // quadDominated
            1,      // optimizeSurfaceMesh
            1,      // optimizeVolumeMesh
            3,      // optimizeSteps2D
            3,      // optimizeSteps3D
            0,      // invertTetrahedra
            0,      // invertTriangles
            1,      // checkOverlap
            1       // checkOverlappingBoundary
        };
    }

    std::uint32_t FloatBits(float value)
    {
        if (value == 0.0f)
        {
            value = 0.0f;
        }

        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    VertexKey MakeVertexKey(const float* vertices, int vertexIndex)
    {
        const int base = vertexIndex * 3;
        return VertexKey
        {
            FloatBits(vertices[base]),
            FloatBits(vertices[base + 1]),
            FloatBits(vertices[base + 2])
        };
    }

    Vec3 ReadVertex(const float* vertices, int vertexIndex)
    {
        const int base = vertexIndex * 3;
        return Vec3
        {
            static_cast<double>(vertices[base]),
            static_cast<double>(vertices[base + 1]),
            static_cast<double>(vertices[base + 2])
        };
    }

    Vec3 Subtract(const Vec3& a, const Vec3& b)
    {
        return Vec3 { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return Vec3
        {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    double LengthSquared(const Vec3& value)
    {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    }

    double ComputeBoundsDiagonal(const float* vertices, int vertexCount)
    {
        Vec3 minValue = ReadVertex(vertices, 0);
        Vec3 maxValue = minValue;

        for (int i = 1; i < vertexCount; ++i)
        {
            const Vec3 point = ReadVertex(vertices, i);
            minValue.x = std::min(minValue.x, point.x);
            minValue.y = std::min(minValue.y, point.y);
            minValue.z = std::min(minValue.z, point.z);
            maxValue.x = std::max(maxValue.x, point.x);
            maxValue.y = std::max(maxValue.y, point.y);
            maxValue.z = std::max(maxValue.z, point.z);
        }

        return std::sqrt(LengthSquared(Subtract(maxValue, minValue)));
    }

    std::string ValidateInput(
        const float* vertices,
        int vertexCount,
        const int* indices,
        int indexCount,
        const char* outputPath)
    {
        if (vertices == nullptr)
        {
            return "Vertex pointer is null.";
        }

        if (indices == nullptr)
        {
            return "Triangle index pointer is null.";
        }

        if (outputPath == nullptr || outputPath[0] == '\0')
        {
            return "Output path is empty.";
        }

        if (vertexCount <= 0)
        {
            return "Mesh is empty: no vertices were provided.";
        }

        if (indexCount <= 0)
        {
            return "Mesh is empty: no triangle indices were provided.";
        }

        if ((indexCount % 3) != 0)
        {
            return "Triangle index count must be divisible by 3.";
        }

        if (vertexCount > (std::numeric_limits<int>::max() / 3))
        {
            return "Vertex count is too large.";
        }

        for (int i = 0; i < vertexCount * 3; ++i)
        {
            if (!IsFinite(vertices[i]))
            {
                std::ostringstream stream;
                stream << "Vertex component " << i << " is not finite.";
                return stream.str();
            }
        }

        for (int i = 0; i < indexCount; ++i)
        {
            const int index = indices[i];
            if (index < 0 || index >= vertexCount)
            {
                std::ostringstream stream;
                stream << "Triangle index " << i << " has invalid vertex index "
                       << index << " for " << vertexCount << " vertices.";
                return stream.str();
            }
        }

        return {};
    }

    std::string ValidateMeshingParameters(const NetgenUnityMeshingParameters& parameters)
    {
        if (!IsBooleanFlag(parameters.useLocalMeshSize))
        {
            return "Use Local Mesh Size must be 0 or 1.";
        }

        if (!IsFinite(parameters.maximumMeshSize) || parameters.maximumMeshSize < 0.0)
        {
            return "Maximum Mesh Size must be finite and greater than or equal to 0. Use 0 for automatic sizing.";
        }

        if (!IsFinite(parameters.minimumMeshSize) || parameters.minimumMeshSize < 0.0)
        {
            return "Minimum Mesh Size must be finite and greater than or equal to 0.";
        }

        if (parameters.maximumMeshSize > 0.0 && parameters.minimumMeshSize > parameters.maximumMeshSize)
        {
            return "Minimum Mesh Size cannot be greater than Maximum Mesh Size.";
        }

        if (!IsFinite(parameters.fineness) || parameters.fineness < 0.0 || parameters.fineness > 1.0)
        {
            return "Fineness must be finite and in the range 0 to 1.";
        }

        if (!IsFinite(parameters.grading) || parameters.grading < 0.0 || parameters.grading > 1.0)
        {
            return "Grading must be finite and in the range 0 to 1.";
        }

        if (!IsFinite(parameters.elementsPerEdge) || parameters.elementsPerEdge <= 0.0)
        {
            return "Elements Per Edge must be finite and greater than 0.";
        }

        if (!IsFinite(parameters.elementsPerCurve) || parameters.elementsPerCurve <= 0.0)
        {
            return "Elements Per Curve must be finite and greater than 0.";
        }

        if (!IsBooleanFlag(parameters.closeEdgeEnable))
        {
            return "Close Edge Enable must be 0 or 1.";
        }

        if (!IsFinite(parameters.closeEdgeFactor) || parameters.closeEdgeFactor <= 0.0)
        {
            return "Close Edge Factor must be finite and greater than 0.";
        }

        if (!IsBooleanFlag(parameters.minimumEdgeLengthEnable))
        {
            return "Minimum Edge Length Enable must be 0 or 1.";
        }

        if (!IsFinite(parameters.minimumEdgeLength) || parameters.minimumEdgeLength <= 0.0)
        {
            return "Minimum Edge Length must be finite and greater than 0.";
        }

        if (!IsBooleanFlag(parameters.secondOrder))
        {
            return "Second Order must be 0 or 1.";
        }

        if (parameters.secondOrder != 0)
        {
            return "Second Order elements are not supported because the importer requires linear tetrahedra.";
        }

        if (!IsBooleanFlag(parameters.quadDominated))
        {
            return "Quad Dominated must be 0 or 1.";
        }

        if (parameters.quadDominated != 0)
        {
            return "Quad Dominated meshing is not supported because this exporter writes tetrahedral volume MSH files.";
        }

        if (!IsBooleanFlag(parameters.optimizeSurfaceMesh)
            || !IsBooleanFlag(parameters.optimizeVolumeMesh)
            || !IsBooleanFlag(parameters.invertTetrahedra)
            || !IsBooleanFlag(parameters.invertTriangles)
            || !IsBooleanFlag(parameters.checkOverlap)
            || !IsBooleanFlag(parameters.checkOverlappingBoundary))
        {
            return "Meshing toggle values must be 0 or 1.";
        }

        if (parameters.optimizeSteps2D < 0 || parameters.optimizeSteps2D > 100)
        {
            return "Optimize Steps 2D must be between 0 and 100.";
        }

        if (parameters.optimizeSteps3D < 0 || parameters.optimizeSteps3D > 100)
        {
            return "Optimize Steps 3D must be between 0 and 100.";
        }

        return {};
    }

    nglib::Ng_Meshing_Parameters ToNgMeshingParameters(
        const NetgenUnityMeshingParameters& source,
        const float* vertices,
        int vertexCount)
    {
        nglib::Ng_Meshing_Parameters target;

        target.uselocalh = source.useLocalMeshSize;
        target.maxh = source.maximumMeshSize > 0.0
            ? source.maximumMeshSize
            : std::max(ComputeBoundsDiagonal(vertices, vertexCount), 1.0e-6);
        target.minh = source.minimumMeshSize;
        target.fineness = source.fineness;
        target.grading = source.grading;
        target.elementsperedge = source.elementsPerEdge;
        target.elementspercurve = source.elementsPerCurve;
        target.closeedgeenable = source.closeEdgeEnable;
        target.closeedgefact = source.closeEdgeFactor;
        target.minedgelenenable = source.minimumEdgeLengthEnable;
        target.minedgelen = source.minimumEdgeLength;
        target.second_order = source.secondOrder;
        target.quad_dominated = source.quadDominated;
        target.meshsize_filename = nullptr;
        target.optsurfmeshenable = source.optimizeSurfaceMesh;
        target.optvolmeshenable = source.optimizeVolumeMesh;
        target.optsteps_2d = source.optimizeSteps2D;
        target.optsteps_3d = source.optimizeSteps3D;
        target.invert_tets = source.invertTetrahedra;
        target.invert_trigs = source.invertTriangles;
        target.check_overlap = source.checkOverlap;
        target.check_overlapping_boundary = source.checkOverlappingBoundary;

        return target;
    }

    std::vector<int> BuildCanonicalVertexMap(const float* vertices, int vertexCount)
    {
        std::unordered_map<VertexKey, int, VertexKeyHash> canonicalIds;
        std::vector<int> canonicalByVertex(static_cast<std::size_t>(vertexCount));

        for (int i = 0; i < vertexCount; ++i)
        {
            const VertexKey key = MakeVertexKey(vertices, i);
            const auto [iterator, inserted] = canonicalIds.emplace(key, static_cast<int>(canonicalIds.size()));
            canonicalByVertex[static_cast<std::size_t>(i)] = iterator->second;
        }

        return canonicalByVertex;
    }

    void AddEdgeUse(std::unordered_map<EdgeKey, EdgeUse, EdgeKeyHash>& edges, int from, int to)
    {
        const EdgeKey key { std::min(from, to), std::max(from, to) };
        EdgeUse& use = edges[key];
        ++use.count;
        use.orientationBalance += (from < to) ? 1 : -1;
    }

    std::string ValidateClosedSurface(const float* vertices, int vertexCount, const int* indices, int indexCount)
    {
        const double diagonal = ComputeBoundsDiagonal(vertices, vertexCount);
        if (diagonal <= 0.0)
        {
            return "Mesh bounds have zero size; a closed volume cannot be tetrahedralized.";
        }

        const double areaToleranceSquared = std::max(1.0e-24, diagonal * diagonal * diagonal * diagonal * 1.0e-20);
        const std::vector<int> canonicalByVertex = BuildCanonicalVertexMap(vertices, vertexCount);
        std::unordered_map<EdgeKey, EdgeUse, EdgeKeyHash> edges;

        for (int i = 0; i < indexCount; i += 3)
        {
            const int i0 = indices[i];
            const int i1 = indices[i + 1];
            const int i2 = indices[i + 2];

            const int c0 = canonicalByVertex[static_cast<std::size_t>(i0)];
            const int c1 = canonicalByVertex[static_cast<std::size_t>(i1)];
            const int c2 = canonicalByVertex[static_cast<std::size_t>(i2)];

            if (c0 == c1 || c1 == c2 || c2 == c0)
            {
                std::ostringstream stream;
                stream << "Triangle " << (i / 3) << " is degenerate because it repeats a vertex position.";
                return stream.str();
            }

            const Vec3 p0 = ReadVertex(vertices, i0);
            const Vec3 p1 = ReadVertex(vertices, i1);
            const Vec3 p2 = ReadVertex(vertices, i2);
            const double areaVectorLengthSquared = LengthSquared(Cross(Subtract(p1, p0), Subtract(p2, p0)));
            if (areaVectorLengthSquared <= areaToleranceSquared)
            {
                std::ostringstream stream;
                stream << "Triangle " << (i / 3) << " is degenerate or has near-zero area.";
                return stream.str();
            }

            AddEdgeUse(edges, c0, c1);
            AddEdgeUse(edges, c1, c2);
            AddEdgeUse(edges, c2, c0);
        }

        for (const auto& entry : edges)
        {
            const EdgeKey& edge = entry.first;
            const EdgeUse& use = entry.second;
            if (use.count == 1)
            {
                std::ostringstream stream;
                stream << "Mesh is not a closed watertight surface: edge between canonical vertices "
                       << edge.a << " and " << edge.b << " is used by only one triangle.";
                return stream.str();
            }

            if (use.count > 2)
            {
                std::ostringstream stream;
                stream << "Mesh contains a non-manifold edge between canonical vertices "
                       << edge.a << " and " << edge.b << " used by " << use.count << " triangles.";
                return stream.str();
            }

            if (use.orientationBalance != 0)
            {
                std::ostringstream stream;
                stream << "Mesh surface orientation is inconsistent around edge between canonical vertices "
                       << edge.a << " and " << edge.b << ".";
                return stream.str();
            }
        }

        return {};
    }

    std::string ResultToString(nglib::Ng_Result result)
    {
        switch (result)
        {
            case nglib::NG_OK:
                return "OK";
            case nglib::NG_SURFACE_INPUT_ERROR:
                return "surface input error";
            case nglib::NG_VOLUME_FAILURE:
                return "volume meshing failure";
            case nglib::NG_STL_INPUT_ERROR:
                return "STL input error";
            case nglib::NG_SURFACE_FAILURE:
                return "surface meshing failure";
            case nglib::NG_FILE_NOT_FOUND:
                return "file not found";
            case nglib::NG_ERROR:
            default:
                return "generic Netgen error";
        }
    }

    bool MeshHasOnlyLinearTetrahedra(nglib::Ng_Mesh* mesh)
    {
        const int volumeElementCount = nglib::Ng_GetNE(mesh);
        if (volumeElementCount <= 0)
        {
            return false;
        }

        int pointIndices[NG_VOLUME_ELEMENT_MAXPOINTS] = {};
        for (int i = 1; i <= volumeElementCount; ++i)
        {
            const nglib::Ng_Volume_Element_Type elementType =
                nglib::Ng_GetVolumeElement(mesh, i, pointIndices);

            if (elementType != nglib::NG_TET)
            {
                return false;
            }
        }

        return true;
    }

    bool WriteImporterCompatibleGmsh(
        nglib::Ng_Mesh* mesh,
        const std::filesystem::path& path,
        std::string& errorMessage)
    {
        const int pointCount = nglib::Ng_GetNP(mesh);
        const int volumeElementCount = nglib::Ng_GetNE(mesh);

        if (pointCount <= 0)
        {
            errorMessage = "MSH generation failed: output does not contain nodes.";
            return false;
        }

        if (volumeElementCount <= 0)
        {
            errorMessage = "MSH generation failed: output does not contain tetrahedral volume elements.";
            return false;
        }

        std::ofstream output(path, std::ios::out | std::ios::trunc);
        if (!output.is_open())
        {
            errorMessage = "Failed to open output file for writing.";
            return false;
        }

        output.imbue(std::locale::classic());
        output << std::setprecision(17);

        // TetrahedralMeshImporter is a narrow Gmsh v2 parser. It expects
        // simple ASCII sections, contiguous 1-based node IDs, type-4 linear
        // tetrahedra, and exactly two element tags: physical and elementary.
        output << "$MeshFormat\n";
        output << "2.2 0 8\n";
        output << "$EndMeshFormat\n";

        output << "$Nodes\n";
        output << pointCount << "\n";
        for (int i = 1; i <= pointCount; ++i)
        {
            double point[3] = {};
            nglib::Ng_GetPoint(mesh, i, point);

            // The importer defaults to ConvertUnits=true and interprets file
            // coordinates as centimeters. Netgen runs on Unity-space input, so
            // write centimeters to preserve the original Unity scale on import.
            output << i << " "
                   << (point[0] * 100.0) << " "
                   << (point[1] * 100.0) << " "
                   << (point[2] * 100.0) << "\n";
        }
        output << "$EndNodes\n";

        output << "$Elements\n";
        output << volumeElementCount << "\n";
        int pointIndices[NG_VOLUME_ELEMENT_MAXPOINTS] = {};
        for (int i = 1; i <= volumeElementCount; ++i)
        {
            const nglib::Ng_Volume_Element_Type elementType =
                nglib::Ng_GetVolumeElement(mesh, i, pointIndices);

            if (elementType != nglib::NG_TET)
            {
                errorMessage = "MSH generation failed: importer only supports linear tetrahedral elements.";
                return false;
            }

            constexpr int gmshLinearTetrahedron = 4;
            constexpr int physicalEntity = 1;
            constexpr int elementaryEntity = 1;

            output << i << " "
                   << gmshLinearTetrahedron << " "
                   << "2 "
                   << physicalEntity << " "
                   << elementaryEntity << " "
                   << pointIndices[0] << " "
                   << pointIndices[1] << " "
                   << pointIndices[2] << " "
                   << pointIndices[3] << "\n";
        }
        output << "$EndElements\n";
        output.close();

        if (!output)
        {
            errorMessage = "Failed while writing output mesh file.";
            return false;
        }

        errorMessage.clear();
        return true;
    }

    bool ParseExactSectionMarker(std::istream& input, const std::string& expected)
    {
        std::string line;
        return std::getline(input, line) && line == expected;
    }

    bool LineHasNoExtraTokens(std::istringstream& lineStream)
    {
        std::string extraToken;
        return !(lineStream >> extraToken);
    }

    bool SavedMeshFileMatchesImporterContract(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::in);
        if (!input.is_open())
        {
            return false;
        }

        input.imbue(std::locale::classic());

        if (!ParseExactSectionMarker(input, "$MeshFormat"))
        {
            return false;
        }

        std::string meshFormatLine;
        if (!std::getline(input, meshFormatLine))
        {
            return false;
        }

        {
            std::istringstream lineStream(meshFormatLine);
            lineStream.imbue(std::locale::classic());
            std::string version;
            int fileType = -1;
            int dataSize = 0;
            if (!(lineStream >> version >> fileType >> dataSize)
                || version != "2.2"
                || fileType != 0
                || dataSize != 8
                || !LineHasNoExtraTokens(lineStream))
            {
                return false;
            }
        }

        if (!ParseExactSectionMarker(input, "$EndMeshFormat")
            || !ParseExactSectionMarker(input, "$Nodes"))
        {
            return false;
        }

        std::string countLine;
        if (!std::getline(input, countLine))
        {
            return false;
        }

        int pointCount = 0;
        {
            std::istringstream lineStream(countLine);
            if (!(lineStream >> pointCount) || pointCount <= 0 || !LineHasNoExtraTokens(lineStream))
            {
                return false;
            }
        }

        for (int i = 1; i <= pointCount; ++i)
        {
            std::string nodeLine;
            if (!std::getline(input, nodeLine))
            {
                return false;
            }

            std::istringstream lineStream(nodeLine);
            lineStream.imbue(std::locale::classic());
            int nodeId = 0;
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
            if (!(lineStream >> nodeId >> x >> y >> z)
                || nodeId != i
                || !LineHasNoExtraTokens(lineStream))
            {
                return false;
            }
        }

        if (!ParseExactSectionMarker(input, "$EndNodes")
            || !ParseExactSectionMarker(input, "$Elements")
            || !std::getline(input, countLine))
        {
            return false;
        }

        int elementCount = 0;
        {
            std::istringstream lineStream(countLine);
            if (!(lineStream >> elementCount) || elementCount <= 0 || !LineHasNoExtraTokens(lineStream))
            {
                return false;
            }
        }

        for (int i = 1; i <= elementCount; ++i)
        {
            std::string elementLine;
            if (!std::getline(input, elementLine))
            {
                return false;
            }

            std::istringstream lineStream(elementLine);
            int elementId = 0;
            int elementType = 0;
            int numberOfTags = 0;
            int physicalEntity = 0;
            int elementaryEntity = 0;
            int nodeIds[4] = {};

            if (!(lineStream >> elementId
                    >> elementType
                    >> numberOfTags
                    >> physicalEntity
                    >> elementaryEntity
                    >> nodeIds[0]
                    >> nodeIds[1]
                    >> nodeIds[2]
                    >> nodeIds[3])
                || elementId != i
                || elementType != 4
                || numberOfTags != 2
                || physicalEntity != 1
                || elementaryEntity != 1
                || !LineHasNoExtraTokens(lineStream))
            {
                return false;
            }

            for (int nodeId : nodeIds)
            {
                if (nodeId < 1 || nodeId > pointCount)
                {
                    return false;
                }
            }
        }

        return ParseExactSectionMarker(input, "$EndElements");
    }

    int GenerateTetrahedralMesh(
        const float* vertices,
        int vertexCount,
        const int* indices,
        int indexCount,
        const char* outputPath,
        const NetgenUnityMeshingParameters& meshingParameters,
        char* errorBuffer,
        int errorBufferSize)
    {
        const std::string validationError =
            ValidateInput(vertices, vertexCount, indices, indexCount, outputPath);

        if (!validationError.empty())
        {
            return Fail(kInvalidArgument, errorBuffer, errorBufferSize, validationError);
        }

        const std::string parameterValidationError = ValidateMeshingParameters(meshingParameters);
        if (!parameterValidationError.empty())
        {
            return Fail(kInvalidArgument, errorBuffer, errorBufferSize, parameterValidationError);
        }

        const std::string surfaceValidationError =
            ValidateClosedSurface(vertices, vertexCount, indices, indexCount);

        if (!surfaceValidationError.empty())
        {
            return Fail(kInvalidMesh, errorBuffer, errorBufferSize, surfaceValidationError);
        }

        const std::filesystem::path path(outputPath);
        const std::filesystem::path parent = path.parent_path();

        if (!parent.empty())
        {
            std::error_code directoryError;
            std::filesystem::create_directories(parent, directoryError);
            if (directoryError)
            {
                return Fail(
                    kFileError,
                    errorBuffer,
                    errorBufferSize,
                    "Failed to create output directory: " + directoryError.message());
            }
        }

        static std::mutex netgenMutex;
        std::lock_guard<std::mutex> netgenLock(netgenMutex);

        static std::once_flag initFlag;
        std::call_once(initFlag, []()
        {
            nglib::Ng_Init();
        });

        std::unique_ptr<nglib::Ng_STL_Geometry, StlGeometryDeleter> geometry(nglib::Ng_STL_NewGeometry());
        if (geometry == nullptr)
        {
            return Fail(kNetgenError, errorBuffer, errorBufferSize, "Netgen failed to allocate STL geometry.");
        }

        std::unique_ptr<nglib::Ng_Mesh, MeshDeleter> mesh(nglib::Ng_NewMesh());
        if (mesh == nullptr)
        {
            return Fail(kNetgenError, errorBuffer, errorBufferSize, "Netgen failed to allocate mesh.");
        }

        for (int i = 0; i < indexCount; i += 3)
        {
            double p0[3] =
            {
                vertices[indices[i] * 3],
                vertices[indices[i] * 3 + 1],
                vertices[indices[i] * 3 + 2]
            };
            double p1[3] =
            {
                vertices[indices[i + 1] * 3],
                vertices[indices[i + 1] * 3 + 1],
                vertices[indices[i + 1] * 3 + 2]
            };
            double p2[3] =
            {
                vertices[indices[i + 2] * 3],
                vertices[indices[i + 2] * 3 + 1],
                vertices[indices[i + 2] * 3 + 2]
            };

            nglib::Ng_STL_AddTriangle(geometry.get(), p0, p1, p2, nullptr);
        }

        nglib::Ng_Meshing_Parameters parameters =
            ToNgMeshingParameters(meshingParameters, vertices, vertexCount);

        nglib::Ng_Result result = nglib::Ng_STL_InitSTLGeometry(geometry.get());
        if (result != nglib::NG_OK)
        {
            return Fail(
                kInvalidMesh,
                errorBuffer,
                errorBufferSize,
                "Netgen rejected the STL surface boundary: " + ResultToString(result) + ".");
        }

        result = nglib::Ng_STL_MakeEdges(geometry.get(), mesh.get(), &parameters);
        if (result != nglib::NG_OK)
        {
            return Fail(
                kNetgenError,
                errorBuffer,
                errorBufferSize,
                "Netgen edge meshing failed: " + ResultToString(result) + ".");
        }

        result = nglib::Ng_STL_GenerateSurfaceMesh(geometry.get(), mesh.get(), &parameters);
        if (result != nglib::NG_OK)
        {
            return Fail(
                kNetgenError,
                errorBuffer,
                errorBufferSize,
                "Netgen surface meshing failed: " + ResultToString(result) + ".");
        }

        result = nglib::Ng_GenerateVolumeMesh(mesh.get(), &parameters);
        if (result != nglib::NG_OK)
        {
            return Fail(
                kNetgenError,
                errorBuffer,
                errorBufferSize,
                "Netgen tetrahedral volume meshing failed: " + ResultToString(result) + ".");
        }

        if (!MeshHasOnlyLinearTetrahedra(mesh.get()))
        {
            return Fail(
                kNetgenError,
                errorBuffer,
                errorBufferSize,
                "MSH generation failed: importer only supports linear tetrahedral volume elements.");
        }

        std::string writeError;
        if (!WriteImporterCompatibleGmsh(mesh.get(), path, writeError))
        {
            return Fail(kFileError, errorBuffer, errorBufferSize, writeError);
        }

        if (!SavedMeshFileMatchesImporterContract(path))
        {
            return Fail(
                kNetgenError,
                errorBuffer,
                errorBufferSize,
                "MSH generation failed: output does not match the importer-compatible Gmsh 2.2 tetrahedral format.");
        }

        std::ostringstream success;
        success << "Importer-compatible Gmsh 2.2 ASCII MSH generated successfully with "
                << nglib::Ng_GetNE(mesh.get()) << " linear tetrahedral volume elements.";
        WriteMessage(errorBuffer, errorBufferSize, success.str());
        return kSuccess;
    }
}

extern "C"
{
    NETGEN_UNITY_BRIDGE_API int GenerateTetrahedralMshFromUnitySurfaceMesh(
        const float* vertices,
        int vertexCount,
        const int* triangleIndices,
        int indexCount,
        const char* outputPath,
        char* errorBuffer,
        int errorBufferSize)
    {
        try
        {
            const NetgenUnityMeshingParameters parameters = CreateDefaultUnityMeshingParameters();
            return GenerateTetrahedralMesh(
                vertices,
                vertexCount,
                triangleIndices,
                indexCount,
                outputPath,
                parameters,
                errorBuffer,
                errorBufferSize);
        }
        catch (const std::exception& exception)
        {
            return Fail(kNetgenError, errorBuffer, errorBufferSize, exception.what());
        }
        catch (...)
        {
            return Fail(kUnexpectedError, errorBuffer, errorBufferSize, "Unexpected native exception.");
        }
    }

    NETGEN_UNITY_BRIDGE_API int GenerateTetrahedralMshFromUnitySurfaceMeshWithParameters(
        const float* vertices,
        int vertexCount,
        const int* triangleIndices,
        int indexCount,
        const char* outputPath,
        const NetgenUnityMeshingParameters* parameters,
        char* errorBuffer,
        int errorBufferSize)
    {
        try
        {
            if (parameters == nullptr)
            {
                return Fail(kInvalidArgument, errorBuffer, errorBufferSize, "Meshing parameters pointer is null.");
            }

            return GenerateTetrahedralMesh(
                vertices,
                vertexCount,
                triangleIndices,
                indexCount,
                outputPath,
                *parameters,
                errorBuffer,
                errorBufferSize);
        }
        catch (const std::exception& exception)
        {
            return Fail(kNetgenError, errorBuffer, errorBufferSize, exception.what());
        }
        catch (...)
        {
            return Fail(kUnexpectedError, errorBuffer, errorBufferSize, "Unexpected native exception.");
        }
    }

    NETGEN_UNITY_BRIDGE_API int GetNetgenUnityBridgeVersion(char* buffer, int bufferSize)
    {
        WriteMessage(buffer, bufferSize, "NetgenUnityBridge 2.2.0 importer-compatible Gmsh 2.2 tetrahedral with meshing parameters");
        return kSuccess;
    }
}
