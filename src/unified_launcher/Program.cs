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
            if (args.Length == 2 && args[0] == "--root") root = Path.GetFullPath(args[1]);
            for (var dir = new DirectoryInfo(AppContext.BaseDirectory); root == null && dir != null; dir = dir.Parent)
                if (File.Exists(Path.Combine(dir.FullName, "scripts", "unified_launcher.py"))) root = dir.FullName;
            if (root == null) throw new InvalidOperationException("Keep the launcher inside the reconstruction workspace, or pass --root PATH.");
            Application.Run(new LauncherForm(root));
        }
        catch (Exception error) { MessageBox.Show(error.Message, "FS15 Reconstruction", MessageBoxButtons.OK, MessageBoxIcon.Error); }
    }
}

internal sealed class LauncherForm : Form
{
    private readonly string root, settingsPath;
    private readonly ComboBox mode = Choice("GE6 + recovered gameplay", "Home engine (experimental)", "GE6 original baseline");
    private readonly ComboBox transport = Choice("Automatic (normal installation startup)", "Direct executable", "Steam client");
    private readonly ComboBox map = Choice("Bjornholm", "Westbridge Hills");
    private readonly ComboBox difficulty = Choice("Easy", "Normal", "Hard");
    private readonly TextBox game = new(), python = new(), steam = new();
    private readonly Label description = new(), status = new(), evidence = new();
    private readonly RichTextBox output = new();
    private readonly Button launch = new(), preview = new(), verify = new();
    private readonly System.Windows.Forms.Timer timer = new() { Interval = 2000 };
    private bool busy;
    private readonly Color surface = Color.FromArgb(30, 36, 42), foreground = Color.FromArgb(232, 238, 242);

