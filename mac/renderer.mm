#import "renderer.h"

#include "audio_synth.h"
#include "band_levels.h"
#include "custom_geometry.h"
#include "feedback.h"
#include "milk_audio.h"
#include "milk_preset.h"
#include "milk_wave.h"
#include "preset.h"
#include "preset_gpu.h"
#include "system_audio.h"
#include "texture_bank.h"

#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace Milk;

namespace {

constexpr int WAVE_SAMPLES = SAMPLE_RATE / 60;
// Stereo history kept for the custom waves' waveform and spectrum.
constexpr int STEREO_HISTORY = 1024;
constexpr int SCRATCH_SAMPLES = 1 << 14;
// The CPU effect renders at this height. Wave widths scale from it.
constexpr float REFERENCE_HEIGHT = 540.0f;
constexpr MTLPixelFormat FEEDBACK_FORMAT = MTLPixelFormatRGBA16Float;
// MilkDrop's equations and shaders assume one step per frame at about this rate.
constexpr NSInteger PRESET_FPS = 60;
constexpr NSInteger MOTION_FPS = 120;

enum AudioSource : int {
    SOURCE_SYSTEM = 0,
    SOURCE_SYNTH = 1,
};

struct WarpVertex {
    float x, y, u, v, u0, v0, rad, ang;
};

struct WaveVertex {
    float x, y, across, pad;
};

struct WaveUniforms {
    float colour[4];
    float viewport[2];
    float halfWidth;
    float pad;
};

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

struct BlurUniforms {
    float direction[2];
    float decodeScale;
    float decodeBias;
    float encodeMin;
    float encodeInvRange;
    float edgeDarken;
    float pad;
};

// One blur level's range. MilkDrop stores blurN as (value - min) / (max - min).
struct BlurRange {
    float min = 0.0f;
    float max = 1.0f;
};

std::array<float, 12> rotation(float ax, float ay, float az) {
    const float cx = std::cos(ax), sx = std::sin(ax);
    const float cy = std::cos(ay), sy = std::sin(ay);
    const float cz = std::cos(az), sz = std::sin(az);
    // Rows of Rx * Ry * Rz, then a zero translation row.
    return {
        cy * cz, cy * sz, -sy,
        sx * sy * cz - cx * sz, sx * sy * sz + cx * cz, sx * cy,
        cx * sy * cz + sx * sz, cx * sy * sz - sx * cz, cx * cy,
        0.0f, 0.0f, 0.0f,
    };
}

} // namespace

@implementation MilkRenderer {
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_queue;
    id<MTLRenderPipelineState> m_warpPipeline;
    id<MTLRenderPipelineState> m_wavePipeline;
    id<MTLRenderPipelineState> m_compositePipeline;
    id<MTLRenderPipelineState> m_classicPipeline;
    id<MTLRenderPipelineState> m_blurPipeline;
    id<MTLRenderPipelineState> m_clampPipeline;
    id<MTLRenderPipelineState> m_noisePipeline;
    bool m_seedNoise;
    // [textured][additive]
    id<MTLRenderPipelineState> m_customPipelines[2][2];
    id<MTLFunction> m_presetWarpVertex;
    id<MTLFunction> m_presetCompVertex;
    MTLPixelFormat m_drawableFormat;
    id<MTLSamplerState> m_clampSampler;
    id<MTLSamplerState> m_repeatSampler;
    id<MTLBuffer> m_meshIndices;
    NSUInteger m_meshIndexCount;
    id<MTLTexture> m_textures[2];
    id<MTLTexture> m_blur[3];
    id<MTLTexture> m_blurTemp[3];
    int m_front;
    bool m_needsClear;

    Params m_params;
    SynthState m_synth;
    SystemAudioTap m_tap;
    std::unique_ptr<BandLevels> m_bands;
    AudioSource m_source;
    std::string m_tapError;
    std::vector<std::string> m_presetPaths;
    int m_presetIndex;
    std::unique_ptr<PresetPlayer> m_player;
    std::unique_ptr<TextureBank> m_bank;
    std::unique_ptr<PresetGpu> m_gpu;
    int m_compVersion;
    uint32_t m_presetSeed;
    uint64_t m_presetFrame;
    std::mt19937 m_random;
    std::array<float, 4> m_randPreset;
    std::vector<float> m_scratch;
    std::vector<float> m_scratchR;
    std::vector<float> m_mono;
    std::vector<float> m_history;
    std::vector<float> m_historyL;
    std::vector<float> m_historyR;
    MilkAudioFrame m_milkAudio;
    CustomGeometry m_custom;
    CustomGeometry m_vectors;
    std::vector<CustomShapeInstance> m_shapeInstances;
    std::vector<CustomWaveOutput> m_customWaves;
    MainWave m_mainWave;
    std::vector<MeshUv> m_mesh;
    std::vector<WarpVertex> m_gridBase;
    std::vector<WarpVertex> m_warpVerts;
    std::vector<WaveVertex> m_waveVerts;
    std::vector<float> m_waveXs;
    std::vector<float> m_waveYs;
    double m_lastFrame;
    float m_time;
}

- (nullable instancetype)initWithView:(MTKView*)view error:(NSString* _Nullable* _Nullable)error {
    NSString* path = [[NSBundle mainBundle] pathForResource:@"shaders" ofType:@"metal"];
    NSString* source = path ? [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil] : nil;
    if (source == nil) {
        if (error) *error = @"shaders.metal is missing from the app bundle";
        return nil;
    }
    self = [self initWithDevice:view.device outputFormat:view.colorPixelFormat shaderSource:source error:error];
    if (self == nil) return nil;
    m_random.seed(std::random_device{}());
    [self findPresets];
    [self startSystemAudio];
    return self;
}

