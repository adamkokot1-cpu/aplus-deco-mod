#include "Parts.hpp"

#include <cctype>
#include <cstdio>

namespace aplus {

char const* slotName(Slot s) {
    switch (s) {
        case Slot::Ground: return "ground";
        case Slot::Hanging: return "hanging";
        case Slot::Floating: return "floating";
        case Slot::Sky: return "sky";
        case Slot::Structure: return "structure";
    }
    return "?";
}

char const* roleName(Role r) {
    switch (r) {
        case Role::Primary: return "primary";
        case Role::Secondary: return "secondary";
        case Role::Accent: return "accent";
        case Role::Structure: return "structure";
        case Role::Sky: return "sky";
    }
    return "?";
}

std::string toHex(RGB c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool parseHex(std::string const& text, RGB& out) {
    std::string s;
    for (char c : text) {
        if (c != '#' && c != ' ') s += c;
    }
    if (s.size() != 6) return false;
    int v[6];
    for (int i = 0; i < 6; ++i) {
        v[i] = hexDigit(s[i]);
        if (v[i] < 0) return false;
    }
    out.r = static_cast<uint8_t>(v[0] * 16 + v[1]);
    out.g = static_cast<uint8_t>(v[2] * 16 + v[3]);
    out.b = static_cast<uint8_t>(v[4] * 16 + v[5]);
    return true;
}

std::vector<std::string> const& knownTags() {
    static std::vector<std::string> tags = {
        "spikes", "bush", "grass", "pole", "chain", "dot", "circle", "ring",
        "heart", "diamond", "star", "note", "cloud", "bokeh", "pillar", "extra",
    };
    return tags;
}

std::vector<PartDef> const& builtinParts() {
    static std::vector<PartDef> parts = {
        // ---- Ground: stands on block tops and the floor ----
        {18, Slot::Ground, "spikes", 0.5f, true, 0.5f, 0.9f},
        {19, Slot::Ground, "spikes", 0.7f, true, 0.6f, 1.0f},
        {20, Slot::Ground, "spikes", 1.0f, true, 0.7f, 1.1f},
        {21, Slot::Ground, "spikes", 1.0f, true, 0.8f, 1.2f},
        {123, Slot::Ground, "bush", 0.6f, true, 0.8f, 1.2f},
        {124, Slot::Ground, "bush", 0.6f, true, 0.8f, 1.2f},
        {125, Slot::Ground, "bush", 0.6f, true, 0.8f, 1.2f},
        {126, Slot::Ground, "bush", 0.6f, true, 0.8f, 1.2f},
        {127, Slot::Ground, "bush", 0.6f, true, 0.8f, 1.2f},
        {128, Slot::Ground, "bush", 0.6f, true, 0.8f, 1.2f},
        {406, Slot::Ground, "grass", 1.0f, true, 0.8f, 1.3f},
        {407, Slot::Ground, "grass", 1.0f, true, 0.8f, 1.3f},
        {408, Slot::Ground, "grass", 1.0f, true, 0.8f, 1.3f},
        {414, Slot::Ground, "grass", 1.0f, true, 0.8f, 1.2f},
        {15, Slot::Ground, "pole", 0.25f, true, 0.7f, 1.0f},
        {16, Slot::Ground, "pole", 0.30f, true, 0.7f, 1.0f},
        {17, Slot::Ground, "pole", 0.35f, true, 0.8f, 1.0f},
        {134, Slot::Ground, "pole", 0.30f, true, 0.7f, 1.0f},

        // ---- Hanging: under blocks (spikes/poles are rotated 180) ----
        {41, Slot::Hanging, "chain", 1.0f, true, 0.6f, 1.0f},
        {19, Slot::Hanging, "spikes", 0.5f, true, 0.6f, 1.0f},
        {20, Slot::Hanging, "spikes", 0.7f, true, 0.7f, 1.1f},
        {21, Slot::Hanging, "spikes", 0.7f, true, 0.8f, 1.2f},
        {16, Slot::Hanging, "pole", 0.25f, true, 0.6f, 0.9f},
        {17, Slot::Hanging, "pole", 0.30f, true, 0.7f, 1.0f},

        // ---- Floating: particles near the gameplay ----
        {1764, Slot::Floating, "dot", 3.0f, true, 0.6f, 1.8f},
        {50, Slot::Floating, "circle", 0.8f, true, 0.4f, 0.9f},
        {51, Slot::Floating, "ring", 0.8f, true, 0.4f, 0.9f},
        {52, Slot::Floating, "heart", 0.4f, true, 0.4f, 0.8f},
        {53, Slot::Floating, "diamond", 0.6f, true, 0.4f, 0.8f},
        {54, Slot::Floating, "star", 0.6f, true, 0.4f, 0.8f},
        {60, Slot::Floating, "note", 0.4f, true, 0.4f, 0.8f},

        // ---- Sky: clouds and big soft "bokeh" circles ----
        {48, Slot::Sky, "cloud", 1.0f, true, 0.9f, 1.8f},
        {49, Slot::Sky, "cloud", 1.0f, true, 0.9f, 1.8f},
        {131, Slot::Sky, "cloud", 1.0f, true, 1.0f, 2.0f},
        {129, Slot::Sky, "cloud", 0.8f, false, 0.9f, 1.6f},
        {130, Slot::Sky, "cloud", 0.8f, false, 0.9f, 1.6f},
        {1764, Slot::Sky, "bokeh", 0.8f, true, 3.0f, 7.0f},
        {50, Slot::Sky, "bokeh", 0.5f, true, 1.5f, 3.5f},
        {51, Slot::Sky, "bokeh", 0.4f, true, 1.5f, 3.0f},

        // ---- Structure: non-solid background pillars (no-touch default block) ----
        {1, Slot::Structure, "pillar", 1.0f, true, 1.0f, 1.0f},
    };
    return parts;
}

std::vector<ExtraRule> const& extraRules() {
    static std::vector<ExtraRule> rules = {
        {"cloud", Slot::Sky, "cloud"},
        {"grass", Slot::Ground, "grass"},
        {"bush", Slot::Ground, "bush"},
        {"plant", Slot::Ground, "bush"},
        {"flower", Slot::Ground, "bush"},
        {"chain", Slot::Hanging, "chain"},
        {"vine", Slot::Hanging, "chain"},
        {"glow", Slot::Floating, "extra"},
        {"light", Slot::Floating, "extra"},
        {"sparkle", Slot::Floating, "star"},
        {"star", Slot::Floating, "star"},
        {"heart", Slot::Floating, "heart"},
        {"bubble", Slot::Floating, "ring"},
        {"circle", Slot::Floating, "circle"},
        {"ring", Slot::Floating, "ring"},
        {"dot", Slot::Floating, "dot"},
    };
    return rules;
}

} // namespace aplus
