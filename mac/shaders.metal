#include <metal_stdlib>
using namespace metal;

// Warp pass: draws the 64x36 mesh over the whole target. Each vertex carries the point in the
// previous frame it samples, so the GPU interpolates the warp between vertices. uvOrig is the
// vertex's own position and radAng its MilkDrop rad and ang.
struct WarpVertex {
    float2 position;
    float2 uv;
    float2 uvOrig;
    float2 radAng;
};

struct WarpOut {
    float4 position [[position]];
    float2 uv;
};

vertex WarpOut warpVertex(uint vid [[vertex_id]], const device WarpVertex* verts [[buffer(0)]]) {
    WarpOut out;
    out.position = float4(verts[vid].position, 0.0, 1.0);
    out.uv = verts[vid].uv;
    return out;
}

fragment float4 warpFragment(WarpOut in [[stage_in]],
                             texture2d<float> previous [[texture(0)]],
                             sampler linear [[sampler(0)]],
                             constant float& decay [[buffer(0)]]) {
    float3 colour = saturate(previous.sample(linear, in.uv).rgb) * decay;
    return float4(colour, 1.0);
}

// Wave pass: quads along the waveform polyline, added onto the warped image.
struct WaveVertex {
    float2 position;
    float across;
};

struct WaveUniforms {
    float4 colour;
    float2 viewport;
    float halfWidth;
};

struct WaveOut {
    float4 position [[position]];
    float across;
};

vertex WaveOut waveVertex(uint vid [[vertex_id]],
                          const device WaveVertex* verts [[buffer(0)]],
                          constant WaveUniforms& u [[buffer(1)]]) {
    float2 p = verts[vid].position / u.viewport * 2.0 - 1.0;
    WaveOut out;
    out.position = float4(p.x, -p.y, 0.0, 1.0);
    out.across = verts[vid].across;
    return out;
}

fragment float4 waveFragment(WaveOut in [[stage_in]], constant WaveUniforms& u [[buffer(0)]]) {
    float coverage = saturate(u.halfWidth + 0.5 - abs(in.across));
    return float4(u.colour.rgb * coverage, 1.0);
}

// Composite pass: one triangle covering the screen, with MilkDrop's gamma_adj brightness gain.
struct QuadOut {
    float4 position [[position]];
    float2 uv;
};

vertex QuadOut fullscreenVertex(uint vid [[vertex_id]]) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    QuadOut out;
    out.position = float4(p * 2.0 - 1.0, 0.0, 1.0);
    out.uv = float2(p.x, 1.0 - p.y);
    return out;
}

fragment float4 compositeFragment(QuadOut in [[stage_in]],
                                  texture2d<float> image [[texture(0)]],
                                  sampler linear [[sampler(0)]],
                                  constant float& gain [[buffer(0)]]) {
    return float4(saturate(image.sample(linear, in.uv).rgb * gain), 1.0);
}

// Vertex stages for translated preset shaders. The attribute numbers match the inputs
// milk_shader.cpp gives the HLSL entry point: colour, uv (xy warped, zw original), rad and ang.
struct PresetOut {
    float4 position [[position]];
    float4 diffuse [[user(locn0)]];
    float4 uv [[user(locn1)]];
    float2 radAng [[user(locn2)]];
};

vertex PresetOut presetWarpVertex(uint vid [[vertex_id]], const device WarpVertex* verts [[buffer(0)]]) {
    PresetOut out;
    out.position = float4(verts[vid].position, 0.0, 1.0);
    out.diffuse = float4(1.0);
    out.uv = float4(verts[vid].uv, verts[vid].uvOrig);
    out.radAng = verts[vid].radAng;
    return out;
}

// The comp pass's hue_shader blends four corner colours: top-left, top-right, bottom-left,
// bottom-right.
vertex PresetOut presetCompVertex(uint vid [[vertex_id]],
                                  const device WarpVertex* verts [[buffer(0)]],
                                  constant float4* corners [[buffer(1)]]) {
    const float2 uv = verts[vid].uvOrig;
    PresetOut out;
    out.position = float4(verts[vid].position, 0.0, 1.0);
    out.diffuse = mix(mix(corners[0], corners[1], uv.x), mix(corners[2], corners[3], uv.x), uv.y);
    out.uv = float4(uv, uv);
    out.radAng = verts[vid].radAng;
    return out;
}

