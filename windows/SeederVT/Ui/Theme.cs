namespace SeederVT.Ui;

/// <summary>Szürke téma (mint a VT és a webes felület).</summary>
public static class Theme
{
    public static readonly Color Bg = Color.FromArgb(59, 62, 67);
    public static readonly Color Panel = Color.FromArgb(74, 78, 84);
    public static readonly Color Panel2 = Color.FromArgb(86, 91, 98);
    public static readonly Color Line = Color.FromArgb(102, 108, 116);
    public static readonly Color Text = Color.FromArgb(243, 245, 247);
    public static readonly Color Muted = Color.FromArgb(195, 200, 206);
    public static readonly Color Ok = Color.FromArgb(76, 208, 127);
    public static readonly Color Warn = Color.FromArgb(242, 182, 50);
    public static readonly Color Bad = Color.FromArgb(255, 100, 100);
    public static readonly Color Accent = Color.FromArgb(74, 163, 255);
    public static readonly Color Go = Color.FromArgb(42, 138, 77);
    public static readonly Color Stop = Color.FromArgb(150, 51, 51);

    public static Button Btn(string text, EventHandler click, Color? bg = null)
    {
        var b = new Button
        {
            Text = text,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            FlatStyle = FlatStyle.Flat,
            BackColor = bg ?? Panel2,
            ForeColor = Text,
            Padding = new Padding(10, 6, 10, 6),
            Margin = new Padding(4),
        };
        b.FlatAppearance.BorderColor = Line;
        b.Click += click;
        return b;
    }

    public static Label Lbl(string text, bool muted = false) => new()
    {
        Text = text,
        AutoSize = true,
        ForeColor = muted ? Muted : Text,
        Margin = new Padding(4, 8, 4, 4),
    };

    public static void Style(ListView lv)
    {
        lv.BackColor = Panel;
        lv.ForeColor = Text;
        lv.BorderStyle = BorderStyle.None;
        lv.FullRowSelect = true;
        lv.View = View.Details;
        lv.HideSelection = false;
    }

    public static FlowLayoutPanel Bar() => new()
    {
        Dock = DockStyle.Top,
        AutoSize = true,
        AutoSizeMode = AutoSizeMode.GrowAndShrink,
        BackColor = Panel,
        Padding = new Padding(6),
        WrapContents = true,
    };
}
