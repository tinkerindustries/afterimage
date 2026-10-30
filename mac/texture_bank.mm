#import "texture_bank.h"

#import <MetalKit/MetalKit.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace Milk {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

// MilkDrop's noise textures. Each texel starts as four random numbers in [RANGE/2, RANGE/2 + RANGE)
// packed into a 32-bit colour without masking, so their high bits spill into the next channel, as
// they do in MilkDrop. Textures with a zoom above 1 keep that noise only on a lattice and fill in
// the rest by cubic interpolation, one axis at a time.
struct NoiseRandom {
    uint32_t state;
    // The range of MSVC's rand().
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (state >> 8) & 0x7fff;
    }
};

float cubicInterpolate(float y0, float y1, float y2, float y3, float t) {
    const float t2 = t * t;
    const float a0 = y3 - y2 - y0 + y1;
    const float a1 = y0 - y1 - a0;
    const float a2 = y2 - y0;
    const float a3 = y1;
    return a0 * t * t2 + a1 * t2 + a2 * t + a3;
}

// Cubic interpolation of each byte of a colour, clamped to 0..1 and truncated.
uint32_t cubicColour(uint32_t y0, uint32_t y1, uint32_t y2, uint32_t y3, float t) {
    uint32_t out = 0;
    for (uint32_t shift = 0; shift < 32; shift += 8) {
        float f = cubicInterpolate(((y0 >> shift) & 0xFF) / 255.0f, ((y1 >> shift) & 0xFF) / 255.0f,
                                   ((y2 >> shift) & 0xFF) / 255.0f, ((y3 >> shift) & 0xFF) / 255.0f, t);
        f = std::clamp(f, 0.0f, 1.0f);
        out |= static_cast<uint32_t>(f * 255.0f) << shift;
    }
    return out;
}

uint32_t noiseTexel(NoiseRandom& rng, int range) {
    auto channel = [&] { return static_cast<uint32_t>(rng.next() % range + range / 2); };
    const uint32_t a = channel(), r = channel(), g = channel(), b = channel();
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// size^dims texels, x fastest.
std::vector<uint32_t> makeNoise(int dims, int size, int zoom, uint32_t seed) {
    NoiseRandom rng{seed * 2654435761u + 0x9e3779b9u};
    if (rng.state == 0) rng.state = 1;
    const int range = zoom > 1 ? 216 : 256;
    const int lines = dims == 3 ? size * size : size;
    std::vector<uint32_t> d(static_cast<size_t>(lines) * size);
    for (int line = 0; line < lines; ++line) {
        uint32_t* row = d.data() + static_cast<size_t>(line) * size;
        for (int x = 0; x < size; ++x) row[x] = noiseTexel(rng, range);
        for (int x = 0; x < size; ++x) {
            const int x1 = static_cast<int>(rng.next() % size);
            const int x2 = static_cast<int>(rng.next() % size);
            std::swap(row[x1], row[x2]);
        }
    }
    if (zoom <= 1) return d;
    const size_t slice = static_cast<size_t>(size) * size;
    auto at = [&](int x, int y, int z) -> uint32_t& { return d[z * slice + static_cast<size_t>(y) * size + x]; };
    const int depth = dims == 3 ? size : 1;
    auto wrap = [&](int v) { return ((v % size) + size) % size; };
    for (int z = 0; z < depth; z += zoom) {
        for (int y = 0; y < size; y += zoom) {
            for (int x = 0; x < size; ++x) {
                if (x % zoom == 0) continue;
                const int base = (x / zoom) * zoom + size;
                at(x, y, z) = cubicColour(at(wrap(base - zoom), y, z), at(wrap(base), y, z), at(wrap(base + zoom), y, z),
                                          at(wrap(base + zoom * 2), y, z), (x % zoom) / static_cast<float>(zoom));
            }
        }
    }
    for (int z = 0; z < depth; z += zoom) {
        for (int x = 0; x < size; ++x) {
            for (int y = 0; y < size; ++y) {
                if (y % zoom == 0) continue;
                const int base = (y / zoom) * zoom + size;
                at(x, y, z) = cubicColour(at(x, wrap(base - zoom), z), at(x, wrap(base), z), at(x, wrap(base + zoom), z),
                                          at(x, wrap(base + zoom * 2), z), (y % zoom) / static_cast<float>(zoom));
            }
        }
    }
    if (dims == 3) {
        for (int x = 0; x < size; ++x) {
            for (int y = 0; y < size; ++y) {
                for (int z = 0; z < size; ++z) {
                    if (z % zoom == 0) continue;
                    const int base = (z / zoom) * zoom + size;
                    at(x, y, z) = cubicColour(at(x, y, wrap(base - zoom)), at(x, y, wrap(base)), at(x, y, wrap(base + zoom)),
                                              at(x, y, wrap(base + zoom * 2)), (z % zoom) / static_cast<float>(zoom));
                }
            }
        }
    }
    return d;
}

// A8R8G8B8 words to RGBA bytes.
std::vector<uint8_t> toRgba(const std::vector<uint32_t>& d) {
    std::vector<uint8_t> out(d.size() * 4);
    for (size_t i = 0; i < d.size(); ++i) {
        out[i * 4 + 0] = static_cast<uint8_t>(d[i] >> 16);
        out[i * 4 + 1] = static_cast<uint8_t>(d[i] >> 8);
        out[i * 4 + 2] = static_cast<uint8_t>(d[i]);
        out[i * 4 + 3] = static_cast<uint8_t>(d[i] >> 24);
    }
    return out;
}

} // namespace