- (nullable instancetype)initWithDevice:(id<MTLDevice>)device
                           outputFormat:(MTLPixelFormat)format
                           shaderSource:(NSString*)source
                                  error:(NSString* _Nullable* _Nullable)error {
    self = [super init];
    if (self == nil) return nil;

    m_device = device;
    m_queue = [m_device newCommandQueue];
    m_drawableFormat = format;
    if (![self buildPipelines:format source:source error:error]) return nil;
    [self buildMeshIndices];

    MTLSamplerDescriptor* sampler = [MTLSamplerDescriptor new];
    sampler.minFilter = MTLSamplerMinMagFilterLinear;
    sampler.magFilter = MTLSamplerMinMagFilterLinear;
    sampler.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler.tAddressMode = MTLSamplerAddressModeClampToEdge;
    m_clampSampler = [m_device newSamplerStateWithDescriptor:sampler];
    sampler.sAddressMode = MTLSamplerAddressModeRepeat;
    sampler.tAddressMode = MTLSamplerAddressModeRepeat;
    m_repeatSampler = [m_device newSamplerStateWithDescriptor:sampler];

    m_params = Params{};
    resetSynth(m_synth, 1, m_params.bpm);
    m_scratch.assign(SCRATCH_SAMPLES, 0.0f);
    m_scratchR.assign(SCRATCH_SAMPLES, 0.0f);
    m_mono.assign(SCRATCH_SAMPLES, 0.0f);
    m_history.assign(WAVE_SAMPLES, 0.0f);
    m_historyL.assign(STEREO_HISTORY, 0.0f);
    m_historyR.assign(STEREO_HISTORY, 0.0f);
    m_front = 0;
    m_needsClear = true;
    m_lastFrame = 0.0;
    m_time = 0.0f;
    m_source = SOURCE_SYNTH;
    m_presetIndex = -1;
    m_compVersion = 2;
    m_seedNoise = false;
    m_presetSeed = 0;
    m_presetFrame = 0;
    m_random.seed(1);
    m_bank = std::make_unique<TextureBank>(m_device, std::string(MILK_PRESET_DIR) + "/textures");
    return self;
}

- (BOOL)buildPipelines:(MTLPixelFormat)drawableFormat source:(NSString*)source error:(NSString* _Nullable* _Nullable)error {
    NSError* nsError = nil;
    id<MTLLibrary> library = [m_device newLibraryWithSource:source options:nil error:&nsError];
    if (library == nil) {
        if (error) *error = [NSString stringWithFormat:@"shader compile failed: %@", nsError.localizedDescription];
        return NO;
    }
    m_presetWarpVertex = [library newFunctionWithName:@"presetWarpVertex"];
    m_presetCompVertex = [library newFunctionWithName:@"presetCompVertex"];

    MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
    desc.colorAttachments[0].pixelFormat = FEEDBACK_FORMAT;
    desc.vertexFunction = [library newFunctionWithName:@"warpVertex"];
    desc.fragmentFunction = [library newFunctionWithName:@"warpFragment"];
    m_warpPipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];

    desc.vertexFunction = [library newFunctionWithName:@"fullscreenVertex"];
    desc.fragmentFunction = [library newFunctionWithName:@"blurFragment"];
    m_blurPipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];

    desc.fragmentFunction = [library newFunctionWithName:@"clampFragment"];
    m_clampPipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];

    desc.fragmentFunction = [library newFunctionWithName:@"noiseFragment"];
    m_noisePipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];

    desc.vertexFunction = [library newFunctionWithName:@"waveVertex"];
    desc.fragmentFunction = [library newFunctionWithName:@"waveFragment"];
    desc.colorAttachments[0].blendingEnabled = YES;
    desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
    desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
    desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    m_wavePipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];

    id<MTLFunction> customVertex = [library newFunctionWithName:@"customVertex"];
    id<MTLFunction> customFragments[2] = {[library newFunctionWithName:@"customFlatFragment"],
                                          [library newFunctionWithName:@"customTexturedFragment"]};
    for (int textured = 0; textured < 2; ++textured) {
        for (int additive = 0; additive < 2; ++additive) {
            desc.vertexFunction = customVertex;
            desc.fragmentFunction = customFragments[textured];
            desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            desc.colorAttachments[0].destinationRGBBlendFactor =
                additive ? MTLBlendFactorOne : MTLBlendFactorOneMinusSourceAlpha;
            m_customPipelines[textured][additive] = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];
            if (m_customPipelines[textured][additive] == nil) {
                if (error) *error = [NSString stringWithFormat:@"custom pipeline failed: %@", nsError.localizedDescription];
                return NO;
            }
        }
    }

    desc.colorAttachments[0].blendingEnabled = NO;
    desc.colorAttachments[0].pixelFormat = drawableFormat;
    desc.vertexFunction = [library newFunctionWithName:@"fullscreenVertex"];
    desc.fragmentFunction = [library newFunctionWithName:@"compositeFragment"];
    m_compositePipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];

    desc.vertexFunction = m_presetCompVertex;
    desc.fragmentFunction = [library newFunctionWithName:@"classicCompositeFragment"];
    m_classicPipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];

    if (m_warpPipeline == nil || m_wavePipeline == nil || m_compositePipeline == nil || m_blurPipeline == nil || m_clampPipeline == nil || m_classicPipeline == nil || m_noisePipeline == nil ||
        m_presetWarpVertex == nil || m_presetCompVertex == nil) {
        if (error) *error = [NSString stringWithFormat:@"pipeline creation failed: %@", nsError.localizedDescription];
        return NO;
    }
    return YES;
}

- (void)buildMeshIndices {
    std::vector<uint16_t> indices;
    for (int j = 0; j < MESH_Y - 1; ++j) {
        for (int i = 0; i < MESH_X - 1; ++i) {
            const uint16_t a = static_cast<uint16_t>(j * MESH_X + i);
            const uint16_t b = a + 1;
            const uint16_t c = static_cast<uint16_t>(a + MESH_X);
            const uint16_t d = c + 1;
            indices.insert(indices.end(), {a, b, c, b, d, c});
        }
    }
    m_meshIndexCount = indices.size();
    m_meshIndices = [m_device newBufferWithBytes:indices.data()
                                          length:indices.size() * sizeof(uint16_t)
                                         options:MTLResourceStorageModeShared];
}

- (void)startSystemAudio {
    m_tapError.clear();
    if (m_tap.start(m_tapError)) {
        m_bands = std::make_unique<BandLevels>(m_tap.sampleRate());
        m_source = SOURCE_SYSTEM;
    } else {
        NSLog(@"System audio unavailable: %s", m_tapError.c_str());
        m_source = SOURCE_SYNTH;
    }
}

