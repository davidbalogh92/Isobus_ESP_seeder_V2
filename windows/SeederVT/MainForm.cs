using SeederVT.Api;
using SeederVT.Core;
using SeederVT.Ui;

namespace SeederVT;

/// <summary>
/// Főablak. Fül: Terminál (a VT felület, ugyanaz, mint az ISOBUS terminálon), Diagnosztika, Napló,
/// Felvétel, Rendszer. Felül a kapcsolat állapota. Minden az ablakmérethez igazodik.
/// </summary>
public sealed class MainForm : Form
{
    private readonly AppSettings _settings = AppSettings.Load();
    private readonly Link _link = new();
    private readonly TabControl _tabs = new() { Dock = DockStyle.Fill };
    private readonly TerminalPage _terminal;
    private readonly Label _conn = Pill("Nincs kapcsolat");
    private readonly Label _vt = Pill("VT");
    private readonly Label _tc = Pill("TC");
    private readonly Label _aog = Pill("AOG");
    private readonly Label _lat = Pill("–");
    private readonly Label _alarm = Pill("");
    private float _baseFont = 10f;

    private static Label Pill(string t) => new()
    {
        Text = t,
        AutoSize = true,
        Padding = new Padding(8, 3, 8, 3),
        Margin = new Padding(4, 6, 4, 4),
        BorderStyle = BorderStyle.FixedSingle,
        ForeColor = Theme.Muted,
    };

    public MainForm()
    {
        Text = "Vetőgép-vezérlő – " + Application.ProductVersion.Split('+')[0];
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        Font = new Font("Segoe UI", _baseFont);
        MinimumSize = new Size(800, 520);
        Size = new Size(Math.Max(800, _settings.WindowW), Math.Max(520, _settings.WindowH));
        StartPosition = FormStartPosition.CenterScreen;
        if (_settings.Maximized) WindowState = FormWindowState.Maximized;
        KeyPreview = true;
        _link.PollMs = _settings.PollMs;

        _terminal = new TerminalPage(_link, _settings);
        var system = new SystemPage(_link, _settings);
        system.ConnectRequested += Connect;
        AddTab("Terminál", _terminal);
        AddTab("Diagnosztika", new DiagPage(_link));
        AddTab("Napló", new LogPage(_link, _settings));
        AddTab("Felvétel", new RecordPage(_link, _settings));
        AddTab("Rendszer", system);
        _tabs.Appearance = TabAppearance.Normal;
        _tabs.SizeMode = TabSizeMode.Fixed;
        _tabs.DrawMode = TabDrawMode.OwnerDrawFixed;
        _tabs.DrawItem += DrawTab;

        var top = new FlowLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = Color.FromArgb(52, 55, 60),
            Padding = new Padding(6, 2, 6, 2),
            WrapContents = false,
        };
        var title = new Label { Text = "Vetőgép", AutoSize = true, Font = new Font("Segoe UI Semibold", 13f), Margin = new Padding(4, 6, 16, 4) };
        top.Controls.AddRange(new Control[] { title, _conn, _vt, _tc, _aog, _lat, _alarm });

        Controls.Add(_tabs);
        Controls.Add(top);

        _link.StateReceived += OnState;
        _link.ConnectionChanged += ok =>
        {
            _conn.Text = ok ? "Kapcsolódva: " + (_link.Client?.BaseUrl ?? "") : "Nincs kapcsolat";
            _conn.ForeColor = ok ? Theme.Ok : Theme.Bad;
        };
        Resize += (_, _) => Rescale();
        Shown += (_, _) =>
        {
            Rescale();
            Connect(_settings.LastHost);
        };
        FormClosing += (_, _) =>
        {
            _settings.Maximized = WindowState == FormWindowState.Maximized;
            if (WindowState == FormWindowState.Normal)
            {
                _settings.WindowW = Width;
                _settings.WindowH = Height;
            }
            _settings.Save();
            _link.Stop();
        };
        KeyDown += (_, e) => { if (_tabs.SelectedIndex == 0 && _terminal.HandleKey(e.KeyCode, true)) e.Handled = true; };
        KeyUp += (_, e) => { if (_tabs.SelectedIndex == 0 && _terminal.HandleKey(e.KeyCode, false)) e.Handled = true; };
    }

    private void AddTab(string text, Control page)
    {
        var tp = new TabPage(text) { BackColor = Theme.Bg, ForeColor = Theme.Text, Padding = new Padding(0) };
        tp.Controls.Add(page);
        _tabs.TabPages.Add(tp);
    }

    private void DrawTab(object? sender, DrawItemEventArgs e)
    {
        var sel = e.Index == _tabs.SelectedIndex;
        using var bg = new SolidBrush(sel ? Theme.Accent : Theme.Panel);
        e.Graphics.FillRectangle(bg, e.Bounds);
        TextRenderer.DrawText(e.Graphics, _tabs.TabPages[e.Index].Text, _tabs.Font, e.Bounds, sel ? Color.White : Theme.Muted,
                              TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
    }

    /// <summary>A betűméret az ablak magasságához igazodik (a terminál kép magától méreteződik).</summary>
    private void Rescale()
    {
        var k = Math.Clamp(ClientSize.Height / 800f, 0.8f, 1.8f);
        var size = _baseFont * k;
        if (Math.Abs(Font.Size - size) < 0.3f) return;
        Font = new Font("Segoe UI", size);
        _tabs.ItemSize = new Size((int)(140 * k), (int)(34 * k));
    }

    private void Connect(string host)
    {
        if (string.IsNullOrWhiteSpace(host)) return;
        _link.Start(host);
        foreach (TabPage tp in _tabs.TabPages)
            if (tp.Controls.Count > 0 && tp.Controls[0] is SystemPage sp) sp.RememberHost(host);
    }

    private void OnState(System.Text.Json.Nodes.JsonNode st)
    {
        var s = st["status"];
        var iso = st["isobus"];
        Set(_vt, iso.B("vt_connected") ? "VT ✓" : "VT –", iso.B("vt_connected") ? Theme.Ok : Theme.Muted);
        Set(_tc, s.B("tc") ? "TC ✓" : "TC –", s.B("tc") ? Theme.Ok : Theme.Muted);
        var aogOn = st.At("hw.aog")?.B("enabled") == true;
        Set(_aog, !aogOn ? "AOG ki" : s.B("aog") ? "AOG ✓" : "AOG –", !aogOn ? Theme.Muted : s.B("aog") ? Theme.Ok : Theme.Warn);
        Set(_lat, $"{_link.LatencyMs:F0} ms", _link.LatencyMs < 150 ? Theme.Muted : Theme.Warn);
        var top = s.Arr("alarm_list").FirstOrDefault(a => a.I("sev") == 2);
        var warn = s.Arr("alarm_list").FirstOrDefault(a => a.I("sev") == 1);
        if (top != null) Set(_alarm, "⚠ " + top.S("text"), Theme.Bad);
        else if (warn != null) Set(_alarm, "! " + warn.S("text"), Theme.Warn);
        else Set(_alarm, "", Theme.Muted);
    }

    private static void Set(Label l, string text, Color c)
    {
        if (l.Text != text) l.Text = text;
        if (l.ForeColor != c) l.ForeColor = c;
        l.Visible = text.Length > 0;
    }
}
