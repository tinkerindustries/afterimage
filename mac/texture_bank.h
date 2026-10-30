#pragma once

#import <Metal/Metal.h>

#include <map>
#include <string>
#include <vector>

namespace Milk {

// Textures a preset shader can sample by name: MilkDrop 2's noise textures, generated here, and
// the image textures in presets/textures. Also hands out the sampler state a name's prefix asks
// for (fw_ filtered+wrap, fc_ filtered+clamp, pw_ point+wrap, pc_ point+clamp).
class TextureBank {
public:
    TextureBank(id<MTLDevice> device, const std::string& textureDir);

    struct Lookup {
        std::string name;       // without "sampler_" or the prefix, in the case the shader wrote
        bool filtered = true;
        bool wrap = true;
        bool prefixed = false;  // the name carried fw_, fc_, pw_ or pc_
    };
    static Lookup parse(const std::string& samplerName);

    id<MTLSamplerState> sampler(bool filtered, bool wrap) const;

    // The texture for a name other than main and blur1..3, or nil when MilkDrop would find none.
    // randNN (slots 0 to 15) picks an image whose name starts with the text after "randNN_",
    // seeded per preset. Built-in names match exactly; image files match without regard to case.
    id<MTLTexture> texture(const std::string& name, uint32_t presetSeed);

    // Whether a name is a texture MilkDrop could load: a built-in, a randNN pick, or an image file.
    bool has(const std::string& name) const;

    // What Direct3D 9 samples from an unbound sampler: opaque black.
    id<MTLTexture> black(bool volume);

private:
    id<MTLTexture> noise2D(int size, int zoom, uint32_t seed);
    id<MTLTexture> noise3D(int size, int zoom, uint32_t seed);
    id<MTLTexture> load(const std::string& file);
    std::vector<std::string> randomPool(const std::string& name) const;

    id<MTLDevice> m_device;
    std::string m_dir;
    std::map<std::string, std::string> m_files;  // lower-case name without extension -> file name
    std::vector<std::string> m_names;             // sorted keys of m_files
    std::map<std::string, id<MTLTexture>> m_cache;
    id<MTLSamplerState> m_samplers[2][2];
    id<MTLTexture> m_black[2] = {nil, nil};
};

} // namespace Milk
