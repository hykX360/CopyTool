// ============================================================
// renderer.js — 纯 DOM 交互层（WebGL 液态玻璃管线已下线）
//
// 背景渐变与主面板毛玻璃全部由 CSS 提供（见 styles.css / materials.css），
// 本文件只保留 .box 按钮的果冻按压 / 拖动弹簧动画，不触碰任何 canvas / GL。
// ============================================================

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

// ============ 果冻按压 / 拖动 ============
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

// 所有 .box 按钮（开始复制 / 保存）使用纯 DOM 果冻交互。
document.querySelectorAll('.box').forEach(el => attachJellyDrag(el));
