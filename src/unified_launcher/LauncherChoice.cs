using System.ComponentModel;

namespace FS15Reconstruction;

// Managed selection control keeps the dark theme consistent in the window,
// keyboard navigation and WinForms layout captures. Choices remain ordinary
// accessible menu items when the popup is open.
internal sealed class LauncherChoice : Control
{
    private int selectedIndex = -1;
    private ContextMenuStrip? popup;
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal List<string> Items { get; } = [];
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    internal int SelectedIndex
    {
        get => selectedIndex;
        set
        {
            if (value < -1 || value >= Items.Count) throw new ArgumentOutOfRangeException(nameof(value));
            if (selectedIndex == value) return;
            selectedIndex = value; Text = value >= 0 ? Items[value] : ""; Invalidate(); SelectedIndexChanged?.Invoke(this, EventArgs.Empty);
            AccessibilityNotifyClients(AccessibleEvents.ValueChange, -1);
        }
    }
    internal event EventHandler? SelectedIndexChanged;
    internal LauncherChoice()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.Selectable, true);
        TabStop = true; Height = 34; Cursor = Cursors.Hand; AccessibleRole = AccessibleRole.ComboBox;
        BackColor = LauncherTheme.Surface; ForeColor = LauncherTheme.Text;
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(BackColor);
        float unit = DeviceDpi / 96f;
        using var border = new Pen(Focused ? LauncherTheme.Green : LauncherTheme.Border);
        e.Graphics.DrawRectangle(border, 0, 0, Width - 1, Height - 1);
        var rect = new Rectangle((int)(12 * unit), 0, Math.Max(0, Width - (int)(38 * unit)), Height);
        TextRenderer.DrawText(e.Graphics, Text, Font, rect, Enabled ? ForeColor : LauncherTheme.Muted,
            TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
        float x = Width - 17 * unit, y = Height / 2f;
        using var arrow = new Pen(Enabled ? LauncherTheme.Text : LauncherTheme.Muted, 1.5f * unit);
        e.Graphics.DrawLines(arrow, [new PointF(x - 4 * unit, y - 2 * unit), new PointF(x, y + 2 * unit), new PointF(x + 4 * unit, y - 2 * unit)]);
    }
    protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
    protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
    protected override void OnEnabledChanged(EventArgs e) { if (!Enabled) popup?.Close(); Invalidate(); base.OnEnabledChanged(e); }
    protected override void OnClick(EventArgs e) { base.OnClick(e); Focus(); ShowChoices(); }
    private void ShowChoices()
    {
        if (!Enabled || Items.Count == 0) return;
        popup?.Dispose(); popup = new ContextMenuStrip { BackColor = LauncherTheme.Surface, ForeColor = LauncherTheme.Text,
            ShowImageMargin = false, ShowCheckMargin = false, Font = Font, Renderer = new ChoiceRenderer(), AutoSize = false, Width = Width };
        int height = (int)(36 * DeviceDpi / 96f); popup.Height = Items.Count * height + 6;
        for (int i = 0; i < Items.Count; i++)
        {
            int index = i; var item = new ToolStripMenuItem(Items[i]) { AutoSize = false, Height = height, Width = Width - 6, ForeColor = LauncherTheme.Text };
            item.Click += (_, _) => { SelectedIndex = index; Focus(); }; popup.Items.Add(item);
        }
        ContextMenuStrip = popup;
        popup.Show(this, new Point(0, Height)); if (selectedIndex >= 0) popup.Items[selectedIndex].Select();
    }
    protected override bool IsInputKey(Keys keyData) => (keyData & Keys.KeyCode) is Keys.Up or Keys.Down or Keys.Home or Keys.End || base.IsInputKey(keyData);
    protected override void OnKeyDown(KeyEventArgs e)
    {
        if ((e.Alt && e.KeyCode == Keys.Down) || e.KeyCode is Keys.Space or Keys.Enter) { ShowChoices(); e.Handled = e.SuppressKeyPress = true; }
        else if (Items.Count > 0 && e.KeyCode is Keys.Up or Keys.Down or Keys.Home or Keys.End)
        {
            SelectedIndex = e.KeyCode switch { Keys.Up => Math.Max(0, selectedIndex - 1), Keys.Down => Math.Min(Items.Count - 1, selectedIndex + 1), Keys.Home => 0, _ => Items.Count - 1 };
            e.Handled = e.SuppressKeyPress = true;
        }
        base.OnKeyDown(e);
    }
    protected override AccessibleObject CreateAccessibilityInstance() => new ChoiceAccessible(this);
    private sealed class ChoiceAccessible(LauncherChoice owner) : ControlAccessibleObject(owner)
    {
        public override string? Value { get => owner.Text; set { int index = owner.Items.IndexOf(value ?? ""); if (index >= 0) owner.SelectedIndex = index; } }
        public override string? DefaultAction => "Open choices";
        public override void DoDefaultAction() => owner.ShowChoices();
    }
    private sealed class ChoiceRenderer : ToolStripProfessionalRenderer
    {
        protected override void OnRenderMenuItemBackground(ToolStripItemRenderEventArgs e)
        { using var fill = new SolidBrush(e.Item.Selected ? Color.FromArgb(57, 84, 45) : LauncherTheme.Surface); e.Graphics.FillRectangle(fill, new Rectangle(Point.Empty, e.Item.Size)); }
    }
    protected override void Dispose(bool disposing) { if (disposing) popup?.Dispose(); base.Dispose(disposing); }
}
