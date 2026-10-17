using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.Web.WebView2.Core;
using System;
using System.IO;

namespace TryLiquidGlass
{
    public sealed partial class MainWindow : Window
    {
        // 不依赖 VS“输出”窗口：同时写 Debug 和 exe 旁的日志文件
        private static readonly string LogPath =
            Path.Combine(AppContext.BaseDirectory, "webview_debug.log");

        private static void Log(string msg)
        {
            var line = $"{DateTime.Now:HH:mm:ss.fff} {msg}";
            System.Diagnostics.Debug.WriteLine(line);
            try { File.AppendAllText(LogPath, line + "\n"); } catch { }
        }

        public MainWindow()
        {
            InitializeComponent();
            Log("=== MainWindow ctor ===");
            WebView.Loaded += OnWebViewLoaded;
        }

        private async void OnWebViewLoaded(object sender, RoutedEventArgs e)
        {
            try
            {
                Log("Loaded fired, EnsureCoreWebView2Async...");
                await WebView.EnsureCoreWebView2Async();
                var core = WebView.CoreWebView2;

                // DevTools / 右键菜单默认即开启（AreBrowserDevToolsEnabled 默认 true）
                string webuiDir = Path.Combine(AppContext.BaseDirectory, "WebUI");

                // 关键修复：把本地文件夹映射成 https 虚拟主机。
                // file:// 下 origin 为 null，ES Module 与 fetch 会被 CORS 整体拦截，
                // 页面只剩 body 的 #111 黑底（这就是纯黑屏的原因）。
                core.SetVirtualHostNameToFolderMapping(
                    "appassets.example",
                    webuiDir,
                    CoreWebView2HostResourceAccessKind.Allow);

                core.NavigationCompleted += (s, ev) => Log(
                    $"NavigationCompleted success={ev.IsSuccess} webError={ev.WebErrorStatus}");

                core.WebMessageReceived += (s, ev) =>
                {
                    string text = "";
                    try { text = ev.TryGetWebMessageAsString() ?? ""; } catch { }
                    Log("PAGE: " + text);
                };

                // 把页面内未捕获的错误回传到宿主日志（模块加载错误也能看到）
                await core.AddScriptToExecuteOnDocumentCreatedAsync(@"
window.addEventListener('error', function(e){
  if (window.chrome && chrome.webview)
    chrome.webview.postMessage('ERROR: ' + e.message + ' @ ' + (e.filename||'') + ':' + e.lineno);
});
window.addEventListener('unhandledrejection', function(e){
  if (window.chrome && chrome.webview)
    chrome.webview.postMessage('REJECT: ' + (e.reason && e.reason.message ? e.reason.message : e.reason));
});
");

                // 透明背景（必须在导航前设置）
                WebView.DefaultBackgroundColor = Windows.UI.Color.FromArgb(0, 0, 0, 0);

                string uri = "https://appassets.example/index.html?t=" + DateTime.Now.Ticks;
                Log("Navigate: " + uri + " | WebUI exists=" + Directory.Exists(webuiDir));
                core.Navigate(uri);
            }
            catch (Exception ex)
            {
                Log("INIT EXCEPTION: " + ex);
            }
        }
    }
}
