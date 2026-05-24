#nullable enable
using System.Text;
using Avalonia;

namespace SharpTalkGui;

class Program {
    public static void Main(string[] args) {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .LogToTrace();
}
