using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Globalization;

namespace SeederVT.Vt;

/// <summary>
/// A VT kezelőfelület kirajzolása a vezérlő poolja és a virtuális képernyő állapota alapján – ugyanaz,
/// mint a terminálon (Müller BASIC elrendezés: 400×400 maszk, jobb oldalt az első 5, bal oldalt a
/// következő 5 softkey). Az egész az ablak méretéhez igazodik.
/// </summary>
public sealed class VtView : Control
{
    public const int Mask = 400;
    public const int KeyW = 120, KeyH = 80, KeyDesign = 80, Gap = 6;
    public const int DesignW = KeyW + Gap + Mask + Gap + KeyW, DesignH = 5 * KeyH;

    private Pool? _pool;
    private VtScreen? _screen;
    private float _scale = 1;
    private PointF _origin;
    private readonly List<(RectangleF R, ushort Id, int Kind)> _hits = new(); // Kind: 0 softkey, 1 gomb, 2 bevitel
    private ushort _pressedKey = 0xFFFF;
    private readonly Dictionary<(int, bool), Font> _fonts = new();
    private string _overlay = "";
    private readonly System.Drawing.Imaging.ImageAttributes _tile = MakeTile();
    private readonly Dictionary<(ushort, int, int), Bitmap> _scaledPics = new();

    private static System.Drawing.Imaging.ImageAttributes MakeTile()
    {
        var a = new System.Drawing.Imaging.ImageAttributes();
        a.SetWrapMode(WrapMode.TileFlipXY);
        return a;
    }

    /// <summary>Softkey esemény: (objektum, aktiválás 0 elengedve / 1 lenyomva / 2 tartva / 3 megszakítva).</summary>
    public event Action<ushort, int>? SoftKey;
    /// <summary>Gomb (Button) megnyomva.</summary>
    public event Action<ushort>? ButtonPressed;
    /// <summary>Beviteli mező (Input Number / Input List) érintve.</summary>
    public event Action<VtObject>? InputRequested;

    public Pool? Pool => _pool;
    public VtScreen? Screen => _screen;