- (void)findPresets {
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(MILK_PRESET_DIR, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".milk") m_presetPaths.push_back(entry.path().string());
    }
    std::sort(m_presetPaths.begin(), m_presetPaths.end());
}

- (void)stepPreset:(int)delta {
    if (m_presetPaths.empty()) return;
    const int count = static_cast<int>(m_presetPaths.size());
    m_presetIndex = m_presetIndex < 0 ? (delta > 0 ? 0 : count - 1) : ((m_presetIndex + delta) % count + count) % count;
    [self loadPresetAtPath:@(m_presetPaths[m_presetIndex].c_str())];
}

- (BOOL)loadPresetAtPath:(NSString*)path {
    PresetFile file;
    std::string error;
    m_gpu.reset();
    if (!loadPresetFile(path.UTF8String, file, error)) {
        NSLog(@"%s", error.c_str());
        m_player.reset();
        return NO;
    }
    m_player = std::make_unique<PresetPlayer>(file);
    for (const std::string& e : m_player->errors()) NSLog(@"%s: %s", file.name.c_str(), e.c_str());
    m_presetSeed = m_random();
    m_presetFrame = 0;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    m_randPreset = {unit(m_random), unit(m_random), unit(m_random), unit(m_random)};
    m_compVersion = file.compVersion;
    // A stage whose shader version is 0 ignores any shader code, as MilkDrop does.
    const std::string warpCode = file.warpVersion > 0 ? file.warpShader : std::string();
    const std::string compCode = file.compVersion > 0 ? file.compShader : std::string();
    if (!warpCode.empty() || !compCode.empty()) {
        m_gpu = std::make_unique<PresetGpu>(m_device, m_presetWarpVertex, m_presetCompVertex, FEEDBACK_FORMAT,
                                            m_drawableFormat, *m_bank, warpCode, compCode, m_presetSeed);
        for (const std::string& e : m_gpu->errors()) NSLog(@"%s: %s", file.name.c_str(), e.c_str());
    }
    return YES;
}

- (void)nextPreset { [self stepPreset:1]; }

- (void)previousPreset { [self stepPreset:-1]; }

- (void)setMotion:(int)motion {
    m_player.reset();
    m_gpu.reset();
    m_params.motion = std::clamp(motion, 0, static_cast<int>(MOTION_NAMES.size()) - 1);
}

- (void)nextWaveMode {
    m_params.waveMode = (m_params.waveMode + 1) % static_cast<int>(WAVE_MODE_NAMES.size());
}

- (void)toggleColourCycle { m_params.colourCycle = !m_params.colourCycle; }

- (void)toggleWrap { m_params.wrap = !m_params.wrap; }

- (void)toggleAudioSource {
    if (m_source == SOURCE_SYSTEM) {
        m_tap.stop();
        m_source = SOURCE_SYNTH;
    } else {
        [self startSystemAudio];
    }
}

- (void)clear { m_needsClear = true; }

- (void)seedNoise {
    m_needsClear = true;
    m_seedNoise = true;
}

- (NSString*)status {
    std::string audio = m_source == SOURCE_SYSTEM ? "system audio" : "synth";
    if (m_source == SOURCE_SYNTH && !m_tapError.empty()) audio += " (system audio unavailable)";
    if (m_player) {
        std::string notes;
        if (m_gpu && !m_gpu->errors().empty()) notes += ", shader errors";
        if (!m_player->errors().empty()) notes += ", code errors";
        return [NSString stringWithFormat:@"Milk: %s (%d/%zu%s), %s", m_player->name().c_str(), m_presetIndex + 1,
                                          m_presetPaths.size(), notes.c_str(), audio.c_str()];
    }
    return [NSString stringWithFormat:@"Milk: %s, %s wave, %s", MOTION_NAMES[m_params.motion].c_str(),
                                      WAVE_MODE_NAMES[m_params.waveMode].c_str(), audio.c_str()];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {}

- (id<MTLTexture>)renderTarget:(NSUInteger)width height:(NSUInteger)height {
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:FEEDBACK_FORMAT
                                                                                    width:std::max<NSUInteger>(1, width)
                                                                                   height:std::max<NSUInteger>(1, height)
                                                                                mipmapped:NO];
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModePrivate;
    return [m_device newTextureWithDescriptor:desc];
}

// Sizes the feedback pair and the blur levels to the drawable, and rebuilds the grid of vertex
// positions, original uvs and rad/ang that every warp and comp pass shares.
- (void)ensureTextures:(CGSize)size {
    const NSUInteger width = std::max<NSUInteger>(1, static_cast<NSUInteger>(size.width));
    const NSUInteger height = std::max<NSUInteger>(1, static_cast<NSUInteger>(size.height));
    if (m_textures[0] != nil && m_textures[0].width == width && m_textures[0].height == height) return;
    m_textures[0] = [self renderTarget:width height:height];
    m_textures[1] = [self renderTarget:width height:height];
    for (int level = 0; level < 3; ++level) {
        const NSUInteger shift = static_cast<NSUInteger>(level + 1);
        m_blur[level] = [self renderTarget:width >> shift height:height >> shift];
        m_blurTemp[level] = [self renderTarget:width >> shift height:height >> shift];
    }
    m_needsClear = true;

    const float aspectX = (height > width) ? static_cast<float>(width) / height : 1.0f;
    const float aspectY = (width > height) ? static_cast<float>(height) / width : 1.0f;
    m_gridBase.resize(MESH_X * MESH_Y);
    for (int j = 0; j < MESH_Y; ++j) {
        for (int i = 0; i < MESH_X; ++i) {
            const float u = static_cast<float>(i) / (MESH_X - 1);
            const float v = static_cast<float>(j) / (MESH_Y - 1);
            const float x = u * 2.0f - 1.0f;
            const float y = -(v * 2.0f - 1.0f);
            m_gridBase[j * MESH_X + i] = WarpVertex{
                x, y, u, v, u, v,
                std::sqrt(x * x * aspectX * aspectX + y * y * aspectY * aspectY),
                std::atan2(y * aspectY, x * aspectX),
            };
        }
    }
}

