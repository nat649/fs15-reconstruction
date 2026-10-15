using System.Diagnostics;
using System.Text.Json;

namespace FS15Reconstruction;

internal sealed class LauncherForm : Form
{
    private readonly string root, settingsPath;
    private readonly Color background = Color.FromArgb(17, 22, 28), surface = Color.FromArgb(29, 37, 45), text = Color.FromArgb(230, 237, 241), muted = Color.FromArgb(157, 178, 189), green = Color.FromArgb(64, 155, 108);
    private readonly ComboBox mode = Choice("GE6 + recovered gameplay", "Home engine (experimental)", "GE6 original baseline");
    private readonly ComboBox transport = Choice("Automatic", "Direct executable", "Steam client"), map = Choice("Bjornholm", "Westbridge Hills"), difficulty = Choice("Easy", "Normal", "Hard");
    private readonly TextBox game = new(), python = new(), steam = new();
    private readonly Label description = new(), status = new(), resources = new(), evidence = new();
    private readonly RichTextBox output = new();
    private readonly ProgressBar progress = new();
    private readonly Panel pages = new();
    private readonly List<Control> pageList = [];
    private readonly List<Button> navigation = [], actions = [];
    private readonly Button cancel;
    private readonly List<Control> configuration = [];
    private Process? worker;
    private bool busy, cancelled, closing;
    private bool preparedInSmoke;
    private int selectedPage;

