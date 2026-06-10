using System.IO;
using UnityEditor;
using UnityEngine;

namespace NetgenMshExporter.Editor
{
    public sealed class NetgenMshExporterWindow : EditorWindow
    {
        private Mesh sourceMesh;
        private NetgenMeshingParameters meshingParameters = NetgenMeshingParameters.CreateDefault();
        private string outputFolder = "Assets";
        private string fileName = "mesh.msh";
        private string statusMessage;
        private MessageType statusType = MessageType.Info;
        private Vector2 scrollPosition;

        [MenuItem("Tools/Netgen/MSH Exporter")]
        public static void Open()
        {
            var window = GetWindow<NetgenMshExporterWindow>("Tetrahedral MSH");
            window.minSize = new Vector2(460, 300);
        }

        private void OnGUI()
        {
            if (meshingParameters == null)
            {
                meshingParameters = NetgenMeshingParameters.CreateDefault();
            }

            scrollPosition = EditorGUILayout.BeginScrollView(scrollPosition);
            try
            {
                EditorGUILayout.Space(8);
                EditorGUILayout.LabelField("Source", EditorStyles.boldLabel);
                EditorGUILayout.HelpBox(
                    "Assign a readable closed watertight Mesh. The exporter reads only mesh.vertices and mesh.triangles.",
                    MessageType.Info);

                sourceMesh = (Mesh)EditorGUILayout.ObjectField(
                    "Mesh",
                    sourceMesh,
                    typeof(Mesh),
                    false);

                EditorGUILayout.Space(8);
                DrawMeshingParameters();

                EditorGUILayout.Space(8);
                EditorGUILayout.LabelField("Output", EditorStyles.boldLabel);

                using (new EditorGUILayout.HorizontalScope())
                {
                    outputFolder = EditorGUILayout.TextField("Folder", outputFolder);
                    if (GUILayout.Button("Select", GUILayout.Width(72)))
                    {
                        SelectOutputFolder();
                    }
                }

                fileName = EditorGUILayout.TextField("File Name", fileName);

                var validation = ValidateUi(sourceMesh);
                if (!string.IsNullOrEmpty(validation))
                {
                    EditorGUILayout.HelpBox(validation, MessageType.Warning);
                }

                if (!string.IsNullOrEmpty(statusMessage))
                {
                    EditorGUILayout.HelpBox(statusMessage, statusType);
                }

                GUILayout.FlexibleSpace();

                using (new EditorGUI.DisabledScope(!string.IsNullOrEmpty(validation)))
                {
                    if (GUILayout.Button("Generate Tetrahedral MSH", GUILayout.Height(34)))
                    {
                        Generate(sourceMesh);
                    }
                }
            }
            finally
            {
                EditorGUILayout.EndScrollView();
            }
        }

        private void DrawMeshingParameters()
        {
            EditorGUILayout.LabelField("Meshing Parameters", EditorStyles.boldLabel);

            meshingParameters.UseLocalMeshSize = EditorGUILayout.Toggle("Use Local Mesh Size", meshingParameters.UseLocalMeshSize);
            meshingParameters.MaximumMeshSize = EditorGUILayout.DoubleField("Maximum Mesh Size", meshingParameters.MaximumMeshSize);
            meshingParameters.MinimumMeshSize = EditorGUILayout.DoubleField("Minimum Mesh Size", meshingParameters.MinimumMeshSize);
            meshingParameters.Fineness = EditorGUILayout.Slider("Fineness", (float)meshingParameters.Fineness, 0f, 1f);
            meshingParameters.Grading = EditorGUILayout.Slider("Grading", (float)meshingParameters.Grading, 0f, 1f);
            meshingParameters.ElementsPerEdge = EditorGUILayout.DoubleField("Elements Per Edge", meshingParameters.ElementsPerEdge);
            meshingParameters.ElementsPerCurve = EditorGUILayout.DoubleField("Elements Per Curve", meshingParameters.ElementsPerCurve);
            meshingParameters.CloseEdgeEnable = EditorGUILayout.Toggle("Close Edge Refinement", meshingParameters.CloseEdgeEnable);
            meshingParameters.CloseEdgeFactor = EditorGUILayout.DoubleField("Close Edge Factor", meshingParameters.CloseEdgeFactor);
            meshingParameters.MinimumEdgeLengthEnable = EditorGUILayout.Toggle("Use Minimum Edge Length", meshingParameters.MinimumEdgeLengthEnable);
            meshingParameters.MinimumEdgeLength = EditorGUILayout.DoubleField("Minimum Edge Length", meshingParameters.MinimumEdgeLength);
            meshingParameters.SecondOrder = EditorGUILayout.Toggle("Second Order", meshingParameters.SecondOrder);
            meshingParameters.QuadDominated = EditorGUILayout.Toggle("Quad Dominated", meshingParameters.QuadDominated);
            meshingParameters.OptimizeSurfaceMesh = EditorGUILayout.Toggle("Optimize Surface Mesh", meshingParameters.OptimizeSurfaceMesh);
            meshingParameters.OptimizeVolumeMesh = EditorGUILayout.Toggle("Optimize Volume Mesh", meshingParameters.OptimizeVolumeMesh);
            meshingParameters.OptimizeSteps2D = EditorGUILayout.IntField("Optimize Steps 2D", meshingParameters.OptimizeSteps2D);
            meshingParameters.OptimizeSteps3D = EditorGUILayout.IntField("Optimize Steps 3D", meshingParameters.OptimizeSteps3D);
            meshingParameters.InvertTetrahedra = EditorGUILayout.Toggle("Invert Tetrahedra", meshingParameters.InvertTetrahedra);
            meshingParameters.InvertTriangles = EditorGUILayout.Toggle("Invert Triangles", meshingParameters.InvertTriangles);
            meshingParameters.CheckOverlap = EditorGUILayout.Toggle("Check Overlap", meshingParameters.CheckOverlap);
            meshingParameters.CheckOverlappingBoundary = EditorGUILayout.Toggle("Check Overlapping Boundary", meshingParameters.CheckOverlappingBoundary);
        }