// Appends `count` new samples to the end of `history`, dropping the oldest.
static void appendHistory(std::vector<float>& history, const float* samples, int count) {
    const int size = static_cast<int>(history.size());
    if (count >= size) {
        std::memcpy(history.data(), samples + count - size, size * sizeof(float));
    } else if (count > 0) {
        std::memmove(history.data(), history.data() + count, (size - count) * sizeof(float));
        std::memcpy(history.data() + size - count, samples, count * sizeof(float));
    }
}

// Pulls this frame's audio into the waveform histories and returns the band levels.
- (AudioLevels)advanceAudio:(float)dt frameScale:(float)frameScale {
    int count = 0;
    AudioLevels levels;
    if (m_source == SOURCE_SYSTEM) {
        count = m_tap.read(m_scratch.data(), m_scratchR.data(), SCRATCH_SAMPLES);
        for (int i = 0; i < count; ++i) m_mono[i] = 0.5f * (m_scratch[i] + m_scratchR[i]);
        m_bands->update(m_mono.data(), count, frameScale);
        levels = m_bands->levels();
    } else {
        count = std::min(SCRATCH_SAMPLES, static_cast<int>(std::lround(dt * SAMPLE_RATE)));
        synthesise(m_synth, m_params.bpm, count, m_mono.data());
        std::memcpy(m_scratch.data(), m_mono.data(), count * sizeof(float));
        std::memcpy(m_scratchR.data(), m_mono.data(), count * sizeof(float));
        levels = m_synth.levels;
    }
    appendHistory(m_history, m_mono.data(), count);
    appendHistory(m_historyL, m_scratch.data(), count);
    appendHistory(m_historyR, m_scratchR.data(), count);
    return levels;
}

// The equations give a displacement per 60 Hz frame. frameScale stretches it to this frame's length.
- (void)buildWarpVertices:(const FrameVars&)f perVertex:(const PerVertexFn&)perVertex levels:(const AudioLevels&)levels width:(int)width height:(int)height frameScale:(float)frameScale {
    computeMesh(f, perVertex, levels, m_time, width, height, m_mesh);
    m_warpVerts = m_gridBase;
    for (size_t k = 0; k < m_warpVerts.size(); ++k) {
        WarpVertex& v = m_warpVerts[k];
        v.u = v.u0 + (m_mesh[k].u - v.u0) * frameScale;
        v.v = v.v0 + (m_mesh[k].v - v.v0) * frameScale;
    }
}

- (void)buildWaveVertices:(const FrameVars&)f halfWidth:(float)halfWidth width:(int)width height:(int)height {
    wavePoints(f, m_history.data(), WAVE_SAMPLES, width, height, m_waveXs, m_waveYs);
    m_waveVerts.clear();
    const float outer = halfWidth + 1.0f;
    for (size_t k = 1; k < m_waveXs.size(); ++k) {
        const float x0 = m_waveXs[k - 1], y0 = m_waveYs[k - 1];
        const float x1 = m_waveXs[k], y1 = m_waveYs[k];
        const float len = std::hypot(x1 - x0, y1 - y0);
        if (len < 1e-4f) continue;
        const float nx = -(y1 - y0) / len * outer;
        const float ny = (x1 - x0) / len * outer;
        const WaveVertex a{x0 + nx, y0 + ny, outer, 0.0f};
        const WaveVertex b{x0 - nx, y0 - ny, -outer, 0.0f};
        const WaveVertex c{x1 + nx, y1 + ny, outer, 0.0f};
        const WaveVertex d{x1 - nx, y1 - ny, -outer, 0.0f};
        m_waveVerts.insert(m_waveVerts.end(), {a, b, c, b, d, c});
    }
}

- (BlurRange)blurRange:(int)level {
    const std::string n = "blur" + std::to_string(level + 1);
    BlurRange r{static_cast<float>(m_player->frameValue(n + "_min")), static_cast<float>(m_player->frameValue(n + "_max"))};
    if (!std::isfinite(r.min)) r.min = 0.0f;
    if (!std::isfinite(r.max)) r.max = 1.0f;
    // MilkDrop keeps each range at least 0.1 wide so the 8-bit encoding stays usable.
    if (r.max - r.min < 0.1f) {
        const float mid = (r.min + r.max) * 0.5f;
        r.min = mid - 0.05f;
        r.max = mid + 0.05f;
    }
    return r;
}

