using SeederVT.Api;
using SeederVT.Core;

namespace SeederVT.Ui;

/// <summary>„Napló” fül: a vezérlő eseménynaplója és a program saját eseményei (kapcsolat, riasztások, visszajelzések).</summary>
public sealed class LogPage : UserControl
{
    private readonly Link _link;
    private readonly ListView _device = new() { Dock = DockStyle.Fill };
    private readonly ListView _local = new() { Dock = DockStyle.Fill };
    private readonly System.Windows.Forms.Timer _timer = new() { Interval = 3000 };
    private readonly List<LocalEvent> _events = new();
    private readonly string _file;

    public LogPage(Link link, AppSettings settings)
    {
        _link = link;
        Dock = DockStyle.Fill;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        Theme.Style(_device);
        Theme.Style(_local);
        _device.Columns.Add("Idő (ms)", 110);
        _device.Columns.Add("Szint", 70);
        _device.Columns.Add("Üzenet", 700);
        _local.Columns.Add("Idő", 150);
        _local.Columns.Add("Szint", 70);
        _local.Columns.Add("Esemény", 700);
        _file = Path.Combine(settings.RecordDir, $"esemenyek_{DateTime.Now:yyyyMMdd}.log");

        var split = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal, BackColor = Theme.Line };
        var top = new Panel { Dock = DockStyle.Fill };
        var bot = new Panel { Dock = DockStyle.Fill };
        var bar1 = Theme.Bar();
        bar1.Controls.Add(Theme.Lbl("A vezérlő eseménynaplója"));
        bar1.Controls.Add(Theme.Btn("Frissítés", async (_, _) => await Refresh()));
        bar1.Controls.Add(Theme.Btn("Mentés fájlba…", async (_, _) => await SaveDevice()));
        top.Controls.Add(_device);
        top.Controls.Add(bar1);
        var bar2 = Theme.Bar();
        bar2.Controls.Add(Theme.Lbl("A program eseményei (riasztások, visszajelzések, kapcsolat) – automatikusan fájlba is: " + _file, true));
        bar2.Controls.Add(Theme.Btn("Törlés", (_, _) => { _events.Clear(); _local.Items.Clear(); }));
        bot.Controls.Add(_local);
        bot.Controls.Add(bar2);
        split.Panel1.Controls.Add(top);
        split.Panel2.Controls.Add(bot);
        Controls.Add(split);

        link.Event += Add;
        _timer.Tick += async (_, _) => { if (Visible) await Refresh(); };
        _timer.Start();
    }

    private void Add(LocalEvent e)
    {
        _events.Add(e);
        var it = new ListViewItem(new[] { e.Time.ToString("yyyy-MM-dd HH:mm:ss.fff"), e.Level, e.Text })
        {
            BackColor = Theme.Panel,
            ForeColor = e.Level switch { "ALARM" or "ERROR" => Theme.Bad, "WARN" => Theme.Warn, "MSG" => Theme.Accent, _ => Theme.Text },
        };
        _local.Items.Insert(0, it);
        if (_local.Items.Count > 2000) _local.Items.RemoveAt(_local.Items.Count - 1);
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_file)!);
            File.AppendAllText(_file, $"{e.Time:yyyy-MM-dd HH:mm:ss.fff}\t{e.Level}\t{e.Text}\r\n");
        }
        catch { }
    }

    private async Task Refresh()
    {
        var c = _link.Client;
        if (c == null) return;
        try
        {
            var logs = await c.Logs();
            _device.BeginUpdate();
            _device.Items.Clear();
            foreach (var l in logs.Arr("").Reverse())
            {
                var lvl = l.S("level");
                _device.Items.Add(new ListViewItem(new[] { l.S("ms"), lvl, l.S("message") })
                {
                    BackColor = Theme.Panel,
                    ForeColor = lvl == "ERROR" ? Theme.Bad : lvl == "WARN" ? Theme.Warn : Theme.Text,
                });
            }
            _device.EndUpdate();
        }
        catch { }
    }

    private async Task SaveDevice()
    {
        var c = _link.Client;
        if (c == null) return;
        using var d = new SaveFileDialog { FileName = $"vetogep_naplo_{DateTime.Now:yyyyMMdd_HHmm}.txt", Filter = "Szöveg|*.txt" };
        if (d.ShowDialog(this) != DialogResult.OK) return;
        try
        {
            var logs = await c.Logs();
            var lines = logs.Arr("").Select(l => $"{l.S("ms"),10}  {l.S("level"),-5}  {l.S("message")}");
            await File.WriteAllLinesAsync(d.FileName, lines);
        }
        catch (Exception e) { MessageBox.Show(this, e.Message, "Hiba"); }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _timer.Dispose();
        base.Dispose(disposing);
    }
}
