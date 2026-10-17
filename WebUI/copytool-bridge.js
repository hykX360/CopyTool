// ============ CopyTool WebView2 桥接层 ============
// 职责：把 C++ 的消息收到、按 type 分发给 ui.js；把 JS 命令发给 C++。

window.chrome?.webview?.addEventListener('message', (e) => {
    const msg = typeof e.data === 'string' ? JSON.parse(e.data) : e.data;
    const ui = window.copyToolUI;
    if (msg.type === 'status' && ui) ui.onStatus(msg);
    else if (msg.type === 'config' && ui) ui.onConfig(msg);
    else if (msg.type === 'finished' && ui) ui.onFinished(msg);
});

function send(cmd, extra = {}) {
    window.chrome?.webview?.postMessage(JSON.stringify({ cmd, ...extra }));
}

// 暴露给 ui.js
window.copyTool = { send };

// 启动时拉一次配置
send('getConfig');

console.log('[CopyTool bridge] ready');