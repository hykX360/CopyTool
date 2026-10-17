#version 300 es

precision highp float;

#define PI (3.14159265359)

in vec2 v_uv;
uniform sampler2D u_blurredBg;
uniform sampler2D u_bg;
uniform vec2 u_resolution;
uniform float u_dpr;
uniform float u_mergeRate;
uniform vec4 u_tint;
uniform float u_refThickness;
uniform float u_refDistance;
uniform float u_refFactor;
uniform float u_refDispersion;
uniform float u_refFresnelRange;
uniform float u_refFresnelFactor;
uniform float u_refFresnelHardness;
uniform float u_glareRange;
uniform float u_glareConvergence;
uniform float u_glareOppositeFactor;
uniform float u_glareFactor;
uniform float u_glareHardness;
uniform float u_glareAngle;

// M2 磨砂面板（在 GL 管线内绘制，避免 CSS backdrop-filter 吃掉透镜折射）
uniform int   u_panelEnabled;
uniform vec4  u_panel;          // cx, cy, halfW, halfH（设备像素）
uniform float u_panelRadius;    // 圆角半径（设备像素）

uniform int STEP;

out vec4 fragColor;

#include './lib/sdf.glsl'
#include './lib/math.glsl'
#include './lib/color.glsl'

// 波长 → RGB
vec3 wavelengthToRGB(float lambda) {
  float r = 0.0, g = 0.0, b = 0.0;
  if (lambda >= 380.0 && lambda < 440.0) { r = -(lambda - 440.0) / 60.0; b = 1.0; }
  else if (lambda >= 440.0 && lambda < 490.0) { g = (lambda - 440.0) / 50.0; b = 1.0; }
  else if (lambda >= 490.0 && lambda < 510.0) { g = 1.0; b = -(lambda - 510.0) / 20.0; }
  else if (lambda >= 510.0 && lambda < 580.0) { r = (lambda - 510.0) / 70.0; g = 1.0; }
  else if (lambda >= 580.0 && lambda < 645.0) { r = 1.0; g = -(lambda - 645.0) / 65.0; }
  else if (lambda >= 645.0 && lambda <= 780.0) { r = 1.0; }
  float factor = 1.0;
  if (lambda < 420.0) factor = 0.3 + 0.7 * (lambda - 380.0) / 40.0;
  else if (lambda > 700.0) factor = 0.3 + 0.7 * (780.0 - lambda) / 80.0;
  return vec3(r, g, b) * factor;
}

#define SPECTRAL_SAMPLES 7

vec4 getTextureDispersion(
  sampler2D tex1,
  sampler2D tex2,
  vec2 offset,
  float factor,
  vec2 uvBase
) {
  vec3 acc = vec3(0.0);
  vec3 wsum = vec3(0.0);
  const float LAMBDA_RED = 680.0;
  const float LAMBDA_VIOLET = 400.0;

  for (int i = 0; i < SPECTRAL_SAMPLES; i++) {
    float t = float(i) / float(SPECTRAL_SAMPLES - 1);
    float lambda = mix(LAMBDA_RED, LAMBDA_VIOLET, t);
    vec3 w = wavelengthToRGB(lambda);
    float nScale = mix(0.98, 1.02, t);
    vec2 uv = uvBase + offset * (1.0 - (nScale - 1.0) * factor);
    vec3 c1 = texture(tex1, uv).rgb;
    vec3 c2 = texture(tex2, uv).rgb;
    acc += c1 * w;
    wsum += w;
  }
  return vec4(acc / max(wsum, vec3(1e-4)), 1.0);
}

float vec2ToAngle(vec2 v) {
  float angle = atan(v.y, v.x);
  if (angle < 0.0) angle += 2.0 * PI;
  return angle;
}

