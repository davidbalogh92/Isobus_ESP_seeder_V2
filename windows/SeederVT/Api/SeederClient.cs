using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace SeederVT.Api;

/// <summary>
/// A vezérlő webes API-ja (ugyanaz, amit a telefonos felület használ):
///   GET  /api/state  – élő állapot (status, isobus, hw, fw)
///   GET  /api/schema – paraméterek leírása (csoportok, típus, tartomány, súgó, hatókör)
///   GET  /api/config – beállítások (lapos JSON), POST: import
///   GET  /api/crops  – leforgatási táblázat
///   POST /api/param  – {key, value}
///   POST /api/cmd    – {cmd, value}
///   GET  /api/logs   – eseménynapló
///   POST /api/ota, /api/restart, /api/factory
/// </summary>
public sealed class SeederClient : IDisposable
{
    private readonly HttpClient _http;
    public string BaseUrl { get; private set; }

    public SeederClient(string baseUrl)
    {
        BaseUrl = Normalize(baseUrl);
        var handler = new SocketsHttpHandler
        {
            PooledConnectionLifetime = TimeSpan.FromMinutes(2),
            ConnectTimeout = TimeSpan.FromSeconds(2),
            MaxConnectionsPerServer = 2,
        };
        _http = new HttpClient(handler) { Timeout = TimeSpan.FromSeconds(3) };
    }

