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
    const int cells = std::max(1, size / zoom);
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    auto lattice = [&](int x, int y, int c) {
        x = ((x % cells) + cells) % cells;
        y = ((y % cells) + cells) % cells;
        return (hash32(seed * 0x9e3779b9u + static_cast<uint32_t>((y * cells + x) * 4 + c)) & 0xffff) / 65535.0f;
    };
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int cx = x / zoom, cy = y / zoom;
            const float tx = smooth(static_cast<float>(x % zoom) / zoom);
            const float ty = smooth(static_cast<float>(y % zoom) / zoom);
            for (int c = 0; c < 4; ++c) {
                const float a = lattice(cx, cy, c) + (lattice(cx + 1, cy, c) - lattice(cx, cy, c)) * tx;
                const float b = lattice(cx, cy + 1, c) + (lattice(cx + 1, cy + 1, c) - lattice(cx, cy + 1, c)) * tx;
                pixels[(static_cast<size_t>(y) * size + x) * 4 + c] = static_cast<uint8_t>(std::lround((a + (b - a) * ty) * 255.0f));
            }
        }
    }
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                    width:size
                                                                                   height:size
                                                                                mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> tex = [m_device newTextureWithDescriptor:desc];
    [tex replaceRegion:MTLRegionMake2D(0, 0, size, size) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:size * 4];
    return tex;
}

id<MTLTexture> TextureBank::noise3D(int size, int zoom, uint32_t seed) {
    const int cells = std::max(1, size / zoom);
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * size * 4);
    auto lattice = [&](int x, int y, int z, int c) {
        x = ((x % cells) + cells) % cells;
        y = ((y % cells) + cells) % cells;
        z = ((z % cells) + cells) % cells;
        return (hash32(seed * 0x9e3779b9u + static_cast<uint32_t>(((z * cells + y) * cells + x) * 4 + c)) & 0xffff) / 65535.0f;
    };
    for (int z = 0; z < size; ++z) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const int cx = x / zoom, cy = y / zoom, cz = z / zoom;
                const float tx = smooth(static_cast<float>(x % zoom) / zoom);
                const float ty = smooth(static_cast<float>(y % zoom) / zoom);
                const float tz = smooth(static_cast<float>(z % zoom) / zoom);
                for (int c = 0; c < 4; ++c) {
                    float v[2];
                    for (int k = 0; k < 2; ++k) {
                        const float a = lattice(cx, cy, cz + k, c) + (lattice(cx + 1, cy, cz + k, c) - lattice(cx, cy, cz + k, c)) * tx;
                        const float b = lattice(cx, cy + 1, cz + k, c) + (lattice(cx + 1, cy + 1, cz + k, c) - lattice(cx, cy + 1, cz + k, c)) * tx;
                        v[k] = a + (b - a) * ty;
                    }
                    const float value = v[0] + (v[1] - v[0]) * tz;
                    pixels[((static_cast<size_t>(z) * size + y) * size + x) * 4 + c] = static_cast<uint8_t>(std::lround(value * 255.0f));
                }
            }
        }
    }
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
