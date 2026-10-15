using System.Diagnostics;

namespace FS15Reconstruction;

internal sealed partial class LauncherForm
{
    private readonly string root, settingsPath;
    private readonly Color background = LauncherTheme.Background, surface = LauncherTheme.Surface,
        text = LauncherTheme.Text, muted = LauncherTheme.Muted, green = LauncherTheme.Green;
    private readonly LauncherChoice mode = Choice("GE6 + recovered gameplay", "ReFarm (experimental)", "GE6 original baseline");
    private readonly LauncherChoice transport = Choice("Automatic", "Direct executable", "Steam client"),
        map = Choice("Bjornholm", "Westbridge Hills"), difficulty = Choice("Easy", "Normal", "Hard");
    private readonly TextBox game = new(), python = new(), steam = new();
    private readonly Label description = new(), status = new(), resources = new(), evidence = new(), libraryStatus = new();
    private readonly RichTextBox output = new();
    private readonly LauncherProgress progress = new();
    private readonly Panel pages = new();
    private readonly List<Control> pageList = [];
    private readonly List<Button> navigation = [], actions = [];
    private readonly Button cancel;
    private readonly List<Control> configuration = [];
    private readonly SceneHero hero = new();
    private readonly RuntimeCard nativeCard = new(), homeCard = new();
    private Process? worker;
    private bool busy, cancelled, closing, preparedInSmoke;
    private int selectedPage;

    private static LauncherChoice Choice(params string[] values)
    {
        var box = new LauncherChoice { Dock = DockStyle.Fill };
        box.Items.AddRange(values); box.SelectedIndex = 0; return box;
    }

