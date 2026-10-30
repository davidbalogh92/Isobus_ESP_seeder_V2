using System.Globalization;
using SeederVT.Vt;

namespace SeederVT.Ui;

/// <summary>Számbevitel érintőképernyőre is: nagy kijelző + számbillentyűzet, tartomány-ellenőrzéssel.</summary>
public sealed class NumberDialog : Form
{
    private readonly TextBox _box;
    private readonly Label _info;
    private readonly double _min, _max;
    private readonly int _decimals;
    public double Value { get; private set; }

    public NumberDialog(string title, double value, double min, double max, int decimals)
    {
        _min = min;
        _max = max;
        _decimals = decimals;
        Text = title;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterParent;
        MaximizeBox = MinimizeBox = false;
        ShowInTaskbar = false;
        BackColor = Color.FromArgb(74, 78, 84);
        ForeColor = Color.White;
        Font = new Font("Segoe UI", 16f);
        ClientSize = new Size(380, 520);
        KeyPreview = true;

        _box = new TextBox
        {
            Text = value.ToString("F" + decimals, CultureInfo.InvariantCulture),
            Font = new Font("Consolas", 28f, FontStyle.Bold),
            TextAlign = HorizontalAlignment.Right,
            Dock = DockStyle.Top,
        };
        _info = new Label
        {
            Text = $"{min.ToString("F" + decimals, CultureInfo.InvariantCulture)} … {max.ToString("F" + decimals, CultureInfo.InvariantCulture)}",
            Dock = DockStyle.Top,
            Height = 36,
            TextAlign = ContentAlignment.MiddleCenter,
            ForeColor = Color.FromArgb(200, 205, 210),
        };
        var grid = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 5, Padding = new Padding(6) };
        for (int i = 0; i < 3; ++i) grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.3f));
        for (int i = 0; i < 5; ++i) grid.RowStyles.Add(new RowStyle(SizeType.Percent, 20));
        string[] keys = { "7", "8", "9", "4", "5", "6", "1", "2", "3", "−", "0", ".", "⌫", "C", "OK" };
        foreach (var k in keys)
        {
            var b = new Button
            {
                Text = k,
                Dock = DockStyle.Fill,
                FlatStyle = FlatStyle.Flat,
                BackColor = k == "OK" ? Color.FromArgb(42, 138, 77) : Color.FromArgb(86, 91, 98),
                ForeColor = Color.White,
                Margin = new Padding(4),
            };
            b.FlatAppearance.BorderColor = Color.FromArgb(110, 116, 124);
            b.Click += (_, _) => Press(k);
            grid.Controls.Add(b);
        }
        Controls.Add(grid);
        Controls.Add(_info);
        Controls.Add(_box);
        KeyDown += (_, e) =>
        {
            if (e.KeyCode == Keys.Enter) { Press("OK"); e.Handled = true; }
            else if (e.KeyCode == Keys.Escape) { DialogResult = DialogResult.Cancel; Close(); }
        };
        Shown += (_, _) => { _box.Focus(); _box.SelectAll(); };
    }

    private void Press(string k)
    {
        var t = _box.SelectionLength == _box.TextLength && k.Length == 1 && char.IsDigit(k[0]) ? "" : _box.Text;
        switch (k)
        {
            case "⌫": if (t.Length > 0) t = t[..^1]; break;
            case "C": t = ""; break;
            case "−": t = t.StartsWith('-') ? t[1..] : "-" + t; break;
            case ".": if (!t.Contains('.') && _decimals > 0) t += "."; break;
            case "OK":
                var s = _box.Text.Replace(',', '.');
                if (!double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out var v))
                {
                    _info.Text = "Érvénytelen szám";
                    _info.ForeColor = Color.FromArgb(255, 120, 120);
                    return;
                }
                if (v < _min - 1e-9 || v > _max + 1e-9)
                {
                    _info.Text = $"Tartomány: {_min.ToString("F" + _decimals, CultureInfo.InvariantCulture)} … {_max.ToString("F" + _decimals, CultureInfo.InvariantCulture)}";
                    _info.ForeColor = Color.FromArgb(255, 120, 120);
                    return;
                }
                Value = v;
                DialogResult = DialogResult.OK;
                Close();
                return;
            default: t += k; break;
        }
        _box.Text = t;
        _box.SelectionStart = _box.TextLength;
        _box.SelectionLength = 0;
    }

    /// <summary>Egy VT Input Number objektum szerkesztése: a nyers értéket adja vissza (vagy null).</summary>
    public static uint? Edit(IWin32Window owner, VtObject o, uint raw)
    {
        double Eng(uint r) => ((double)r + o.Offset) * o.Scale;
        using var d = new NumberDialog("Érték megadása", Eng(raw), Eng(o.Min), Eng(o.Max), o.Decimals);
        if (d.ShowDialog(owner) != DialogResult.OK) return null;
        var r = Math.Round(d.Value / (o.Scale == 0 ? 1 : o.Scale)) - o.Offset;
        r = Math.Clamp(r, o.Min, o.Max);
        return (uint)r;
    }
}

/// <summary>Listából választás nagy gombokkal (VT Input List).</summary>
public sealed class ListDialog : Form
{
    public int Selected { get; private set; } = -1;

    public ListDialog(string title, IReadOnlyList<string> items, int current)
    {
        Text = title;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterParent;
        MaximizeBox = MinimizeBox = false;
        ShowInTaskbar = false;
        BackColor = Color.FromArgb(74, 78, 84);
        Font = new Font("Segoe UI", 15f);
        var h = Math.Min(64 * items.Count + 20, 680);
        ClientSize = new Size(420, h);
        var panel = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true,
            Padding = new Padding(8),
        };
        for (int i = 0; i < items.Count; ++i)
        {
            var idx = i;
            var b = new Button
            {
                Text = items[i],
                Width = 380,
                Height = 56,
                FlatStyle = FlatStyle.Flat,
                BackColor = i == current ? Color.FromArgb(74, 163, 255) : Color.FromArgb(86, 91, 98),
                ForeColor = Color.White,
                TextAlign = ContentAlignment.MiddleLeft,
                Margin = new Padding(2, 2, 2, 4),
            };
            b.Click += (_, _) => { Selected = idx; DialogResult = DialogResult.OK; Close(); };
            panel.Controls.Add(b);
        }
        Controls.Add(panel);
        KeyPreview = true;
        KeyDown += (_, e) => { if (e.KeyCode == Keys.Escape) Close(); };
    }
}
