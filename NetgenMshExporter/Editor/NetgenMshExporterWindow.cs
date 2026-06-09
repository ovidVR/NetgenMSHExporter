using System.IO;
using UnityEditor;
using UnityEngine;

namespace NetgenMshExporter.Editor
{
    public sealed class NetgenMshExporterWindow : EditorWindow
    {
        private Object sourceObject;
        private string outputFolder = "Assets";
        private string fileName = "mesh.msh";
        private string statusMessage;
        private MessageType statusType = MessageType.Info;

        [MenuItem("Tools/Netgen/MSH Exporter")]
        public static void Open()
        {
            var window = GetWindow<NetgenMshExporterWindow>("Tetrahedral MSH");
            window.minSize = new Vector2(460, 300);
        }

        private void OnGUI()
        {
            EditorGUILayout.Space(8);
            EditorGUILayout.LabelField("Source", EditorStyles.boldLabel);
            EditorGUILayout.HelpBox(
                "The assigned Unity mesh must be a closed watertight manifold surface. Its triangles are used as the boundary for Netgen tetrahedral volume meshing.",
                MessageType.Info);

            using (new EditorGUI.ChangeCheckScope())
            {
                sourceObject = EditorGUILayout.ObjectField(
                    "Mesh / MeshFilter / GameObject",
                    sourceObject,
                    typeof(Object),
                    true);
            }

            var mesh = ResolveMesh(sourceObject);
            using (new EditorGUI.DisabledScope(true))
            {
                EditorGUILayout.ObjectField("Resolved Mesh", mesh, typeof(Mesh), false);
            }

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

            var validation = ValidateUi(mesh);
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
                    Generate(mesh);
                }
            }
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
            if (!global::NetgenMshExporter.NetgenMshExporter.GenerateTetrahedralMshFromSurfaceMesh(mesh, outputPath, out var errorMessage))
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

        private static Mesh ResolveMesh(Object obj)
        {
            switch (obj)
            {
                case Mesh mesh:
                    return mesh;
                case MeshFilter meshFilter:
                    return meshFilter.sharedMesh;
                case GameObject gameObject:
                    return gameObject.GetComponent<MeshFilter>() != null
                        ? gameObject.GetComponent<MeshFilter>().sharedMesh
                        : null;
                default:
                    return null;
            }
        }

        private string ValidateUi(Mesh mesh)
        {
            if (mesh == null)
            {
                return "Assign a closed watertight Mesh, MeshFilter, or GameObject with a MeshFilter.";
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
