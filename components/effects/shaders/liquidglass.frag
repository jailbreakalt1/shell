#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 resolution;
    float radius;
    float morph;
    float iTime;
    float refraction;
    float bevelDepth;
    float bevelWidth;
    float frost;
    float specularIntensity;
    float pad;
    vec4 tint;
    float tintStrength;
    float glassOpacity;
    float chroma;
};

layout(binding = 1) uniform sampler2D source;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    if (resolution.x < 1.0 || resolution.y < 1.0) {
        fragColor = vec4(0.0);
        return;
    }

    vec2 pixel = qt_TexCoord0 * resolution;
    vec2 halfSize = 0.5 * resolution;
    vec2 p = pixel - halfSize;

    float r = clamp(radius, 0.0, min(halfSize.x, halfSize.y) - 1.0);
    float angle = atan(p.y, p.x);
    float wobble = sin(angle * 3.0 + iTime * 1.1) * 0.55 + sin(angle * 5.0 - iTime * 0.9 + 1.7) * 0.45;
    float d = sdRoundBox(p, halfSize, r) + wobble * (morph * 3.0 + 0.7);
    float inShape = 1.0 - smoothstep(-1.0, 1.0, d);

    if (inShape <= 0.001) {
        fragColor = vec4(0.0);
        return;
    }

    float distFromEdge = -d;
    float bevel = clamp(bevelWidth * min(resolution.x, resolution.y), 2.0, min(halfSize.x, halfSize.y) - 1.0);
    float edge = 1.0 - smoothstep(0.0, bevel, distFromEdge);

    vec2 grad;
    float eps = 0.75;
    grad.x = sdRoundBox(p + vec2(eps, 0.0), halfSize, r) - sdRoundBox(p - vec2(eps, 0.0), halfSize, r);
    grad.y = sdRoundBox(p + vec2(0.0, eps), halfSize, r) - sdRoundBox(p - vec2(0.0, eps), halfSize, r);
    grad = normalize(grad + vec2(0.0001));

    float offsetAmt = (edge * refraction + pow(edge, 10.0) * bevelDepth) * min(resolution.x, resolution.y);
    float centreBlend = smoothstep(0.12, 0.48, length(p / halfSize));
    vec2 offset = normalize(p + vec2(0.0001)) * offsetAmt * centreBlend;

    vec2 texSize = resolution + 2.0 * pad;
    vec2 baseUV = (pixel + pad) / texSize;
    vec2 sampleUV = baseUV + offset / texSize;
    vec2 texel = 1.0 / texSize;

    vec4 baseCol = texture(source, clamp(baseUV, vec2(0.001), vec2(0.999)));
    vec4 refrCol = baseCol;

    if (frost > 0.0) {
        float radiusPx = frost * 7.0;
        vec4 sum = vec4(0.0);
        const int SAMPLES = 12;
        for (int i = 0; i < SAMPLES; i++) {
            float a = float(i) * 0.5235987756;
            vec2 off = vec2(cos(a), sin(a)) * radiusPx * texel;
            sum += texture(source, clamp(sampleUV + off, vec2(0.001), vec2(0.999)));
        }
        refrCol = sum / float(SAMPLES);
    } else {
        refrCol = texture(source, clamp(sampleUV, vec2(0.001), vec2(0.999)));
    }

    vec2 caOffset = normalize(p + vec2(0.0001)) * edge * chroma * 2.0 * texel;
    vec3 refrRGB = refrCol.rgb;
    if (chroma > 0.0 && edge > 0.0) {
        refrRGB.r = texture(source, clamp(sampleUV + caOffset, vec2(0.001), vec2(0.999))).r;
        refrRGB.b = texture(source, clamp(sampleUV - caOffset, vec2(0.001), vec2(0.999))).b;
    }

    float hasSource = smoothstep(0.0, 0.08, refrCol.a);
    vec3 glass = mix(tint.rgb, refrRGB, hasSource);
    glass = mix(glass, tint.rgb, tintStrength);

    // A faint top-to-bottom light gradient keeps the sheet from looking flat.
    glass *= 1.0 + 0.06 * (1.0 - qt_TexCoord0.y);
    glass = mix(glass, vec3(1.0), (1.0 - qt_TexCoord0.y) * 0.06);

    vec2 glowCentre = vec2(0.18, 0.0);
    float topGlow = smoothstep(0.65, 0.0, length((qt_TexCoord0 - glowCentre) * vec2(1.0, 1.6)));
    glass += vec3(1.0) * topGlow * 0.09;

    float prism = pow(edge, 3.0);
    glass += vec3(0.18, 0.45, 0.85) * prism * 0.08;

    // Broad, fixed reflection band plus a slow travelling sheen.
    float band = exp(-pow((qt_TexCoord0.x - qt_TexCoord0.y * 0.45 - 0.22) / 0.28, 2.0));
    glass += vec3(1.0) * band * 0.035;

    // Soft inner shading and a crisp, light-facing rim.
    float innerShadow = 1.0 - smoothstep(0.0, bevel * 0.8, distFromEdge);
    glass *= mix(1.0, 0.78, innerShadow * 0.5);

    vec2 lightDir = normalize(vec2(-0.55, -0.83));
    float rim = 1.0 - smoothstep(0.0, bevel, distFromEdge);
    float rimDot = max(dot(grad, lightDir), 0.0);
    float spec = pow(rimDot, 1.2) * rim * specularIntensity;
    glass += vec3(0.95, 0.98, 1.0) * spec * 1.15;

    float bevelGlow = smoothstep(0.0, bevel, distFromEdge) * (1.0 - smoothstep(bevel, bevel * 1.6, distFromEdge));
    glass += vec3(1.0) * bevelGlow * 0.05;

    float border = smoothstep(0.0, 1.0, distFromEdge) * (1.0 - smoothstep(1.0, 2.4, distFromEdge));
    glass += vec3(1.0) * border * 0.22;

    float innerRim = smoothstep(0.0, 1.5, distFromEdge) * (1.0 - smoothstep(1.5, 6.0, distFromEdge));
    glass += vec3(1.0) * innerRim * 0.08;

    float sheenX = fract(iTime * 0.045) * 1.9 - 0.45;
    float diag = qt_TexCoord0.x + qt_TexCoord0.y * 0.6;
    float sheen = exp(-pow((diag - sheenX) / 0.24, 2.0));
    glass += vec3(1.0) * sheen * 0.06 * specularIntensity;

    float alpha = inShape * glassOpacity;
    fragColor = vec4(glass * alpha, alpha) * qt_Opacity;
}