// The shader header's uniforms for this frame, following MilkDrop 2's layout of _c0.._c13.
- (ShaderFrameValues)shaderValues:(const AudioLevels&)a width:(int)width height:(int)height fps:(float)fps {
    ShaderFrameValues out;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float aspectX = (h > w) ? w / h : 1.0f;
    const float aspectY = (w > h) ? h / w : 1.0f;
    const float t = m_time;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    BlurRange blur[3];
    for (int i = 0; i < 3; ++i) blur[i] = [self blurRange:i];
    const auto& rp = m_randPreset;

    out.vectors["rand_frame"] = {unit(m_random), unit(m_random), unit(m_random), unit(m_random)};
    out.vectors["rand_preset"] = rp;
    out.vectors["_c0"] = {aspectX, aspectY, 1.0f / aspectX, 1.0f / aspectY};
    out.vectors["_c1"] = {0.0f, 0.0f, 0.0f, 0.0f};
    out.vectors["_c2"] = {t, fps, static_cast<float>(m_presetFrame), static_cast<float>(m_player->frameValue("progress"))};
    out.vectors["_c3"] = {a.bass, a.mid, a.treb, (a.bass + a.mid + a.treb) / 3.0f};
    out.vectors["_c4"] = {a.bassAtt, a.midAtt, a.trebAtt, (a.bassAtt + a.midAtt + a.trebAtt) / 3.0f};
    out.vectors["_c5"] = {blur[0].max - blur[0].min, blur[0].min, blur[1].max - blur[1].min, blur[1].min};
    out.vectors["_c6"] = {blur[2].max - blur[2].min, blur[2].min, blur[0].min, blur[0].max};
    out.vectors["_c7"] = {w, h, 1.0f / w, 1.0f / h};
    const float roam[4] = {0.329f, 1.293f, 5.070f, 20.051f};
    const float slow[4] = {0.0050f, 0.0083f, 0.0130f, 0.0220f};
    std::array<float, 4> c8, c9, c10, c11;
    for (int i = 0; i < 4; ++i) {
        c8[i] = 0.5f + 0.5f * std::cos(t * roam[i] + rp[i] * 6.28f);
        c9[i] = 0.5f + 0.5f * std::sin(t * roam[i] + rp[i] * 6.28f);
        c10[i] = 0.5f + 0.5f * std::cos(t * slow[i] + rp[i] * 6.28f);
        c11[i] = 0.5f + 0.5f * std::sin(t * slow[i] + rp[i] * 6.28f);
    }
    out.vectors["_c8"] = c8;
    out.vectors["_c9"] = c9;
    out.vectors["_c10"] = c10;
    out.vectors["_c11"] = c11;
    out.vectors["_c12"] = {1.0f, 1.0f, 0.5f, 0.0f};
    out.vectors["_c13"] = {blur[1].min, blur[1].max, blur[2].min, blur[2].max};
    const char* qNames[8] = {"_qa", "_qb", "_qc", "_qd", "_qe", "_qf", "_qg", "_qh"};
    for (int i = 0; i < 8; ++i) {
        out.vectors[qNames[i]] = {m_player->q(i * 4), m_player->q(i * 4 + 1), m_player->q(i * 4 + 2), m_player->q(i * 4 + 3)};
    }

    // Rotation matrices: static (s), then slow to ultra-fast spins (d, f, vf, uf), and a fresh
    // random one each frame (rand). Axes and phases come from the preset's seed.
    const struct { const char* name; float speed; } GROUPS[] = {
        {"rot_s", 0.0f}, {"rot_d", 0.02f}, {"rot_f", 0.1f}, {"rot_vf", 0.4f}, {"rot_uf", 1.6f},
    };
    std::mt19937 seeded(m_presetSeed);
    std::uniform_real_distribution<float> signedUnit(-1.0f, 1.0f);
    for (const auto& g : GROUPS) {
        for (int k = 1; k <= 4; ++k) {
            float angle[3];
            for (float& v : angle) v = signedUnit(seeded) * 3.14159f + signedUnit(seeded) * g.speed * t;
            out.matrices[std::string(g.name) + std::to_string(k)] = rotation(angle[0], angle[1], angle[2]);
        }
    }
    for (int k = 1; k <= 4; ++k) {
        out.matrices["rot_rand" + std::to_string(k)] =
            rotation(unit(m_random) * 6.28f, unit(m_random) * 6.28f, unit(m_random) * 6.28f);
    }
    return out;
}

// MilkDrop 2's hue_shader corner colours, which drift slowly with time.
- (void)hueCorners:(float*)out {
    for (int i = 0; i < 4; ++i) {
        float c[3] = {
            0.6f + 0.3f * std::sin(m_time * 30.0f * 0.0143f + 3.0f + i * 21.0f + m_randPreset[3]),
            0.6f + 0.3f * std::sin(m_time * 30.0f * 0.0107f + 1.0f + i * 13.0f + m_randPreset[1]),
            0.6f + 0.3f * std::sin(m_time * 30.0f * 0.0129f + 6.0f + i * 9.0f + m_randPreset[2]),
        };
        const float peak = std::max({c[0], c[1], c[2]});
        for (int k = 0; k < 3; ++k) out[i * 4 + k] = 0.5f + 0.5f * c[k] / peak;
        out[i * 4 + 3] = 1.0f;
    }
}

- (void)encodeBlurPass:(id<MTLCommandBuffer>)commands
                source:(id<MTLTexture>)source
                target:(id<MTLTexture>)target
              uniforms:(const BlurUniforms&)uniforms {
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:m_blurPipeline];
    [encoder setFragmentTexture:source atIndex:0];
    [encoder setFragmentSamplerState:m_clampSampler atIndex:0];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
}

// Builds blur1..levels from the finished frame. Each level halves the size and runs a horizontal
// then a vertical Gaussian pass, storing the result in its own 0..1 range.
- (void)encodeBlur:(id<MTLCommandBuffer>)commands from:(id<MTLTexture>)image levels:(int)levels {
    float decodeScale = 1.0f;
    float decodeBias = 0.0f;
    id<MTLTexture> source = image;
    for (int level = 0; level < levels; ++level) {
        const BlurRange range = [self blurRange:level];
        const BlurUniforms horizontal{{1.0f, 0.0f}, decodeScale, decodeBias, 0.0f, 1.0f, 0.0f, 0.0f};
        [self encodeBlurPass:commands source:source target:m_blurTemp[level] uniforms:horizontal];
        const float edge = level == 0 ? static_cast<float>(m_player->frameValue("blur1_edge_darken")) : 0.0f;
        const BlurUniforms vertical{{0.0f, 1.0f}, 1.0f, 0.0f, range.min, 1.0f / (range.max - range.min), edge, 0.0f};
        [self encodeBlurPass:commands source:m_blurTemp[level] target:m_blur[level] uniforms:vertical];
        decodeScale = range.max - range.min;
        decodeBias = range.min;
        source = m_blur[level];
    }
}

// MilkDrop 1's composite. Gamma is drawn as repeated additive passes in MilkDrop: with video echo
// the passes add up to max(gamma, 1), without it to gamma.
- (void)encodeClassicComposite:(id<MTLRenderCommandEncoder>)encoder image:(id<MTLTexture>)image {
    auto flag = [&](const char* name) { return std::fabs(m_player->frameValue(name)) > 0.00001 ? 1 : 0; };
    const float gamma = static_cast<float>(m_player->frameValue("gamma"));
    const float echoAlpha = static_cast<float>(m_player->frameValue("echo_alpha"));
    ClassicUniforms uniforms{
        echoAlpha > 0.001f ? std::max(gamma, 1.0f) : gamma,
        std::clamp(echoAlpha, 0.0f, 1.0f),
        std::max(static_cast<float>(m_player->frameValue("echo_zoom")), 0.001f),
        ((static_cast<int>(m_player->frameValue("echo_orient")) % 4) + 4) % 4,
        flag("brighten"),
        flag("darken"),
        flag("solarize"),
        flag("invert"),
    };
    float corners[16];
    [self hueCorners:corners];
    id<MTLBuffer> grid = [m_device newBufferWithBytes:m_gridBase.data()
                                               length:m_gridBase.size() * sizeof(WarpVertex)
                                              options:MTLResourceStorageModeShared];
    [encoder setRenderPipelineState:m_classicPipeline];
    [encoder setVertexBuffer:grid offset:0 atIndex:0];
    [encoder setVertexBytes:corners length:sizeof(corners) atIndex:1];
    [encoder setFragmentTexture:image atIndex:0];
    [encoder setFragmentSamplerState:m_clampSampler atIndex:0];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:m_meshIndexCount
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:m_meshIndices
                 indexBufferOffset:0];
}