    private static ComboBox Choice(params string[] items)
    {
        var box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Dock = DockStyle.Fill };
        box.Items.AddRange(items); box.SelectedIndex = 0; return box;
    }

    public LauncherForm(string workspace)
    {
        root = workspace;
        settingsPath = Path.Combine(root, "build", "launcher", "settings.json");
        Text = "FS15 Reconstruction | Engine Launcher";
        ClientSize = new Size(1000, 760); MinimumSize = new Size(850, 720);
        BackColor = Color.FromArgb(19, 24, 29); ForeColor = foreground;
        Font = new Font("Segoe UI", 10);
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(26), ColumnCount = 1, RowCount = 7 };
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 64));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 62));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 255));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 56));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 62));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));
        Controls.Add(layout);
        var heading = new Label { Text = "FS15 RECONSTRUCTION", Font = new Font(Font.FontFamily, 22, FontStyle.Bold), Dock = DockStyle.Fill };
        layout.Controls.Add(heading);
        description.Dock = DockStyle.Fill; description.ForeColor = Color.FromArgb(170, 193, 201);
        layout.Controls.Add(description);
        var fields = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 7 };
        fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 145));
        fields.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 96));
        for (int i = 0; i < 7; i++) fields.RowStyles.Add(new RowStyle(SizeType.Percent, 100f / 7));
        layout.Controls.Add(fields);
        Row(fields, 0, "Runtime", mode);
        Row(fields, 1, "Native startup", transport);
        Row(fields, 2, "FS15 installation", game, () => BrowseFolder(game));
        Row(fields, 3, "Python executable", python, () => BrowseExe(python));
        Row(fields, 4, "Steam (optional)", steam, () => BrowseExe(steam));
        Row(fields, 5, "Home-engine map", map);
        Row(fields, 6, "Home difficulty", difficulty);
        var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(0, 6, 0, 0) };
        SetButton(launch, "Launch game", Color.FromArgb(80, 178, 107)); launch.Width = 180;
        SetButton(preview, "Check launch plan", surface); preview.Width = 180;
        SetButton(verify, "Verify native session", surface); verify.Width = 180;
        actions.Controls.AddRange([launch, preview, verify]);
        var logs = new Button(); SetButton(logs, "Open local logs", surface); logs.Width = 160;
        actions.Controls.Add(logs); layout.Controls.Add(actions);
        evidence.Dock = DockStyle.Fill; evidence.ForeColor = Color.FromArgb(170, 193, 201); layout.Controls.Add(evidence);
        output.Dock = DockStyle.Fill; output.ReadOnly = true; output.BackColor = surface; output.ForeColor = foreground;
        output.BorderStyle = BorderStyle.None; output.Font = new Font("Consolas", 9); layout.Controls.Add(output);
        status.Dock = DockStyle.Fill; status.TextAlign = ContentAlignment.MiddleLeft; layout.Controls.Add(status);
        foreach (var field in new Control[] { mode, transport, map, difficulty, game, python, steam })
        { field.BackColor = surface; field.ForeColor = foreground; field.Margin = new Padding(3); }
        launch.Click += async (_, _) => await Run(false);
        preview.Click += async (_, _) => await Run(true);
        verify.Click += async (_, _) => await Verify();
        logs.Click += (_, _) => { var path = mode.SelectedIndex == 1 ? Path.Combine(root, "logs") : Path.Combine(root, "build", "native-engine", "profile"); Directory.CreateDirectory(path); Process.Start(new ProcessStartInfo("explorer.exe", path) { UseShellExecute = true }); };
        mode.SelectedIndexChanged += (_, _) => UpdateMode();
        FormClosing += (_, _) => { SaveSettings(); timer.Stop(); };
        timer.Tick += (_, _) => RefreshEvidence();
        LoadSettings(); UpdateMode(); RefreshEvidence(); timer.Start();
    }

    private void Row(TableLayoutPanel parent, int row, string title, Control control, Action? browse = null)
    {
        parent.Controls.Add(new Label { Text = title, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft }, 0, row);
        control.Dock = DockStyle.Fill; parent.Controls.Add(control, 1, row);
        if (browse == null) parent.SetColumnSpan(control, 2);
        else { var button = new Button(); SetButton(button, "Browse", surface); button.Dock = DockStyle.Fill; button.Click += (_, _) => browse(); parent.Controls.Add(button, 2, row); }
    }

    private void SetButton(Button button, string text, Color background)
    { button.Text = text; button.BackColor = background; button.ForeColor = foreground; button.FlatStyle = FlatStyle.Flat; button.FlatAppearance.BorderSize = 0; button.Height = 38; button.Cursor = Cursors.Hand; button.Margin = new Padding(0, 0, 10, 0); }
    private void BrowseFolder(TextBox field)
    { using var dialog = new FolderBrowserDialog { Description = "Select your installed Farming Simulator 15 directory", UseDescriptionForTitle = true }; if (Directory.Exists(field.Text)) dialog.InitialDirectory = field.Text; if (dialog.ShowDialog(this) == DialogResult.OK) field.Text = dialog.SelectedPath; }
    private void BrowseExe(TextBox field)
    { using var dialog = new OpenFileDialog { Filter = "Executable (*.exe)|*.exe", CheckFileExists = true }; if (dialog.ShowDialog(this) == DialogResult.OK) field.Text = dialog.FileName; }

    private void LoadSettings()
    {
        Settings settings = new();
        try { if (File.Exists(settingsPath)) settings = JsonSerializer.Deserialize<Settings>(File.ReadAllText(settingsPath)) ?? settings; } catch (JsonException) { }
        game.Text = settings.GameDirectory;
        if (game.Text.Length == 0)
        {
            game.Text = Environment.GetEnvironmentVariable("FS15_GAME_DIR") ?? "";
            try { using var data = JsonDocument.Parse(File.ReadAllText(Path.Combine(root, "build", "native-engine", "manifest.json"))); game.Text = data.RootElement.GetProperty("game_directory").GetString() ?? game.Text; } catch (Exception e) when (e is IOException or JsonException or KeyNotFoundException) { }
        }
        python.Text = settings.Python;
        if (python.Text.Length == 0)
        {
            string bundled = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".cache", "codex-runtimes", "codex-primary-runtime", "dependencies", "python", "python.exe");
            python.Text = File.Exists(bundled) ? bundled : "python.exe";
        }
        steam.Text = settings.Steam;
        mode.SelectedIndex = Math.Clamp(settings.Mode, 0, 2); transport.SelectedIndex = Math.Clamp(settings.Transport, 0, 2);
        map.SelectedIndex = Math.Clamp(settings.Map, 0, 1); difficulty.SelectedIndex = Math.Clamp(settings.Difficulty, 0, 2);
    }
    private void SaveSettings()
    {
        try { Directory.CreateDirectory(Path.GetDirectoryName(settingsPath)!); File.WriteAllText(settingsPath, JsonSerializer.Serialize(new Settings { GameDirectory = game.Text, Python = python.Text, Steam = steam.Text, Mode = mode.SelectedIndex, Transport = transport.SelectedIndex, Map = map.SelectedIndex, Difficulty = difficulty.SelectedIndex }, new JsonSerializerOptions { WriteIndented = true })); }
        catch (IOException error) { Append("Cannot save launcher settings: " + error.Message); }
    }
    private void UpdateMode()
    {
        bool home = mode.SelectedIndex == 1;
        transport.Enabled = !home; game.Enabled = !home; steam.Enabled = !home;
        map.Enabled = home; difficulty.Enabled = home;
        description.Text = home ? "Authored Direct3D renderer with recovered Lua and original assets. Physics and farming remain partial. Runs without Steam."
            : mode.SelectedIndex == 0 ? "Original GE6 rendering and physics + three recovered wheel helpers after 104 parity checks. Choose your career and map inside GE6."
            : "Original installed GE6 with an isolated source-comparison probe. Original global gameplay stays intact. Choose your career inside GE6.";
    }
    private void Append(string line)
    { if (IsDisposed) return; if (InvokeRequired) { BeginInvoke(() => Append(line)); return; } output.AppendText(line + Environment.NewLine); output.SelectionStart = output.TextLength; output.ScrollToCaret(); }
    private async Task<int> Worker(string script, IEnumerable<string> arguments)
    {
        var start = new ProcessStartInfo(python.Text.Trim()) { WorkingDirectory = root, UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
        start.ArgumentList.Add(Path.Combine(root, "scripts", script)); foreach (var arg in arguments) start.ArgumentList.Add(arg);
        using var child = new Process { StartInfo = start };
        child.OutputDataReceived += (_, e) => { if (e.Data != null) Append(e.Data); };
        child.ErrorDataReceived += (_, e) => { if (e.Data != null) Append(e.Data); };
        child.Start(); child.BeginOutputReadLine(); child.BeginErrorReadLine(); await child.WaitForExitAsync(); return child.ExitCode;
    }
    private void SetBusy(bool value)
    { busy = value; launch.Enabled = preview.Enabled = verify.Enabled = !value; }
    private async Task Run(bool dryRun)
    {
        if (busy) return;
        SaveSettings(); SetBusy(true); status.Text = dryRun ? "Checking launch configuration..." : "Starting selected runtime...";
        try
        {
            var args = new List<string> { "--mode", new[] { "hybrid", "home", "ge6" }[mode.SelectedIndex], "--transport", new[] { "auto", "direct", "steam" }[transport.SelectedIndex], "--map", map.SelectedIndex == 0 ? "Map01" : "Map02", "--difficulty", (difficulty.SelectedIndex + 1).ToString() };
            if (game.Text.Trim().Length != 0) args.AddRange(["--game-dir", game.Text.Trim()]);
            if (steam.Text.Trim().Length != 0) args.AddRange(["--steam", steam.Text.Trim()]);
            if (dryRun) args.Add("--dry-run");
            int result = await Worker("unified_launcher.py", args);
            status.Text = result == 0 ? dryRun ? "Launch plan checked. No game process started." : "Selected runtime started. Check its window and session evidence." : "Launch failed. See the details below; no alternate runtime was substituted.";
        }
        catch (Exception error) { Append(error.Message); status.Text = "Launch failed. Check executable paths and the details below."; }
        finally { SetBusy(false); RefreshEvidence(); }
    }
    private async Task Verify()
    {
        if (busy) return; SetBusy(true);
        try { int result = await Worker("verify_native_engine.py", []); status.Text = result == 0 ? "Native log evidence verified. Visual gameplay checks remain separate." : "Native evidence is incomplete. Start a career and inspect the log."; }
        catch (Exception error) { Append(error.Message); }
        finally { SetBusy(false); RefreshEvidence(); }
    }
    private void RefreshEvidence()
    {
        try
        {
            using var report = JsonDocument.Parse(File.ReadAllText(Path.Combine(root, "analysis", "native-engine-validation.json")));
            var data = report.RootElement;
            string native = data.GetProperty("native_boot_verified").GetBoolean() ? "recorded" : "pending";
            string source = data.GetProperty("recovered_script_execution_verified").GetBoolean() ? "104 comparisons passed" : "pending";
            string hybrid = data.TryGetProperty("hybrid_runtime_verified", out var h) && h.GetBoolean() ? "3 recovered helpers used in native gameplay" : "native gameplay integration pending verification";
            evidence.Text = $"Saved evidence | GE6 boot: {native} | Source execution: {source}\nHybrid: {hybrid}. These are saved checks, not proof that a new session is running.";
        }
        catch (Exception error) when (error is IOException or JsonException or KeyNotFoundException) { evidence.Text = "No native session evidence recorded. Launch GE6, load a career, then verify the session."; }
    }
}