    public LauncherForm(string workspace)
    {
        root = workspace; settingsPath = Path.Combine(root, "build", "launcher", "settings.json");
        Text = "FS15 Launcher"; ClientSize = new Size(1280, 810); MinimumSize = new Size(1100, 760);
        AutoScaleMode = AutoScaleMode.Dpi; StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Segoe UI", 10); BackColor = background; ForeColor = text; DoubleBuffered = true;
        var shell = Table(2, 1); shell.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 224)); shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); Controls.Add(shell);
        shell.Controls.Add(BuildSidebar(), 0, 0);
        var body = Table(1, 5);
        body.RowStyles.Add(new RowStyle(SizeType.Absolute, 70)); body.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        body.RowStyles.Add(new RowStyle(SizeType.Absolute, 116)); body.RowStyles.Add(new RowStyle(SizeType.Absolute, 3)); body.RowStyles.Add(new RowStyle(SizeType.Absolute, 35)); shell.Controls.Add(body, 1, 0);
        body.Controls.Add(BuildHeader()); pages.Dock = DockStyle.Fill; pages.Padding = new Padding(24, 0, 24, 20); body.Controls.Add(pages);
        BuildPlay(); BuildResources(); BuildDiagnostics(); body.Controls.Add(BuildLaunchBar());
        progress.Dock = DockStyle.Fill; progress.Margin = Padding.Empty; body.Controls.Add(progress);
        var footer = Table(3, 1); footer.Padding = new Padding(24, 0, 16, 0);
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90)); footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 126));
        status.Dock = DockStyle.Fill; status.AutoEllipsis = true; status.TextAlign = ContentAlignment.MiddleLeft; status.Font = new Font(Font.FontFamily, 8.5f); status.ForeColor = muted;
        status.Text = "Ready. Your next session starts here."; footer.Controls.Add(status);
        cancel = Button("Cancel", CancelWorker); cancel.MinimumSize = Size.Empty; cancel.Dock = DockStyle.Fill; cancel.Enabled = false; footer.Controls.Add(cancel);
        var activity = Button("View activity", () => SelectPage(2)); activity.MinimumSize = Size.Empty; activity.Margin = Padding.Empty; activity.Dock = DockStyle.Fill; footer.Controls.Add(activity); body.Controls.Add(footer);
        foreach (var control in new Control[] { mode, transport, map, difficulty, game, python, steam })
        { control.BackColor = surface; control.ForeColor = text; control.Margin = new Padding(0, 5, 0, 5); configuration.Add(control); }
        LoadSettings(); mode.SelectedIndexChanged += (_, _) => UpdateMode(); map.SelectedIndexChanged += (_, _) => hero.SetMap(map.SelectedIndex == 1);
        mode.AccessibleName = "Launch profile"; map.AccessibleName = "ReFarm map"; difficulty.AccessibleName = "ReFarm difficulty"; transport.AccessibleName = "Native startup";
        hero.SetMap(map.SelectedIndex == 1); SelectPage(0); UpdateMode(); RefreshEvidence();
        Shown += async (_, _) => { if (game.Text.Length == 0) await Detect(); await CheckResources(); };
        FormClosing += (_, e) => { if (busy && !closing) { e.Cancel = true; closing = true; CancelWorker(); return; } SaveSettings(); };
    }

    private static TableLayoutPanel Table(int columns, int rows) => new()
    { Dock = DockStyle.Fill, ColumnCount = columns, RowCount = rows, Margin = Padding.Empty, Padding = Padding.Empty };

    private Label Label(string value, float size = 10, bool bold = false, int width = 0, int height = 0)
        => new() { Text = value, Font = new Font(Font.FontFamily, size, bold ? FontStyle.Bold : FontStyle.Regular), ForeColor = bold ? text : muted, Width = width, Height = height, AutoSize = width == 0, Margin = Padding.Empty };

    private Button Button(string title, Action click)
    {
        var button = new LauncherButton { Text = title, BackColor = surface, ForeColor = text, Font = Font, Height = 38, AutoSize = false, Width = 160,
            MinimumSize = new Size(100, 34), Margin = new Padding(0, 0, 10, 0) };
        button.Click += (_, _) => click(); return button;
    }
    private Button ActionButton(string title, Func<Task> action, bool primary = false)
    {
        var button = (LauncherButton)Button(title, async () => await action()); button.Primary = primary;
        if (primary) button.Font = new Font(Font.FontFamily, 11, FontStyle.Bold); actions.Add(button); return button;
    }

    private Control BuildSidebar()
    {
        var sidebar = Table(1, 3); sidebar.BackColor = LauncherTheme.Sidebar;
        sidebar.RowStyles.Add(new RowStyle(SizeType.Absolute, 155)); sidebar.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); sidebar.RowStyles.Add(new RowStyle(SizeType.Absolute, 100));
        var brand = Table(1, 3); brand.Padding = new Padding(24, 26, 18, 0);
        brand.RowStyles.Add(new RowStyle(SizeType.Absolute, 49)); brand.RowStyles.Add(new RowStyle(SizeType.Absolute, 25)); brand.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
        brand.Controls.Add(Label("FS15", 28, true)); brand.Controls.Add(Label("RECONSTRUCTION", 9, true)); brand.Controls.Add(Label("GAME LAUNCHER", 8)); sidebar.Controls.Add(brand);
        var navigationPanel = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, Padding = new Padding(14, 6, 14, 0), Margin = Padding.Empty };
        navigationPanel.Controls.Add(Label("YOUR LIBRARY", 8, false, 192, 30));
        foreach (string title in new[] { "Farming Simulator 15", "Game resources", "Settings & activity" })
        {
            int index = navigation.Count; var button = (LauncherButton)Button(title, () => SelectPage(index));
            button.Navigation = true; button.Width = 196; button.Height = 54; button.Margin = new Padding(0, 0, 0, 7); button.AccessibleName = title;
            navigation.Add(button); navigationPanel.Controls.Add(button);
        }
        navigationPanel.Controls.Add(Label("", 8, false, 190, 27));
        var guide = Button("Preparation guide  ↗", () => Process.Start(new ProcessStartInfo(Path.Combine(root, "docs", "LAUNCHER.md")) { UseShellExecute = true }));
        guide.Width = 196; guide.BackColor = LauncherTheme.Sidebar; navigationPanel.Controls.Add(guide); sidebar.Controls.Add(navigationPanel);
        var local = Table(1, 2); local.Padding = new Padding(24, 13, 12, 15); local.RowStyles.Add(new RowStyle(SizeType.Absolute, 25)); local.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        local.Controls.Add(Label("LOCAL INSTALLATION", 9, true)); libraryStatus.Dock = DockStyle.Fill; libraryStatus.Font = new Font(Font.FontFamily, 8.5f); libraryStatus.ForeColor = muted; libraryStatus.Text = "Checking resources..."; local.Controls.Add(libraryStatus); sidebar.Controls.Add(local);
        return sidebar;
    }

    private Control BuildHeader()
    {
        var header = Table(2, 1); header.Padding = new Padding(26, 0, 24, 0);
        header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); header.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 240));
        var title = Label("Farming Simulator 15", 16, true); title.Dock = DockStyle.Fill; title.TextAlign = ContentAlignment.MiddleLeft; header.Controls.Add(title);
        var badge = Label("WINDOWS  /  DEVELOPMENT BUILD", 8); badge.Dock = DockStyle.Fill; badge.TextAlign = ContentAlignment.MiddleRight; header.Controls.Add(badge); return header;
    }

    private TableLayoutPanel Page(string title, string subtitle)
    {
        var page = Table(1, 3); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 50)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 49)); page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        page.Controls.Add(Label(title, 24, true)); var copy = Label(subtitle); copy.Dock = DockStyle.Fill; page.Controls.Add(copy);
        pages.Controls.Add(page); pageList.Add(page); return page;
    }

    private void BuildPlay()
    {
        var page = Table(1, 3); page.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 46)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 120));
        hero.Dock = DockStyle.Fill; hero.Margin = Padding.Empty; page.Controls.Add(hero);
        var line = Table(2, 1); line.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); line.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 180));
        var choose = Label("CHOOSE YOUR EXPERIENCE", 9, true); choose.Dock = DockStyle.Fill; choose.TextAlign = ContentAlignment.MiddleLeft; line.Controls.Add(choose);
        var check = ActionButton("Check launch plan  →", () => Run(true)); check.Dock = DockStyle.Fill; check.Margin = new Padding(0, 6, 0, 6); check.BackColor = background; line.Controls.Add(check); page.Controls.Add(line);
        var cards = Table(2, 1); cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50)); cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        nativeCard.Heading = "Original engine + recovered code"; nativeCard.Badge = "GIANTS ENGINE 6"; nativeCard.Copy = "Native rendering and physics.\nYour career opens inside GE6.";
        homeCard.Heading = "ReFarm"; homeCard.Badge = "EXPERIMENTAL ENGINE"; homeCard.Copy = "Explore the original maps and recovered gameplay.\nPhysics and farming are still in progress.";
        nativeCard.Dock = homeCard.Dock = DockStyle.Fill; nativeCard.Margin = new Padding(0, 0, 8, 0); homeCard.Margin = new Padding(8, 0, 0, 0);
        nativeCard.AccessibleName = "Select original GE6 with recovered code"; homeCard.AccessibleName = "Select ReFarm experimental engine";
        nativeCard.Click += (_, _) => { if (!busy) mode.SelectedIndex = 0; }; homeCard.Click += (_, _) => { if (!busy) mode.SelectedIndex = 1; };
        configuration.AddRange([nativeCard, homeCard]); cards.Controls.Add(nativeCard); cards.Controls.Add(homeCard); page.Controls.Add(cards);
        pages.Controls.Add(page); pageList.Add(page);
    }

    private TableLayoutPanel Fields(int rows)
    {
        var panel = Table(3, rows); panel.Height = rows * 45; panel.Dock = DockStyle.Top;
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150)); panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100));
        for (int i = 0; i < rows; i++) panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 45)); return panel;
    }
    private void Row(TableLayoutPanel panel, int row, string name, Control control, Action? browse = null)
    {
        var label = Label(name, 9); label.Dock = DockStyle.Fill; label.TextAlign = ContentAlignment.MiddleLeft; panel.Controls.Add(label, 0, row);
        control.Dock = DockStyle.Fill; panel.Controls.Add(control, 1, row);
        if (browse == null) panel.SetColumnSpan(control, 2);
        else { var button = Button("Browse", browse); button.MinimumSize = Size.Empty; button.Dock = DockStyle.Fill; button.Margin = new Padding(8, 4, 0, 5); panel.Controls.Add(button, 2, row); configuration.Add(button); }
    }

    private void BuildResources()
    {
        var page = Page("Your game, ready to play.", "Connect your installed copy of FS15 and let the launcher prepare its local resources.");
        var content = Table(1, 4); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 75)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 53));
        content.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));
        var fields = Fields(1); fields.Padding = new Padding(16, 10, 16, 0); fields.BackColor = surface; Row(fields, 0, "FS15 installation", game, () => BrowseFolder(game)); fields.Height = 65; content.Controls.Add(fields);
        var checks = new FlowLayoutPanel { Dock = DockStyle.Fill, Margin = Padding.Empty }; checks.Controls.Add(ActionButton("Detect installation", Detect)); checks.Controls.Add(ActionButton("Check resources", CheckResources)); content.Controls.Add(checks);
        resources.Dock = DockStyle.Fill; resources.ForeColor = muted; resources.Padding = new Padding(22); resources.BackColor = surface;
        resources.Text = "Checking your local resources..."; resources.AutoEllipsis = true; content.Controls.Add(resources);
        var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(0, 12, 0, 0), Margin = Padding.Empty };
        var prepare = ActionButton("Prepare / repair assets", PrepareAssets, true); prepare.Width = 228; prepare.Height = 44; buttons.Controls.Add(prepare);
        buttons.Controls.Add(Label("Original files stay on your PC.\nYour installation is kept intact.", 9, false, 330, 43)); content.Controls.Add(buttons); page.Controls.Add(content);
    }

    private void BuildDiagnostics()
    {
        var page = Page("Settings & activity", "Manage startup options, check the current setup and follow local operations.");
        var content = Table(1, 4); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 138)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 100));
        content.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 47));
        var fields = Fields(3); Row(fields, 0, "Python executable", python, () => BrowseExe(python)); Row(fields, 1, "Steam (optional)", steam, () => BrowseExe(steam)); Row(fields, 2, "Native startup", transport); content.Controls.Add(fields);
        evidence.Dock = DockStyle.Fill; evidence.ForeColor = muted; evidence.Font = new Font(Font.FontFamily, 9); evidence.Padding = new Padding(12, 7, 12, 7); evidence.BackColor = surface; evidence.AutoEllipsis = true; content.Controls.Add(evidence);
        output.Dock = DockStyle.Fill; output.Margin = new Padding(0, 9, 0, 8); output.ReadOnly = true; output.BorderStyle = BorderStyle.None; output.BackColor = Color.FromArgb(14, 16, 17); output.ForeColor = muted; output.Font = new Font("Consolas", 8.5f); content.Controls.Add(output);
        var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, Margin = Padding.Empty };
        buttons.Controls.Add(ActionButton("Verify native session", Verify)); buttons.Controls.Add(Button("Open logs", OpenLogs)); buttons.Controls.Add(Button("Copy activity", () => { if (output.TextLength > 0) Clipboard.SetText(output.Text); })); content.Controls.Add(buttons); page.Controls.Add(content);
    }

    private Control BuildLaunchBar()
    {
        var bar = Table(4, 1); bar.BackColor = Color.FromArgb(31, 34, 31); bar.Padding = new Padding(24, 17, 24, 10);
        bar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 178));
        bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 132)); bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 202));
        Control Field(string title, Control control, int right)
        {
            var field = Table(1, 3); field.Padding = new Padding(0, 0, right, 0); field.RowStyles.Add(new RowStyle(SizeType.Absolute, 20)); field.RowStyles.Add(new RowStyle(SizeType.Absolute, 37)); field.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            field.Controls.Add(Label(title, 8, true)); control.Dock = DockStyle.Fill; field.Controls.Add(control); return field;
        }
        var profile = (TableLayoutPanel)Field("LAUNCH PROFILE", mode, 18);
        description.Dock = DockStyle.Fill; description.Font = new Font(Font.FontFamily, 8); description.ForeColor = muted; description.AutoEllipsis = true; profile.Controls.Add(description); bar.Controls.Add(profile);
        bar.Controls.Add(Field("MAP  /  REFARM", map, 18)); bar.Controls.Add(Field("DIFFICULTY", difficulty, 18));
        var play = ActionButton("PLAY", () => Run(false), true); play.Dock = DockStyle.Fill; play.Font = new Font(Font.FontFamily, 18, FontStyle.Bold); play.Margin = new Padding(8, 5, 0, 18); play.AccessibleName = "Play selected runtime"; bar.Controls.Add(play); return bar;
    }

    private void SelectPage(int index)
    {
        selectedPage = index;
        for (int i = 0; i < pageList.Count; i++) { pageList[i].Visible = i == index; ((LauncherButton)navigation[i]).Active = i == index; navigation[i].Invalidate(); }
    }
}