// The main waveform's settings for this frame: per-frame variables, plus the values MilkDrop
// only reads from the preset file.
- (MainWaveSettings)mainWaveSettings:(const AudioLevels&)levels {
    PresetPlayer& p = *m_player;
    auto on = [&](const char* name) { return std::fabs(p.frameValue(name)) > 0.00001; };
    MainWaveSettings s;
    s.mode = static_cast<int>(p.frameValue("wave_mode"));
    s.x = static_cast<float>(p.frameValue("wave_x"));
    s.y = static_cast<float>(p.frameValue("wave_y"));
    s.mystery = static_cast<float>(p.frameValue("wave_mystery"));
    s.alpha = static_cast<float>(p.frameValue("wave_a"));
    s.r = static_cast<float>(p.frameValue("wave_r"));
    s.g = static_cast<float>(p.frameValue("wave_g"));
    s.b = static_cast<float>(p.frameValue("wave_b"));
    s.brighten = on("wave_brighten");
    s.dots = on("wave_usedots");
    s.thick = on("wave_thick");
    s.additive = on("wave_additive");
    s.modAlphaByVolume = p.baseValue("bmodwavealphabyvolume", 0.0) != 0.0;
    s.modAlphaStart = static_cast<float>(p.baseValue("fmodwavealphastart", 0.75));
    s.modAlphaEnd = static_cast<float>(p.baseValue("fmodwavealphaend", 0.95));
    s.scale = static_cast<float>(p.baseValue("wave_scale", 1.0));
    s.smoothing = static_cast<float>(p.baseValue("wave_smoothing", 0.75));
    s.time = m_time;
    s.vol = (levels.bass + levels.mid + levels.treb) / 3.0f;
    s.treb = levels.treb;
    return s;
}

// MilkDrop's outer border runs in from the screen edge by ob_size (in clip units, where the
// screen is 2 across), and the inner border continues in from there by ib_size.
// Where the warp samples from at (u, v), by bilinear interpolation of this frame's mesh.
- (void)warpSourceAt:(float)u v:(float)v outU:(float&)su outV:(float&)sv {
    const float gx = std::clamp(u, 0.0f, 1.0f) * (MESH_X - 1);
    const float gy = std::clamp(v, 0.0f, 1.0f) * (MESH_Y - 1);
    const int i0 = std::min(static_cast<int>(gx), MESH_X - 2);
    const int j0 = std::min(static_cast<int>(gy), MESH_Y - 2);
    const float fx = gx - i0, fy = gy - j0;
    const MeshUv& a = m_mesh[j0 * MESH_X + i0];
    const MeshUv& b = m_mesh[j0 * MESH_X + i0 + 1];
    const MeshUv& c = m_mesh[(j0 + 1) * MESH_X + i0];
    const MeshUv& d = m_mesh[(j0 + 1) * MESH_X + i0 + 1];
    su = (a.u * (1 - fx) + b.u * fx) * (1 - fy) + (c.u * (1 - fx) + d.u * fx) * fy;
    sv = (a.v * (1 - fx) + b.v * fx) * (1 - fy) + (c.v * (1 - fx) + d.v * fx) * fy;
}

// MilkDrop's motion vectors: a grid of mv_x by mv_y short lines, each from a grid point towards
// where the warp pulls that point from, scaled by mv_l. They are drawn onto the previous frame
// before it is warped, so they smear along the flow.
- (void)encodeMotionVectors:(id<MTLCommandBuffer>)commands onto:(id<MTLTexture>)image {
    auto value = [&](const char* name) { return static_cast<float>(m_player->frameValue(name)); };
    const float alpha = value("mv_a");
    if (alpha < 0.0001f) return;
    int countX = static_cast<int>(value("mv_x"));
    int countY = static_cast<int>(value("mv_y"));
    if (countX <= 0 || countY <= 0) return;
    float divertX = value("mv_x") - countX;
    float divertY = value("mv_y") - countY;
    if (countX > 64) { countX = 64; divertX = 0.0f; }
    if (countY > 48) { countY = 48; divertY = 0.0f; }
    divertX = std::clamp(divertX, 0.0f, 1.0f);
    divertY = std::clamp(divertY, 0.0f, 1.0f);
    const float offsetX = value("mv_dx");
    const float offsetY = value("mv_dy");
    const float length = value("mv_l");
    const int width = static_cast<int>(image.width);
    const int height = static_cast<int>(image.height);
    const float minX = 1.25f / width, minY = 1.25f / height;
    const float minimum = std::sqrt(minX * minX + minY * minY);
    const float colour[4] = {std::clamp(value("mv_r"), 0.0f, 1.0f), std::clamp(value("mv_g"), 0.0f, 1.0f),
                             std::clamp(value("mv_b"), 0.0f, 1.0f), std::min(alpha, 1.0f)};

    const bool none[CUSTOM_COUNT] = {};
    m_vectors.begin(width, height, none);
    for (int y = 0; y < countY; ++y) {
        const float v = (y + 0.25f) / (countY + divertY + 0.25f - 1.0f) - offsetY;
        if (v <= 0.0001f || v >= 0.9999f) continue;
        for (int x = 0; x < countX; ++x) {
            const float u = (x + 0.25f) / (countX + divertX + 0.25f - 1.0f) + offsetX;
            if (u <= 0.0001f || u >= 0.9999f) continue;
            float su, sv;
            [self warpSourceAt:u v:v outU:su outV:sv];
            float dx = (su - u) * length, dy = (sv - v) * length;
            const float len = std::hypot(dx, dy);
            if (len <= minimum) {
                if (len > 1e-8f) {
                    dx *= minimum / len;
                    dy *= minimum / len;
                } else {
                    dx = dy = minimum;
                }
            }
            m_vectors.addTextureLine(u, v, u + dx, v + dy, colour);
        }
    }
    const std::vector<CustomVertex>& verts = m_vectors.vertices();
    if (verts.empty()) return;
    id<MTLBuffer> buffer = [m_device newBufferWithBytes:verts.data()
                                                 length:verts.size() * sizeof(CustomVertex)
                                                options:MTLResourceStorageModeShared];
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = image;
    pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
    const float viewport[2] = {static_cast<float>(width), static_cast<float>(height)};
    [encoder setRenderPipelineState:m_customPipelines[0][0]];
    [encoder setVertexBuffer:buffer offset:0 atIndex:0];
    [encoder setVertexBytes:viewport length:sizeof(viewport) atIndex:1];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:verts.size()];
    [encoder endEncoding];
}

