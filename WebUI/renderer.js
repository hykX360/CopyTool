// ============ 配置 ============
const CANVAS = document.getElementById('gl');
const gl = CANVAS.getContext('webgl2', { alpha: true, premultipliedAlpha: false, antialias: false });
if (!gl) throw new Error('WebGL2 not supported');

const MAX_LENSES = 16;

// ============ 工具 ============
async function loadText(url) {
    const r = await fetch(url);
    if (!r.ok) throw new Error(`Failed to load ${url}: ${r.status}`);
    return await r.text();
}

async function loadShaderWithIncludes(url, seen = new Set()) {
    if (seen.has(url)) return '';
    seen.add(url);
    let src = await loadText(url);
    const includeRe = /#include\s+['"](.+?)['"]/g;
    for (const m of [...src.matchAll(includeRe)]) {
        const includePath = new URL(m[1], new URL(url, location.href)).href;
        const includeSrc = await loadShaderWithIncludes(includePath, seen);
        src = src.replace(m[0], includeSrc);
    }
    return src;
}

function compile(type, src, name) {
    const s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
        const log = gl.getShaderInfoLog(s);
        console.error(`[${name}] compile error:\n`, log);
        throw new Error(`${name} compile failed: ${log}`);
    }
    return s;
}

function createProgram(vsSrc, fsSrc, name) {
    const vs = compile(gl.VERTEX_SHADER, vsSrc, name + '.vs');
    const fs = compile(gl.FRAGMENT_SHADER, fsSrc, name + '.fs');
    const p = gl.createProgram();
    gl.attachShader(p, vs);
    gl.attachShader(p, fs);
    gl.linkProgram(p);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
        throw new Error(`${name} link failed: ${gl.getProgramInfoLog(p)}`);
    }
    return p;
}

function cacheUniforms(prog, names) {
    const out = {};
    for (const n of names) out[n] = gl.getUniformLocation(prog, n);
    return out;
}

// ============ 渲染目标 ============
function createRenderTarget(w, h) {
    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    const fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    return { tex, fbo, w, h };
}

// ============ 弹簧 ============
class Spring {
    constructor(stiffness = 400, damping = 29, mass = 1.0) {
        this.stiffness = stiffness;
        this.damping = damping;
        this.mass = mass;
        this.value = 0;
        this.velocity = 0;
        this.target = 0;
    }
    set(v) { this.target = v; }
    setVelocity(v) { this.velocity = v; }
    update(dt) {
        const dtSec = Math.min(dt, 1 / 30);
        const maxStep = 1 / 240;
        const steps = Math.min(8, Math.ceil(dtSec / maxStep));
        const h = dtSec / steps;
        for (let i = 0; i < steps; i++) {
            const force = (this.target - this.value) * this.stiffness;
            const dampingForce = -this.damping * this.velocity;
            const acc = (force + dampingForce) / this.mass;
            this.velocity += acc * h;
            this.value += this.velocity * h;
        }
        return this.value;
    }
}

