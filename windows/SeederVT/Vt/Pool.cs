using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

namespace SeederVT.Vt;

/// <summary>ISO 11783-6 objektumtípusok, amelyeket a vezérlő poolja használ.</summary>
public enum VtType : byte
{
    WorkingSet = 0, DataMask = 1, AlarmMask = 2, Container = 3, SoftKeyMask = 4, Key = 5, Button = 6,
    InputNumber = 9, InputList = 10, OutputString = 11, OutputNumber = 12, Line = 13, Rectangle = 14,
    Meter = 17, BarGraph = 18, Picture = 20, NumberVariable = 21, StringVariable = 22, FontAttr = 23,
    LineAttr = 24, FillAttr = 25, InputAttr = 26, ObjectPointer = 27,
}

public readonly record struct Child(ushort Id, short X, short Y);

/// <summary>Egy pool-objektum: a vezérlő poolja csak az alábbi mezőket használja.</summary>
public sealed class VtObject
{
    public ushort Id;
    public VtType Type;
    public byte Bg = 7;
    public ushort SoftKeyMask = 0xFFFF;
    public ushort ActiveMask = 0xFFFF;
    public int W, H;
    public bool Hidden;
    public byte KeyCode;
    public byte Border;
    public ushort Font = 0xFFFF;
    public bool Transparent;
    public byte Align;
    public string Text = "";
    public uint Value;
    public uint Min, Max;
    public int Offset;
    public float Scale = 1;
    public byte Decimals;
    public bool Enabled = true;
    public List<ushort> Items = new();
    public List<ushort> Keys = new();
    public List<Child> Kids = new();
    public ushort LineAttr = 0xFFFF, FillAttr = 0xFFFF;
    public byte Suppress;
    public byte Colour;
    public byte FontSize;
    public byte LineWidth = 1;
    public byte FillType;
    public ushort Target = 0xFFFF;
    // kép
    public byte[]? Pixels;
    public Bitmap? Bitmap;
}

/// <summary>A vezérlő object poolja (bináris) beolvasva; a képek RGB bitmapekké alakítva.</summary>
public sealed class Pool
{
    public readonly Dictionary<ushort, VtObject> Objects = new();
    public ushort WorkingSet = 0xFFFF;

    public VtObject? Get(ushort id) => id != 0xFFFF && Objects.TryGetValue(id, out var o) ? o : null;

    private static readonly Encoding Latin1 = Encoding.Latin1;

    /// <summary>A VT Latin-1 szövegei: a magyar ő/ű helyett a pool õ/û-t tárol – Windowson az igazit rajzoljuk.</summary>
    public static string FixHungarian(string s) =>
        s.Replace('õ', 'ő').Replace('Õ', 'Ő').Replace('û', 'ű').Replace('Û', 'Ű');

