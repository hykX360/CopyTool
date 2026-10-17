# CopyTool

一个基于 WebView2 + WebGL2 液态玻璃 UI 的现代化 Windows 文件复制工具。
内置独立复制内核 CopyCore.dll，支持智能磁盘检测、多线程调度、断点跳过与 SHA256 校验。

> 早期版本曾尝试基于 Flutter / Fluent 实现，现已全面弃用，改为 WebView2 + 纯 C++ 复制内核。

## ✨ 核心特性
- **极致性能**：在海量小文件场景下，比 FastCopy 快近 50%。
- **智能调度**：自动识别 SSD / HDD / USB / 网络路径，动态分配 16~32 个并发线程。
- **扫描/复制并行**：边扫描边复制，告别“先等扫描进度条”。
- **断点秒过**：重跑同一批文件，耗时仅 235ms。
- **现代化 UI**：WebView2 + WebGL2 渲染的液态玻璃材质，支持按压弹簧。

## 📊 性能基准测试 (Benchmark)
测试数据：D:\Steam++ 目录（931 个小文件，总计约 360MB，同盘 SSD 复制）

| 工具 / 模式 | 平均耗时 |
|---|---|
| **CopyTool (智能检测)** | **2.97s** |
| CopyTool (平衡模式) | 3.37s |
| FastCopy (64MB 缓冲区) | 6.03s |
| FastCopy (默认 512MB 缓冲区) | 5.99s |

> 结论：CopyTool 速度约为 FastCopy 的 2 倍。

## 🚀 快速开始
1. 前往 Releases 页面下载最新的 CopyTool-v1.0.0.zip。
2. 解压后双击 CopyToolWeb.exe 运行。
3. 确保系统已安装 [WebView2 Runtime](https://developer.microsoft.com/zh-cn/microsoft-edge/webview2/)。