- (void)addBorders {
    auto value = [&](const char* name) { return static_cast<float>(m_player->frameValue(name)); };
    auto colour = [&](const char* prefix, float out[4]) {
        const char* channels[4] = {"_r", "_g", "_b", "_a"};
        for (int i = 0; i < 4; ++i) out[i] = std::clamp(value((std::string(prefix) + channels[i]).c_str()), 0.0f, 1.0f);
    };
    const float outerSize = value("ob_size");
    const float innerSize = value("ib_size");
    float outer[4], inner[4];
    colour("ob", outer);
    colour("ib", inner);
    m_custom.addBorder(1.0f, outerSize, outer);
    m_custom.addBorder(1.0f - outerSize, innerSize, inner);
}

// Runs the preset's custom shapes and waves and draws them over the warped image, then the main
// waveform and the borders: shapes, custom waves, main wave, borders, as in MilkDrop.
- (void)encodeCustom:(id<MTLRenderCommandEncoder>)encoder
               front:(id<MTLTexture>)front
              levels:(const AudioLevels&)levels
               width:(int)width
              height:(int)height {
    m_player->runCustomShapes(m_shapeInstances);
    buildMilkAudioFrame(m_historyL.data(), m_historyR.data(), STEREO_HISTORY, m_milkAudio);
    m_player->runCustomWaves(m_milkAudio, m_customWaves);
    buildMainWave([self mainWaveSettings:levels], m_milkAudio, width, height, m_mainWave);

    bool imageShapes[CUSTOM_COUNT];
    id<MTLTexture> shapeTextures[CUSTOM_COUNT];
    for (int i = 0; i < CUSTOM_COUNT; ++i) {
        std::string image = m_player->shapeImage(i);
        const size_t slash = image.find_last_of("/\\");
        if (slash != std::string::npos) image = image.substr(slash + 1);
        const size_t dot = image.find_last_of('.');
        if (dot != std::string::npos) image = image.substr(0, dot);
        std::transform(image.begin(), image.end(), image.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        imageShapes[i] = !image.empty();
        shapeTextures[i] = imageShapes[i] ? m_bank->texture(image, m_presetSeed) : front;
    }
    m_custom.begin(width, height, imageShapes);
    for (const CustomShapeInstance& shape : m_shapeInstances) m_custom.addShape(shape);
    for (const CustomWaveOutput& wave : m_customWaves) m_custom.addWave(wave);
    m_custom.addMainWave(m_mainWave);
    if (m_player->frameValue("darken_center") > 0.0) m_custom.addDarkenCentre();
    [self addBorders];
    const std::vector<CustomVertex>& verts = m_custom.vertices();
    if (verts.empty()) return;

    id<MTLBuffer> buffer = [m_device newBufferWithBytes:verts.data()
                                                 length:verts.size() * sizeof(CustomVertex)
                                                options:MTLResourceStorageModeShared];
    const float viewport[2] = {static_cast<float>(width), static_cast<float>(height)};
    [encoder setVertexBuffer:buffer offset:0 atIndex:0];
    [encoder setVertexBytes:viewport length:sizeof(viewport) atIndex:1];
    for (const CustomBatch& batch : m_custom.batches()) {
        if (batch.count == 0) continue;
        [encoder setRenderPipelineState:m_customPipelines[batch.textured ? 1 : 0][batch.additive ? 1 : 0]];
        if (batch.textured && batch.shape >= 0) {
            [encoder setFragmentTexture:shapeTextures[batch.shape] atIndex:0];
            [encoder setFragmentSamplerState:m_repeatSampler atIndex:0];
        }
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:batch.first vertexCount:batch.count];
    }
}

// The window draws at its size in points, which is what a MilkDrop window of the same physical
// size would have rendered at on a non-Retina screen. The composite scales it to the drawable.
- (void)drawInMTKView:(MTKView*)view {
    const NSInteger wantFps = m_player ? PRESET_FPS : MOTION_FPS;
    if (view.preferredFramesPerSecond != wantFps) view.preferredFramesPerSecond = wantFps;

    const double now = CACurrentMediaTime();
    const float dt = m_lastFrame == 0.0 ? 1.0f / 60.0f : std::clamp(static_cast<float>(now - m_lastFrame), 1.0f / 240.0f, 1.0f / 20.0f);
    m_lastFrame = now;

    id<MTLCommandBuffer> commands = [m_queue commandBuffer];
    MTLRenderPassDescriptor* screenPass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    [self encodeFrame:dt internalSize:view.bounds.size commands:commands screenPass:(drawable ? screenPass : nil)];
    if (drawable != nil && screenPass != nil) [commands presentDrawable:drawable];
    [commands commit];
}

- (void)renderFrame:(float)dt internalSize:(CGSize)size target:(id<MTLTexture>)target commands:(id<MTLCommandBuffer>)commands {
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    [self encodeFrame:dt internalSize:size commands:commands screenPass:pass];
}

