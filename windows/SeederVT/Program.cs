namespace SeederVT;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
        Application.ThreadException += (_, e) =>
            MessageBox.Show(e.Exception.Message, "Vetőgép – hiba", MessageBoxButtons.OK, MessageBoxIcon.Error);
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}