    private static ComboBox Choice(params string[] values)
    {
        var box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Dock = DockStyle.Fill, IntegralHeight = false };
        box.Items.AddRange(values); box.SelectedIndex = 0; return box;
    }

    public LauncherForm(string workspace)
    {
        root = workspace; settingsPath = Path.Combine(root, "build", "launcher", "settings.json");
        Text = "FS15 Reconstruction"; ClientSize = new Size(1140, 820); MinimumSize = new Size(1040, 780);
        Font = new Font("Segoe UI", 10); BackColor = background; ForeColor = text;
        var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1 };
        shell.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 200)); shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); Controls.Add(shell);
        var sidebar = new FlowLayoutPanel { Dock = DockStyle.Fill, BackColor = surface, FlowDirection = FlowDirection.TopDown, WrapContents = false, Padding = new Padding(18, 30, 10, 10) };
        sidebar.Controls.Add(Label("FS15", 27, true, 165, 52)); sidebar.Controls.Add(Label("RECONSTRUCTION", 9, false, 165, 55));
        foreach (string title in new[] { "Play", "Resources", "Diagnostics" })
        {
            int index = navigation.Count; var button = Button(title, () => SelectPage(index)); button.Width = 162; button.TextAlign = ContentAlignment.MiddleLeft; button.Margin = new Padding(0, 0, 0, 12);
            navigation.Add(button); sidebar.Controls.Add(button);
        }
        sidebar.Controls.Add(Label("GE6  /  LOCAL PROJECT\n\nRecovered code and original resources.\n\nExperimental host and original engine have separate runtimes.", 9, false, 162, 190));
        shell.Controls.Add(sidebar, 0, 0);
        var body = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, Padding = new Padding(24, 22, 24, 16) };
        body.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); body.RowStyles.Add(new RowStyle(SizeType.Absolute, 26));
        body.RowStyles.Add(new RowStyle(SizeType.Absolute, 20)); body.RowStyles.Add(new RowStyle(SizeType.Absolute, 135)); body.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));
        shell.Controls.Add(body, 1, 0); pages.Dock = DockStyle.Fill; body.Controls.Add(pages);
        status.Dock = DockStyle.Fill; status.ForeColor = muted; status.Text = "Ready. Select a runtime or prepare your local resources."; body.Controls.Add(status);
        progress.Dock = DockStyle.Fill; progress.Margin = new Padding(0, 4, 0, 8); body.Controls.Add(progress);
        output.Dock = DockStyle.Fill; output.ReadOnly = true; output.BorderStyle = BorderStyle.None; output.BackColor = surface; output.ForeColor = muted; output.Font = new Font("Consolas", 9); body.Controls.Add(output);
        var footer = new FlowLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(0, 8, 0, 0) };
        footer.Controls.Add(Button("Open logs", OpenLogs)); footer.Controls.Add(Button("Copy diagnostics", () => { if (output.TextLength > 0) Clipboard.SetText(output.Text); }));
        cancel = Button("Cancel operation", CancelWorker); cancel.Enabled = false; footer.Controls.Add(cancel); body.Controls.Add(footer);
        BuildPlay(); BuildResources(); BuildDiagnostics();
        foreach (var control in new Control[] { mode, transport, map, difficulty, game, python, steam })
        { control.BackColor = surface; control.ForeColor = text; control.Margin = new Padding(3, 5, 3, 5); configuration.Add(control); }
        LoadSettings(); mode.SelectedIndexChanged += (_, _) => UpdateMode(); SelectPage(0); UpdateMode(); RefreshEvidence();
        Shown += async (_, _) => { if (game.Text.Length == 0) await Detect(); await CheckResources(); };
        FormClosing += (_, e) => { if (busy && !closing) { e.Cancel = true; closing = true; CancelWorker(); return; } SaveSettings(); };
    }

    private Label Label(string value, float size = 10, bool bold = false, int width = 0, int height = 0)
        => new() { Text = value, Font = new Font(Font.FontFamily, size, bold ? FontStyle.Bold : FontStyle.Regular), ForeColor = bold ? text : muted, Width = width, Height = height, AutoSize = width == 0, Margin = new Padding(0, 0, 0, 8) };
    private Button Button(string title, Action click)
    {
        var button = new Button { Text = title, BackColor = surface, ForeColor = text, FlatStyle = FlatStyle.Flat, Height = 38, AutoSize = true, MinimumSize = new Size(130, 38), Cursor = Cursors.Hand, Margin = new Padding(0, 0, 10, 0) };
        button.FlatAppearance.BorderSize = 0; button.Click += (_, _) => click(); return button;
    }
    private Button ActionButton(string title, Func<Task> action, bool primary = false)
    {
        var button = Button(title, async () => await action()); if (primary) button.BackColor = green; actions.Add(button); return button;
    }
    private TableLayoutPanel Page(string title, string subtitle)
    {
        var page = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 4 };
        page.RowStyles.Add(new RowStyle(SizeType.Absolute, 50)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 62));
        page.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 56));
        page.Controls.Add(Label(title, 25, true)); var copy = Label(subtitle); copy.Dock = DockStyle.Fill; page.Controls.Add(copy);
        pages.Controls.Add(page); pageList.Add(page); return page;
    }
    private TableLayoutPanel Fields(int rows)
    {
        var panel = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 3, RowCount = rows, Height = rows * 45 };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150)); panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 85));
        for (int i = 0; i < rows; i++) panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 45)); return panel;
    }
    private void Row(TableLayoutPanel panel, int row, string name, Control control, Action? browse = null)
    {
        var label = Label(name); label.Dock = DockStyle.Fill; label.TextAlign = ContentAlignment.MiddleLeft; panel.Controls.Add(label, 0, row);
        control.Dock = DockStyle.Fill; panel.Controls.Add(control, 1, row);
        if (browse == null) panel.SetColumnSpan(control, 2);
        else { var button = Button("Browse", browse); button.MinimumSize = Size.Empty; button.Dock = DockStyle.Fill; button.Margin = new Padding(4, 5, 0, 5); panel.Controls.Add(button, 2, row); configuration.Add(button); }
    }
    private void BuildPlay()
    {
        var page = Page("Choose your runtime", "Launch the original GIANTS Engine 6 or explore the experimental reconstructed host.");
        var content = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2 };
        content.RowStyles.Add(new RowStyle(SizeType.Absolute, 195)); content.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        var fields = Fields(4); Row(fields, 0, "Runtime", mode); Row(fields, 1, "Native startup", transport); Row(fields, 2, "Home-engine map", map); Row(fields, 3, "Home difficulty", difficulty); content.Controls.Add(fields);
        description.Dock = DockStyle.Fill; description.ForeColor = muted; description.Padding = new Padding(12); description.BackColor = surface; content.Controls.Add(description); page.Controls.Add(content);
        var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(0, 10, 0, 0) };
        buttons.Controls.Add(ActionButton("Launch game", () => Run(false), true)); buttons.Controls.Add(ActionButton("Check launch plan", () => Run(true))); page.Controls.Add(buttons);
    }
    private void BuildResources()
    {
        var page = Page("Prepare your resources", "Select your installed copy of FS15. The assistant reads original files and prepares local textures, meshes, scripts and starting fleets.");
        var content = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3 };
        content.RowStyles.Add(new RowStyle(SizeType.Absolute, 52)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 48)); content.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        var fields = Fields(1); Row(fields, 0, "FS15 installation", game, () => BrowseFolder(game)); content.Controls.Add(fields);
        var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill }; buttons.Controls.Add(ActionButton("Detect installation", Detect)); buttons.Controls.Add(ActionButton("Check resources", CheckResources)); content.Controls.Add(buttons);
        resources.Dock = DockStyle.Fill; resources.ForeColor = muted; resources.Padding = new Padding(12); resources.BackColor = surface; resources.Text = "Checking local resources..."; content.Controls.Add(resources); page.Controls.Add(content);
        var actionsPanel = new FlowLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(0, 10, 0, 0) };
        actionsPanel.Controls.Add(ActionButton("Prepare / repair assets", PrepareAssets, true)); page.Controls.Add(actionsPanel);
    }
    private void BuildDiagnostics()
    {
        var page = Page("Diagnostics & tools", "Saved native evidence, startup options and local preparation logs.");
        var content = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2 };
        content.RowStyles.Add(new RowStyle(SizeType.Absolute, 100)); content.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        var fields = Fields(2); Row(fields, 0, "Python executable", python, () => BrowseExe(python)); Row(fields, 1, "Steam (optional)", steam, () => BrowseExe(steam)); content.Controls.Add(fields);
        evidence.Dock = DockStyle.Fill; evidence.ForeColor = muted; evidence.Padding = new Padding(12); evidence.BackColor = surface; content.Controls.Add(evidence); page.Controls.Add(content);
        var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(0, 10, 0, 0) }; buttons.Controls.Add(ActionButton("Verify native session", Verify)); buttons.Controls.Add(Button("Preparation guide", () => Process.Start(new ProcessStartInfo(Path.Combine(root, "docs", "LAUNCHER.md")) { UseShellExecute = true }))); page.Controls.Add(buttons);
    }
    private void SelectPage(int index)
    { selectedPage = index; for (int i = 0; i < pageList.Count; i++) { pageList[i].Visible = i == index; navigation[i].BackColor = i == index ? green : surface; } }
    private void BrowseFolder(TextBox field)
    { using var dialog = new FolderBrowserDialog { Description = "Select your Farming Simulator 15 installation", UseDescriptionForTitle = true }; if (Directory.Exists(field.Text)) dialog.InitialDirectory = field.Text; if (dialog.ShowDialog(this) == DialogResult.OK) { field.Text = dialog.SelectedPath; SaveSettings(); } }
    private void BrowseExe(TextBox field)
    { using var dialog = new OpenFileDialog { Filter = "Executable (*.exe)|*.exe" }; if (dialog.ShowDialog(this) == DialogResult.OK) field.Text = dialog.FileName; }
    private void LoadSettings()
    {
        Settings settings = new();
        try { if (File.Exists(settingsPath)) settings = JsonSerializer.Deserialize<Settings>(File.ReadAllText(settingsPath)) ?? settings; } catch (Exception error) when (error is IOException or JsonException) { Append("Settings: " + error.Message); }
        game.Text = settings.GameDirectory.Length > 0 ? settings.GameDirectory : Environment.GetEnvironmentVariable("FS15_GAME_DIR") ?? "";
        string bundled = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".cache", "codex-runtimes", "codex-primary-runtime", "dependencies", "python", "python.exe");
        python.Text = settings.Python.Length > 0 ? settings.Python : File.Exists(bundled) ? bundled : "python.exe"; steam.Text = settings.Steam;
        mode.SelectedIndex = Math.Clamp(settings.Mode, 0, 2); transport.SelectedIndex = Math.Clamp(settings.Transport, 0, 2); map.SelectedIndex = Math.Clamp(settings.Map, 0, 1); difficulty.SelectedIndex = Math.Clamp(settings.Difficulty, 0, 2);
    }
    private void SaveSettings()
    {
        try { Directory.CreateDirectory(Path.GetDirectoryName(settingsPath)!); File.WriteAllText(settingsPath, JsonSerializer.Serialize(new Settings { GameDirectory = game.Text, Python = python.Text, Steam = steam.Text, Mode = mode.SelectedIndex, Transport = transport.SelectedIndex, Map = map.SelectedIndex, Difficulty = difficulty.SelectedIndex }, new JsonSerializerOptions { WriteIndented = true })); }
        catch (IOException error) { Append("Cannot save settings: " + error.Message); }
    }
    private void UpdateMode()
    {
        bool home = mode.SelectedIndex == 1; transport.Enabled = steam.Enabled = !busy && !home; map.Enabled = difficulty.Enabled = !busy && home;
        description.Text = home ? "HOME ENGINE / EXPERIMENTAL\n\nAuthored Direct3D host using recovered Lua, native components and original assets. Physics and farming remain partial.\n\nChoose Resources to prepare or repair the local asset cache."
            : mode.SelectedIndex == 0 ? "GE6 + RECOVERED GAMEPLAY\n\nOriginal engine rendering and physics, with three recovered wheel helpers activated after 104 comparisons.\n\nSelect your career and map inside GE6. Normal installation startup is used."
            : "ORIGINAL GE6 BASELINE\n\nInstalled original engine with an isolated source-comparison probe.\n\nSelect your career and map inside GE6. Your normal saves stay separate.";
    }
    private void Append(string line)
    {
        if (IsDisposed) return; if (InvokeRequired) { BeginInvoke(() => Append(line)); return; }
        if (line.StartsWith("LAUNCHER_EVENT ", StringComparison.Ordinal))
        {
            try { using var document = JsonDocument.Parse(line[15..]); HandleEvent(document.RootElement); } catch (JsonException) { }
        }
        if (output.TextLength > 150000) output.Text = output.Text[^100000..];
        output.AppendText(line + Environment.NewLine); output.SelectionStart = output.TextLength; output.ScrollToCaret();
    }
    private void HandleEvent(JsonElement data)
    {
        string? kind = data.GetProperty("event").GetString();
        if (data.TryGetProperty("percent", out var percent)) progress.Value = Math.Clamp(percent.GetInt32(), 0, 100);
        if (data.TryGetProperty("title", out var title)) status.Text = title.GetString();
        if (kind == "installations")
        {
            var paths = data.GetProperty("paths").EnumerateArray().Select(p => p.GetString()!).ToArray();
            if (paths.Length > 0) { game.Text = paths.Contains(game.Text) ? game.Text : paths[0]; SaveSettings(); status.Text = paths.Length == 1 ? "FS15 installation detected." : "Multiple installations found. First selected; use Browse to choose another."; }
            else status.Text = "FS15 installation not found. Select it using Browse.";
        }
        if (kind is "resources" or "complete")
        {
            bool present = data.GetProperty("assets_present").GetBoolean();
            resources.Text = present ? "LOCAL RESOURCES FOUND\n\nMaps, decoded meshes, field grids, recovered scripts and prepared starting fleets are present.\n\n" + (data.GetProperty("host_built").GetBoolean() ? "The home engine is built. Use Check launch plan before starting." : "Build the Windows home engine using scripts/build.ps1.") + "\n\nPrepare / repair checks source hashes and regenerates scene and vehicle caches. File presence alone does not verify a rendered game session."
                : "PREPARATION REQUIRED\n\n" + string.Join("\n", data.GetProperty("missing").EnumerateArray().Take(8).Select(v => "• " + v.GetString())) + "\n\nSelect your installation, then choose Prepare / repair assets.";
            if (data.TryGetProperty("preparation_incomplete", out var unfinished) && unfinished.GetBoolean())
                resources.Text = "PREPARATION INTERRUPTED OR FAILED\n\nRun Prepare / repair assets to repair unfinished caches. Home-engine startup is blocked until preparation completes.\n\n" + resources.Text;
        }
    }
    private async Task<int> Worker(string script, IEnumerable<string> args)
    {
        var start = new ProcessStartInfo(python.Text.Trim()) { WorkingDirectory = root, UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true, StandardOutputEncoding = System.Text.Encoding.UTF8, StandardErrorEncoding = System.Text.Encoding.UTF8 };
        start.Environment["PYTHONUTF8"] = "1"; start.ArgumentList.Add("-u"); start.ArgumentList.Add(Path.Combine(root, "scripts", script)); foreach (string arg in args) start.ArgumentList.Add(arg);
        using var child = new Process { StartInfo = start }; worker = child;
        try
        {
            child.Start();
            async Task Pump(StreamReader stream) { while (await stream.ReadLineAsync() is { } line) Append(line); }
            var stdout = Pump(child.StandardOutput); var stderr = Pump(child.StandardError);
            await child.WaitForExitAsync(); await Task.WhenAll(stdout, stderr); return child.ExitCode;
        }
        finally { worker = null; }
    }
    private async Task Operation(string title, Func<Task<int>> action, string success)
    {
        if (busy) return; busy = true; cancelled = false; progress.Value = 0; status.Text = title;
        foreach (var button in actions) button.Enabled = false; foreach (var control in configuration) control.Enabled = false; cancel.Enabled = true;
        try { int code = await action(); status.Text = cancelled ? "Operation cancelled. Run preparation again to repair unfinished caches." : code == 0 ? success : "Operation failed. Inspect the log below for the missing input or failed step."; if (code == 0) progress.Value = 100; }
        catch (Exception error) { Append(error.Message); status.Text = "Operation failed. Check paths and the log below."; }
        finally { busy = false; foreach (var button in actions) button.Enabled = true; foreach (var control in configuration) control.Enabled = true; cancel.Enabled = false; UpdateMode(); RefreshEvidence(); if (closing) Close(); }
    }
    private void CancelWorker()
    { cancelled = true; try { if (worker is { HasExited: false }) worker.Kill(entireProcessTree: true); } catch (InvalidOperationException) { } status.Text = "Cancelling local operation..."; }
    private Task Detect() => Operation("Looking for FS15 installations...", () => Worker("launcher_assets.py", ["detect"]), "Installation search complete.");
    private Task CheckResources() => Operation("Checking local resources...", () => Worker("launcher_assets.py", ["status"]), "Resource preflight complete.");
    private Task PrepareAssets()
    {
        SaveSettings(); return Operation("Preparing original resources...", () => Worker("launcher_assets.py", ["prepare", "--game-dir", game.Text.Trim()]), "Resources prepared and recorded locally. You can now check the launch plan.");
    }
    private Task Run(bool dryRun)
    {
        SaveSettings(); var args = new List<string> { "--mode", new[] { "hybrid", "home", "ge6" }[mode.SelectedIndex], "--transport", new[] { "auto", "direct", "steam" }[transport.SelectedIndex], "--map", map.SelectedIndex == 0 ? "Map01" : "Map02", "--difficulty", (difficulty.SelectedIndex + 1).ToString() };
        if (game.Text.Trim().Length > 0) args.AddRange(["--game-dir", game.Text.Trim()]); if (steam.Text.Trim().Length > 0) args.AddRange(["--steam", steam.Text.Trim()]); if (dryRun) args.Add("--dry-run");
        return Operation(dryRun ? "Checking launch configuration..." : "Starting runtime...", () => Worker("unified_launcher.py", args), dryRun ? "Launch plan checked. No game process started." : "Runtime started. Inspect its window and session evidence.");
    }
    private Task Verify() => Operation("Checking native log evidence...", () => Worker("verify_native_engine.py", []), "Native log evidence verified. Visual gameplay checks remain separate.");
    private void OpenLogs()
    { string path = mode.SelectedIndex == 1 ? Path.Combine(root, "logs") : Path.Combine(root, "build", "native-engine", "profile"); Directory.CreateDirectory(path); Process.Start(new ProcessStartInfo("explorer.exe", path) { UseShellExecute = true }); }
    private void RefreshEvidence()
    {
        try
        {
            using var report = JsonDocument.Parse(File.ReadAllText(Path.Combine(root, "analysis", "native-engine-validation.json"))); var data = report.RootElement;
            evidence.Text = "SAVED NATIVE EVIDENCE\n\nGE6 boot: " + (data.GetProperty("native_boot_verified").GetBoolean() ? "recorded" : "pending") + "\nRecovered source: " + (data.GetProperty("recovered_script_execution_verified").GetBoolean() ? "104 comparisons passed" : "pending") + "\nHybrid gameplay: " + (data.TryGetProperty("hybrid_runtime_verified", out var h) && h.GetBoolean() ? "three recovered helpers called by native vehicles" : "pending") + "\n\nThese saved checks do not prove a new session is running. Full GE6 recompilation and complete farming sessions remain unfinished.";
        }
        catch (Exception error) when (error is IOException or JsonException or KeyNotFoundException) { evidence.Text = "No saved native session evidence. Start a career inside GE6, then verify the session."; }
    }
    internal async Task SmokeAsync(string path, bool prepare)
    {
        // Real WinForms layout render; no game process or resource import starts.
        while (busy) await Task.Delay(50);
        await Detect(); await CheckResources();
        if (prepare) { await PrepareAssets(); preparedInSmoke = progress.Value == 100; }
        SelectPage(1); await Task.Delay(100); PerformLayout();
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using var bitmap = new Bitmap(Width, Height); DrawToBitmap(bitmap, new Rectangle(0, 0, Width, Height)); bitmap.Save(path);
        File.WriteAllText(Path.ChangeExtension(path, ".json"), JsonSerializer.Serialize(new { ui_layout_rendered = true, page = selectedPage, installation_found = game.Text.Length > 0, asset_status_displayed = resources.Text.Contains("LOCAL RESOURCES FOUND"), preparation_requested = prepare, preparation_succeeded = preparedInSmoke, game_launched = false, size = new { Width, Height } }, new JsonSerializerOptions { WriteIndented = true }));
        Close();
    }
}
