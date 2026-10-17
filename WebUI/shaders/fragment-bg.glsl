#version 300 es

precision highp float;

in vec2 v_uv;
out vec4 fragColor;

uniform vec2 u_resolution;
uniform float u_dpr;
uniform float u_bgAspect;   // 背景纹理宽高比（桌面帧动态传入，占位为 4:3）
uniform float u_mergeRate;
uniform float u_shadowExpand;
uniform float u_shadowFactor;
uniform vec2 u_shadowPosition;
uniform sampler2D u_bgTexture;
uniform vec4 u_windowRect;

#include './lib/sdf.glsl'

void main() {
  vec2 fragPx = gl_FragCoord.xy;

  // 桌面抓取已停用：背景是一张静态渐变纹理（窗口相对坐标，随窗口移动
  // 一起走，不会产生"画面滑动"），直接按 v_uv 采样，不做任何屏幕 UV
  // 映射或 Y 翻转。u_windowRect 恒为 [0,0,1,1]，保留声明仅为兼容。
  vec3 bgColor = texture(u_bgTexture, v_uv).rgb;

  // 阴影（偏移后的 SDF）
  vec2 shadowOffsetPx = vec2(u_shadowPosition.x * u_dpr, u_shadowPosition.y * u_dpr);
  float sdfShadow = mainSDF(fragPx + shadowOffsetPx);

  float d = max(sdfShadow, 0.0);
  float shadowNear = exp(-d / max(u_shadowExpand * 0.35, 0.001)) * 0.55;
  float shadowFar  = exp(-d / max(u_shadowExpand * 1.60, 0.001)) * 0.25;
  float shadow = (shadowNear + shadowFar) * u_shadowFactor;

  float sdfRaw = mainSDF(fragPx);
  // 阴影不仅要排除透镜内部，还要排除透镜外缘约一个模糊半径的环带：
  // Pass2/3 半分辨率高斯（半径 7 ≈ 14 设备像素）会把透镜外的阴影
  // 反向 bleed 进透镜中心采样，亮背景下透镜内部会糊出一圈灰胶囊。
  const float shadowExcludePx = 15.0;
  float outsideMask = smoothstep(shadowExcludePx - 2.0, shadowExcludePx + 2.0, sdfRaw);
  shadow *= outsideMask;

  vec3 shadowed = bgColor - vec3(shadow);
  shadowed = max(shadowed, bgColor * 0.15);

  fragColor = vec4(shadowed, 1.0);
}