// One frame: equations, warp, custom shapes and waves, the main wave, a clamp to MilkDrop's 0..1
// range, blur levels, then the composite into `screenPass` when there is one.
- (void)encodeFrame:(float)dt
       internalSize:(CGSize)internalSize
           commands:(id<MTLCommandBuffer>)commands
         screenPass:(nullable MTLRenderPassDescriptor*)screenPass {
    const bool presetMode = m_player != nullptr;
    m_time += dt;
    // Presets step once per frame, as in MilkDrop. The built-in motions follow the clock.
    const float frameScale = presetMode ? 1.0f : dt * 60.0f;

    const AudioLevels levels = [self advanceAudio:dt frameScale:frameScale];
    [self ensureTextures:internalSize];
    const int width = static_cast<int>(m_textures[0].width);
    const int height = static_cast<int>(m_textures[0].height);

    FrameVars f;
    float gain = m_params.gammaAdj;
    if (presetMode) {
        f = m_player->runFrame(levels, m_time, 1.0f / dt, width, height);
        gain = m_player->gamma();
    } else {
        f = frameFromParams(m_params, m_time);
        applyMotion(m_params.motion, f, levels, m_time);
        clampFrame(f);
    }
    const PerVertexFn& perVertex = presetMode ? m_player->perVertex() : perVertexFor(m_params.motion);
    [self buildWarpVertices:f perVertex:perVertex levels:levels width:width height:height frameScale:frameScale];
    const float halfWidth = (f.waveThick ? 1.0f : 0.5f) * std::min(width, height) / REFERENCE_HEIGHT;
    [self buildWaveVertices:f halfWidth:halfWidth width:width height:height];

    PresetGpu* gpu = m_gpu.get();
    ShaderFrameValues values;
    if (gpu != nullptr) values = [self shaderValues:levels width:width height:height fps:1.0f / dt];

    id<MTLTexture> front = m_textures[m_front];
    id<MTLTexture> back = m_textures[1 - m_front];

    if (m_needsClear) {
        MTLRenderPassDescriptor* clearPass = [MTLRenderPassDescriptor renderPassDescriptor];
        clearPass.colorAttachments[0].texture = front;
        clearPass.colorAttachments[0].loadAction = MTLLoadActionClear;
        clearPass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        clearPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> clearEncoder = [commands renderCommandEncoderWithDescriptor:clearPass];
        if (m_seedNoise) {
            [clearEncoder setRenderPipelineState:m_noisePipeline];
            [clearEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            m_seedNoise = false;
        }
        [clearEncoder endEncoding];
        for (int level = 0; level < 3; ++level) {
            clearPass.colorAttachments[0].texture = m_blur[level];
            [[commands renderCommandEncoderWithDescriptor:clearPass] endEncoding];
        }
        m_needsClear = false;
    }

    if (presetMode) [self encodeMotionVectors:commands onto:front];

    id<MTLBuffer> gridBuffer = [m_device newBufferWithBytes:m_warpVerts.data()
                                                     length:m_warpVerts.size() * sizeof(WarpVertex)
                                                    options:MTLResourceStorageModeShared];

    MTLRenderPassDescriptor* feedbackPass = [MTLRenderPassDescriptor renderPassDescriptor];
    feedbackPass.colorAttachments[0].texture = back;
    feedbackPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    feedbackPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:feedbackPass];

    if (gpu != nullptr && gpu->hasWarp()) {
        gpu->bindWarp(encoder, front, m_blur, f.wrap, values);
    } else {
        const float decay = std::pow(f.decay, frameScale);
        [encoder setRenderPipelineState:m_warpPipeline];
        [encoder setFragmentTexture:front atIndex:0];
        [encoder setFragmentSamplerState:(f.wrap ? m_repeatSampler : m_clampSampler) atIndex:0];
        [encoder setFragmentBytes:&decay length:sizeof(decay) atIndex:0];
    }
    [encoder setVertexBuffer:gridBuffer offset:0 atIndex:0];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:m_meshIndexCount
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:m_meshIndices
                 indexBufferOffset:0];

    if (presetMode) [self encodeCustom:encoder front:front levels:levels width:width height:height];

    if (!presetMode && !m_waveVerts.empty()) {
        WaveUniforms uniforms{{f.waveR * f.waveA, f.waveG * f.waveA, f.waveB * f.waveA, 1.0f},
                              {static_cast<float>(width), static_cast<float>(height)},
                              halfWidth,
                              0.0f};
        id<MTLBuffer> waveBuffer = [m_device newBufferWithBytes:m_waveVerts.data()
                                                         length:m_waveVerts.size() * sizeof(WaveVertex)
                                                        options:MTLResourceStorageModeShared];
        [encoder setRenderPipelineState:m_wavePipeline];
        [encoder setVertexBuffer:waveBuffer offset:0 atIndex:0];
        [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
        [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:m_waveVerts.size()];
    }
    // MilkDrop keeps its image in 8 bits per channel, so every frame ends inside 0..1.
    [encoder setRenderPipelineState:m_clampPipeline];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];

    if (gpu != nullptr && gpu->blurLevels() > 0) [self encodeBlur:commands from:back levels:gpu->blurLevels()];

    if (screenPass != nil) {
        id<MTLRenderCommandEncoder> composite = [commands renderCommandEncoderWithDescriptor:screenPass];
        if (gpu != nullptr && gpu->hasComp()) {
            float corners[16];
            [self hueCorners:corners];
            id<MTLBuffer> compGrid = [m_device newBufferWithBytes:m_gridBase.data()
                                                           length:m_gridBase.size() * sizeof(WarpVertex)
                                                          options:MTLResourceStorageModeShared];
            gpu->bindComp(composite, back, m_blur, f.wrap, values);
            [composite setVertexBuffer:compGrid offset:0 atIndex:0];
            [composite setVertexBytes:corners length:sizeof(corners) atIndex:1];
            [composite drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                  indexCount:m_meshIndexCount
                                   indexType:MTLIndexTypeUInt16
                                 indexBuffer:m_meshIndices
                           indexBufferOffset:0];
        } else if (presetMode && m_compVersion == 0) {
            [self encodeClassicComposite:composite image:back];
        } else {
            // A preset with a comp shader version but no comp code gets a plain copy.
            const float copyGain = presetMode ? 1.0f : gain;
            [composite setRenderPipelineState:m_compositePipeline];
            [composite setFragmentTexture:back atIndex:0];
            [composite setFragmentSamplerState:m_clampSampler atIndex:0];
            [composite setFragmentBytes:&copyGain length:sizeof(copyGain) atIndex:0];
            [composite drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        }
        [composite endEncoding];
    }
    m_front = 1 - m_front;
    ++m_presetFrame;
}

@end
