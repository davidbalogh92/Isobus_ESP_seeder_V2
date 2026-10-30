using System.Media;
using SeederVT.Api;
using SeederVT.Core;
using SeederVT.Vt;

namespace SeederVT.Ui;

/// <summary>
/// A „Terminál” fül: a vezérlő virtuális termináljának kirajzolása (ugyanaz a felület, mint az ISOBUS
/// terminálon), a softkey-ek, gombok és beviteli mezők kezelése.
/// </summary>
public sealed class TerminalPage : UserControl
{
    private readonly Link _link;
    private readonly AppSettings _settings;
    private readonly VtView _view = new() { Dock = DockStyle.Fill };
    private readonly System.Windows.Forms.Timer _poll = new() { Interval = 150 };
    private bool _busy;
    private uint _lastVer;
    private string _poolVer = "";
    private bool _loadingPool;
    private DateTime _lastAlarmSound = DateTime.MinValue;

    public VtView View => _view;

    public TerminalPage(Link link, AppSettings settings)
    {
        _link = link;
        _settings = settings;
        Dock = DockStyle.Fill;
        BackColor = Theme.Bg;
        Controls.Add(_view);
        _view.SetOverlay("Nincs kapcsolat");
        _view.SoftKey += (id, act) => Send("key", id, act);
        _view.ButtonPressed += id =>
        {
            Send("button", id, 1);
            Send("button", id, 0);
        };
        _view.InputRequested += OnInput;
        _poll.Tick += async (_, _) => await Poll();
        _poll.Start();
    }

    private async void Send(string kind, ushort id, int act, uint value = 0)
    {
        var c = _link.Client;
        if (c == null) return;
        var r = await c.VtEvent(kind, id, act, value);
        if (!r.Ok && act != 2) _view.SetOverlay("Hiba: " + r.Error);
        _lastVer = 0; // azonnali frissítés
    }

    private void OnInput(VtObject o)
    {
        var sc = _view.Screen;
        if (sc == null) return;
        if (o.Type == VtType.InputNumber)
        {
            var v = NumberDialog.Edit(FindForm()!, o, sc.Value(o));
            if (v.HasValue) Send("num", o.Id, 0, v.Value);
        }
        else if (o.Type == VtType.InputList && _view.Pool != null)
        {
            var items = o.Items.Select(i => _view.Pool.Get(i)?.Text ?? "?").ToList();
            using var d = new ListDialog("Választás", items, (int)sc.Value(o));
            if (d.ShowDialog(FindForm()) == DialogResult.OK && d.Selected >= 0) Send("num", o.Id, 0, (uint)d.Selected);
        }
    }

    private async Task Poll()
    {
        if (_busy || !Visible) return;
        var c = _link.Client;
        if (c == null)
        {
            _view.SetOverlay("Nincs kapcsolat – Rendszer fül: vezérlő címe");
            return;
        }
        _busy = true;
        try
        {
            var n = await c.Vt();
            if (n == null) return;
            var sc = VtScreen.From(n);
            if (sc.PoolVersion != _poolVer && !_loadingPool) await LoadPool(c, sc.PoolVersion);
            if (sc.Version != _lastVer || _view.Screen == null)
            {
                _lastVer = sc.Version;
                _view.SetScreen(sc);
            }
            _view.SetOverlay(_view.Pool == null ? "Kezelőfelület betöltése…" : "");
            if (_view.AlarmShown && _settings.Sound && DateTime.Now - _lastAlarmSound > TimeSpan.FromSeconds(2))
            {
                _lastAlarmSound = DateTime.Now;
                SystemSounds.Exclamation.Play();
            }
        }
        catch (Exception e)
        {
            _view.SetOverlay("Nincs kapcsolat: " + Short(e.Message));
        }
        finally { _busy = false; }
    }

    private static string Short(string s) => s.Length > 60 ? s[..60] + "…" : s;

    /// <summary>A pool a vezérlőről jön (a firmware-rel mindig egyező), verziónként gyorsítótárazva.</summary>
    private async Task LoadPool(SeederClient c, string ver)
    {
        _loadingPool = true;
        try
        {
            var file = Path.Combine(AppSettings.Dir, $"pool_{ver}.iop");
            byte[] data;
            if (ver.Length > 0 && File.Exists(file)) data = await File.ReadAllBytesAsync(file);
            else
            {
                _view.SetOverlay("Kezelőfelület letöltése…");
                data = await c.Pool();
                try
                {
                    Directory.CreateDirectory(AppSettings.Dir);
                    if (ver.Length > 0) await File.WriteAllBytesAsync(file, data);
                }
                catch { }
            }
            _view.SetPool(Pool.Parse(data));
            _poolVer = ver;
        }
        catch (Exception e)
        {
            _view.SetOverlay("A kezelőfelület nem tölthető be: " + Short(e.Message));
        }
        finally { _loadingPool = false; }
    }

    /// <summary>F1–F5: jobb oszlop, F6–F10: bal oszlop.</summary>
    public bool HandleKey(Keys key, bool down)
    {
        if (key >= Keys.F1 && key <= Keys.F10) return _view.KeyByIndex(key - Keys.F1, down);
        return false;
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _poll.Dispose();
        base.Dispose(disposing);
    }
}
