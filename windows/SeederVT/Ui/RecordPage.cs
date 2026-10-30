using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using SeederVT.Api;
using SeederVT.Core;

namespace SeederVT.Ui;

/// <summary>
/// „Felvétel” fül (hibakereséshez): kiválasztott értékek élő görbéi (utolsó 2 perc), folyamatos CSV
/// felvétel a lekérdezési ütemben, és riasztáskor a teljes állapot automatikus mentése (pillanatkép).
/// </summary>
public sealed class RecordPage : UserControl
{
    private readonly Link _link;
    private readonly AppSettings _settings;
    private readonly CheckedListBox _fields = new() { Dock = DockStyle.Left, Width = 300, CheckOnClick = true };
    private readonly TrendView _trend = new() { Dock = DockStyle.Fill };
    private readonly Button _rec;
    private readonly Label _status = Theme.Lbl("", true);
    private StreamWriter? _csv;
    private List<string> _csvFields = new();
    private long _rows;

    // alapértelmezett, hibakereséshez hasznos mezők
    private static readonly string[] Preset =
    {
        "status.speed", "status.wheel_kmh", "status.ref_kmh", "status.slip", "status.fan_rpm",
        "status.meter.0.rpm", "status.meter.0.target_rpm", "status.meter.0.pwm", "status.meter.0.actual_rate",
        "status.meter.1.rpm", "status.meter.1.target_rpm", "status.meter.1.pwm", "status.meter.1.actual_rate",
        "status.work_down", "status.master", "status.state_id", "status.sec_auto", "status.section_on",
        "status.pressure_bar", "status.temp", "status.cooler_on", "status.cooler_rev_id", "status.alarms",
        "status.crank.counted_revs", "status.crank.hold", "status.aog", "status.aog_sections", "status.tc",
        "isobus.vt_connected", "isobus.tc_connected", "hw.aog.connected", "hw.heap",
    };

    public RecordPage(Link link, AppSettings settings)
    {
        _link = link;
        _settings = settings;
        Dock = DockStyle.Fill;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        _fields.BackColor = Theme.Panel;
        _fields.ForeColor = Theme.Text;
        _fields.BorderStyle = BorderStyle.None;
        foreach (var f in Preset) _fields.Items.Add(f, f is "status.speed" or "status.fan_rpm" or "status.meter.0.rpm"
                                                         or "status.meter.0.pwm" or "status.slip");
        _fields.ItemCheck += (_, _) => BeginInvoke(() => _trend.SetSeries(Checked()));
        _trend.SetSeries(Checked());

        var bar = Theme.Bar();
        _rec = Theme.Btn("● CSV felvétel indítása", (_, _) => ToggleRecord(), Theme.Go);
        bar.Controls.Add(_rec);
        bar.Controls.Add(Theme.Btn("Mappa megnyitása", (_, _) =>
        {
            Directory.CreateDirectory(_settings.RecordDir);
            System.Diagnostics.Process.Start("explorer.exe", _settings.RecordDir);
        }));
        bar.Controls.Add(Theme.Btn("Pillanatkép most", async (_, _) => await Snapshot("kezi", "")));
        bar.Controls.Add(_status);

        Controls.Add(_trend);
        Controls.Add(_fields);
        Controls.Add(bar);

        link.StateReceived += OnState;
        link.NewAlarm += async (code, text) =>
        {
            if (_settings.AlarmSnapshots) await Snapshot(code, text);
        };
    }

    private List<string> Checked() => _fields.CheckedItems.Cast<string>().ToList();