// MilkDrop's blur: a long horizontal pass that also moves the source's encoded range onto this
// level's range, then a short vertical pass that darkens the edges of the first level. Each level
// is stored in the 0..1 range [min, max] given by its blur_min and blur_max. The weights are
// MilkDrop's eight, folded into pairs of bilinear taps.
struct BlurUniforms {
    float scale;
    float bias;
    float edgeDarken;
    float pad;
};

constant float BLUR_W[8] = {4.0, 3.8, 3.5, 2.9, 1.9, 1.2, 0.7, 0.3};

fragment float4 blurHorizontalFragment(QuadOut in [[stage_in]],
                                       texture2d<float> source [[texture(0)]],
                                       sampler linear [[sampler(0)]],
                                       constant BlurUniforms& u [[buffer(0)]]) {
    const float w1 = BLUR_W[0] + BLUR_W[1];
    const float w2 = BLUR_W[2] + BLUR_W[3];
    const float w3 = BLUR_W[4] + BLUR_W[5];
    const float w4 = BLUR_W[6] + BLUR_W[7];
    const float d1 = 0.0 + 2.0 * BLUR_W[1] / w1;
    const float d2 = 2.0 + 2.0 * BLUR_W[3] / w2;
    const float d3 = 4.0 + 2.0 * BLUR_W[5] / w3;
    const float d4 = 6.0 + 2.0 * BLUR_W[7] / w4;
    const float wDiv = 0.5 / (w1 + w2 + w3 + w4);
    const float2 texel = 1.0 / float2(source.get_width(), source.get_height());
    const float2 uv = in.uv + texel;
    float3 blur = (source.sample(linear, uv + float2(d1 * texel.x, 0.0)).rgb + source.sample(linear, uv - float2(d1 * texel.x, 0.0)).rgb) * w1 +
                  (source.sample(linear, uv + float2(d2 * texel.x, 0.0)).rgb + source.sample(linear, uv - float2(d2 * texel.x, 0.0)).rgb) * w2 +
                  (source.sample(linear, uv + float2(d3 * texel.x, 0.0)).rgb + source.sample(linear, uv - float2(d3 * texel.x, 0.0)).rgb) * w3 +
                  (source.sample(linear, uv + float2(d4 * texel.x, 0.0)).rgb + source.sample(linear, uv - float2(d4 * texel.x, 0.0)).rgb) * w4;
    blur *= wDiv;
    return float4(saturate(blur * u.scale + u.bias), 1.0);
}

fragment float4 blurVerticalFragment(QuadOut in [[stage_in]],
                                     texture2d<float> source [[texture(0)]],
                                     sampler linear [[sampler(0)]],
                                     constant BlurUniforms& u [[buffer(0)]]) {
    const float w1 = BLUR_W[0] + BLUR_W[1] + BLUR_W[2] + BLUR_W[3];
    const float w2 = BLUR_W[4] + BLUR_W[5] + BLUR_W[6] + BLUR_W[7];
    const float d1 = 0.0 + 2.0 * ((BLUR_W[2] + BLUR_W[3]) / w1);
    const float d2 = 2.0 + 2.0 * ((BLUR_W[6] + BLUR_W[7]) / w2);
    const float wDiv = 1.0 / ((w1 + w2) * 2.0);
    const float2 texel = 1.0 / float2(source.get_width(), source.get_height());
    const float2 uv = in.uv + float2(texel.x, 0.0);
    float3 blur = (source.sample(linear, uv + float2(0.0, d1 * texel.y)).rgb + source.sample(linear, uv - float2(0.0, d1 * texel.y)).rgb) * w1 +
                  (source.sample(linear, uv + float2(0.0, d2 * texel.y)).rgb + source.sample(linear, uv - float2(0.0, d2 * texel.y)).rgb) * w2;
    blur *= wDiv;
    float t = min(min(in.uv.x, in.uv.y), 1.0 - max(in.uv.x, in.uv.y));
    t = (1.0 - u.edgeDarken) + u.edgeDarken * saturate(sqrt(max(t, 0.0)) * 5.0);
    return float4(saturate(blur * t), 1.0);
}