// ============ 主流程 ============
async function main() {
    const [vs, fsBg, fsVblur, fsHblur, fsMain] = await Promise.all([
        loadShaderWithIncludes('shaders/vertex.glsl'),
        loadShaderWithIncludes('shaders/fragment-bg.glsl'),
        loadShaderWithIncludes('shaders/fragment-bg-vblur.glsl'),
        loadShaderWithIncludes('shaders/fragment-bg-hblur.glsl'),
        loadShaderWithIncludes('shaders/fragment-main.glsl'),
    ]);

    const progBg = createProgram(vs, fsBg, 'bg');
    const progVblur = createProgram(vs, fsVblur, 'vblur');
    const progHblur = createProgram(vs, fsHblur, 'hblur');
    const progMain = createProgram(vs, fsMain, 'main');

    const U = {
        bg: cacheUniforms(progBg, [
            'u_bgTexture', 'u_resolution', 'u_bgAspect', 'u_dpr', 'u_windowRect',
            'u_lensCount', 'u_lensData', 'u_lensRadius', 'u_lensPress',
            'u_mergeRate', 'u_shadowExpand', 'u_shadowFactor', 'u_shadowPosition',
        ]),
        vblur: cacheUniforms(progVblur, [
            'u_prevPassTexture', 'u_resolution', 'u_blurRadius', 'u_blurWeights',
        ]),
        hblur: cacheUniforms(progHblur, [
            'u_prevPassTexture', 'u_resolution', 'u_blurRadius', 'u_blurWeights',
        ]),
        main: cacheUniforms(progMain, [
            'u_blurredBg', 'u_bg', 'u_resolution', 'u_dpr', 'u_lensCount', 'u_lensData',
            'u_lensRadius', 'u_lensPress', 'u_mergeRate', 'u_tint', 'u_refThickness',
            'u_refDistance', 'u_refFactor', 'u_refDispersion', 'u_refFresnelRange',
            'u_refFresnelFactor', 'u_refFresnelHardness', 'u_glareRange', 'u_glareConvergence',
            'u_glareOppositeFactor', 'u_glareFactor', 'u_glareHardness', 'u_glareAngle',
            'STEP', 'u_panelEnabled', 'u_panel', 'u_panelRadius',
        ]),
    };

    const vao = gl.createVertexArray();

    let dpr = Math.min(window.devicePixelRatio, 2);
    let W = 0, H = 0, BW = 0, BH = 0;
    let rtBg, rtVblur, rtHblur;
    let sizeChanged = false;

    function rebuildRenderTargets() {
        const newDpr = Math.min(window.devicePixelRatio, 2);
        const newW = Math.floor(CANVAS.clientWidth * newDpr);
        const newH = Math.floor(CANVAS.clientHeight * newDpr);
        if (newW === W && newH === H && newDpr === dpr) return;
        sizeChanged = true;

        dpr = newDpr;
        W = newW;
        H = newH;
        CANVAS.width = W;
        CANVAS.height = H;

        if (!rtBg || rtBg.w !== W || rtBg.h !== H) {
            if (rtBg) { gl.deleteTexture(rtBg.tex); gl.deleteFramebuffer(rtBg.fbo); }
            if (rtVblur) { gl.deleteTexture(rtVblur.tex); gl.deleteFramebuffer(rtVblur.fbo); }
            if (rtHblur) { gl.deleteTexture(rtHblur.tex); gl.deleteFramebuffer(rtHblur.fbo); }

            BW = Math.max(1, W >> 1);
            BH = Math.max(1, H >> 1);
            rtBg = createRenderTarget(W, H);
            rtVblur = createRenderTarget(BW, BH);
            rtHblur = createRenderTarget(BW, BH);
        }
    }
    rebuildRenderTargets();

    const BG_W = 512, BG_H = 384;
    const bgTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, bgTex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, BG_W, BG_H, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    function generateGradientBackgroundTexture() {
        const c = document.createElement('canvas');
        c.width = BG_W; c.height = BG_H;
        const ctx = c.getContext('2d');

        const lg = ctx.createLinearGradient(0, 0, BG_W, BG_H);
        lg.addColorStop(0.0, '#eef3fb');
        lg.addColorStop(0.5, '#f4f2f7');
        lg.addColorStop(1.0, '#f7eef3');
        ctx.fillStyle = lg;
        ctx.fillRect(0, 0, BG_W, BG_H);

        const g1 = ctx.createRadialGradient(
            BG_W * 0.18, BG_H * 0.12, 0,
            BG_W * 0.18, BG_H * 0.12, BG_W * 0.62);
        g1.addColorStop(0, 'rgba(135,180,255,0.18)');
        g1.addColorStop(1, 'rgba(135,180,255,0)');
        ctx.fillStyle = g1;
        ctx.fillRect(0, 0, BG_W, BG_H);

        const g2 = ctx.createRadialGradient(
            BG_W * 0.82, BG_H * 0.88, 0,
            BG_W * 0.82, BG_H * 0.88, BG_W * 0.62);
        g2.addColorStop(0, 'rgba(255,180,220,0.15)');
        g2.addColorStop(1, 'rgba(255,180,220,0)');
        ctx.fillStyle = g2;
        ctx.fillRect(0, 0, BG_W, BG_H);

        gl.bindTexture(gl.TEXTURE_2D, bgTex);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, c);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
    }
    generateGradientBackgroundTexture();

    const windowRect = [0, 0, 1, 1];

    const lenses = [];

    function register(el, opts = {}) {
        if (lenses.length >= MAX_LENSES) {
            console.warn('[liquidGlass] 达到上限', MAX_LENSES);
            return;
        }
        const lens = {
            el,
            radius: parseRadius(el, opts.radius),
            press: new Spring(400, 29),
            isDown: false,
            cx: 0, cy: 0, halfW: 0, halfH: 0,
            lastScale: 1,
        };
        lenses.push(lens);
        updateGeometry(lens);
        return () => unregister(el);
    }

    function unregister(el) {
        const i = lenses.findIndex(L => L.el === el);
        if (i >= 0) lenses.splice(i, 1);
    }

    function parseRadius(el, override) {
        const v = override || el.dataset.lgRadius || getComputedStyle(el).borderTopLeftRadius;
        if (typeof v === 'string' && v.endsWith('%')) {
            return { type: 'percent', value: parseFloat(v) / 100 };
        }
        return { type: 'px', value: parseFloat(v) || 0 };
    }

    function updateGeometry(lens) {
        const el = lens.el;
        const rect = el.getBoundingClientRect();
        const canvasRect = CANVAS.getBoundingClientRect();

        const cssX = rect.left - canvasRect.left;
        const cssY = rect.top - canvasRect.top;

        const scale = lens.lastScale || 1;
        const w = rect.width / scale;
        const h = rect.height / scale;

        lens.baseHalfW = w * 0.5 * dpr;
        lens.baseHalfH = h * 0.5 * dpr;
        lens.cx = (cssX + rect.width * 0.5) * dpr;
        lens.cy = (canvasRect.height - (cssY + rect.height * 0.5)) * dpr;

        const r = lens.radius;
        if (r.type === 'percent') {
            lens.radiusPx = Math.min(lens.baseHalfW, lens.baseHalfH) * r.value * 2;
        } else {
            lens.radiusPx = r.value * dpr;
        }
    }

    const panelEl = document.querySelector('.m2-panel');
    const panel = { enabled: 0, cx: 0, cy: 0, halfW: 0, halfH: 0, radiusPx: 0 };

    function updatePanelGeometry() {
        if (!panelEl) { panel.enabled = 0; return; }
        const rect = panelEl.getBoundingClientRect();
        const canvasRect = CANVAS.getBoundingClientRect();
        if (rect.width < 4 || rect.height < 4) { panel.enabled = 0; return; }
        const cssX = rect.left - canvasRect.left;
        const cssY = rect.top - canvasRect.top;
        panel.enabled = 1;
        panel.halfW = rect.width * 0.5 * dpr;
        panel.halfH = rect.height * 0.5 * dpr;
        panel.cx = (cssX + rect.width * 0.5) * dpr;
        panel.cy = (canvasRect.height - (cssY + rect.height * 0.5)) * dpr;
        const br = parseFloat(getComputedStyle(panelEl).borderTopLeftRadius) || 20;
        panel.radiusPx = br * dpr;
    }

    function updateAllGeometry() {
        for (const L of lenses) updateGeometry(L);
        updatePanelGeometry();
    }
    updatePanelGeometry();

    window.addEventListener('resize', () => {
        rebuildRenderTargets();
        updateAllGeometry();
        kickRender();
    });

    document.querySelectorAll('[data-liquid-glass]').forEach(el => register(el));
    window.liquidGlass = { register, unregister, lenses };

    function attachJellyDrag(el) {
        const springX = new Spring(320, 16);
        const springY = new Spring(320, 16);

        springX.value = springX.target = 1;
        springY.value = springY.target = 1;

        // 不能在 attach 时测量：设置页按钮此时 display:none，rect 为 0×0，
        // 补偿会恒为 0，缩放退化成以中心为原点向两侧扩张。改为按下时测量。
        let baseW = 0;
        let baseH = 0;

        let state = 'idle';
        let pointerId = null;
        let startX = 0, startY = 0;
        let lastX = 0, lastY = 0, lastTime = 0;
        let velocityX = 0, velocityY = 0;
        let rafId = 0, lastFrameTime = 0;

        let dirX = 0;
        let dirY = 0;

        const MAX_STRETCH = 0.15;
        const FULL_STRETCH_DIST = 120;
        const PRESS_SCALE = 1.04;

        function tick(now) {
            if (lastFrameTime === 0) lastFrameTime = now;
            const dt = Math.min(1, (now - lastFrameTime) / 1000);
            lastFrameTime = now;

            const sx = springX.update(dt);
            const sy = springY.update(dt);

            // 补偿用 Math.sign 而非连续分量。拖动方向一侧固定不动，
            // 反方向一侧拉长；斜向拖动时两个轴各自独立做单边固定。
            // press 态 dirX/dirY 为 0，sign 也为 0，保持居中原点按压放大。
            const signX = Math.sign(dirX);
            const signY = Math.sign(dirY);
            const tx = signX * (sx - 1) * baseW / 2;
            const ty = signY * (sy - 1) * baseH / 2;

            el.style.transform = `translate(${tx}px, ${ty}px) scale(${sx}, ${sy})`;

            const settled = Math.abs(springX.value - springX.target) < 0.001 &&
                            Math.abs(springX.velocity) < 0.001 &&
                            Math.abs(springY.value - springY.target) < 0.001 &&
                            Math.abs(springY.velocity) < 0.001;

            if (!settled) {
                rafId = requestAnimationFrame(tick);
            } else {
                if (Math.abs(springX.target - 1) < 0.001 &&
                    Math.abs(springY.target - 1) < 0.001) {
                    el.style.transform = '';
                }
                rafId = 0;
            }
        }

        function startAnim() {
            if (rafId) return;
            if (lastFrameTime === 0 || performance.now() - lastFrameTime > 100) {
                lastFrameTime = performance.now() - 16;
            }
            rafId = requestAnimationFrame(tick);
        }

        function onWindowPointerMove(e) {
            if (e.pointerId !== pointerId) return;

            const now = performance.now();
            const dt = (now - lastTime) / 1000;
            if (dt > 0.001) {
                velocityX = (e.clientX - lastX) / dt;
                velocityY = (e.clientY - lastY) / dt;
                lastX = e.clientX;
                lastY = e.clientY;
                lastTime = now;
            }

            const dx = e.clientX - startX;
            const dy = e.clientY - startY;
            const dist = Math.sqrt(dx * dx + dy * dy);

            if (state === 'pressing' && dist > 3) {
                state = 'dragging';
            }

            if (state === 'dragging') {
                const distSafe = Math.max(dist, 1e-3);
                const unitX = dx / distSafe;
                const unitY = dy / distSafe;

                const weightX = unitX * unitX;
                const weightY = unitY * unitY;

                const stretchAmount = Math.min(dist / FULL_STRETCH_DIST, 1) * MAX_STRETCH;

                const targetSX = 1 + stretchAmount * weightX - stretchAmount * 0.35 * weightY;
                const targetSY = 1 + stretchAmount * weightY - stretchAmount * 0.35 * weightX;

                springX.set(Math.max(0.7, targetSX));
                springY.set(Math.max(0.7, targetSY));

                dirX = unitX;
                dirY = unitY;

                startAnim();
            }
        }

        function onWindowPointerUp(e) {
            if (e.pointerId !== pointerId) return;

            window.removeEventListener('pointermove', onWindowPointerMove);
            window.removeEventListener('pointerup', onWindowPointerUp);
            window.removeEventListener('pointercancel', onWindowPointerUp);

            pointerId = null;
            state = 'idle';

            springX.set(1);
            springY.set(1);

            const maxVel = 2.0;
            springX.setVelocity(Math.max(-maxVel, Math.min(maxVel, velocityX / 400)));
            springY.setVelocity(Math.max(-maxVel, Math.min(maxVel, velocityY / 400)));

            startAnim();
        }

        function onPointerDown(e) {
            if (e.button !== 0 || state !== 'idle') return;
            e.preventDefault();

            // 按下时元素必然可见，此刻测得的尺寸才是有效的补偿基准。
            const r = el.getBoundingClientRect();
            baseW = r.width;
            baseH = r.height;
            if (baseW <= 0 || baseH <= 0) return;

            pointerId = e.pointerId;
            state = 'pressing';
            startX = lastX = e.clientX;
            startY = lastY = e.clientY;
            lastTime = performance.now();
            velocityX = velocityY = 0;
            dirX = 0;
            dirY = 0;

            springX.set(PRESS_SCALE);
            springY.set(PRESS_SCALE);
            startAnim();

            window.addEventListener('pointermove', onWindowPointerMove);
            window.addEventListener('pointerup', onWindowPointerUp);
            window.addEventListener('pointercancel', onWindowPointerUp);
        }

        el.addEventListener('pointerdown', onPointerDown);
    }

    document.querySelectorAll('.box').forEach(el => {
        if (!el.hasAttribute('data-liquid-glass')) {
            attachJellyDrag(el);
        }
    });

    // ============================================================
    // WebGL 透镜交互
    // ============================================================
    const mouse = { x: 0, y: 0 };
    let activeLens = null;
    let lastInputTime = performance.now();

    CANVAS.parentElement.addEventListener('pointermove', (e) => {
        const r = CANVAS.getBoundingClientRect();
        mouse.x = (e.clientX - r.left) * dpr;
        mouse.y = (r.height - (e.clientY - r.top)) * dpr;
        lastInputTime = performance.now();
        kickRender();
    });

    document.addEventListener('pointerdown', (e) => {
        const rect = CANVAS.getBoundingClientRect();
        const px = (e.clientX - rect.left) * dpr;
        const py = (rect.height - (e.clientY - rect.top)) * dpr;

        for (let i = lenses.length - 1; i >= 0; i--) {
            const L = lenses[i];
            if (hitTest(L, px, py)) {
                activeLens = L;
                L.isDown = true;
                L.press.set(1.0);
                lastInputTime = performance.now();
                kickRender();
                return;
            }
        }
    });

    function endPointer() {
        if (activeLens) {
            const L = activeLens;
            L.isDown = false;
            L.press.set(0.0);
            L.press.setVelocity(-2.5);
            activeLens = null;
            lastInputTime = performance.now();
            kickRender();
        }
    }
    document.addEventListener('pointerup', endPointer);
    document.addEventListener('pointercancel', endPointer);

    function hitTest(lens, px, py) {
        const dx = px - lens.cx;
        const dy = py - lens.cy;
        return Math.abs(dx) <= lens.baseHalfW && Math.abs(dy) <= lens.baseHalfH;
    }

    function gaussianWeights(radius, sigma) {
        const w = [1.0];
        let sum = 1.0;
        for (let i = 1; i <= radius; i++) {
            const v = Math.exp(-(i * i) / (2 * sigma * sigma));
            w.push(v);
            sum += v * 2;
        }
        return w.map(v => v / sum);
    }
    const blurRadius = 7;
    const blurWeights = new Float32Array(201);
    gaussianWeights(blurRadius, 5.0).forEach((v, i) => blurWeights[i] = v);

    let lastFrameTime = 0;
    let frameCount = 0;
    let startupFrames = 2;

    let rafPending = false;
    function kickRender() {
        if (rafPending) return;
        rafPending = true;
        requestAnimationFrame(render);
    }
    document.addEventListener('visibilitychange', () => {
        if (!document.hidden) kickRender();
    });

    function drawFullscreen() {
        gl.bindVertexArray(vao);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
    }

    const lensPosArr = new Float32Array(MAX_LENSES * 4);
    const lensRadiusArr = new Float32Array(MAX_LENSES);
    const lensPressArr = new Float32Array(MAX_LENSES);

    function render(now) {
        const idle = (now - lastInputTime) > 500;
        const targetFps = idle ? 30 : 60;
        const frameInterval = 1000 / targetFps;
        if (now - lastFrameTime < frameInterval - 1) {
            requestAnimationFrame(render);
            return;
        }

        let springActive = false;
        for (const L of lenses) {
            if (Math.abs(L.press.value) > 0.0005) { springActive = true; break; }
        }
        const inputRecent = (now - lastInputTime) < 400;
        const needRender = springActive || inputRecent ||
                           sizeChanged || startupFrames > 0;
        if (!needRender) {
            rafPending = false;
            return;
        }

        const dt = lastFrameTime > 0 ? (now - lastFrameTime) / 1000 : 1 / 60;
        lastFrameTime = now;
        if (startupFrames > 0) startupFrames--;

        rebuildRenderTargets();

        if (sizeChanged) {
            updateAllGeometry();
            sizeChanged = false;
        } else if (++frameCount % 30 === 0) {
            updateAllGeometry();
        }

        for (const L of lenses) {
            const press = L.press.update(dt);
            const pressDown = Math.max(0, press);
            const pressOver = Math.max(0, -press);

            const scale = 1.0 - pressDown * 0.07 + pressOver * 0.65;

            if (L.el) L.el.style.transform = `scale(${scale})`;
            L.lastScale = scale;

            L.currentHalfW = L.baseHalfW * scale;
            L.currentHalfH = L.baseHalfH * scale;
        }

        const n = Math.min(lenses.length, MAX_LENSES);
        for (let i = 0; i < n; i++) {
            const L = lenses[i];
            lensPosArr[i * 4 + 0] = L.cx;
            lensPosArr[i * 4 + 1] = L.cy;
            lensPosArr[i * 4 + 2] = L.currentHalfW || L.baseHalfW;
            lensPosArr[i * 4 + 3] = L.currentHalfH || L.baseHalfH;
            lensRadiusArr[i] = L.radiusPx;
            lensPressArr[i] = L.press.value;
        }

        gl.bindFramebuffer(gl.FRAMEBUFFER, rtBg.fbo);
        gl.viewport(0, 0, W, H);
        gl.useProgram(progBg);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, bgTex);
        gl.uniform1i(U.bg.u_bgTexture, 0);
        gl.uniform2f(U.bg.u_resolution, W, H);
        gl.uniform1f(U.bg.u_bgAspect, BG_W / BG_H);
        gl.uniform1f(U.bg.u_dpr, dpr);
        gl.uniform4fv(U.bg.u_windowRect, windowRect);
        gl.uniform1i(U.bg.u_lensCount, n);
        gl.uniform4fv(U.bg.u_lensData, lensPosArr);
        gl.uniform1fv(U.bg.u_lensRadius, lensRadiusArr);
        gl.uniform1fv(U.bg.u_lensPress, lensPressArr);
        gl.uniform1f(U.bg.u_mergeRate, 0.06);
        gl.uniform1f(U.bg.u_shadowExpand, 25);
        gl.uniform1f(U.bg.u_shadowFactor, 0.30);
        gl.uniform2f(U.bg.u_shadowPosition, 0, -6);
        drawFullscreen();

        gl.bindFramebuffer(gl.FRAMEBUFFER, rtVblur.fbo);
        gl.viewport(0, 0, BW, BH);
        gl.useProgram(progVblur);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, rtBg.tex);
        gl.uniform1i(U.vblur.u_prevPassTexture, 0);
        gl.uniform2f(U.vblur.u_resolution, BW, BH);
        gl.uniform1i(U.vblur.u_blurRadius, blurRadius);
        gl.uniform1fv(U.vblur.u_blurWeights, blurWeights);
        drawFullscreen();

        gl.bindFramebuffer(gl.FRAMEBUFFER, rtHblur.fbo);
        gl.viewport(0, 0, BW, BH);
        gl.useProgram(progHblur);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, rtVblur.tex);
        gl.uniform1i(U.hblur.u_prevPassTexture, 0);
        gl.uniform2f(U.hblur.u_resolution, BW, BH);
        gl.uniform1i(U.hblur.u_blurRadius, blurRadius);
        gl.uniform1fv(U.hblur.u_blurWeights, blurWeights);
        drawFullscreen();

        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, W, H);
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);
        gl.useProgram(progMain);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, rtHblur.tex);
        gl.activeTexture(gl.TEXTURE1);
        gl.bindTexture(gl.TEXTURE_2D, rtBg.tex);
        gl.uniform1i(U.main.u_blurredBg, 0);
        gl.uniform1i(U.main.u_bg, 1);
        gl.uniform2f(U.main.u_resolution, W, H);
        gl.uniform1f(U.main.u_dpr, dpr);
        gl.uniform1i(U.main.u_lensCount, n);
        gl.uniform4fv(U.main.u_lensData, lensPosArr);
        gl.uniform1fv(U.main.u_lensRadius, lensRadiusArr);
        gl.uniform1fv(U.main.u_lensPress, lensPressArr);
        gl.uniform1f(U.main.u_mergeRate, 0.06);
        gl.uniform4f(U.main.u_tint, 1, 1, 1, 0.04);
        gl.uniform1f(U.main.u_refThickness, 22);
        gl.uniform1f(U.main.u_refDistance, 0.12);
        gl.uniform1f(U.main.u_refFactor, 1.5);
        gl.uniform1f(U.main.u_refDispersion, 1.5);
        gl.uniform1f(U.main.u_refFresnelRange, 8);
        gl.uniform1f(U.main.u_refFresnelFactor, 1.3);
        gl.uniform1f(U.main.u_refFresnelHardness, 0.8);
        gl.uniform1f(U.main.u_glareRange, 14);
        gl.uniform1f(U.main.u_glareConvergence, 0.5);
        gl.uniform1f(U.main.u_glareOppositeFactor, 0.6);
        gl.uniform1f(U.main.u_glareFactor, 1.2);
        gl.uniform1f(U.main.u_glareHardness, 0.6);
        gl.uniform1f(U.main.u_glareAngle, -Math.PI / 4);
        gl.uniform1i(U.main.STEP, 9);
        gl.uniform1i(U.main.u_panelEnabled, panel.enabled);
        gl.uniform4f(U.main.u_panel, panel.cx, panel.cy, panel.halfW, panel.halfH);
        gl.uniform1f(U.main.u_panelRadius, panel.radiusPx);
        drawFullscreen();

        const stillActive = springActive ||
                            (performance.now() - lastInputTime) < 400 ||
                            sizeChanged || startupFrames > 0;
        if (stillActive) {
            requestAnimationFrame(render);
        } else {
            rafPending = false;
        }
    }

    kickRender();
}

main().catch(e => {
    console.error(e);
    document.body.innerHTML = `<pre style="color:#f88;padding:20px;font-family:monospace">${e.message}\n\n${e.stack}</pre>`;
});