    public static string Normalize(string url)
    {
        url = url.Trim();
        if (!url.StartsWith("http://", StringComparison.OrdinalIgnoreCase) &&
            !url.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
            url = "http://" + url;
        return url.TrimEnd('/');
    }

    public void Dispose() => _http.Dispose();

    private async Task<JsonNode?> GetJson(string path, CancellationToken ct = default)
    {
        using var resp = await _http.GetAsync(BaseUrl + path, ct).ConfigureAwait(false);
        resp.EnsureSuccessStatusCode();
        var text = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
        return JsonNode.Parse(text);
    }

    public sealed record Result(bool Ok, string? Error, JsonNode? Body);

    private async Task<Result> PostJson(string path, object body, TimeSpan? timeout = null)
    {
        try
        {
            using var content = new StringContent(JsonSerializer.Serialize(body), Encoding.UTF8, "application/json");
            using var cts = new CancellationTokenSource(timeout ?? TimeSpan.FromSeconds(3));
            using var resp = await _http.PostAsync(BaseUrl + path, content, cts.Token).ConfigureAwait(false);
            var text = await resp.Content.ReadAsStringAsync().ConfigureAwait(false);
            JsonNode? node = null;
            try { node = string.IsNullOrWhiteSpace(text) ? null : JsonNode.Parse(text); } catch { }
            var okFlag = node?["ok"]?.GetValue<bool>() ?? resp.IsSuccessStatusCode;
            if (!resp.IsSuccessStatusCode || !okFlag)
                return new Result(false, node?["error"]?.GetValue<string>() ?? $"HTTP {(int)resp.StatusCode}", node);
            return new Result(true, null, node);
        }
        catch (Exception e)
        {
            return new Result(false, "Kapcsolati hiba: " + e.Message, null);
        }
    }

    public Task<JsonNode?> State(CancellationToken ct = default) => GetJson("/api/state", ct);
    public Task<JsonNode?> Schema() => GetJson("/api/schema");
    public Task<JsonNode?> Config() => GetJson("/api/config");
    public Task<JsonNode?> Crops() => GetJson("/api/crops");
    public Task<JsonNode?> Logs() => GetJson("/api/logs");

    public Task<Result> Cmd(string cmd, double value = 0) => PostJson("/api/cmd", new { cmd, value });

    /// <summary>Virtuális terminál: a képernyő állapota.</summary>
    public Task<JsonNode?> Vt(CancellationToken ct = default) => GetJson("/api/vt", ct);

    /// <summary>Virtuális terminál esemény: kind = key | button | num; act: 0 elengedve, 1 lenyomva, 2 tartva, 3 megszakítva.</summary>
    public Task<Result> VtEvent(string kind, int id, int act, uint value = 0) =>
        PostJson("/api/vt/event", new { kind, id, act, value });

    /// <summary>A VT object pool (bináris).</summary>
    public async Task<byte[]> Pool()
    {
        using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
        return await client.GetByteArrayAsync(BaseUrl + "/api/pool").ConfigureAwait(false);
    }

    public Task<Result> Param(string key, double value) => PostJson("/api/param", new { key, value });
    public Task<Result> ParamStr(string key, string value) => PostJson("/api/param", new { key, value });
    public Task<Result> Restart() => PostJson("/api/restart", new { });
    public Task<Result> Factory() => PostJson("/api/factory", new { });

    public async Task<string> ConfigText()
    {
        return await _http.GetStringAsync(BaseUrl + "/api/config").ConfigureAwait(false);
    }

    public async Task<Result> ImportConfig(string json)
    {
        try
        {
            using var content = new StringContent(json, Encoding.UTF8, "application/json");
            using var resp = await _http.PostAsync(BaseUrl + "/api/config", content).ConfigureAwait(false);
            var text = await resp.Content.ReadAsStringAsync().ConfigureAwait(false);
            JsonNode? node = null;
            try { node = JsonNode.Parse(text); } catch { }
            if (!resp.IsSuccessStatusCode) return new Result(false, node?["error"]?.GetValue<string>() ?? text, node);
            return new Result(true, null, node);
        }
        catch (Exception e) { return new Result(false, e.Message, null); }
    }

    public async Task<Result> Ota(string file, IProgress<string>? progress = null)
    {
        try
        {
            var bytes = await File.ReadAllBytesAsync(file).ConfigureAwait(false);
            progress?.Report($"Feltöltés: {bytes.Length / 1024} kB…");
            using var client = new HttpClient { Timeout = TimeSpan.FromMinutes(4) };
            using var content = new ByteArrayContent(bytes);
            content.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            using var resp = await client.PostAsync(BaseUrl + "/api/ota", content).ConfigureAwait(false);
            var text = await resp.Content.ReadAsStringAsync().ConfigureAwait(false);
            JsonNode? node = null;
            try { node = JsonNode.Parse(text); } catch { }
            if (!resp.IsSuccessStatusCode) return new Result(false, node?["error"]?.GetValue<string>() ?? text, node);
            return new Result(true, node?["message"]?.GetValue<string>() ?? "Kész", node);
        }
        catch (Exception e) { return new Result(false, e.Message, null); }
    }
}

/// <summary>Kényelmi olvasók a JSON-ból (hiányzó mező = alapérték, sosem dob kivételt).</summary>
public static class J
{
    public static JsonNode? At(this JsonNode? n, string path)
    {
        if (string.IsNullOrEmpty(path)) return n;
        var cur = n;
        foreach (var part in path.Split('.'))
        {
            if (cur == null) return null;
            if (int.TryParse(part, out var idx) && cur is JsonArray arr)
                cur = idx >= 0 && idx < arr.Count ? arr[idx] : null;
            else if (cur is JsonObject obj)
                cur = obj.TryGetPropertyValue(part, out var v) ? v : null;
            else return null;
        }
        return cur;
    }

    public static double D(this JsonNode? n, string path, double def = 0)
    {
        var v = n.At(path);
        if (v is JsonValue jv)
        {
            if (jv.TryGetValue<double>(out var d)) return d;
            if (jv.TryGetValue<bool>(out var b)) return b ? 1 : 0;
            if (jv.TryGetValue<string>(out var s) &&
                double.TryParse(s, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var p))
                return p;
        }
        return def;
    }

    public static int I(this JsonNode? n, string path, int def = 0) => (int)Math.Round(n.D(path, def));

    public static bool B(this JsonNode? n, string path, bool def = false)
    {
        var v = n.At(path);
        if (v is JsonValue jv)
        {
            if (jv.TryGetValue<bool>(out var b)) return b;
            if (jv.TryGetValue<double>(out var d)) return d != 0;
        }
        return def;
    }

    public static string S(this JsonNode? n, string path, string def = "")
    {
        var v = n.At(path);
        if (v is JsonValue jv)
        {
            if (jv.TryGetValue<string>(out var s)) return s;
            return jv.ToJsonString();
        }
        return def;
    }

    public static IEnumerable<JsonNode?> Arr(this JsonNode? n, string path) =>
        n.At(path) is JsonArray a ? a : Enumerable.Empty<JsonNode?>();
}
