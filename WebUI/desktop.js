// ============ CopyTool 实时桌面帧接收层 ============
// 职责：接收 C++ 通过 PostSharedBufferToScript 推来的桌面帧（共享内存），
//       只保留最新一帧，供 renderer.js 在渲染循环里取走并上传 GL 纹理。
//
// 生命周期关键点（不能在收到事件时立刻 releaseBuffer）：
//   releaseBuffer 之后 ArrayBuffer 会被 detach（失效），
//   renderer 还没上传纹理就释放 = 黑屏。
//   所以：取帧 -> renderer 同步 texSubImage2D -> 再 release。
(function () {
    const wv = window.chrome && window.chrome.webview;
    if (!wv) {
        console.log('[desktop] no webview host, desktop capture disabled');
        return;
    }

    let latest = null;

    wv.addEventListener('sharedbufferreceived', (e) => {
        let meta = e.additionalData;
        if (typeof meta === 'string') {
            try { meta = JSON.parse(meta); } catch (_) { meta = null; }
        }
        if (!meta || meta.type !== 'desktop') {
            // 非桌面用途的 buffer（目前没有）也要释放，避免泄漏
            try { wv.releaseBuffer(e.getBuffer()); } catch (_) {}
            return;
        }

        let buf = null;
        try { buf = e.getBuffer(); } catch (err) {
            console.warn('[desktop] getBuffer failed', err);
            return;
        }
        // 只保留最新帧：上一帧没被 renderer 取走就直接释放
        if (latest) {
            try { wv.releaseBuffer(latest.buf); } catch (_) {}
        }
        latest = {
            buf: buf,
            w: meta.w | 0,
            h: meta.h | 0,
            ts: meta.ts | 0,
            winX: meta.winX | 0,
            winY: meta.winY | 0,
            winW: meta.winW | 0,
            winH: meta.winH | 0,
            screenW: meta.screenW | 0,
            screenH: meta.screenH | 0,
        };
        // 诊断计数：累计收到的桌面帧数（含导航完成后的 Poke 补帧）
        window.__desktopFramesReceived = (window.__desktopFramesReceived || 0) + 1;
        // 通知渲染器：有新帧才需要唤醒 rAF（静态时渲染循环完全停转）
        try { window.dispatchEvent(new Event('desktopframe')); } catch (_) {}
    });

    // renderer.js 用来判断是否还有未消费帧（决定 rAF 能否停转）
    window.copytoolHasDesktopFrame = function () { return latest !== null; };

    // renderer.js 每帧调用：有新帧则返回 {data, w, h, release()}，无则 null。
    // 返回的 Uint8Array 是共享内存的零拷贝视图，release() 必须在
    // texSubImage2D 完成之后调用。
    window.copytoolTakeDesktopFrame = function () {
        if (!latest) return null;
        const f = latest;
        latest = null;
        let view;
        try {
            view = new Uint8Array(f.buf);
        } catch (err) {
            try { wv.releaseBuffer(f.buf); } catch (_) {}
            return null;
        }
        return {
            data: view,
            w: f.w,
            h: f.h,
            ts: f.ts,
            winX: f.winX,
            winY: f.winY,
            winW: f.winW,
            winH: f.winH,
            screenW: f.screenW,
            screenH: f.screenH,
            release: function () {
                try { wv.releaseBuffer(f.buf); } catch (_) {}
            }
        };
    };

    console.log('[desktop] bridge ready');
})();