    private void OnState(JsonNode st)
    {
        _trend.Add(st);
        if (_csv == null) return;
        var sb = new StringBuilder();
        sb.Append(DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff", CultureInfo.InvariantCulture));
        foreach (var f in _csvFields)
        {
            sb.Append(';');
            var v = st.At(f);
            sb.Append(v is JsonValue jv && jv.TryGetValue<double>(out var d) ? d.ToString(CultureInfo.InvariantCulture)
                      : v?.ToJsonString().Trim('"').Replace(';', ',') ?? "");
        }
        _csv.WriteLine(sb.ToString());
        if (++_rows % 50 == 0)
        {
            _csv.Flush();
            _status.Text = $"Felvétel: {_rows} sor";
        }
    }

    private void ToggleRecord()
    {
        if (_csv != null)
        {
            _csv.Dispose();
            _csv = null;
            _rec.Text = "● CSV felvétel indítása";
            _rec.BackColor = Theme.Go;
            _status.Text = $"Felvétel leállítva ({_rows} sor)";
            return;
        }
        var all = new List<(string, string)>();
        if (_link.State != null) DiagPage.Flatten(_link.State, "", all);
        _csvFields = all.Select(a => a.Item1).Where(p => !p.StartsWith("status.alarm_list") && !p.StartsWith("hw.io.pins") &&
                                                          !p.StartsWith("status.message")).ToList();
        if (_csvFields.Count == 0) _csvFields = Preset.ToList();
        Directory.CreateDirectory(_settings.RecordDir);
        var file = Path.Combine(_settings.RecordDir, $"felvetel_{DateTime.Now:yyyyMMdd_HHmmss}.csv");
        _csv = new StreamWriter(file, false, new UTF8Encoding(true));
        _csv.WriteLine("ido;" + string.Join(';', _csvFields));
        _rows = 0;
        _rec.Text = "■ Felvétel leállítása";
        _rec.BackColor = Theme.Stop;
        _status.Text = "Felvétel: " + file;
    }

    /// <summary>A teljes állapot (és a virtuális terminál) mentése egy JSON fájlba.</summary>
    private async Task Snapshot(string code, string text)
    {
        try
        {
            var c = _link.Client;
            JsonNode? vt = null;
            if (c != null) { try { vt = await c.Vt(); } catch { } }
            var o = new JsonObject
            {
                ["time"] = DateTime.Now.ToString("O"),
                ["reason"] = code,
                ["text"] = text,
                ["state"] = _link.State?.DeepClone(),
                ["vt"] = vt?.DeepClone(),
            };
            Directory.CreateDirectory(_settings.RecordDir);
            var file = Path.Combine(_settings.RecordDir, $"pillanatkep_{DateTime.Now:yyyyMMdd_HHmmss}_{code}.json");
            await File.WriteAllTextAsync(file, o.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
            _status.Text = "Pillanatkép: " + file;
        }
        catch (Exception e) { _status.Text = "Pillanatkép hiba: " + e.Message; }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _csv?.Dispose();
        base.Dispose(disposing);
    }
}

/// <summary>Egyszerű görbék: soronként egy érték az utolsó 2 percből, saját skálával, aktuális / min / max értékkel.</summary>
public sealed class TrendView : Control
{
    private readonly Dictionary<string, List<(DateTime T, double V)>> _data = new();
    private List<string> _series = new();
    private static readonly TimeSpan Window = TimeSpan.FromMinutes(2);
    private static readonly Color[] Colors =
    {
        Color.FromArgb(74, 163, 255), Color.FromArgb(76, 208, 127), Color.FromArgb(242, 182, 50), Color.FromArgb(255, 100, 100),
        Color.FromArgb(190, 120, 255), Color.FromArgb(90, 220, 220), Color.FromArgb(255, 150, 80), Color.FromArgb(230, 230, 230),
    };

    public TrendView()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
                 ControlStyles.UserPaint, true);
        BackColor = Theme.Bg;
    }

    public void SetSeries(List<string> s)
    {
        _series = s;
        Invalidate();
    }

    public void Add(JsonNode st)
    {
        var now = DateTime.Now;
        foreach (var s in _series)
        {
            var v = st.At(s);
            double d;
            if (v is JsonValue jv && jv.TryGetValue<double>(out d)) { }
            else if (v is JsonValue jb && jb.TryGetValue<bool>(out var b)) d = b ? 1 : 0;
            else if (v is JsonArray a && a.Count > 0 && a[0] is JsonValue a0 && a0.TryGetValue<bool>(out var b0))
                d = a.Select((x, i) => x is JsonValue xv && xv.TryGetValue<bool>(out var bb) && bb ? 1 << i : 0).Sum();
            else continue;
            if (!_data.TryGetValue(s, out var list)) _data[s] = list = new();
            list.Add((now, d));
            while (list.Count > 0 && now - list[0].T > Window) list.RemoveAt(0);
        }
        if (Visible) Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(BackColor);
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        if (_series.Count == 0) return;
        var now = DateTime.Now;
        float rowH = Math.Max(40, (Height - 8) / (float)_series.Count);
        using var font = new Font("Segoe UI", Math.Clamp(rowH * 0.18f, 9, 14), GraphicsUnit.Pixel);
        using var grid = new Pen(Theme.Line);
        for (int i = 0; i < _series.Count; ++i)
        {
            var name = _series[i];
            var top = 4 + i * rowH;
            var plot = new RectangleF(200, top + 4, Width - 210, rowH - 8);
            g.DrawRectangle(grid, plot.X, plot.Y, plot.Width, plot.Height);
            using var brush = new SolidBrush(Colors[i % Colors.Length]);
            g.DrawString(name, font, brush, 6, top + 4);
            if (!_data.TryGetValue(name, out var pts) || pts.Count == 0) continue;
            var min = pts.Min(p => p.V);
            var max = pts.Max(p => p.V);
            var cur = pts[^1].V;
            g.DrawString($"{cur:0.##}   ({min:0.##} … {max:0.##})", font, Brushes.White, 6, top + 6 + font.Height);
            if (max - min < 1e-9) { max += 0.5; min -= 0.5; }
            var path = new List<PointF>();
            foreach (var p in pts)
            {
                var x = plot.Right - (float)((now - p.T).TotalSeconds / Window.TotalSeconds) * plot.Width;
                var y = plot.Bottom - (float)((p.V - min) / (max - min)) * plot.Height;
                path.Add(new PointF(x, y));
            }
            if (path.Count > 1)
                using (var pen = new Pen(Colors[i % Colors.Length], 2)) g.DrawLines(pen, path.ToArray());
        }
    }
}
