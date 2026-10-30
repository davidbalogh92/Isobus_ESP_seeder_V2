using System.Text.Json.Nodes;
using SeederVT.Api;
using SeederVT.Core;

namespace SeederVT.Ui;

/// <summary>
/// „Diagnosztika” fül: a vezérlő teljes élő állapota (/api/state) mezőnként, a változó értékek kiemelve,
/// szűrhetően; a fontos összefoglaló felül; ISOBUS újracsatlakozás, nyugtázás.
/// </summary>
public sealed class DiagPage : UserControl
{
    private readonly Link _link;
    private readonly ListView _list = new() { Dock = DockStyle.Fill };
    private readonly TextBox _filter = new() { Width = 260, PlaceholderText = "Szűrés (pl. meter, aog, temp)" };
    private readonly Label _summary = new() { Dock = DockStyle.Top, AutoSize = false, Height = 92, Padding = new Padding(10) };
    private readonly Dictionary<string, (ListViewItem Item, string Value, DateTime Changed)> _rows = new();
    private bool _paused;

    public DiagPage(Link link)
    {
        _link = link;
        Dock = DockStyle.Fill;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        Theme.Style(_list);
        _list.Columns.Add("Mező", 360);
        _list.Columns.Add("Érték", 320);
        _list.Columns.Add("Változott", 140);
        _summary.BackColor = Theme.Panel;
        _summary.ForeColor = Theme.Text;

        var bar = Theme.Bar();
        bar.Controls.Add(_filter);
        var pause = Theme.Btn("Megállítás", (_, _) => { });
        pause.Click += (_, _) => { _paused = !_paused; pause.Text = _paused ? "Folytatás" : "Megállítás"; };
        bar.Controls.Add(pause);
        bar.Controls.Add(Theme.Btn("Riasztások nyugtázása", async (_, _) => await _link.Cmd("ack")));
        bar.Controls.Add(Theme.Btn("ISOBUS újracsatlakozás", async (_, _) =>
        {
            if (MessageBox.Show(this, "Újra bejelentkezés a buszra, a VT és a TC kapcsolat újrakezdése. A vetés nem áll le. Folytatod?",
                    "ISOBUS újracsatlakozás", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            {
                var r = await _link.Cmd("isobus_reconnect");
                MessageBox.Show(this, r.Ok ? "Újracsatlakozás kérve." : r.Error, "ISOBUS");
            }
        }));
        bar.Controls.Add(Theme.Btn("Másolás a vágólapra", (_, _) =>
        {
            if (_link.State != null) Clipboard.SetText(_link.State.ToJsonString(new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
        }));
        _filter.TextChanged += (_, _) => { _list.Items.Clear(); _rows.Clear(); if (_link.State != null) Render(_link.State); };

        Controls.Add(_list);
        Controls.Add(_summary);
        Controls.Add(bar);
        link.StateReceived += st => { if (Visible && !_paused) Render(st); };
    }

    private void Render(JsonNode st)
    {
        var s = st["status"];
        var iso = st["isobus"];
        var aog = st.At("hw.aog");
        string yn(bool v) => v ? "OK" : "nincs";
        _summary.Text =
            $"Vezérlő: {st.S("fw")} · {st.S("hw.board")} · üzemidő {s.D("uptime") / 60:F0} perc · válaszidő {_link.LatencyMs:F0} ms · " +
            $"lekérdezés {_link.Polls} (hiba {_link.Errors})\n" +
            $"ISOBUS: cím {(iso.B("claimed") ? "0x" + iso.I("address").ToString("X2") : "—")} · VT {yn(iso.B("vt_connected"))} · " +
            $"TC {yn(iso.B("tc_connected"))} · indulás: {(iso.B("started") ? "elindult" : iso.D("start_in") > 0 ? $"még {iso.D("start_in"):F0} s" : "nem fut")} · " +
            $"bejelentkezés/újracsatl. {iso.I("announces")}/{iso.I("reconnects")} · CAN {st.S("hw.can_state")}\n" +
            $"AgOpenGPS: {(aog == null ? "—" : !aog.B("enabled") ? "kikapcsolva" : aog.B("connected") ? "kapcsolódva" : "nincs forgalom")} · " +
            $"IP {aog.S("ip", "—")} · sebesség {s.S("speed_src")} {s.D("speed"):F1} km/h · kerék {s.D("wheel_kmh"):F1} · GPS " +
            $"{(s.D("ref_kmh", -1) >= 0 ? s.D("ref_kmh").ToString("F1") : "—")} · csúszás {(s.D("slip", -1) >= 0 ? s.D("slip").ToString("F1") + " %" : "—")}";

        var flat = new List<(string, string)>();
        Flatten(st, "", flat);
        var f = _filter.Text.Trim();
        var now = DateTime.Now;
        _list.BeginUpdate();
        foreach (var (path, value) in flat)
        {
            if (f.Length > 0 && !path.Contains(f, StringComparison.OrdinalIgnoreCase)) continue;
            if (!_rows.TryGetValue(path, out var row))
            {
                var it = new ListViewItem(new[] { path, value, "" }) { BackColor = Theme.Panel, ForeColor = Theme.Text };
                _list.Items.Add(it);
                _rows[path] = (it, value, DateTime.MinValue);
                continue;
            }
            if (row.Value != value)
            {
                row.Item.SubItems[1].Text = value;
                row.Item.SubItems[2].Text = now.ToString("HH:mm:ss.f");
                _rows[path] = (row.Item, value, now);
            }
            var age = (now - _rows[path].Changed).TotalSeconds;
            row.Item.BackColor = age < 1.5 ? Color.FromArgb(106, 84, 32) : Theme.Panel;
        }
        _list.EndUpdate();
    }

    public static void Flatten(JsonNode? n, string prefix, List<(string, string)> out_)
    {
        switch (n)
        {
            case JsonObject o:
                foreach (var kv in o) Flatten(kv.Value, prefix.Length == 0 ? kv.Key : prefix + "." + kv.Key, out_);
                break;
            case JsonArray a:
                if (a.All(x => x is JsonValue) && a.Count <= 8)
                {
                    out_.Add((prefix, string.Join(" | ", a.Select(x => x?.ToJsonString() ?? "null"))));
                    break;
                }
                for (int i = 0; i < a.Count; ++i) Flatten(a[i], prefix + "." + i, out_);
                break;
            case null:
                out_.Add((prefix, "null"));
                break;
            default:
                out_.Add((prefix, n.ToJsonString().Trim('"')));
                break;
        }
    }
}