// Custom shapes and waves. Positions arrive in pixels. A line's coverage falls off over its last
// pixel using `across`; fills carry a huge halfWidth so coverage stays 1.
struct CustomVertex {
    float2 position;
    float2 uv;
    float4 colour;
    float across;
    float halfWidth;
    float2 pad;
};

struct CustomOut {
    float4 position [[position]];
    float2 uv;
    float4 colour;
    float across;
    float halfWidth;
};

vertex CustomOut customVertex(uint vid [[vertex_id]],
                              const device CustomVertex* verts [[buffer(0)]],
                              constant float2& viewport [[buffer(1)]]) {
    const float2 p = verts[vid].position / viewport * 2.0 - 1.0;
    CustomOut out;
    out.position = float4(p.x, -p.y, 0.0, 1.0);
    out.uv = verts[vid].uv;
    out.colour = verts[vid].colour;
    out.across = verts[vid].across;
    out.halfWidth = verts[vid].halfWidth;
    return out;
}

fragment float4 customFlatFragment(CustomOut in [[stage_in]]) {
    const float coverage = saturate(in.halfWidth + 0.5 - abs(in.across));
    return float4(in.colour.rgb, in.colour.a * coverage);
}

fragment float4 customTexturedFragment(CustomOut in [[stage_in]],
                                       texture2d<float> image [[texture(0)]],
                                       sampler linear [[sampler(0)]]) {
    const float4 t = image.sample(linear, in.uv);
    return float4(t.rgb * in.colour.rgb, in.colour.a);
}

// Clamps the finished feedback image to 0..1 in place, reading the pixel already in the tile.
// NaN becomes 0, as it would in MilkDrop's 8-bit image. The test looks at the bits because fast
// math may drop isnan().
fragment float4 clampFragment(QuadOut in [[stage_in]], float4 current [[color(0)]]) {
    const uint4 bits = as_type<uint4>(current);
    const bool4 nan = (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0u;
    return select(saturate(current), float4(0.0), nan);
}

// MilkDrop 1's composite, used by presets whose PSVERSION_COMP is 0. The image is multiplied by
// the gamma factor and the drifting corner tint, mixed with a zoomed and flipped copy of itself
// when video echo is on, clamped, then put through the brighten, darken, solarize and invert
// filters in that order.
struct ClassicUniforms {
    float gamma;
    float echoAlpha;
    float echoZoom;
    int echoOrient;
    int brighten;
    int darken;
    int solarize;
    int invert;
};

fragment float4 classicCompositeFragment(PresetOut in [[stage_in]],
                                         texture2d<float> image [[texture(0)]],
                                         sampler linear [[sampler(0)]],
                                         constant ClassicUniforms& u [[buffer(0)]]) {
    const float2 uv = in.uv.xy;
    float3 c = image.sample(linear, uv).rgb;
    if (u.echoAlpha > 0.001) {
        float2 echo = 0.5 + (uv - 0.5) / u.echoZoom;
        if (u.echoOrient % 2 == 1) echo.x = 1.0 - echo.x;
        if (u.echoOrient >= 2) echo.y = 1.0 - echo.y;
        c = c * (1.0 - u.echoAlpha) + image.sample(linear, echo).rgb * u.echoAlpha;
    }
    c = saturate(c * in.diffuse.rgb * u.gamma);
    if (u.brighten != 0) c = 1.0 - (1.0 - c) * (1.0 - c);
    if (u.darken != 0) c = c * c;
    if (u.solarize != 0) c = saturate(2.0 * c * (1.0 - c));
    if (u.invert != 0) c = 1.0 - c;
    return float4(c, 1.0);
}

// Random colour noise, for starting an image somewhere other than black.
fragment float4 noiseFragment(QuadOut in [[stage_in]]) {
    uint3 h = uint3(uint2(in.position.xy), 0x9e3779b9u);
    h = h * 1664525u + 1013904223u;
    h.x += h.y * h.z;
    h.y += h.z * h.x;
    h.z += h.x * h.y;
    h ^= h >> 16u;
    return float4(float3(h & 0xffffu) / 65535.0, 1.0);
}
