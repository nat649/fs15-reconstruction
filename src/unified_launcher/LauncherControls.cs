using System.Drawing.Drawing2D;
using System.Reflection;
using System.ComponentModel;

namespace FS15Reconstruction;

internal static class LauncherTheme
{
    internal static readonly Color Background = Color.FromArgb(20, 22, 24), Sidebar = Color.FromArgb(29, 31, 33),
        Surface = Color.FromArgb(37, 40, 43), Border = Color.FromArgb(57, 61, 63),
        Text = Color.FromArgb(244, 245, 242), Muted = Color.FromArgb(165, 174, 170), Green = Color.FromArgb(93, 174, 61);

    internal static Image? Scene(string name)
    {
        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("FS15.Scene." + name);
        if (stream == null) return null;
        using var original = Image.FromStream(stream); return new Bitmap(original);
    }

    internal static void Cover(Graphics graphics, Image image, Rectangle bounds)
    {
        float scale = Math.Max(bounds.Width / (float)image.Width, bounds.Height / (float)image.Height);
        float width = bounds.Width / scale, height = bounds.Height / scale;
        graphics.DrawImage(image, bounds, (image.Width - width) / 2, (image.Height - height) / 2, width, height, GraphicsUnit.Pixel);
    }
}

internal sealed class LauncherButton : Button
{
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal bool Primary { get; set; }
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal bool Navigation { get; set; }
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal bool Active { get; set; }
    private bool hover;
    internal LauncherButton()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
        FlatStyle = FlatStyle.Flat; FlatAppearance.BorderSize = 0; Cursor = Cursors.Hand;
    }
    protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }
    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        Color fill = Primary ? (hover ? Color.FromArgb(108, 193, 74) : LauncherTheme.Green)
            : Active ? Color.FromArgb(47, 61, 43) : hover ? Color.FromArgb(53, 57, 59) : BackColor;
        using var brush = new SolidBrush(Enabled ? fill : Color.FromArgb(39, 43, 41)); g.FillRectangle(brush, ClientRectangle);
        float unit = DeviceDpi / 96f;
        if (Primary)
        {
            using var edge = new SolidBrush(Color.FromArgb(50, 94, 33)); g.FillRectangle(edge, 0, Height - 4 * unit, Width, 4 * unit);
            using var shine = new Pen(Color.FromArgb(143, 208, 116)); g.DrawLine(shine, 1, 1, Width - 2, 1);
        }
        if (Active && Navigation) { using var stripe = new SolidBrush(LauncherTheme.Green); g.FillRectangle(stripe, 0, 0, 4 * unit, Height); }
        var rect = ClientRectangle; rect.Inflate(-(int)(14 * unit), -(int)(2 * unit));
        var flags = TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix;
        flags |= Navigation ? TextFormatFlags.Left : TextFormatFlags.HorizontalCenter;
        TextRenderer.DrawText(g, Text, Font, rect, Enabled ? ForeColor : LauncherTheme.Muted, flags);
        if (Focused && ShowFocusCues) { var focus = ClientRectangle; focus.Inflate(-5, -5); ControlPaint.DrawFocusRectangle(g, focus); }
    }
}

