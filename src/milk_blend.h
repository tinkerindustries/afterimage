#pragma once

#include <random>
#include <vector>

namespace Milk {

// How a blend between two presets moves across the warp mesh. Each vertex's mix toward the new
// preset is clamp(a * progress + c, 0, 1), so a pattern is a set of (a, c) per vertex.
struct BlendPattern {
    std::vector<float> a;
    std::vector<float> c;
};

// MilkDrop's blend patterns for a mesh of (gridX + 1) x (gridY + 1) vertices, row 0 at the top:
// 1 a directional wipe, 2 plasma, 3 radial. Milkwave 4.1 adds 0 (uniform) and 4 to 18. `aspectX`
// and `aspectY` are the mesh's aspect multipliers (1 on the long side).
void makeBlendPattern(int type, int gridX, int gridY, float aspectX, float aspectY, std::mt19937& rng, BlendPattern& out);

// MilkDrop's cosine ease from 0 to 1.
float cosineInterp(float x);

} // namespace Milk
