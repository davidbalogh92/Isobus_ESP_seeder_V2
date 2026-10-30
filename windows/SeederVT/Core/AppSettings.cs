using System.Text.Json;

namespace SeederVT.Core;

/// <summary>A program helyi beállításai (%LOCALAPPDATA%\SeederVT\settings.json).</summary>
public sealed class AppSettings
{
    public List<string> Hosts { get; set; } = new() { "192.168.5.130", "192.168.4.1" };
    public string LastHost { get; set; } = "192.168.5.130";
    public int PollMs { get; set; } = 200;
    public bool Sound { get; set; } = true;
    public bool AlarmSnapshots { get; set; } = true;
    public string RecordDir { get; set; } =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "SeederVT");
    public int WindowW { get; set; } = 1280;
    public int WindowH { get; set; } = 800;
    public bool Maximized { get; set; }

    public static string Dir => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SeederVT");
    private static string FilePath => Path.Combine(Dir, "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<AppSettings>(File.ReadAllText(FilePath)) ?? new AppSettings();
        }
        catch { }
        return new AppSettings();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Dir);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }
}