internal sealed class SceneHero : Control
{
    private readonly Image? bjornholm = LauncherTheme.Scene("bjornholm"), westbridge = LauncherTheme.Scene("westbridge");
    private bool second;
    internal void SetMap(bool value) { second = value; Invalidate(); }
    internal SceneHero() { DoubleBuffered = true; BackColor = LauncherTheme.Background; }
    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias;
        var image = second ? westbridge : bjornholm;
        if (image != null) LauncherTheme.Cover(g, image, ClientRectangle);
        using var overlay = new LinearGradientBrush(ClientRectangle, Color.FromArgb(228, 13, 19, 14), Color.FromArgb(15, 13, 19, 14), LinearGradientMode.Horizontal);
        g.FillRectangle(overlay, ClientRectangle);
        using var bottom = new LinearGradientBrush(ClientRectangle, Color.Transparent, Color.FromArgb(190, 13, 19, 14), LinearGradientMode.Vertical);
        g.FillRectangle(bottom, ClientRectangle);
        float scale = DeviceDpi / 96f; float width = Width / scale, height = Height / scale;
        g.ScaleTransform(scale, scale);
        using var eyebrow = new Font("Segoe UI", 10, FontStyle.Bold, GraphicsUnit.Pixel);
        using var title = new Font("Segoe UI", height < 300 ? 37 : 47, FontStyle.Bold, GraphicsUnit.Pixel);
        using var copy = new Font("Segoe UI", 13, FontStyle.Regular, GraphicsUnit.Pixel);
        using var white = new SolidBrush(LauncherTheme.Text); using var green = new SolidBrush(Color.FromArgb(173, 222, 144)); using var grey = new SolidBrush(Color.FromArgb(218, 225, 214));
        g.DrawString("THE RECONSTRUCTION PROJECT", eyebrow, green, 32, 30);
        g.DrawString("FARMING\nSIMULATOR 15", title, white, new RectangleF(28, Math.Max(54, height * 0.22f), width - 50, 140));
        if (height > 300) g.DrawString("Return to the fields. Explore familiar worlds.", copy, grey, 33, Math.Min(height - 85, height * 0.22f + 145));
        g.DrawString(second ? "WESTBRIDGE HILLS  /  PROJECT SCENE" : "BJORNHOLM  /  PROJECT SCENE", eyebrow, white, 33, height - 35);
        const string credit = "ReFarm / experimental capture";
        var size = g.MeasureString(credit, eyebrow);
        if (width > 680) g.DrawString(credit, eyebrow, grey, width - size.Width - 24, height - 35);
    }
    protected override void Dispose(bool disposing) { if (disposing) { bjornholm?.Dispose(); westbridge?.Dispose(); } base.Dispose(disposing); }
}

internal sealed class RuntimeCard : Button
{
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal string Heading { get; set; } = "";
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal string Copy { get; set; } = "";
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal string Badge { get; set; } = "";
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal bool Selected { get; set; }
    private bool hover;
    internal RuntimeCard()
    {
        DoubleBuffered = true; FlatStyle = FlatStyle.Flat; FlatAppearance.BorderSize = 0; Cursor = Cursors.Hand;
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint, true);
    }
    protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }
    protected override void OnPaint(PaintEventArgs e)
    {
        float s = DeviceDpi / 96f; var g = e.Graphics; g.ScaleTransform(s, s); float w = Width / s, h = Height / s;
        using var fill = new SolidBrush(Selected ? Color.FromArgb(39, 48, 35) : hover ? Color.FromArgb(44, 47, 46) : LauncherTheme.Surface);
        g.FillRectangle(fill, 0, 0, w, h);
        using var border = new Pen(Selected ? LauncherTheme.Green : LauncherTheme.Border, Selected ? 2 : 1); g.DrawRectangle(border, 1, 1, w - 2, h - 2);
        using var heading = new Font("Segoe UI", 16, FontStyle.Bold, GraphicsUnit.Pixel);
        using var body = new Font("Segoe UI", 12, FontStyle.Regular, GraphicsUnit.Pixel);
        using var tag = new Font("Segoe UI", 9, FontStyle.Bold, GraphicsUnit.Pixel);
        using var white = new SolidBrush(LauncherTheme.Text); using var grey = new SolidBrush(LauncherTheme.Muted); using var green = new SolidBrush(Color.FromArgb(170, 215, 148));
        g.DrawString(Badge.ToUpperInvariant(), tag, Selected ? green : grey, 18, 14);
        g.DrawString(Heading, heading, white, 17, 34);
        g.DrawString(Copy, body, grey, new RectangleF(18, 61, w - 36, h - 66));
        if (Selected) g.FillEllipse(green, w - 24, 17, 7, 7);
        if (Focused && ShowFocusCues) { g.ResetTransform(); var r = ClientRectangle; r.Inflate(-5, -5); ControlPaint.DrawFocusRectangle(g, r); }
    }
}

internal sealed class LauncherProgress : Control
{
    private int value;
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)] internal int Value { get => value; set { this.value = Math.Clamp(value, 0, 100); Invalidate(); } }
    internal LauncherProgress() { DoubleBuffered = true; }
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(LauncherTheme.Surface);
        using var brush = new SolidBrush(LauncherTheme.Green); e.Graphics.FillRectangle(brush, 0, 0, Width * value / 100f, Height);
    }
}