TextureBank::TextureBank(id<MTLDevice> device, const std::string& textureDir) : m_device(device), m_dir(textureDir) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(textureDir, ec)) {
        const std::string ext = lower(entry.path().extension().string());
        if (ext != ".jpg" && ext != ".jpeg" && ext != ".png" && ext != ".tga" && ext != ".bmp") continue;
        m_files[lower(entry.path().stem().string())] = entry.path().filename().string();
    }
    for (const auto& [name, file] : m_files) m_names.push_back(name);

    for (int f = 0; f < 2; ++f) {
        for (int w = 0; w < 2; ++w) {
            MTLSamplerDescriptor* desc = [MTLSamplerDescriptor new];
            desc.minFilter = f ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
            desc.magFilter = desc.minFilter;
            desc.mipFilter = f ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
            const MTLSamplerAddressMode mode = w ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
            desc.sAddressMode = mode;
            desc.tAddressMode = mode;
            desc.rAddressMode = mode;
            m_samplers[f][w] = [device newSamplerStateWithDescriptor:desc];
        }
    }
}

TextureBank::Lookup TextureBank::parse(const std::string& samplerName) {
    Lookup out;
    out.name = lower(samplerName);
    if (out.name.rfind("sampler_", 0) == 0) out.name = out.name.substr(8);
    static const struct { const char* prefix; bool filtered; bool wrap; } PREFIXES[] = {
        {"fw_", true, true}, {"fc_", true, false}, {"pw_", false, true}, {"pc_", false, false},
        {"wf_", true, true}, {"cf_", true, false}, {"wp_", false, true}, {"cp_", false, false},
    };
    for (const auto& p : PREFIXES) {
        if (out.name.rfind(p.prefix, 0) == 0) {
            out.name = out.name.substr(3);
            out.filtered = p.filtered;
            out.wrap = p.wrap;
            out.prefixed = true;
            break;
        }
    }
    return out;
}

id<MTLSamplerState> TextureBank::sampler(bool filtered, bool wrap) const { return m_samplers[filtered ? 1 : 0][wrap ? 1 : 0]; }

bool TextureBank::has(const std::string& name) const {
    static const char* BUILT_IN[] = {"noise_lq", "noise_lq_lite", "noise_mq", "noise_hq", "noisevol_lq", "noisevol_hq"};
    for (const char* n : BUILT_IN) {
        if (name == n) return true;
    }
    if (name.rfind("rand", 0) == 0 && name.size() >= 6 && std::isdigit(static_cast<unsigned char>(name[4])) &&
        std::isdigit(static_cast<unsigned char>(name[5])) && (name.size() == 6 || name[6] == '_')) {
        return !m_names.empty();
    }
    return m_files.count(name) > 0;
}

