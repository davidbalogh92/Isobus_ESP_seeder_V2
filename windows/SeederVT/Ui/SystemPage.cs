using System.Text.Json.Nodes;
using SeederVT.Api;
using SeederVT.Core;

namespace SeederVT.Ui;

/// <summary>„Rendszer” fül: kapcsolat, program-beállítások, firmware (OTA), beállítások export / import, lábkiosztás.</summary>
public sealed class SystemPage : UserControl
{
    private readonly Link _link;
    private readonly AppSettings _settings;
    private readonly ComboBox _host = new() { Width = 220 };
    private readonly Label _info = Theme.Lbl("", true);
    private readonly Label _ota = Theme.Lbl("", true);
    private readonly ListView _pins = new() { Dock = DockStyle.Fill };
    private string _pinsBoard = "";
    public event Action<string>? ConnectRequested;

    public SystemPage(Link link, AppSettings settings)
    {
        _link = link;
        _settings = settings;
        Dock = DockStyle.Fill;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;

        _host.Items.AddRange(settings.Hosts.Cast<object>().ToArray());
        _host.Text = settings.LastHost;

        var conn = Theme.Bar();
        conn.Controls.Add(Theme.Lbl("Vezérlő címe:"));
        conn.Controls.Add(_host);
        conn.Controls.Add(Theme.Btn("Kapcsolódás", (_, _) => ConnectRequested?.Invoke(_host.Text.Trim()), Theme.Go));
        conn.Controls.Add(Theme.Lbl("Ethernet: 192.168.5.130 (AgIO hálózat) · Wi-Fi: VETOGEP-xxxx → 192.168.4.1", true));

        var opts = Theme.Bar();
        var sound = new CheckBox { Text = "Riasztás hang", Checked = settings.Sound, AutoSize = true, ForeColor = Theme.Text, Margin = new Padding(8) };
        sound.CheckedChanged += (_, _) => { settings.Sound = sound.Checked; settings.Save(); };
        var snap = new CheckBox { Text = "Riasztáskor pillanatkép (Felvétel mappa)", Checked = settings.AlarmSnapshots, AutoSize = true, ForeColor = Theme.Text, Margin = new Padding(8) };
        snap.CheckedChanged += (_, _) => { settings.AlarmSnapshots = snap.Checked; settings.Save(); };
        var poll = new NumericUpDown { Minimum = 100, Maximum = 2000, Increment = 50, Value = Math.Clamp(settings.PollMs, 100, 2000), Width = 80 };
        poll.ValueChanged += (_, _) => { settings.PollMs = (int)poll.Value; link.PollMs = settings.PollMs; settings.Save(); };
        opts.Controls.Add(sound);
        opts.Controls.Add(snap);
        opts.Controls.Add(Theme.Lbl("Lekérdezés (ms):"));
        opts.Controls.Add(poll);

        var fw = Theme.Bar();
        fw.Controls.Add(Theme.Btn("Firmware frissítés (OTA)…", async (_, _) => await Ota()));
        fw.Controls.Add(Theme.Btn("Beállítások exportja…", async (_, _) => await Export()));
        fw.Controls.Add(Theme.Btn("Beállítások importja…", async (_, _) => await Import()));
        fw.Controls.Add(Theme.Btn("Vezérlő újraindítása", async (_, _) =>
        {
            if (Ask("Újraindítod a vezérlőt?") && _link.Client != null) Show2(await _link.Client.Restart());
        }));
        fw.Controls.Add(Theme.Btn("Gyári beállítások", async (_, _) =>
        {
            if (Ask("Minden beállítás alapra áll (a számlálók megmaradnak). Folytatod?") && _link.Client != null)
                Show2(await _link.Client.Factory());
        }, Theme.Stop));
        fw.Controls.Add(_ota);

        var infoBar = Theme.Bar();
        infoBar.Controls.Add(_info);

        Theme.Style(_pins);
        _pins.Columns.Add("Láb / csatlakozó", 260);
        _pins.Columns.Add("Funkció", 360);
        _pins.Columns.Add("Bekötés, megjegyzés", 420);

        Controls.Add(_pins);
        Controls.Add(infoBar);
        Controls.Add(fw);
        Controls.Add(opts);
        Controls.Add(conn);
        link.StateReceived += st => { if (Visible) Render(st); };
    }

