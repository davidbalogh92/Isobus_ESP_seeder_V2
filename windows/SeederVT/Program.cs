using System.Text.Json.Nodes;
using SeederVT.Vt;

namespace SeederVT;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        // Ellenőrző mód (CI): SeederVT.exe --render pool.iop vt.json kep.png [szélesség magasság]
        if (args.Length >= 4 && args[0] == "--render") return Render(args);

        Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
        Application.ThreadException += (_, e) =>
            MessageBox.Show(e.Exception.Message, "Vetőgép – hiba", MessageBoxButtons.OK, MessageBoxIcon.Error);
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
        return 0;
    }

    private static int Render(string[] a)
    {
        try
        {
            var pool = Pool.Parse(File.ReadAllBytes(a[1]));
            var screen = VtScreen.From(JsonNode.Parse(File.ReadAllText(a[2]))!);
            int w = a.Length > 4 ? int.Parse(a[4]) : 1296, h = a.Length > 5 ? int.Parse(a[5]) : 800;
            using var view = new VtView { Width = w, Height = h };
            view.SetPool(pool);
            view.SetScreen(screen);
            using var bmp = new Bitmap(w, h);
            view.DrawToBitmap(bmp, new Rectangle(0, 0, w, h));
            bmp.Save(a[3], System.Drawing.Imaging.ImageFormat.Png);
            Console.WriteLine($"{a[3]}: {pool.Objects.Count} objektum");
            return 0;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine("Hiba: " + e);
            return 1;
        }
    }
}