void main() {
  vec2 fragPx = gl_FragCoord.xy;
  float sdf = mainSDF(fragPx);   // 像素单位

  vec2 u_resolution1x = u_resolution.xy / u_dpr;
  vec4 outColor;

  float aaWidth = max(fwidth(sdf), 0.5);
  float insideMask = smoothstep(aaWidth, -aaWidth, sdf);

  // 计算 SDF 梯度（像素单位）
  vec2 h = vec2(1.0, 1.0);
  vec2 grad = vec2(
    mainSDF(fragPx + vec2(h.x, 0.0)) - mainSDF(fragPx - vec2(h.x, 0.0)),
    mainSDF(fragPx + vec2(0.0, h.y)) - mainSDF(fragPx - vec2(0.0, h.y))
  ) * 0.5;

  vec2 normalSDF = length(grad) > 1e-6 ? normalize(grad) : vec2(0.0);

  // 面板遮罩在面板块内赋值，最终透明合成时也要用
  float panelMask = 0.0;

  if (STEP <= 9) {
    vec3 bgOut = texture(u_bg, v_uv).rgb;

    // ============================================================
    // ★ M2 磨砂面板（GL 内合成：背景 → 面板磨砂 → 透镜折射）
    //   面板必须在 GL 里画：CSS backdrop-filter 会把 canvas 上已经
    //   画好的透镜折射边缘整体模糊掉，这是折射消失的根因之一。
    // ============================================================
    if (u_panelEnabled == 1) {
      float psdf = sdRoundedBoxPx(fragPx - u_panel.xy, u_panel.zw, u_panelRadius);
      float pAA = max(fwidth(psdf), 0.5);
      panelMask = smoothstep(pAA, -pAA, psdf);

      // 磨砂：采样半分辨率高斯模糊纹理
      vec3 frost = texture(u_blurredBg, v_uv).rgb;
      // saturate(1.4)：围绕亮度轴提升饱和度
      float fLuma = dot(frost, vec3(0.2126, 0.7152, 0.0722));
      frost = mix(vec3(fLuma), frost, 1.4);
      // 12% 白色染色（对应 M2 的 rgba(255,255,255,0.12)）
      frost = mix(frost, vec3(1.0), 0.12);

      bgOut = mix(bgOut, frost, panelMask);

      // 顶部 1px 镜面细边（光从上方来）：只在边缘内侧加一条白线
      float rim = (1.0 - smoothstep(0.0, 1.6, abs(psdf)))
                * smoothstep(pAA, -pAA, psdf);
      bgOut = mix(bgOut, vec3(1.0), rim * 0.55);

      // 面板内侧顶部柔光（inset 0 1px 0 rgba(255,255,255,.7) 的扩展版）
      float topBand = smoothstep(10.0, 0.0, abs(psdf + 5.0)) * panelMask;
      bgOut += vec3(topBand * 0.05);
    }

    // ============================================================
    // ★ 基于 SDF 距离的弯月斜面（BEVEL_PX 像素宽）
    // ============================================================
    const float BEVEL_PX = 30.0;
    const float DISP_GAIN_PX = 6000.0;

    float dist = -sdf;   // 到边缘的正距离（内部为正）
    float u = clamp(dist / BEVEL_PX, 0.0, 1.0);
    // 弯月高度剖面 h(u) = 1 - (1-u)^4
    float hProfile = 1.0 - pow(1.0 - u, 4.0);

    // 斜面在像素空间的梯度模长 → 倾角
    // dh/dx 用 BEVEL_PX 归一化
    float dhdu = 4.0 * pow(1.0 - u, 3.0);
    float dhdp = dhdu / BEVEL_PX;
    float thetaI = atan(dhdp);

    // Snell
    float sinThetaT = clamp(sin(thetaI) / u_refFactor, -1.0, 1.0);
    float thetaT = asin(sinThetaT);
    float angleDiff = thetaI - thetaT;

    // Fresnel 透射率
    float cosI = cos(thetaI);
    float cosT = cos(thetaT);
    float rs = (1.0 * cosI - u_refFactor * cosT) / (1.0 * cosI + u_refFactor * cosT);
    float rp = (u_refFactor * cosI - 1.0 * cosT) / (u_refFactor * cosI + 1.0 * cosT);
    float fresnelR = 0.5 * (rs * rs + rp * rp);
    float fresnelT = clamp(1.0 - fresnelR, 0.0, 1.0);

    // 位移量
    float dispPx = tan(clamp(angleDiff, 0.0, 1.4)) * hProfile * DISP_GAIN_PX * fresnelT;
    dispPx = clamp(dispPx, 0.0, 40.0);   // 防止过折射

    // 位移方向 = SDF 内法线（朝内）
    vec2 dispDir = -normalSDF;
    vec2 dispUV = dispDir * dispPx / u_resolution.y;

    vec3 glassColor;
    // ★ 只有弯月斜面（位移显著）才走折射色散；平坦中心 dispPx≈0，
    //   必须采样模糊纹理（毛玻璃）。旧条件 u>0.001 几乎对整个透镜内部
    //   成立，导致中心也按零位移采样清晰的 u_bg —— 透镜把上一帧
    //   自身画面（DOM 文字/阴影反馈）收敛成了一个灰胶囊。
    if (dispPx > 0.05 && insideMask > 0.0) {
      // 边缘环：折射 + 色散
      vec4 refracted = getTextureDispersion(
        u_bg, u_blurredBg,
        dispUV * u_refDistance * 10.0,
        u_refDispersion,
        v_uv
      );
      glassColor = refracted.rgb;
    } else {
      // 中心：毛玻璃
      glassColor = texture(u_blurredBg, v_uv).rgb;
    }

    // 菲涅尔边缘光
    float fresnelFactor = clamp(
      pow(1.0 + sdf / 500.0 * pow(500.0 / u_refFresnelRange, 2.0) + u_refFresnelHardness, 5.0),
      0.0, 1.0
    );
    glassColor = mix(glassColor, vec3(1.0), fresnelFactor * u_refFresnelFactor * 0.7 * insideMask);

    // 自适应染色
    vec3 bgUnderGlass = texture(u_blurredBg, v_uv).rgb;
    float bgLuma = dot(bgUnderGlass, vec3(0.2126, 0.7152, 0.0722));
    float adaptive = smoothstep(0.35, 0.85, bgLuma);
    vec3 tintColor = mix(u_tint.rgb, u_tint.rgb * 0.12, adaptive);
    float tintAmt = u_tint.a * (0.6 + 1.2 * adaptive);
    glassColor = mix(glassColor, tintColor, tintAmt * insideMask);

    // ============================================================
    // ★ 不透明窗口合成：宿主是普通不透明窗口（无每像素 DWM 透明），
    //   整张画面必须由 GL 全不透明输出——面板外是清晰实时桌面，
    //   面板内是磨砂桌面，透镜在最上层折射。透镜投影阴影已在
    //   fragment-bg 烤进 rtBg，这里不再二次压暗；也不能输出
    //   alpha=0，否则透出的是 body 的 CSS 网格背景（"格子去不掉"
    //   的根因）。
    // ============================================================
    vec3 finalRgb = mix(bgOut, glassColor, insideMask);

    // panelMask 仅在上方面板合成中使用，这里不再参与 alpha
    outColor = vec4(finalRgb, 1.0);
  } else {
    outColor = vec4(texture(u_bg, v_uv).rgb, 1.0);
  }

  fragColor = outColor;
}