        private void SelectOutputFolder()
        {
            var assetsPath = Path.GetFullPath(Application.dataPath);
            var selected = EditorUtility.OpenFolderPanel("Choose MSH Output Folder", Application.dataPath, string.Empty);
            if (string.IsNullOrEmpty(selected))
            {
                return;
            }

            selected = Path.GetFullPath(selected);
            if (!IsPathInsideAssets(selected, assetsPath))
            {
                statusType = MessageType.Error;
                statusMessage = "Choose a folder inside this project's Assets folder.";
                return;
            }

            outputFolder = ToProjectRelativePath(selected);
        }

        private void Generate(Mesh mesh)
        {
            var outputPath = Path.Combine(outputFolder, EnsureMshExtension(fileName));
            if (!global::NetgenMshExporter.NetgenMshExporter.GenerateTetrahedralMshFromSurfaceMesh(
                    mesh,
                    outputPath,
                    meshingParameters,
                    out var errorMessage))
            {
                statusType = MessageType.Error;
                statusMessage = errorMessage;
                return;
            }

            AssetDatabase.Refresh();
            statusType = MessageType.Info;
            statusMessage = $"Generated tetrahedral MSH: {ToProjectRelativePath(Path.GetFullPath(outputPath))}";
            Debug.Log($"Netgen tetrahedral MSH generation complete: {outputPath}");
        }

        private string ValidateUi(Mesh mesh)
        {
            if (mesh == null)
            {
                return "Assign a closed watertight Mesh.";
            }

            if (!meshingParameters.Validate(out var parameterError))
            {
                return parameterError;
            }

            if (string.IsNullOrWhiteSpace(outputFolder))
            {
                return "Choose an output folder.";
            }

            if (!IsProjectRelativeAssetsPath(outputFolder))
            {
                return "Output folder must be inside Assets.";
            }

            if (string.IsNullOrWhiteSpace(fileName))
            {
                return "Enter a file name.";
            }

            if (!mesh.isReadable)
            {
                return "Mesh is not readable. Enable Read/Write on the mesh import settings.";
            }

            if (mesh.vertexCount == 0)
            {
                return "Mesh has no vertices.";
            }

            if (mesh.triangles == null || mesh.triangles.Length == 0)
            {
                return "Mesh has no triangles.";
            }

            if ((mesh.triangles.Length % 3) != 0)
            {
                return "Mesh triangle index count is not divisible by 3.";
            }

            return string.Empty;
        }

        private static string EnsureMshExtension(string name)
        {
            return Path.GetExtension(name).ToLowerInvariant() == ".msh"
                ? name
                : $"{Path.GetFileNameWithoutExtension(name)}.msh";
        }

        private static string ToProjectRelativePath(string fullPath)
        {
            var projectPath = Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
            var normalizedFullPath = Path.GetFullPath(fullPath);
            var relative = Path.GetRelativePath(projectPath, normalizedFullPath);
            return relative.Replace('\\', '/');
        }

        private static bool IsProjectRelativeAssetsPath(string path)
        {
            var normalized = path.Replace('\\', '/').TrimEnd('/');
            return normalized == "Assets" || normalized.StartsWith("Assets/", System.StringComparison.Ordinal);
        }

        private static bool IsPathInsideAssets(string fullPath, string assetsPath)
        {
            var normalizedFullPath = Path.GetFullPath(fullPath).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var normalizedAssetsPath = Path.GetFullPath(assetsPath).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);

            return string.Equals(normalizedFullPath, normalizedAssetsPath, System.StringComparison.OrdinalIgnoreCase)
                || normalizedFullPath.StartsWith(
                    normalizedAssetsPath + Path.DirectorySeparatorChar,
                    System.StringComparison.OrdinalIgnoreCase)
                || normalizedFullPath.StartsWith(
                    normalizedAssetsPath + Path.AltDirectorySeparatorChar,
                    System.StringComparison.OrdinalIgnoreCase);
        }
    }
}
