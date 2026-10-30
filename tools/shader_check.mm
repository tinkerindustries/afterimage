// Translates every preset's warp and comp shaders to Metal and compiles the result on this Mac's
// GPU, then links it against the app's vertex stages. Prints how many got through each step and groups the failures by their first error line.
// `shader_check <dir> -v` lists each failure; `shader_check <file.milk> --dump` prints the MSL.

#include "milk_preset.h"
#include "milk_shader.h"

#import <Metal/Metal.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace Milk;

namespace {

struct Counts {
    int present = 0;
    int translated = 0;
    int compiled = 0;
    int linked = 0;
};

std::string firstErrorLine(const std::string& log) {
    size_t start = 0;
    while (start < log.size()) {
        size_t end = log.find('\n', start);
        if (end == std::string::npos) end = log.size();
        std::string line = log.substr(start, end - start);
        if (line.find("error") != std::string::npos || line.find("ERROR") != std::string::npos) {
            // Drop the "file:line:" location so the same error groups together.
            const size_t pos = line.find("ERROR: ");
            if (pos != std::string::npos) {
                line = line.substr(pos + 7);
                const size_t colon = line.find(": ", line.find(':') + 1);
                if (colon != std::string::npos) line = line.substr(colon + 2);
            }
            const size_t err = line.find("error: ");
            if (err != std::string::npos) line = line.substr(err + 7);
            return line.substr(0, 110);
        }
        start = end + 1;
    }
    return log.substr(0, std::min<size_t>(log.size(), 110));
}

} // namespace

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc < 2) {
            std::fprintf(stderr, "usage: %s <preset dir or file> [-v | --dump]\n", argv[0]);
            return 2;
        }
        const std::string flag = argc > 2 ? argv[2] : "";
        const bool verbose = flag == "-v";
        const bool dump = flag == "--dump";
        std::vector<std::filesystem::path> paths;
        if (std::filesystem::is_regular_file(argv[1])) {
            paths.push_back(argv[1]);
        } else {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[1])) {
                if (entry.is_regular_file() && entry.path().extension() == ".milk") paths.push_back(entry.path());
            }
        }
        std::sort(paths.begin(), paths.end());

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        // Link each shader against the app's vertex stages, so a mismatch in the inputs shows up.
        NSString* appShaders = [NSString stringWithContentsOfFile:@MILK_SOURCE_DIR "/mac/shaders.metal"
                                                         encoding:NSUTF8StringEncoding
                                                            error:nil];
        NSError* appError = nil;
        id<MTLLibrary> appLibrary = appShaders ? [device newLibraryWithSource:appShaders options:nil error:&appError] : nil;
        if (appLibrary == nil) {
            std::fprintf(stderr, "cannot compile mac/shaders.metal: %s\n", appError.localizedDescription.UTF8String);
            return 1;
        }
        id<MTLFunction> vertexStages[2] = {[appLibrary newFunctionWithName:@"presetWarpVertex"],
                                           [appLibrary newFunctionWithName:@"presetCompVertex"]};
        const MTLPixelFormat formats[2] = {MTLPixelFormatRGBA16Float, MTLPixelFormatBGRA8Unorm};
        std::map<std::string, int> linkErrors;
        Counts counts[2];
        std::map<std::string, int> translateErrors;
        std::map<std::string, int> compileErrors;
        int fullyWorking = 0;
        int withShaders = 0;

        for (const auto& path : paths) {
            PresetFile file;
            std::string error;
            if (!loadPresetFile(path.string(), file, error)) continue;
            const std::string* code[2] = {&file.warpShader, &file.compShader};
            bool any = false;
            bool allOk = true;
            for (int s = 0; s < 2; ++s) {
                if (code[s]->find("shader_body") == std::string::npos) continue;
                any = true;
                ++counts[s].present;
                TranslatedShader shader;
                if (!translateShader(s == 0 ? ShaderStage::WARP : ShaderStage::COMP, *code[s], shader, error)) {
                    allOk = false;
                    ++translateErrors[firstErrorLine(error)];
                    if (verbose) std::printf("HLSL %s %s: %s\n", s == 0 ? "warp" : "comp", path.c_str(), firstErrorLine(error).c_str());
                    if (dump) std::printf("%s\n", error.c_str());
                    continue;
                }
                ++counts[s].translated;
                if (dump) {
                    std::printf("==== %s MSL ====\n%s\n", s == 0 ? "warp" : "comp", shader.msl.c_str());
                    std::printf("uniform buffer %u, %zu bytes:", shader.uniformBuffer, shader.uniformSize);
                    for (const auto& [name, offset] : shader.uniformOffsets) std::printf(" %s@%u", name.c_str(), offset);
                    std::printf("\n");
                    for (const auto& b : shader.samplers) {
                        std::printf("sampler %s texture %u sampler %u%s\n", b.name.c_str(), b.texture, b.sampler, b.volume ? " 3D" : "");
                    }
                }
                NSError* nsError = nil;
                id<MTLLibrary> library = [device newLibraryWithSource:@(shader.msl.c_str()) options:nil error:&nsError];
                if (library == nil) {
                    allOk = false;
                    const std::string log = nsError.localizedDescription.UTF8String;
                    ++compileErrors[firstErrorLine(log)];
                    if (verbose) std::printf("MSL  %s %s: %s\n", s == 0 ? "warp" : "comp", path.c_str(), firstErrorLine(log).c_str());
                    if (dump) std::printf("%s\n", log.c_str());
                    continue;
                }
                ++counts[s].compiled;
                bool missingUniform = false;
                for (const std::string& name : shaderHeaderUniforms()) missingUniform |= shader.uniformOffsets.count(name) == 0;
                if (missingUniform) {
                    allOk = false;
                    ++linkErrors["a header uniform is missing from the reflected names"];
                    continue;
                }
                bool badBinding = false;
                for (const auto& b : shader.samplers) badBinding |= b.texture >= 31 || b.sampler >= 16;
                if (badBinding) {
                    allOk = false;
                    ++linkErrors["texture or sampler index out of Metal's range"];
                    continue;
                }
                MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
                desc.vertexFunction = vertexStages[s];
                desc.fragmentFunction = [library newFunctionWithName:@(shader.entryPoint.c_str())];
                desc.colorAttachments[0].pixelFormat = formats[s];
                if ([device newRenderPipelineStateWithDescriptor:desc error:&nsError] == nil) {
                    allOk = false;
                    ++linkErrors[firstErrorLine(nsError.localizedDescription.UTF8String)];
                    continue;
                }
                ++counts[s].linked;
            }
            if (any) {
                ++withShaders;
                if (allOk) ++fullyWorking;
            }
        }

        std::printf("presets: %zu, with shaders: %d, all shaders compiled: %d\n", paths.size(), withShaders, fullyWorking);
        const char* names[2] = {"warp", "comp"};
        for (int s = 0; s < 2; ++s) {
            std::printf("%s: %d present, %d translated, %d compiled as MSL, %d linked\n", names[s], counts[s].present,
                        counts[s].translated, counts[s].compiled, counts[s].linked);
        }
        std::vector<std::pair<int, std::string>> sorted;
        for (const auto& [k, v] : translateErrors) sorted.push_back({v, k});
        std::sort(sorted.rbegin(), sorted.rend());
        std::printf("HLSL errors:\n");
        for (size_t i = 0; i < std::min<size_t>(sorted.size(), 25); ++i) std::printf("  %4d  %s\n", sorted[i].first, sorted[i].second.c_str());
        sorted.clear();
        for (const auto& [k, v] : compileErrors) sorted.push_back({v, k});
        std::sort(sorted.rbegin(), sorted.rend());
        std::printf("MSL errors:\n");
        for (size_t i = 0; i < std::min<size_t>(sorted.size(), 25); ++i) std::printf("  %4d  %s\n", sorted[i].first, sorted[i].second.c_str());
        std::printf("pipeline link errors:\n");
        for (const auto& [k, v] : linkErrors) std::printf("  %4d  %s\n", v, k.c_str());
    }
    return 0;
}
