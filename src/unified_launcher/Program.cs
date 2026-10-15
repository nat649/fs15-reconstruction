using System.Diagnostics;
using System.Text.Json;

namespace FS15Reconstruction;

internal sealed class Settings
{
    public string GameDirectory { get; set; } = "";
    public string Python { get; set; } = "";
    public string Steam { get; set; } = "";
    public int Mode { get; set; }
    public int Transport { get; set; }
    public int Map { get; set; }
    public int Difficulty { get; set; }
}
internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        ApplicationConfiguration.Initialize();
        try
        {
            string? root = null;
            int rootIndex = Array.IndexOf(args, "--root");
            if (rootIndex >= 0 && rootIndex + 1 < args.Length) root = Path.GetFullPath(args[rootIndex + 1]);
            for (var dir = new DirectoryInfo(AppContext.BaseDirectory); root == null && dir != null; dir = dir.Parent)
                if (File.Exists(Path.Combine(dir.FullName, "scripts", "unified_launcher.py"))) root = dir.FullName;
            if (root == null) throw new InvalidOperationException("Keep the launcher inside the reconstruction workspace, or pass --root PATH.");
            var form = new LauncherForm(root);
            int smokeIndex = Array.IndexOf(args, "--smoke-capture");
            if (smokeIndex >= 0 && smokeIndex + 1 < args.Length)
                form.Shown += async (_, _) => await form.SmokeAsync(Path.GetFullPath(args[smokeIndex + 1]), args.Contains("--smoke-prepare"));
            Application.Run(form);
        }
        catch (Exception error) { MessageBox.Show(error.Message, "FS15 Reconstruction", MessageBoxButtons.OK, MessageBoxIcon.Error); }
    }
}