    public static Pool Parse(byte[] d)
    {
        var p = new Pool();
        int i = 0;
        ushort U16() { var v = (ushort)(d[i] | d[i + 1] << 8); i += 2; return v; }
        short S16() { var v = (short)(d[i] | d[i + 1] << 8); i += 2; return v; }
        uint U32() { var v = (uint)(d[i] | d[i + 1] << 8 | d[i + 2] << 16 | d[i + 3] << 24); i += 4; return v; }
        int S32() => (int)U32();
        float F32() { var v = BitConverter.ToSingle(d, i); i += 4; return v; }
        byte U8() => d[i++];
        List<Child> Kids(int n)
        {
            var l = new List<Child>(n);
            for (int k = 0; k < n; ++k) l.Add(new Child(U16(), S16(), S16()));
            return l;
        }
        void SkipMacros(int n) => i += 2 * n;

        while (i + 3 <= d.Length)
        {
            var o = new VtObject { Id = U16(), Type = (VtType)U8() };
            switch (o.Type)
            {
                case VtType.WorkingSet:
                {
                    o.Bg = U8(); U8(); o.ActiveMask = U16();
                    int nk = U8(), nm = U8(), nl = U8();
                    o.Kids = Kids(nk); SkipMacros(nm); i += 2 * nl;
                    p.WorkingSet = o.Id;
                    break;
                }
                case VtType.DataMask:
                {
                    o.Bg = U8(); o.SoftKeyMask = U16();
                    int nk = U8(), nm = U8();
                    o.Kids = Kids(nk); SkipMacros(nm);
                    break;
                }
                case VtType.AlarmMask:
                {
                    o.Bg = U8(); o.SoftKeyMask = U16(); U8(); U8();
                    int nk = U8(), nm = U8();
                    o.Kids = Kids(nk); SkipMacros(nm);
                    break;
                }
                case VtType.Container:
                {
                    o.W = U16(); o.H = U16(); o.Hidden = U8() != 0;
                    int nk = U8(), nm = U8();
                    o.Kids = Kids(nk); SkipMacros(nm);
                    break;
                }
                case VtType.SoftKeyMask:
                {
                    o.Bg = U8();
                    int nk = U8(), nm = U8();
                    for (int k = 0; k < nk; ++k) o.Keys.Add(U16());
                    SkipMacros(nm);
                    break;
                }
                case VtType.Key:
                {
                    o.Bg = U8(); o.KeyCode = U8();
                    int nk = U8(), nm = U8();
                    o.Kids = Kids(nk); SkipMacros(nm);
                    break;
                }
                case VtType.Button:
                {
                    o.W = U16(); o.H = U16(); o.Bg = U8(); o.Border = U8(); o.KeyCode = U8(); U8();
                    int nk = U8(), nm = U8();
                    o.Kids = Kids(nk); SkipMacros(nm);
                    break;
                }
                case VtType.InputNumber:
                {
                    o.W = U16(); o.H = U16(); o.Bg = U8(); o.Font = U16(); U8(); U16();
                    o.Value = U32(); o.Min = U32(); o.Max = U32(); o.Offset = S32(); o.Scale = F32();
                    o.Decimals = U8(); U8(); o.Align = U8(); o.Enabled = (U8() & 1) != 0;
                    SkipMacros(U8());
                    break;
                }
                case VtType.InputList:
                {
                    o.W = U16(); o.H = U16(); U16(); o.Value = U8();
                    int n = U8(); o.Enabled = (U8() & 1) != 0; int nm = U8();
                    for (int k = 0; k < n; ++k) o.Items.Add(U16());
                    SkipMacros(nm);
                    break;
                }
                case VtType.OutputString:
                {
                    o.W = U16(); o.H = U16(); o.Bg = U8(); o.Font = U16(); o.Transparent = (U8() & 1) != 0; U16();
                    o.Align = U8(); int len = U16();
                    o.Text = FixHungarian(Latin1.GetString(d, i, len)); i += len;
                    SkipMacros(U8());
                    break;
                }
                case VtType.OutputNumber:
                {
                    o.W = U16(); o.H = U16(); o.Bg = U8(); o.Font = U16(); o.Transparent = (U8() & 1) != 0; U16();
                    o.Value = U32(); o.Offset = S32(); o.Scale = F32(); o.Decimals = U8(); U8(); o.Align = U8();
                    SkipMacros(U8());
                    break;
                }
                case VtType.Rectangle:
                {
                    o.LineAttr = U16(); o.W = U16(); o.H = U16(); o.Suppress = U8(); o.FillAttr = U16();
                    SkipMacros(U8());
                    break;
                }
                case VtType.Picture:
                {
                    o.W = U16(); U16(); o.H = U16(); var format = U8(); var opts = U8(); U8();
                    int len = (int)U32();
                    int nm = U8();   // a makrók száma a nyers adatok ELŐTT áll
                    var raw = new byte[len];
                    Array.Copy(d, i, raw, 0, len); i += len;
                    SkipMacros(nm);
                    if (format != 2) throw new InvalidDataException($"Kép {o.Id}: csak 8 bites kép támogatott");
                    o.Pixels = (opts & 0x04) != 0 ? Rle8(raw, o.W * o.H) : raw;
                    break;
                }
                case VtType.FontAttr:
                {
                    o.Colour = U8(); o.FontSize = U8(); U8(); U8();
                    SkipMacros(U8());
                    break;
                }
                case VtType.LineAttr:
                {
                    o.Colour = U8(); o.LineWidth = U8(); U16();
                    SkipMacros(U8());
                    break;
                }
                case VtType.FillAttr:
                {
                    o.FillType = U8(); o.Colour = U8(); U16();
                    SkipMacros(U8());
                    break;
                }
                case VtType.ObjectPointer:
                    o.Target = U16();
                    break;
                default:
                    throw new InvalidDataException($"Ismeretlen objektumtípus {(int)o.Type} (ID {o.Id}) – frissítsd a programot");
            }
            p.Objects[o.Id] = o;
        }
        return p;
    }

