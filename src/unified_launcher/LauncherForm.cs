using System.Diagnostics;
using System.Text.Json;

namespace FS15Reconstruction;

internal sealed partial class LauncherForm : Form
{
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
        description.Text = home ? "Experimental gameplay · local assets" : mode.SelectedIndex == 0 ? "Original GE6 + recovered helpers" : "Original GE6 · comparison baseline";
        nativeCard.Selected = !home; homeCard.Selected = home; nativeCard.Invalidate(); homeCard.Invalidate();
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
            libraryStatus.Text = present ? "Resources found on this PC" : "Resource setup needed";
            resources.Text = present ? "LOCAL RESOURCES FOUND\n\nYour maps, textures, recovered scripts and starting fleets are available.\n\n" + (data.GetProperty("host_built").GetBoolean() ? "ReFarm is built and available in your launch profiles." : "Build ReFarm using scripts/build.ps1 to enable its launch profile.") + "\n\nUse Prepare / repair to check source files and rebuild the local caches."
                : "PREPARATION REQUIRED\n\n" + string.Join("\n", data.GetProperty("missing").EnumerateArray().Take(8).Select(v => "• " + v.GetString())) + "\n\nSelect your installation, then choose Prepare / repair assets.";
            if (data.TryGetProperty("preparation_incomplete", out var unfinished) && unfinished.GetBoolean())
                resources.Text = "PREPARATION INTERRUPTED OR FAILED\n\nRun Prepare / repair assets to repair unfinished caches. ReFarm startup is blocked until preparation completes.\n\n" + resources.Text;
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
        SaveSettings(); var args = new List<string> { "--mode", new[] { "hybrid", "refarm", "ge6" }[mode.SelectedIndex], "--transport", new[] { "auto", "direct", "steam" }[transport.SelectedIndex], "--map", map.SelectedIndex == 0 ? "Map01" : "Map02", "--difficulty", (difficulty.SelectedIndex + 1).ToString() };
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
            evidence.Text = "SAVED SESSION CHECKS  ·  GE6 boot: " + (data.GetProperty("native_boot_verified").GetBoolean() ? "recorded" : "pending") + "  ·  Recovered source: " + (data.GetProperty("recovered_script_execution_verified").GetBoolean() ? "104 comparisons passed" : "pending") + "\n\n" + (data.TryGetProperty("hybrid_runtime_verified", out var h) && h.GetBoolean() ? "Three recovered wheel helpers were used in a recorded native session." : "Native helper integration is pending verification.") + "\nSaved checks describe earlier sessions. Complete GE6 reconstruction remains in progress.";
        }
        catch (Exception error) when (error is IOException or JsonException or KeyNotFoundException) { evidence.Text = "No saved native session evidence. Start a career inside GE6, then verify the session."; }
    }
    internal async Task SmokeAsync(string path, bool prepare)
    {
        // Real WinForms layouts. Game startup is deliberately excluded here.
        while (busy) await Task.Delay(50);
        await Detect(); await CheckResources();
        if (prepare) { await PrepareAssets(); preparedInSmoke = progress.Value == 100; }
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        int originalMode = mode.SelectedIndex, originalMap = map.SelectedIndex;
        string Suffix(string suffix) => Path.Combine(Path.GetDirectoryName(path)!, Path.GetFileNameWithoutExtension(path) + suffix + ".png");
        async Task Capture(string target, int page)
        {
            SelectPage(page); await Task.Delay(100); PerformLayout(); Refresh();
            using var bitmap = new Bitmap(Width, Height); DrawToBitmap(bitmap, new Rectangle(0, 0, Width, Height)); bitmap.Save(target);
        }
        mode.SelectedIndex = 0; map.SelectedIndex = 0;
        await Capture(path, 0); await Capture(Suffix("-resources"), 1); await Capture(Suffix("-settings"), 2);
        SelectPage(0); homeCard.PerformClick(); bool homeSelection = mode.SelectedIndex == 1 && map.Enabled && difficulty.Enabled && !transport.Enabled;
        map.SelectedIndex = 1; await Capture(Suffix("-home"), 0);
        nativeCard.PerformClick(); bool nativeSelection = mode.SelectedIndex == 0 && !map.Enabled && !difficulty.Enabled && transport.Enabled;
        var originalSize = ClientSize; ClientSize = new Size(1100, 760); await Capture(Suffix("-compact"), 0); ClientSize = originalSize;
        mode.SelectedIndex = 2; bool baselineAvailable = !map.Enabled && transport.Enabled;
        var keyHandler = typeof(LauncherChoice).GetMethod("OnKeyDown", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic)!;
        keyHandler.Invoke(mode, [new KeyEventArgs(Keys.Home)]); bool keyboardSelection = mode.SelectedIndex == 0;
        keyHandler.Invoke(mode, [new KeyEventArgs(Keys.End)]); keyboardSelection &= mode.SelectedIndex == 2;
        mode.AccessibilityObject.Value = mode.Items[1]; bool accessibleSelection = mode.SelectedIndex == 1;
        mode.AccessibilityObject.DoDefaultAction(); await Task.Delay(80);
        bool popupSelection = mode.ContextMenuStrip is { Visible: true } menu && menu.Items.Count == 3;
        if (mode.ContextMenuStrip is { } choices)
        {
            using var menuImage = new Bitmap(choices.Width, choices.Height); choices.DrawToBitmap(menuImage, new Rectangle(Point.Empty, choices.Size)); menuImage.Save(Suffix("-profile-menu"));
            choices.Items[0].PerformClick(); popupSelection &= mode.SelectedIndex == 0; choices.Close();
        }
        mode.SelectedIndex = originalMode; map.SelectedIndex = originalMap; SelectPage(0);
        bool visualAssets = GetType().Assembly.GetManifestResourceStream("FS15.Scene.bjornholm") != null && GetType().Assembly.GetManifestResourceStream("FS15.Scene.westbridge") != null;
        bool verified = homeSelection && nativeSelection && baselineAvailable && visualAssets && keyboardSelection && accessibleSelection && popupSelection;
        File.WriteAllText(Path.ChangeExtension(path, ".json"), JsonSerializer.Serialize(new { ui_layout_rendered = true, pages_captured = 3, compact_layout_captured = true,
            embedded_project_scenes = visualAssets, home_selection_verified = homeSelection, native_selection_verified = nativeSelection,
            keyboard_selection_verified = keyboardSelection, accessible_selection_verified = accessibleSelection, popup_selection_verified = popupSelection,
            baseline_available = baselineAvailable, selected_settings_restored = mode.SelectedIndex == originalMode && map.SelectedIndex == originalMap,
            installation_found = game.Text.Length > 0, asset_status_displayed = resources.Text.Contains("LOCAL RESOURCES FOUND"),
            preparation_requested = prepare, preparation_succeeded = preparedInSmoke, game_launched = false, size = new { Width, Height } }, new JsonSerializerOptions { WriteIndented = true }));
        if (!verified) Environment.ExitCode = 1;
        Close();
    }
}
