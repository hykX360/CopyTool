// Signed distance functions shared by the background and main passes.

#define MAX_LENSES 16

uniform int   u_lensCount;
uniform vec4  u_lensData[MAX_LENSES];    // cx, cy, halfW, halfH (pixels)
uniform float u_lensRadius[MAX_LENSES];  // corner radius (pixels)
uniform float u_lensPress[MAX_LENSES];   // press amount 0..1

float sdRoundedBoxPx(vec2 p, vec2 halfSize, float r) {
    vec2 q = abs(p) - halfSize + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

// mainSDF：像素单位的 SDF（正外负内）
float mainSDF(vec2 fragCoordPx) {
    float d = 1e10;
    for (int i = 0; i < MAX_LENSES; i++) {
        if (i >= u_lensCount) break;
        vec4 L = u_lensData[i];
        vec2 dp = fragCoordPx - L.xy;
        float di = sdRoundedBoxPx(dp, L.zw, u_lensRadius[i]);
        d = smin(d, di, u_mergeRate);
    }
    return d;
}