    public VtView()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
                 ControlStyles.UserPaint, true);
        BackColor = Color.FromArgb(58, 61, 66);
        TabStop = true;
    }

    public void SetPool(Pool? pool)
    {
        _pool = pool;
        ClearScaled();
        Invalidate();
    }

    public void SetScreen(VtScreen? screen)
    {
        _screen = screen;
        Invalidate();
    }

    /// <summary>Szöveg a kép közepén (pl. „Nincs kapcsolat”).</summary>
    public void SetOverlay(string text)
    {
        if (_overlay == text) return;
        _overlay = text;
        Invalidate();
    }

    public bool AlarmShown => _pool?.Get(_screen?.Mask ?? 0xFFFF)?.Type == VtType.AlarmMask;

    // ------------------------------------------------------------------ rajzolás
    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(BackColor);
        _hits.Clear();
        _scale = Math.Min(ClientSize.Width / (float)DesignW, ClientSize.Height / (float)DesignH);
        if (_scale <= 0.05f) return;
        _origin = new PointF((ClientSize.Width - DesignW * _scale) / 2, (ClientSize.Height - DesignH * _scale) / 2);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;

        var state = g.Save();
        g.TranslateTransform(_origin.X, _origin.Y);
        g.ScaleTransform(_scale, _scale);
        try
        {
            if (_pool != null && _screen != null) DrawScreen(g);
            else DrawEmpty(g);
        }
        catch (Exception ex)
        {
            using var f = new Font("Segoe UI", 12, GraphicsUnit.Pixel);
            g.DrawString("Rajzolási hiba: " + ex.Message, f, Brushes.Red, 10, 10);
        }
        g.Restore(state);

        if (_overlay.Length > 0)
        {
            using var f = new Font("Segoe UI Semibold", Math.Max(10, 22 * _scale), GraphicsUnit.Pixel);
            var sz = g.MeasureString(_overlay, f);
            var r = new RectangleF((ClientSize.Width - sz.Width) / 2 - 16, (ClientSize.Height - sz.Height) / 2 - 10,
                                   sz.Width + 32, sz.Height + 20);
            using var b = new SolidBrush(Color.FromArgb(220, 120, 30, 30));
            g.FillRectangle(b, r);
            g.DrawString(_overlay, f, Brushes.White, r.X + 16, r.Y + 10);
        }
    }

    private void DrawEmpty(Graphics g)
    {
        using var b = new SolidBrush(Palette.Get(7));
        g.FillRectangle(b, KeyW + Gap, 0, Mask, Mask);
    }

    private void DrawScreen(Graphics g)
    {
        var pool = _pool!;
        var sc = _screen!;
        var mask = pool.Get(sc.Mask) ?? pool.Get(pool.Get(pool.WorkingSet)?.ActiveMask ?? 0xFFFF);
        var mx = KeyW + Gap;
        if (mask != null)
        {
            using (var bg = new SolidBrush(Palette.Get(mask.Bg)))
                g.FillRectangle(bg, mx, 0, Mask, Mask);
            var clip = g.Clip;
            g.SetClip(new RectangleF(mx, 0, Mask, Mask));
            foreach (var c in mask.Kids) Draw(g, c.Id, mx + c.X, c.Y, true);
            g.Clip = clip;
            // softkey-ek: BASIC elrendezés
            var sk = pool.Get(sc.SoftKeyMaskOf(mask));
            for (int i = 0; i < 10; ++i)
            {
                var kx = i < 5 ? mx + Mask + Gap : 0;
                var ky = (i % 5) * KeyH;
                var key = sk != null && i < sk.Keys.Count ? pool.Get(ResolveKey(sk.Keys[i])) : null;
                DrawKey(g, key, kx, ky);
            }
        }
    }

    private ushort ResolveKey(ushort id)
    {
        var o = _pool!.Get(id);
        return o?.Type == VtType.ObjectPointer ? _screen!.Pointer(o) : id;
    }

    private void DrawKey(Graphics g, VtObject? key, float x, float y)
    {
        var r = new RectangleF(x + 1, y + 2, KeyW - 2, KeyH - 4);
        var pressed = key != null && key.Id == _pressedKey;
        using (var bg = new SolidBrush(key == null ? Color.FromArgb(150, 153, 157)
                                                    : pressed ? Color.FromArgb(160, 175, 190) : Palette.Get(key.Bg)))
            g.FillRectangle(bg, r);
        using (var pen = new Pen(Color.FromArgb(30, 30, 30), pressed ? 3 : 1.5f))
            g.DrawRectangle(pen, r.X, r.Y, r.Width, r.Height);
        if (key == null) return;
        var k = KeyW / (float)KeyDesign;   // a pozíciók a 80 px-es tervezési gombhoz képest
        foreach (var c in key.Kids) Draw(g, c.Id, x + c.X * k, y + c.Y * k, false);
        _hits.Add((Map(r), key.Id, 0));
    }

    private RectangleF Map(RectangleF r) =>
        new(_origin.X + r.X * _scale, _origin.Y + r.Y * _scale, r.Width * _scale, r.Height * _scale);

    private void Draw(Graphics g, ushort id, float x, float y, bool interactive)
    {
        var o = _pool!.Get(id);
        if (o == null) return;
        var sc = _screen!;
        switch (o.Type)
        {
            case VtType.ObjectPointer:
                Draw(g, sc.Pointer(o), x, y, interactive);
                break;
            case VtType.Container:
                if (sc.IsHidden(o)) return;
                foreach (var c in o.Kids) Draw(g, c.Id, x + c.X, y + c.Y, interactive);
                break;
            case VtType.Button:
            {
                var r = new RectangleF(x, y, o.W, o.H);
                using (var bg = new SolidBrush(Palette.Get(o.Bg))) g.FillRectangle(bg, r);
                using (var hi = new Pen(Color.FromArgb(240, 240, 240), 2)) g.DrawLine(hi, x + 1, y + 1, x + o.W - 2, y + 1);
                using (var pen = new Pen(Palette.Get(o.Border), 2)) g.DrawRectangle(pen, x, y, o.W, o.H);
                foreach (var c in o.Kids) Draw(g, c.Id, x + c.X, y + c.Y, interactive);
                if (interactive) _hits.Add((Map(r), o.Id, 1));
                break;
            }
            case VtType.Rectangle:
                DrawRect(g, o, x, y);
                break;
            case VtType.OutputString:
            {
                if (!o.Transparent)
                    using (var bg = new SolidBrush(Palette.Get(o.Bg))) g.FillRectangle(bg, x, y, o.W, o.H);
                DrawText(g, sc.Text(o), o.Font, new RectangleF(x, y, o.W, o.H), o.Align);
                break;
            }
            case VtType.OutputNumber:
            {
                if (!o.Transparent)
                    using (var bg = new SolidBrush(Palette.Get(o.Bg))) g.FillRectangle(bg, x, y, o.W, o.H);
                DrawText(g, FormatNumber(o, sc.Value(o)), o.Font, new RectangleF(x, y, o.W, o.H), o.Align);
                break;
            }
            case VtType.InputNumber:
            {
                var r = new RectangleF(x, y, o.W, o.H);
                using (var bg = new SolidBrush(Palette.Get(o.Bg))) g.FillRectangle(bg, r);
                using (var pen = new Pen(Palette.Get(8), 1.5f)) g.DrawRectangle(pen, x, y, o.W, o.H);
                DrawText(g, FormatNumber(o, sc.Value(o)), o.Font, new RectangleF(x + 4, y, o.W - 8, o.H),
                         (byte)(2 | 4));
                if (interactive && o.Enabled) _hits.Add((Map(r), o.Id, 2));
                break;
            }
            case VtType.InputList:
            {
                var r = new RectangleF(x, y, o.W, o.H);
                using (var bg = new SolidBrush(Palette.Get(1))) g.FillRectangle(bg, r);
                using (var pen = new Pen(Palette.Get(8), 1.5f)) g.DrawRectangle(pen, x, y, o.W, o.H);
                var idx = (int)sc.Value(o);
                if (idx >= 0 && idx < o.Items.Count)
                {
                    var item = _pool.Get(o.Items[idx]);
                    if (item != null) Draw(g, item.Id, x + 4, y + Math.Max(0, (o.H - Math.Max(item.H, 16)) / 2f), false);
                }
                var ax = x + o.W - 16;
                var ay = y + o.H / 2f - 3;
                g.FillPolygon(Brushes.Black, new[] { new PointF(ax, ay), new PointF(ax + 11, ay), new PointF(ax + 5.5f, ay + 7) });
                if (interactive && o.Enabled) _hits.Add((Map(r), o.Id, 2));
                break;
            }
            case VtType.Picture:
            {
                var bmp = _pool.Picture(o);
                if (bmp != null) DrawPicture(g, o, bmp, x, y);
                break;
            }
        }
    }

    /// <summary>
    /// Kép kirajzolása halvány keret nélkül: a képet egyszer, egész képpontos méretre átméretezzük
    /// (SourceCopy, a szélek tükrözve), és egész képpontos helyre, további méretezés nélkül tesszük ki.
    /// </summary>
    private void DrawPicture(Graphics g, VtObject o, Bitmap bmp, float x, float y)
    {
        var pts = new[] { new PointF(x, y), new PointF(x + o.W, y + o.H) };
        using (var m = g.Transform) m.TransformPoints(pts);
        int dx = (int)Math.Round(pts[0].X), dy = (int)Math.Round(pts[0].Y);
        int dw = (int)Math.Round(pts[1].X) - dx, dh = (int)Math.Round(pts[1].Y) - dy;
        if (dw <= 0 || dh <= 0) return;
        if (!_scaledPics.TryGetValue((o.Id, dw, dh), out var sb))
        {
            sb = new Bitmap(dw, dh, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            using (var sg = Graphics.FromImage(sb))
            {
                sg.CompositingMode = CompositingMode.SourceCopy;
                sg.InterpolationMode = dw == o.W && dh == o.H ? InterpolationMode.NearestNeighbor : InterpolationMode.HighQualityBicubic;
                sg.PixelOffsetMode = PixelOffsetMode.Half;
                sg.DrawImage(bmp, new Rectangle(0, 0, dw, dh), 0, 0, o.W, o.H, GraphicsUnit.Pixel, _tile);
            }
            if (_scaledPics.Count > 600) ClearScaled();   // ablakméret-váltáskor ne nőjön a végtelenségig
            _scaledPics[(o.Id, dw, dh)] = sb;
        }
        var st = g.Save();
        g.ResetTransform();
        g.InterpolationMode = InterpolationMode.NearestNeighbor;
        g.PixelOffsetMode = PixelOffsetMode.Half;
        g.DrawImage(sb, new Rectangle(dx, dy, dw, dh), 0, 0, dw, dh, GraphicsUnit.Pixel);
        g.Restore(st);
    }

    private void ClearScaled()
    {
        foreach (var b in _scaledPics.Values) b.Dispose();
        _scaledPics.Clear();
    }

    private void DrawRect(Graphics g, VtObject o, float x, float y)
    {
        var fill = _pool!.Get(o.FillAttr);
        if (fill != null && fill.FillType != 0)
            using (var b = new SolidBrush(Palette.Get(_screen!.FillColour(fill)))) g.FillRectangle(b, x, y, o.W, o.H);
        var line = _pool.Get(o.LineAttr);
        if (line == null || line.LineWidth == 0) return;
        using var pen = new Pen(Palette.Get(line.Colour), line.LineWidth);
        float x2 = x + o.W - 1, y2 = y + o.H - 1;
        if ((o.Suppress & 1) == 0) g.DrawLine(pen, x, y, x2, y);
        if ((o.Suppress & 2) == 0) g.DrawLine(pen, x2, y, x2, y2);
        if ((o.Suppress & 4) == 0) g.DrawLine(pen, x, y2, x2, y2);
        if ((o.Suppress & 8) == 0) g.DrawLine(pen, x, y, x, y2);
    }

    public static string FormatNumber(VtObject o, uint raw)
    {
        // a VT a nyers értéket előjel nélküliként tárolja; az eltolással együtt lesz negatív
        var v = ((double)raw + o.Offset) * o.Scale;
        return v.ToString("F" + o.Decimals, CultureInfo.InvariantCulture);
    }

    private Font FontFor(int px, bool bold)
    {
        if (!_fonts.TryGetValue((px, bold), out var f))
        {
            f = new Font("Consolas", px, bold ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Pixel);
            _fonts[(px, bold)] = f;
        }
        return f;
    }

    private void DrawText(Graphics g, string text, ushort fontId, RectangleF r, byte align)
    {
        var fa = _pool!.Get(fontId);
        var (fw, fh) = VtFonts.Px(fa?.FontSize ?? 3);
        // a VT fix szélességű fontja: a magasság és a karakterszélesség közül a szűkebb dönt
        var px = (int)Math.Max(6, Math.Min(fh * 0.92, fw / 0.55));
        var font = FontFor(px, true);
        using var brush = new SolidBrush(Palette.Get(fa?.Colour ?? 0));
        using var sf = new StringFormat(StringFormatFlags.NoWrap | StringFormatFlags.NoClip)
        {
            Alignment = (align & 3) switch { 1 => StringAlignment.Center, 2 => StringAlignment.Far, _ => StringAlignment.Near },
            LineAlignment = ((align >> 2) & 3) switch { 1 => StringAlignment.Center, 2 => StringAlignment.Far, _ => StringAlignment.Near },
            Trimming = StringTrimming.None,
        };
        // felül igazításnál a VT a font cellájának tetejétől rajzol
        var rr = ((align >> 2) & 3) == 0 ? new RectangleF(r.X, r.Y + Math.Max(0, (fh - px) / 2f), r.Width, r.Height) : r;
        g.DrawString(text, font, brush, rr, sf);
    }

    // ------------------------------------------------------------------ érintés / egér
    private (ushort Id, int Kind)? HitAt(Point p)
    {
        for (int i = _hits.Count - 1; i >= 0; --i)
            if (_hits[i].R.Contains(p)) return (_hits[i].Id, _hits[i].Kind);
        return null;
    }

    private readonly System.Windows.Forms.Timer _holdTimer = new() { Interval = 200 };
    private bool _holdWired;

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        Focus();
        if (e.Button != MouseButtons.Left) return;
        var h = HitAt(e.Location);
        if (h == null) return;
        switch (h.Value.Kind)
        {
            case 0:
                PressKey(h.Value.Id);
                break;
            case 1:
                ButtonPressed?.Invoke(h.Value.Id);
                break;
            case 2:
                var o = _pool?.Get(h.Value.Id);
                if (o != null) InputRequested?.Invoke(o);
                break;
        }
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        ReleaseKey(false);
    }

    protected override void OnMouseLeave(EventArgs e)
    {
        base.OnMouseLeave(e);
        ReleaseKey(true);
    }

    private void PressKey(ushort id)
    {
        if (!_holdWired)
        {
            _holdTimer.Tick += (_, _) => { if (_pressedKey != 0xFFFF) SoftKey?.Invoke(_pressedKey, 2); };
            _holdWired = true;
        }
        _pressedKey = id;
        SoftKey?.Invoke(id, 1);
        _holdTimer.Start();
        Invalidate();
    }

    private void ReleaseKey(bool aborted)
    {
        if (_pressedKey == 0xFFFF) return;
        _holdTimer.Stop();
        var id = _pressedKey;
        _pressedKey = 0xFFFF;
        SoftKey?.Invoke(id, aborted ? 3 : 0);
        Invalidate();
    }

    /// <summary>Billentyűzet: F1–F5 a jobb oszlop, F6–F10 a bal oszlop softkey-ei.</summary>
    public bool KeyByIndex(int index, bool down)
    {
        if (_pool == null || _screen == null) return false;
        var mask = _pool.Get(_screen.Mask);
        var sk = mask == null ? null : _pool.Get(_screen.SoftKeyMaskOf(mask));
        if (sk == null || index < 0 || index >= sk.Keys.Count) return false;
        var id = ResolveKey(sk.Keys[index]);
        if (down) { if (_pressedKey != id) PressKey(id); }
        else ReleaseKey(false);
        return true;
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _holdTimer.Dispose();
            _tile.Dispose();
            ClearScaled();
            foreach (var f in _fonts.Values) f.Dispose();
        }
        base.Dispose(disposing);
    }
}
