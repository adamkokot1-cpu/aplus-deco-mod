#pragma once
// A+ Deco Mod — core data types.
// Everything in src/core is plain C++ (no Geode / cocos2d), so it can be unit-tested
// on any machine with `tests/test_core.cpp`.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aplus {

struct RGB {
    uint8_t r = 255, g = 255, b = 255;
};

// Where a decoration piece lives.
enum class Slot : int {
    Ground = 0,    // standing on top of blocks and on the floor
    Hanging = 1,   // hanging under blocks (chains, icicles...)
    Floating = 2,  // particles floating near the gameplay
    Sky = 3,       // clouds and big soft circles high up
    Structure = 4, // a skyline of non-solid background pillars
};
constexpr int SLOT_COUNT = 5;

// Color roles. The engine maps each role to a real GD color channel.
enum class Role : int {
    Primary = 0,   // ground + hanging deco
    Secondary = 1, // floating particles
    Accent = 2,    // pulse shapes, chains, poles
    Structure = 3, // background pillars
    Sky = 4,       // clouds / bokeh
};
constexpr int ROLE_COUNT = 5;

// The "style plan" the AI produces from the user's description.
// The placement engine turns it into actual objects.
struct DecoPlan {
    std::string theme = "Classic";
    std::string source = "built-in AI";

    RGB colors[ROLE_COUNT] = {
        {0, 200, 255}, {255, 255, 255}, {0, 255, 120}, {0, 40, 90}, {255, 255, 255},
    };
    float opacity[ROLE_COUNT] = {1.f, 0.9f, 1.f, 0.35f, 0.55f};
    bool glow = false; // additive blending on the secondary + accent channels

    // Share of the object budget per slot (normalised by the engine).
    float slotWeight[SLOT_COUNT] = {0.30f, 0.10f, 0.35f, 0.10f, 0.15f};

    // Per-tag multiplier (missing tag = 1, 0 = never use).
    std::map<std::string, float> tagBoost;

    float scaleMul = 1.f;    // global size multiplier
    float spin = 0.2f;       // 0..1, how much random rotation floating deco gets
    float clustering = 0.5f; // 0 = spread out, 1 = hugging the gameplay

    std::vector<std::string> notes; // human readable "what the AI understood"
};

char const* slotName(Slot s);
char const* roleName(Role r);
std::string toHex(RGB c);
bool parseHex(std::string const& text, RGB& out);

// Every tag the engine knows (used by the Claude prompt and the JSON parser).
std::vector<std::string> const& knownTags();

} // namespace aplus