    private static byte[] Rle8(byte[] raw, int n)
    {
        var o = new byte[n];
        int k = 0;
        for (int j = 0; j + 1 < raw.Length && k < n; j += 2)
        {
            int cnt = raw[j];
            byte v = raw[j + 1];
            for (int c = 0; c < cnt && k < n; ++c) o[k++] = v;
        }
        return o;
    }

    public Bitmap? Picture(VtObject o)
    {
        if (o.Bitmap != null || o.Pixels == null || o.W <= 0 || o.H <= 0) return o.Bitmap;
        var bmp = new Bitmap(o.W, o.H, PixelFormat.Format32bppArgb);
        var data = bmp.LockBits(new Rectangle(0, 0, o.W, o.H), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        var buf = new int[o.W * o.H];
        for (int k = 0; k < buf.Length && k < o.Pixels.Length; ++k) buf[k] = Palette.Argb[o.Pixels[k]];
        for (int y = 0; y < o.H; ++y)
            Marshal.Copy(buf, y * o.W, data.Scan0 + y * data.Stride, o.W);
        bmp.UnlockBits(data);
        o.Bitmap = bmp;
        return bmp;
    }
}

/// <summary>A VT 256 színű palettája (ISO 11783-6 B. melléklet).</summary>
public static class Palette
{
    public static readonly int[] Argb = Build();
    public static readonly Color[] Colors = Argb.Select(Color.FromArgb).ToArray();

    private static int[] Build()
    {
        var basic = new (int r, int g, int b)[]
        {
            (0, 0, 0), (255, 255, 255), (0, 153, 0), (0, 153, 153), (153, 0, 0), (153, 0, 153), (153, 153, 0),
            (204, 204, 204), (153, 153, 153), (0, 0, 255), (0, 255, 0), (0, 255, 255), (255, 0, 0), (255, 0, 255),
            (255, 255, 0), (0, 0, 153),
        };
        var lv = new[] { 0, 51, 102, 153, 204, 255 };
        var list = new List<int>();
        foreach (var c in basic) list.Add(unchecked((int)0xFF000000) | c.r << 16 | c.g << 8 | c.b);
        foreach (var r in lv) foreach (var g in lv) foreach (var b in lv)
            list.Add(unchecked((int)0xFF000000) | r << 16 | g << 8 | b);
        while (list.Count < 256) list.Add(unchecked((int)0xFF000000));
        return list.ToArray();
    }

    public static Color Get(int i) => i >= 0 && i < 256 ? Colors[i] : Color.Black;
}

/// <summary>VT fontméret-kódok → pixel (szélesség, magasság).</summary>
public static class VtFonts
{
    public static (int W, int H) Px(byte code) => code switch
    {
        0 => (6, 8), 1 => (8, 8), 2 => (8, 12), 3 => (12, 16), 4 => (16, 16), 5 => (16, 24), 6 => (24, 32),
        7 => (32, 32), 8 => (32, 48), 9 => (48, 64), 10 => (64, 64), 11 => (64, 96), 12 => (96, 128),
        13 => (128, 128), 14 => (128, 192), _ => (12, 16),
    };
}
