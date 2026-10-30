using System.Diagnostics;
using System.Text.Json.Nodes;
using SeederVT.Api;

namespace SeederVT.Core;

/// <summary>Egy helyi (a program által észlelt) esemény: kapcsolat, riasztás, visszajelzés.</summary>
public sealed record LocalEvent(DateTime Time, string Level, string Text);

/// <summary>
/// Kapcsolat a vezérlővel: háttérben kérdezi le az állapotot (alap 200 ms), az eredményt a
/// felhasználói szálon adja tovább. Figyeli a kapcsolat, a riasztások és a visszajelzések változását,
/// ezekből helyi eseménynaplót készít (hibakereséshez).
/// </summary>
public sealed class Link : IDisposable
{
    private SynchronizationContext _ui;
    private CancellationTokenSource? _cts;
    private SeederClient? _client;
    private uint _lastAlarms;
    private double _lastMsgSeq = -1;
    private bool _lastOk;

    public SeederClient? Client => _client;
    public JsonNode? State { get; private set; }
    public bool Connected { get; private set; }
    public double LatencyMs { get; private set; }
    public long Polls { get; private set; }
    public long Errors { get; private set; }
    public string LastError { get; private set; } = "";
    public DateTime LastOk { get; private set; }
    public int PollMs { get; set; } = 200;

    public event Action<JsonNode>? StateReceived;
    public event Action<bool>? ConnectionChanged;
    public event Action<LocalEvent>? Event;
    /// <summary>Új (nem nyugtázott) súlyos riasztás felfutó éle: (kód, szöveg).</summary>
    public event Action<string, string>? NewAlarm;

    public Link()
    {
        _ui = SynchronizationContext.Current ?? new SynchronizationContext();
    }

    public void Start(string host)
    {
        Stop();
        // a felhasználói szál (ablak) környezete: ide érkeznek az események
        _ui = SynchronizationContext.Current ?? _ui;
        _client = new SeederClient(host);
        _cts = new CancellationTokenSource();
        _lastAlarms = 0;
        _lastMsgSeq = -1;
        var token = _cts.Token;
        Post(() => Event?.Invoke(new LocalEvent(DateTime.Now, "INFO", $"Kapcsolódás: {_client.BaseUrl}")));
        Task.Run(() => Loop(_client, token));
    }

    public void Stop()
    {
        _cts?.Cancel();
        _cts = null;
        _client?.Dispose();
        _client = null;
        if (Connected)
        {
            Connected = false;
            ConnectionChanged?.Invoke(false);
        }
    }

    public void Dispose() => Stop();

    private void Post(Action a) => _ui.Post(_ => a(), null);

    private async Task Loop(SeederClient client, CancellationToken ct)
    {
        var sw = new Stopwatch();
        while (!ct.IsCancellationRequested)
        {
            sw.Restart();
            JsonNode? st = null;
            string err = "";
            try
            {
                st = await client.State(ct).ConfigureAwait(false);
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested) { return; }
            catch (Exception e) { err = e.Message; }
            var ms = sw.Elapsed.TotalMilliseconds;
            if (ct.IsCancellationRequested) return;
            var node = st;
            Post(() => Handle(node, err, ms));
            var wait = Math.Max(20, PollMs - (int)ms);
            try { await Task.Delay(node == null ? Math.Max(wait, 800) : wait, ct).ConfigureAwait(false); }
            catch (OperationCanceledException) { return; }
        }
    }

    private void Handle(JsonNode? st, string err, double ms)
    {
        Polls++;
        if (st == null)
        {
            Errors++;
            LastError = err;
            if (_lastOk)
            {
                Event?.Invoke(new LocalEvent(DateTime.Now, "ERROR", "Kapcsolat megszakadt: " + err));
                _lastOk = false;
            }
            if (Connected)
            {
                Connected = false;
                ConnectionChanged?.Invoke(false);
            }
            return;
        }
        LatencyMs = ms;
        LastOk = DateTime.Now;
        State = st;
        if (!_lastOk)
        {
            Event?.Invoke(new LocalEvent(DateTime.Now, "INFO",
                $"Kapcsolódva ({st.S("fw")}, {st.S("hw.board")})"));
            _lastOk = true;
        }
        if (!Connected)
        {
            Connected = true;
            ConnectionChanged?.Invoke(true);
        }
        Detect(st);
        StateReceived?.Invoke(st);
    }

    /// <summary>Riasztás- és visszajelzés-változások naplózása.</summary>
    private void Detect(JsonNode st)
    {
        var s = st["status"];
        var alarms = (uint)s.D("alarms");
        var rose = alarms & ~_lastAlarms;
        var fell = _lastAlarms & ~alarms;
        if (rose != 0 || fell != 0)
        {
            foreach (var a in s.Arr("alarm_list"))
            {
                var code = a.S("code");
                var bit = AlarmBit(s, code);
                if (bit >= 0 && ((rose >> bit) & 1) != 0)
                {
                    var sev = a.I("sev");
                    Event?.Invoke(new LocalEvent(DateTime.Now, sev == 2 ? "ALARM" : sev == 1 ? "WARN" : "INFO",
                        "Riasztás: " + a.S("text")));
                    if (sev == 2 && a.B("new")) NewAlarm?.Invoke(code, a.S("text"));
                }
            }
            if (fell != 0) Event?.Invoke(new LocalEvent(DateTime.Now, "INFO", $"Riasztás megszűnt (maszk 0x{fell:X})"));
        }
        _lastAlarms = alarms;
        var seq = s.D("message_seq");
        if (_lastMsgSeq >= 0 && seq != _lastMsgSeq && s.S("message").Length > 0)
            Event?.Invoke(new LocalEvent(DateTime.Now, "MSG", s.S("message")));
        _lastMsgSeq = seq;
    }

    // A riasztáskódok sorrendje a firmware AlarmId felsorolása szerint
    public static readonly string[] AlarmCodes =
    {
        "METER1_STOPPED", "METER2_STOPPED", "METER1_RATE", "METER2_RATE", "FAN_LOW", "FAN_HIGH",
        "HOPPER1_EMPTY", "HOPPER2_EMPTY", "NO_SPEED", "SPEED_FALLBACK", "OIL_HOT", "TEMP_SENSOR",
        "PRESSURE_SENSOR", "SCALE_SENSOR", "WHEEL_NOT_CAL", "SIMULATION", "TC_LOST", "SPEED_MISMATCH", "AOG_LOST",
    };

    private static int AlarmBit(JsonNode? s, string code) => Array.IndexOf(AlarmCodes, code);

    /// <summary>Parancs küldése; hiba esetén helyi eseményt is ír.</summary>
    public async Task<SeederClient.Result> Cmd(string cmd, double value = 0)
    {
        var c = _client;
        if (c == null) return new SeederClient.Result(false, "Nincs kapcsolat", null);
        var r = await c.Cmd(cmd, value);
        if (!r.Ok) Event?.Invoke(new LocalEvent(DateTime.Now, "WARN", $"{cmd}: {r.Error}"));
        return r;
    }
}