    private bool Ask(string q) =>
        MessageBox.Show(this, q, "Vetőgép", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes;

    private void Show2(SeederClient.Result r) =>
        MessageBox.Show(this, r.Ok ? "Kész." : r.Error, "Vetőgép");

    public void RememberHost(string host)
    {
        _settings.LastHost = host;
        if (!_settings.Hosts.Contains(host)) { _settings.Hosts.Insert(0, host); _host.Items.Insert(0, host); }
        _settings.Save();
    }

    private void Render(JsonNode st)
    {
        _info.Text = $"Firmware: {st.S("fw")} · Wi-Fi: {st.S("ssid")} · Panel: {st.S("hw.board")} · Szabad memória: " +
                     $"{st.D("hw.heap") / 1024:F0} kB (min {st.D("hw.heap_min") / 1024:F0} kB) · Object pool: {st.D("hw.pool"):F0} bájt";
        var board = st.S("hw.io.board");
        if (board == _pinsBoard) return;
        _pinsBoard = board;
        _pins.Items.Clear();
        foreach (var row in st.Arr("hw.io.pins"))
            if (row is JsonArray a && a.Count >= 3)
                _pins.Items.Add(new ListViewItem(new[] { a[0]?.GetValue<string>() ?? "", a[1]?.GetValue<string>() ?? "", a[2]?.GetValue<string>() ?? "" })
                { BackColor = Theme.Panel, ForeColor = Theme.Text });
    }

    private async Task Ota()
    {
        var c = _link.Client;
        if (c == null) return;
        using var d = new OpenFileDialog { Filter = "OTA firmware (*_OTA.bin)|*_OTA.bin|Minden .bin|*.bin" };
        if (d.ShowDialog(this) != DialogResult.OK) return;
        if (d.FileName.Contains("FULL", StringComparison.OrdinalIgnoreCase))
        {
            MessageBox.Show(this, "A FULL_0x0.bin csak USB-s első telepítéshez való. Az _OTA.bin fájlt válaszd.", "OTA");
            return;
        }
        if (!Ask($"Feltöltöd: {Path.GetFileName(d.FileName)}? Utána a vezérlő újraindul. A vetés legyen KI.")) return;
        _ota.Text = "Feltöltés… ne kapcsold ki a tápot.";
        var r = await c.Ota(d.FileName, new Progress<string>(s => _ota.Text = s));
        _ota.Text = r.Ok ? (r.Error ?? "Kész – újraindul") : "Hiba: " + r.Error;
    }

    private async Task Export()
    {
        var c = _link.Client;
        if (c == null) return;
        using var d = new SaveFileDialog { FileName = $"vetogep-beallitasok_{DateTime.Now:yyyyMMdd}.json", Filter = "JSON|*.json" };
        if (d.ShowDialog(this) != DialogResult.OK) return;
        try { await File.WriteAllTextAsync(d.FileName, await c.ConfigText()); }
        catch (Exception e) { MessageBox.Show(this, e.Message, "Hiba"); }
    }

    private async Task Import()
    {
        var c = _link.Client;
        if (c == null) return;
        using var d = new OpenFileDialog { Filter = "JSON|*.json" };
        if (d.ShowDialog(this) != DialogResult.OK) return;
        var r = await c.ImportConfig(await File.ReadAllTextAsync(d.FileName));
        MessageBox.Show(this, r.Ok ? $"Importálva: {r.Body.D("applied"):F0} beállítás" : r.Error, "Import");
    }
}
