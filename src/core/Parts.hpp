#pragma once
#include "DecoPlan.hpp"

namespace aplus {

// A decoration object the engine may place.
struct PartDef {
    int id;          // GD object ID
    Slot slot;       // where it goes
    char const* tag; // style tag the AI can boost (e.g. "cloud")
    float weight;    // base chance inside its slot
    bool verified;   // ID confirmed from public GD object-ID references
    float scaleLo, scaleHi;
};

// Built-in catalog. IDs marked verified come from documented GD object ID lists
// (e.g. 15-17 pulse poles, 18-21 transparent spikes, 41 tall chain, 48/49 clouds,
// 50-54 + 60 pulse shapes, 123-128 bushes, 406-408 + 414 grass, 1764 dot).
// Unverified IDs are only used if the game itself reports them as decoration.
std::vector<PartDef> const& builtinParts();

// A part after the engine resolved its sprite size in-game.
struct PartInfo {
    int id = 0;
    Slot slot = Slot::Floating;
    std::string tag;
    float weight = 1.f;
    float w = 30.f, h = 30.f; // sprite size at scale 1 (GD units)
    float scaleLo = 1.f, scaleHi = 1.f;
};

// Keyword -> slot/tag rules used to discover extra deco objects from sprite names.
struct ExtraRule {
    char const* keyword;
    Slot slot;
    char const* tag;
};
std::vector<ExtraRule> const& extraRules();

} // namespace aplus