id<MTLTexture> TextureBank::texture(const std::string& name, uint32_t presetSeed) {
    // Random picks vary per preset, so they skip the cache.
    if (name.rfind("rand", 0) == 0 && name.size() >= 6 && std::isdigit(static_cast<unsigned char>(name[4])) &&
        std::isdigit(static_cast<unsigned char>(name[5]))) {
        const std::string filter = name.size() > 7 && name[6] == '_' ? name.substr(7) : "";
        std::vector<std::string> pool;
        for (const std::string& n : m_names) {
            if (filter.empty() || n.rfind(filter, 0) == 0) pool.push_back(n);
        }
        if (pool.empty()) pool = m_names;
        if (!pool.empty()) {
            const uint32_t pick = hash32(presetSeed * 31u + static_cast<uint32_t>(std::stoi(name.substr(4, 2))));
            return texture(pool[pick % pool.size()], presetSeed);
        }
    }

    auto cached = m_cache.find(name);
    if (cached != m_cache.end()) return cached->second;

    id<MTLTexture> tex = nil;
    if (name == "noise_lq") tex = noise2D(256, 1, 1);
    else if (name == "noise_lq_lite") tex = noise2D(32, 1, 2);
    else if (name == "noise_mq") tex = noise2D(256, 4, 3);
    else if (name == "noise_hq") tex = noise2D(256, 8, 4);
    else if (name == "noisevol_lq") tex = noise3D(32, 1, 5);
    else if (name == "noisevol_hq") tex = noise3D(32, 4, 6);
    else {
        auto file = m_files.find(name);
        if (file != m_files.end()) tex = load(file->second);
        if (tex == nil) {
            NSLog(@"no texture named %s, using noise", name.c_str());
            tex = texture("noise_hq", presetSeed);
        }
    }
    m_cache[name] = tex;
    return tex;
}

// Random RGBA values on a lattice every `zoom` texels, smoothly interpolated. The lattice wraps,
// so the texture tiles.
id<MTLTexture> TextureBank::noise2D(int size, int zoom, uint32_t seed) {
    const std::vector<uint8_t> pixels = toRgba(makeNoise(2, size, zoom, seed));
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                    width:size
                                                                                   height:size
                                                                                mipmapped:YES];
    desc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> tex = [m_device newTextureWithDescriptor:desc];
    [tex replaceRegion:MTLRegionMake2D(0, 0, size, size) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:size * 4];
    // MilkDrop's 2D noise textures have a full mip chain.
    id<MTLCommandBuffer> commands = [[m_device newCommandQueue] commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
    [blit generateMipmapsForTexture:tex];
    [blit endEncoding];
    [commands commit];
    [commands waitUntilCompleted];
    return tex;
}

id<MTLTexture> TextureBank::noise3D(int size, int zoom, uint32_t seed) {
    const std::vector<uint8_t> pixels = toRgba(makeNoise(3, size, zoom, seed));
    MTLTextureDescriptor* desc = [MTLTextureDescriptor new];
    desc.textureType = MTLTextureType3D;
    desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    desc.width = size;
    desc.height = size;
    desc.depth = size;
    desc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> tex = [m_device newTextureWithDescriptor:desc];
    [tex replaceRegion:MTLRegionMake3D(0, 0, 0, size, size, size)
           mipmapLevel:0
                 slice:0
             withBytes:pixels.data()
           bytesPerRow:size * 4
         bytesPerImage:static_cast<NSUInteger>(size) * size * 4];
    return tex;
}

id<MTLTexture> TextureBank::load(const std::string& file) {
    MTKTextureLoader* loader = [[MTKTextureLoader alloc] initWithDevice:m_device];
    NSURL* url = [NSURL fileURLWithPath:@((m_dir + "/" + file).c_str())];
    NSError* error = nil;
    id<MTLTexture> tex = [loader newTextureWithContentsOfURL:url
                                                     options:@{
                                                         MTKTextureLoaderOptionSRGB: @NO,
                                                         MTKTextureLoaderOptionGenerateMipmaps: @YES,
                                                         MTKTextureLoaderOptionTextureStorageMode: @(MTLStorageModePrivate),
                                                     }
                                                       error:&error];
    if (tex == nil) NSLog(@"loading %s failed: %@", file.c_str(), error.localizedDescription);
    return tex;
}

} // namespace Milk
