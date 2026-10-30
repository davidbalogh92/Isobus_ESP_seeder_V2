using System.Text.Json.Nodes;
using SeederVT.Api;

namespace SeederVT.Vt;

/// <summary>A vezérlő virtuális termináljának állapota (/api/vt): az objektumok aktuális értékei.</summary>
public sealed class VtScreen
{
    public uint Version;
    public string PoolVersion = "";
    public ushort Mask = 0xFFFF;
    public readonly Dictionary<ushort, ushort> SoftKeys = new();
    public readonly Dictionary<ushort, uint> Num = new();
    public readonly Dictionary<ushort, bool> Hidden = new();
    public readonly Dictionary<ushort, byte> Fill = new();
    public readonly Dictionary<ushort, string> Str = new();

    public static VtScreen From(JsonNode n)
    {
        var s = new VtScreen
        {
            Version = (uint)n.D("ver"),
            PoolVersion = n.S("pool"),
            Mask = (ushort)n.D("mask", 0xFFFF),
        };
        Map(n["sk"], (k, v) => s.SoftKeys[k] = (ushort)v.GetValue<double>());
        Map(n["num"], (k, v) => s.Num[k] = (uint)v.GetValue<double>());
        Map(n["hid"], (k, v) => s.Hidden[k] = v.GetValue<double>() != 0);
        Map(n["fill"], (k, v) => s.Fill[k] = (byte)v.GetValue<double>());
        Map(n["str"], (k, v) => s.Str[k] = Pool.FixHungarian(v.GetValue<string>()));
        return s;
    }

    private static void Map(JsonNode? obj, Action<ushort, JsonNode> set)
    {
        if (obj is not JsonObject o) return;
        foreach (var kv in o)
            if (kv.Value != null && ushort.TryParse(kv.Key, out var id))
            {
                try { set(id, kv.Value); } catch { }
            }
    }

    // --- értékfeloldás (ha a vezérlő még nem küldött értéket, a pool kezdőértéke érvényes)
    public uint Value(VtObject o) => Num.TryGetValue(o.Id, out var v) ? v : o.Value;
    public ushort Pointer(VtObject o) => Num.TryGetValue(o.Id, out var v) ? (ushort)v : o.Target;
    public bool IsHidden(VtObject o) => Hidden.TryGetValue(o.Id, out var h) ? h : o.Hidden;
    public byte FillColour(VtObject fillAttr) => Fill.TryGetValue(fillAttr.Id, out var c) ? c : fillAttr.Colour;
    public string Text(VtObject o) => Str.TryGetValue(o.Id, out var t) ? t : o.Text;
    public ushort SoftKeyMaskOf(VtObject mask) =>
        SoftKeys.TryGetValue(mask.Id, out var sk) ? sk : mask.SoftKeyMask;
}
