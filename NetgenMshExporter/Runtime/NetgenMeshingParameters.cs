using System;

namespace NetgenMshExporter
{
    /// <summary>
    /// Interop-safe wrapper for the scalar fields of nglib::Ng_Meshing_Parameters.
    /// Mesh size file input is intentionally not exposed because it is a native char pointer.
    /// </summary>
    public sealed class NetgenMeshingParameters
    {
        public bool UseLocalMeshSize { get; set; } = true;
        public double MaximumMeshSize { get; set; }
        public double MinimumMeshSize { get; set; }
        public double Fineness { get; set; } = 0.4;
        public double Grading { get; set; } = 0.3;
        public double ElementsPerEdge { get; set; } = 2.0;
        public double ElementsPerCurve { get; set; } = 2.0;
        public bool CloseEdgeEnable { get; set; }
        public double CloseEdgeFactor { get; set; } = 2.0;
        public bool MinimumEdgeLengthEnable { get; set; }
        public double MinimumEdgeLength { get; set; } = 1e-4;
        public bool SecondOrder { get; set; }
        public bool QuadDominated { get; set; }
        public bool OptimizeSurfaceMesh { get; set; } = true;
        public bool OptimizeVolumeMesh { get; set; } = true;
        public int OptimizeSteps2D { get; set; } = 3;
        public int OptimizeSteps3D { get; set; } = 3;
        public bool InvertTetrahedra { get; set; }
        public bool InvertTriangles { get; set; }
        public bool CheckOverlap { get; set; } = true;
        public bool CheckOverlappingBoundary { get; set; } = true;

        public static NetgenMeshingParameters CreateDefault()
        {
            return new NetgenMeshingParameters();
        }

        public bool Validate(out string errorMessage)
        {
            if (!IsFinite(MaximumMeshSize) || MaximumMeshSize < 0.0)
            {
                errorMessage = "Maximum Mesh Size must be finite and greater than or equal to 0. Use 0 for automatic sizing.";
                return false;
            }

            if (!IsFinite(MinimumMeshSize) || MinimumMeshSize < 0.0)
            {
                errorMessage = "Minimum Mesh Size must be finite and greater than or equal to 0.";
                return false;
            }

            if (MaximumMeshSize > 0.0 && MinimumMeshSize > MaximumMeshSize)
            {
                errorMessage = "Minimum Mesh Size cannot be greater than Maximum Mesh Size.";
                return false;
            }

            if (!IsInRange(Fineness, 0.0, 1.0))
            {
                errorMessage = "Fineness must be finite and in the range 0 to 1.";
                return false;
            }

            if (!IsInRange(Grading, 0.0, 1.0))
            {
                errorMessage = "Grading must be finite and in the range 0 to 1.";
                return false;
            }

            if (!IsFinite(ElementsPerEdge) || ElementsPerEdge <= 0.0)
            {
                errorMessage = "Elements Per Edge must be finite and greater than 0.";
                return false;
            }

            if (!IsFinite(ElementsPerCurve) || ElementsPerCurve <= 0.0)
            {
                errorMessage = "Elements Per Curve must be finite and greater than 0.";
                return false;
            }

            if (!IsFinite(CloseEdgeFactor) || CloseEdgeFactor <= 0.0)
            {
                errorMessage = "Close Edge Factor must be finite and greater than 0.";
                return false;
            }

            if (!IsFinite(MinimumEdgeLength) || MinimumEdgeLength <= 0.0)
            {
                errorMessage = "Minimum Edge Length must be finite and greater than 0.";
                return false;
            }

            if (SecondOrder)
            {
                errorMessage = "Second Order elements are not supported because the importer requires linear tetrahedra.";
                return false;
            }

            if (QuadDominated)
            {
                errorMessage = "Quad Dominated meshing is not supported because this exporter writes tetrahedral volume MSH files.";
                return false;
            }

            if (OptimizeSteps2D < 0 || OptimizeSteps2D > 100)
            {
                errorMessage = "Optimize Steps 2D must be between 0 and 100.";
                return false;
            }

            if (OptimizeSteps3D < 0 || OptimizeSteps3D > 100)
            {
                errorMessage = "Optimize Steps 3D must be between 0 and 100.";
                return false;
            }

            errorMessage = string.Empty;
            return true;
        }

        internal NetgenNativeBindings.NativeMeshingParameters ToNative()
        {
            return new NetgenNativeBindings.NativeMeshingParameters
            {
                UseLocalMeshSize = BoolToInt(UseLocalMeshSize),
                MaximumMeshSize = MaximumMeshSize,
                MinimumMeshSize = MinimumMeshSize,
                Fineness = Fineness,
                Grading = Grading,
                ElementsPerEdge = ElementsPerEdge,
                ElementsPerCurve = ElementsPerCurve,
                CloseEdgeEnable = BoolToInt(CloseEdgeEnable),
                CloseEdgeFactor = CloseEdgeFactor,
                MinimumEdgeLengthEnable = BoolToInt(MinimumEdgeLengthEnable),
                MinimumEdgeLength = MinimumEdgeLength,
                SecondOrder = BoolToInt(SecondOrder),
                QuadDominated = BoolToInt(QuadDominated),
                OptimizeSurfaceMesh = BoolToInt(OptimizeSurfaceMesh),
                OptimizeVolumeMesh = BoolToInt(OptimizeVolumeMesh),
                OptimizeSteps2D = OptimizeSteps2D,
                OptimizeSteps3D = OptimizeSteps3D,
                InvertTetrahedra = BoolToInt(InvertTetrahedra),
                InvertTriangles = BoolToInt(InvertTriangles),
                CheckOverlap = BoolToInt(CheckOverlap),
                CheckOverlappingBoundary = BoolToInt(CheckOverlappingBoundary)
            };
        }

        private static bool IsInRange(double value, double minimum, double maximum)
        {
            return IsFinite(value) && value >= minimum && value <= maximum;
        }

        private static bool IsFinite(double value)
        {
            return !double.IsNaN(value) && !double.IsInfinity(value);
        }

        private static int BoolToInt(bool value)
        {
            return value ? 1 : 0;
        }
    }
}
