#include "DecoBrain.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <set>

namespace aplus {

// ---------------------------------------------------------------- helpers

std::string toLowerAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static uint8_t clampByte(float v) {
    return static_cast<uint8_t>(std::lround(std::clamp(v, 0.f, 255.f)));
}

RGB mixColor(RGB a, RGB b, float t) {
    return {
        clampByte(a.r + (b.r - a.r) * t),
        clampByte(a.g + (b.g - a.g) * t),
        clampByte(a.b + (b.b - a.b) * t),
    };
}

RGB scaleColor(RGB c, float f) {
    return {clampByte(c.r * f), clampByte(c.g * f), clampByte(c.b * f)};
}

RGB shiftHue(RGB c, float degrees) {
    float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
    float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    float d = mx - mn;
    float h = 0.f;
    if (d > 1e-5f) {
        if (mx == r) h = 60.f * std::fmod((g - b) / d, 6.f);
        else if (mx == g) h = 60.f * ((b - r) / d + 2.f);
        else h = 60.f * ((r - g) / d + 4.f);
    }
    float s = mx <= 0.f ? 0.f : d / mx;
    float v = mx;
    h = std::fmod(h + degrees + 720.f, 360.f);
    float C = v * s;
    float X = C * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
    float m = v - C;
    float rr = 0, gg = 0, bb = 0;
    if (h < 60) { rr = C; gg = X; }
    else if (h < 120) { rr = X; gg = C; }
    else if (h < 180) { gg = C; bb = X; }
    else if (h < 240) { gg = X; bb = C; }
    else if (h < 300) { rr = X; bb = C; }
    else { rr = C; bb = X; }
    return {clampByte((rr + m) * 255.f), clampByte((gg + m) * 255.f), clampByte((bb + m) * 255.f)};
}

static std::vector<std::string> tokenize(std::string const& text) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : text) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) {
            cur += static_cast<char>(std::tolower(c));
        } else if (!cur.empty()) {
            out.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// "neon" matches neon/neons/neony..., "cyber*" matches any word starting with cyber.
static bool matchKey(std::string const& word, std::string const& key) {
    if (!key.empty() && key.back() == '*') {
        auto prefix = key.substr(0, key.size() - 1);
        return word.rfind(prefix, 0) == 0;
    }
    if (word == key) return true;
    if (word.size() <= key.size() || word.rfind(key, 0) != 0) return false;
    static std::set<std::string> const suffixes = {
        "s", "es", "y", "ey", "ish", "ing", "ic", "al", "er", "ers", "ed", "ies", "ly", "ness", "ry", "ie",
    };
    return suffixes.count(word.substr(key.size())) > 0;
}

static bool matchAny(std::string const& word, std::initializer_list<char const*> keys) {
    for (auto k : keys) {
        if (matchKey(word, k)) return true;
    }
    return false;
}

// ---------------------------------------------------------------- themes

namespace {

struct Theme {
    char const* name;
    std::vector<char const*> keys;
    RGB colors[ROLE_COUNT]; // primary, secondary, accent, structure, sky
    float slots[SLOT_COUNT]; // ground, hanging, floating, sky, structure
    std::vector<std::pair<char const*, float>> boosts;
    bool glow;
    float spin;
    float clustering;
};

std::vector<Theme> const& themes() {
    static std::vector<Theme> list = {
        {"Neon City",
         {"neon", "cyber*", "synth*", "retro", "vapor*", "tech", "futur*", "digital", "electr*", "laser", "arcade", "city", "hologra*", "matrix"},
         {{0, 255, 240}, {255, 60, 220}, {160, 80, 255}, {25, 10, 50}, {255, 120, 240}},
         {0.15f, 0.05f, 0.40f, 0.10f, 0.30f},
         {{"ring", 2.f}, {"diamond", 1.5f}, {"pole", 2.f}, {"bush", 0.f}, {"grass", 0.f}, {"cloud", 0.3f}, {"bokeh", 1.5f}, {"heart", 0.3f}},
         true, 0.1f, 0.6f},
        {"Enchanted Forest",
         {"nature", "forest", "jungle", "garden", "spring", "tree", "wood", "woods", "leaf", "leaves", "flower", "meadow", "plant", "fairy", "elf", "moss*", "swamp"},
         {{60, 200, 80}, {255, 240, 140}, {150, 255, 120}, {20, 50, 25}, {220, 255, 230}},
         {0.45f, 0.05f, 0.25f, 0.15f, 0.10f},
         {{"bush", 2.5f}, {"grass", 2.5f}, {"spikes", 0.4f}, {"pole", 0.2f}, {"chain", 0.3f}, {"note", 0.f}, {"heart", 0.3f}, {"diamond", 0.3f}, {"ring", 0.4f}},
         false, 0.2f, 0.5f},
        {"Inferno",
         {"hell", "lava", "fire", "flame", "demon", "volcan*", "inferno", "magma", "burn*", "heat", "devil", "blood", "ember", "molten"},
         {{255, 70, 0}, {255, 200, 40}, {200, 0, 0}, {40, 5, 0}, {255, 90, 30}},
         {0.30f, 0.20f, 0.35f, 0.05f, 0.10f},
         {{"spikes", 3.f}, {"chain", 2.f}, {"dot", 2.f}, {"bush", 0.f}, {"grass", 0.f}, {"heart", 0.f}, {"note", 0.f}, {"cloud", 0.2f}, {"bokeh", 1.2f}},
         true, 0.5f, 0.6f},
        {"Frozen Peaks",
         {"ice", "icy", "snow", "winter", "frozen", "frost", "cold", "christmas", "xmas", "glacier", "arctic", "crystal", "blizzard", "freez*"},
         {{170, 230, 255}, {255, 255, 255}, {90, 180, 255}, {30, 60, 100}, {230, 245, 255}},
         {0.20f, 0.20f, 0.40f, 0.15f, 0.05f},
         {{"spikes", 2.5f}, {"diamond", 2.5f}, {"star", 1.5f}, {"dot", 3.f}, {"cloud", 1.5f}, {"bush", 0.2f}, {"note", 0.f}, {"heart", 0.f}},
         false, 0.4f, 0.3f},
        {"Cosmic Void",
         {"space", "galaxy", "cosmic", "cosmos", "nebula", "planet", "moon", "astro*", "universe", "orbit", "night", "alien", "starry", "void"},
         {{120, 80, 255}, {255, 255, 255}, {255, 100, 200}, {15, 10, 40}, {140, 100, 255}},
         {0.05f, 0.0f, 0.55f, 0.30f, 0.10f},
         {{"star", 3.f}, {"dot", 3.f}, {"ring", 2.f}, {"bokeh", 3.f}, {"cloud", 0.f}, {"bush", 0.f}, {"grass", 0.f}, {"chain", 0.f}},
         true, 0.6f, 0.2f},
        {"Haunted",
         {"spook*", "horror", "hallow*", "ghost", "creep*", "scary", "haunt*", "zombi*", "grave*", "witch", "crypt", "curse*", "nightmare", "evil"},
         {{120, 40, 180}, {140, 255, 90}, {255, 120, 0}, {20, 5, 25}, {90, 60, 110}},
         {0.30f, 0.25f, 0.25f, 0.10f, 0.10f},
         {{"chain", 3.f}, {"spikes", 2.5f}, {"pole", 1.5f}, {"dot", 1.5f}, {"heart", 0.f}, {"note", 0.f}, {"star", 0.3f}},
         false, 0.5f, 0.6f},
        {"Deep Ocean",
         {"ocean", "water", "sea", "underwater", "aqua*", "coral", "beach", "fish", "bubble", "tide", "marine", "abyss", "reef"},
         {{0, 160, 200}, {200, 250, 255}, {0, 255, 200}, {0, 30, 60}, {80, 200, 255}},
         {0.30f, 0.05f, 0.45f, 0.10f, 0.10f},
         {{"grass", 2.5f}, {"ring", 3.f}, {"dot", 2.5f}, {"circle", 1.5f}, {"cloud", 0.f}, {"note", 0.f}, {"chain", 0.2f}, {"spikes", 0.5f}},
         false, 0.2f, 0.4f},
        {"Sweet Dream",
         {"love", "cute", "valentin*", "kawaii", "candy", "sweet", "pastel", "dream*", "princess", "cotton", "bubblegum", "fluffy", "adorable"},
         {{255, 140, 200}, {255, 255, 255}, {255, 90, 150}, {120, 60, 110}, {255, 220, 240}},
         {0.20f, 0.05f, 0.45f, 0.25f, 0.05f},
         {{"heart", 4.f}, {"star", 2.f}, {"cloud", 2.5f}, {"circle", 1.5f}, {"spikes", 0.f}, {"chain", 0.f}, {"pole", 0.3f}},
         false, 0.3f, 0.4f},
        {"Party Rave",
         {"music", "party", "disco", "rave", "dance", "edm", "beat", "club", "festival", "dj", "rhythm", "song", "bass", "dubstep", "concert", "techno"},
         {{255, 230, 0}, {0, 255, 255}, {255, 0, 200}, {30, 0, 50}, {255, 255, 255}},
         {0.15f, 0.05f, 0.50f, 0.10f, 0.20f},
         {{"note", 4.f}, {"circle", 2.f}, {"ring", 2.f}, {"star", 2.f}, {"pole", 2.5f}, {"bokeh", 2.f}},
         true, 0.7f, 0.7f},
        {"Golden Desert",
         {"desert", "sand", "egypt*", "sun", "sunny", "dune", "pharaoh", "pyramid", "ancient", "temple", "sahara", "mummy"},
         {{255, 180, 60}, {255, 240, 180}, {255, 120, 30}, {110, 60, 20}, {255, 210, 140}},
         {0.30f, 0.05f, 0.20f, 0.15f, 0.30f},
         {{"diamond", 3.f}, {"pole", 2.f}, {"spikes", 1.5f}, {"bush", 0.3f}, {"grass", 0.3f}, {"cloud", 0.4f}, {"note", 0.f}, {"heart", 0.f}},
         false, 0.1f, 0.4f},
        {"Clean Minimal",
         {"minimal*", "clean", "simple", "modern", "flat", "mono*", "plain", "elegant", "sleek", "geometric"},
         {{255, 255, 255}, {200, 200, 200}, {0, 200, 255}, {40, 40, 40}, {255, 255, 255}},
         {0.10f, 0.05f, 0.35f, 0.15f, 0.35f},
         {{"ring", 2.f}, {"dot", 2.f}, {"circle", 1.5f}, {"bush", 0.f}, {"grass", 0.f}, {"heart", 0.f}, {"note", 0.f}, {"spikes", 0.3f}, {"chain", 0.3f}, {"cloud", 0.2f}},
         false, 0.f, 0.3f},
        {"Dark Dungeon",
         {"castle", "medieval", "dungeon", "prison", "knight", "stone", "cave", "mine", "ruin", "tomb", "catacomb*", "torch", "fortress"},
         {{150, 140, 130}, {255, 180, 80}, {200, 80, 40}, {35, 30, 30}, {120, 110, 100}},
         {0.25f, 0.30f, 0.20f, 0.0f, 0.25f},
         {{"chain", 3.5f}, {"spikes", 2.f}, {"pole", 2.f}, {"dot", 1.2f}, {"heart", 0.f}, {"note", 0.f}, {"cloud", 0.f}, {"star", 0.2f}},
         false, 0.3f, 0.6f},
        {"Industrial",
         {"factory", "industr*", "metal", "steel", "machine*", "robot*", "mechanic*", "gear", "pipe", "steam*", "rust*", "engine"},
         {{180, 180, 190}, {255, 200, 0}, {255, 120, 0}, {40, 40, 45}, {150, 150, 160}},
         {0.20f, 0.25f, 0.20f, 0.05f, 0.30f},
         {{"chain", 3.f}, {"pole", 2.5f}, {"diamond", 1.2f}, {"bush", 0.f}, {"grass", 0.f}, {"heart", 0.f}, {"note", 0.f}, {"cloud", 0.3f}},
         false, 0.1f, 0.5f},
        // Keep Classic last: it is the fallback for empty descriptions.
        {"Classic GD",
         {"classic", "normal", "default", "basic", "gd", "geometry", "original"},
         {{0, 200, 255}, {255, 255, 255}, {0, 255, 120}, {0, 40, 90}, {255, 255, 255}},
         {0.30f, 0.10f, 0.35f, 0.10f, 0.15f},
         {},
         false, 0.2f, 0.5f},
    };
    return list;
}

struct NamedColor {
    char const* name;
    RGB color;
};

std::vector<NamedColor> const& namedColors() {
    static std::vector<NamedColor> list = {
        {"red", {255, 40, 40}}, {"crimson", {200, 0, 40}}, {"ruby", {220, 20, 60}}, {"maroon", {120, 10, 30}},
        {"blue", {40, 110, 255}}, {"navy", {20, 30, 120}}, {"sapphire", {30, 80, 220}}, {"indigo", {80, 40, 200}},
        {"green", {40, 220, 80}}, {"lime", {160, 255, 40}}, {"emerald", {20, 200, 120}}, {"mint", {150, 255, 200}},
        {"yellow", {255, 230, 40}}, {"gold", {255, 190, 40}}, {"golden", {255, 190, 40}}, {"amber", {255, 170, 0}},
        {"orange", {255, 140, 20}}, {"peach", {255, 190, 150}}, {"coral", {255, 120, 100}},
        {"purple", {160, 60, 255}}, {"violet", {150, 70, 255}}, {"lavender", {190, 160, 255}}, {"magenta", {255, 0, 200}},
        {"pink", {255, 110, 200}}, {"rose", {255, 80, 140}},
        {"cyan", {0, 240, 255}}, {"teal", {0, 180, 170}}, {"turquoise", {60, 230, 210}}, {"aqua", {0, 255, 230}},
        {"white", {255, 255, 255}}, {"black", {15, 15, 20}}, {"gray", {140, 140, 150}}, {"grey", {140, 140, 150}},
        {"silver", {200, 200, 210}}, {"brown", {130, 80, 40}}, {"beige", {230, 210, 170}},
    };
    return list;
}

bool findColor(std::string const& word, RGB& out) {
    for (auto const& c : namedColors()) {
        if (word == c.name || word == std::string(c.name) + "ish" || word == std::string(c.name) + "s") {
            out = c.color;
            return true;
        }
    }
    return false;
}

struct TagWord {
    std::vector<char const*> words;
    char const* tag;
    Slot slot;
};

std::vector<TagWord> const& tagWords() {
    static std::vector<TagWord> list = {
        {{"heart"}, "heart", Slot::Floating},
        {{"star", "sparkle", "twinkle"}, "star", Slot::Floating},
        {{"note", "melody"}, "note", Slot::Floating},
        {{"ring", "bubble", "halo"}, "ring", Slot::Floating},
        {{"circle", "orb", "ball"}, "circle", Slot::Floating},
        {{"diamond", "gem", "crystal", "jewel"}, "diamond", Slot::Floating},
        {{"dot", "particle", "ember", "firefl*", "snowflake", "dust", "spark"}, "dot", Slot::Floating},
        {{"cloud", "fog", "mist"}, "cloud", Slot::Sky},
        {{"bokeh", "blur"}, "bokeh", Slot::Sky},
        {{"chain"}, "chain", Slot::Hanging},
        {{"grass", "weed", "seaweed"}, "grass", Slot::Ground},
        {{"bush", "shrub", "plant", "leaf", "leaves", "hedge"}, "bush", Slot::Ground},
        {{"spike", "thorn", "icicle", "stalactite", "fang"}, "spikes", Slot::Ground},
        {{"pole", "rod", "lamp", "post", "antenna"}, "pole", Slot::Ground},
        {{"pillar", "tower", "building", "skyline", "column", "skyscraper"}, "pillar", Slot::Structure},
    };
    return list;
}

uint32_t fnv1a(std::string const& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

std::string join(std::vector<std::string> const& v, char const* sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

void applyTheme(DecoPlan& plan, Theme const& t) {
    plan.theme = t.name;
    for (int i = 0; i < ROLE_COUNT; ++i) plan.colors[i] = t.colors[i];
    for (int i = 0; i < SLOT_COUNT; ++i) plan.slotWeight[i] = t.slots[i];
    plan.tagBoost.clear();
    for (auto const& [tag, v] : t.boosts) plan.tagBoost[tag] = v;
    plan.glow = t.glow;
    plan.spin = t.spin;
    plan.clustering = t.clustering;
}

float boostOf(Theme const& t, std::string const& tag) {
    for (auto const& [k, v] : t.boosts) {
        if (tag == k) return v;
    }
    return 1.f;
}

} // namespace

// ---------------------------------------------------------------- the brain

DecoPlan interpretDescription(std::string const& description) {
    DecoPlan plan;
    auto words = tokenize(description);
    auto const& T = themes();

    // Negation: "no clouds", "without any chains", "not spooky".
    auto isNegator = [](std::string const& w) {
        return w == "no" || w == "without" || w == "not" || w == "zero" || w == "never" || w == "nothing" || w == "dont" || w == "don";
    };
    std::vector<bool> negated(words.size(), false);
    for (size_t i = 0; i < words.size(); ++i) {
        if (isNegator(words[i])) {
            // "don't want clouds" tokenizes to don / t / want / clouds
            size_t span = (words[i] == "don" || words[i] == "dont") ? 3 : 2;
            for (size_t j = i + 1; j < words.size() && j <= i + span; ++j) negated[j] = true;
        }
    }

    // 1) Score themes.
    std::vector<int> score(T.size(), 0);
    std::vector<size_t> firstHit(T.size(), SIZE_MAX);
    std::vector<std::vector<std::string>> hits(T.size());
    for (size_t i = 0; i < words.size(); ++i) {
        if (negated[i]) continue;
        for (size_t t = 0; t < T.size(); ++t) {
            for (auto key : T[t].keys) {
                if (matchKey(words[i], key)) {
                    score[t]++;
                    firstHit[t] = std::min(firstHit[t], i);
                    hits[t].push_back(words[i]);
                    break;
                }
            }
        }
    }
    auto better = [&](size_t a, size_t b) {
        if (score[a] != score[b]) return score[a] > score[b];
        return firstHit[a] < firstHit[b];
    };
    std::vector<size_t> order(T.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), better);

    size_t best = order[0];
    if (score[best] == 0) {
        if (words.empty()) {
            best = T.size() - 1; // Classic
            applyTheme(plan, T[best]);
            plan.notes.push_back("Theme: " + plan.theme + " (empty description)");
        } else {
            best = fnv1a(toLowerAscii(description)) % (T.size() - 1);
            applyTheme(plan, T[best]);
            plan.notes.push_back("Theme: " + plan.theme + " (improvised - no theme words found)");
        }
    } else {
        applyTheme(plan, T[best]);
        plan.notes.push_back("Theme: " + plan.theme + " (from: " + join(hits[best], ", ") + ")");
        size_t second = order[1];
        if (score[second] > 0) {
            Theme const& a = T[best];
            Theme const& b = T[second];
            for (int i = 0; i < SLOT_COUNT; ++i) plan.slotWeight[i] = a.slots[i] * 0.65f + b.slots[i] * 0.35f;
            for (auto const& tag : knownTags()) {
                float v = boostOf(a, tag) * 0.65f + boostOf(b, tag) * 0.35f;
                if (std::fabs(v - 1.f) > 0.01f) plan.tagBoost[tag] = v;
                else plan.tagBoost.erase(tag);
            }
            plan.colors[(int)Role::Accent] = b.colors[(int)Role::Primary];
            plan.colors[(int)Role::Sky] = mixColor(a.colors[(int)Role::Sky], b.colors[(int)Role::Sky], 0.4f);
            plan.glow = a.glow || b.glow;
            plan.spin = a.spin * 0.65f + b.spin * 0.35f;
            plan.clustering = a.clustering * 0.65f + b.clustering * 0.35f;
            plan.theme = std::string(a.name) + " x " + b.name;
            plan.notes.push_back(std::string("Mixed with: ") + b.name + " (from: " + join(hits[second], ", ") + ")");
        }
    }

    // 2) Modifiers.
    std::vector<RGB> userColors;
    std::vector<std::string> colorNames, boosted, removed, style;
    bool rainbow = false;
    bool darkMood = false;
    int pendingTone = 0; // 1 = dark, 2 = light, 3 = vivid
    bool glowSet = false;

    for (size_t i = 0; i < words.size(); ++i) {
        auto const& w = words[i];
        bool neg = negated[i];
        if (isNegator(w)) continue;

        // color tone words modify the next color word
        if (w == "dark" || w == "deep") {
            bool nextIsColor = false;
            RGB tmp;
            if (i + 1 < words.size() && findColor(words[i + 1], tmp)) nextIsColor = true;
            if (nextIsColor) pendingTone = 1;
            else if (!neg && w == "dark") darkMood = true;
            continue;
        }
        if (w == "light" || w == "pale" || w == "pastel" || w == "soft") {
            RGB tmp;
            if (i + 1 < words.size() && findColor(words[i + 1], tmp)) {
                pendingTone = 2;
                continue;
            }
        }
        if (w == "vivid" || w == "hot" || w == "electric") {
            RGB tmp;
            if (i + 1 < words.size() && findColor(words[i + 1], tmp)) {
                pendingTone = 3;
                continue;
            }
        }

        RGB c;
        if (findColor(w, c)) {
            if (neg) { pendingTone = 0; continue; }
            if (pendingTone == 1) c = scaleColor(c, 0.5f);
            else if (pendingTone == 2) c = mixColor(c, {255, 255, 255}, 0.45f);
            else if (pendingTone == 3) {
                float mx = std::max({(float)c.r, (float)c.g, (float)c.b, 1.f});
                c = scaleColor(c, 255.f / mx);
            }
            std::string label = (pendingTone == 1 ? "dark " : pendingTone == 2 ? "light " : pendingTone == 3 ? "vivid " : "") + w;
            pendingTone = 0;
            bool dup = false;
            for (auto const& u : userColors) {
                if (u.r == c.r && u.g == c.g && u.b == c.b) dup = true;
            }
            if (!dup) {
                userColors.push_back(c);
                colorNames.push_back(label);
            }
            continue;
        }
        pendingTone = 0;

        if (w == "rainbow" || w == "colorful" || w == "colourful" || w == "rgb") {
            if (!neg) rainbow = true;
            continue;
        }
        if (matchAny(w, {"glow", "glowing", "neon", "bright", "shiny", "luminous", "radiant", "lit"})) {
            plan.glow = !neg;
            glowSet = true;
        }
        if (matchAny(w, {"gloomy", "shadow", "moody", "grim"}) && !neg) darkMood = true;

        // object tags
        for (auto const& tw : tagWords()) {
            bool m = false;
            for (auto k : tw.words) {
                if (matchKey(w, k)) { m = true; break; }
            }
            if (!m) continue;
            if (neg) {
                plan.tagBoost[tw.tag] = 0.f;
                removed.push_back(tw.tag);
            } else {
                // the user named it: make sure it shows up, whatever the theme thinks
                float cur = plan.tagBoost.count(tw.tag) ? plan.tagBoost[tw.tag] : 1.f;
                plan.tagBoost[tw.tag] = std::max(cur * 3.5f, 3.5f);
                plan.slotWeight[(int)tw.slot] = std::max(plan.slotWeight[(int)tw.slot], 0.15f) * 1.3f;
                boosted.push_back(tw.tag);
            }
        }

        // slots
        auto slotWord = [&](Slot s, float mul, char const* label) {
            if (neg) {
                plan.slotWeight[(int)s] = 0.f;
                removed.push_back(label);
            } else {
                plan.slotWeight[(int)s] = std::max(plan.slotWeight[(int)s], 0.05f) * mul;
                boosted.push_back(label);
            }
        };
        if (matchAny(w, {"ground", "floor"})) slotWord(Slot::Ground, 1.6f, "ground deco");
        if (matchAny(w, {"ceiling", "hanging", "hang", "dangling"})) slotWord(Slot::Hanging, 1.8f, "hanging deco");
        if (matchAny(w, {"floating", "float", "air", "airborne"})) slotWord(Slot::Floating, 1.6f, "floating deco");
        if (matchAny(w, {"sky", "background", "bg", "backdrop"})) {
            slotWord(Slot::Sky, 1.5f, "sky");
            if (!neg) plan.slotWeight[(int)Slot::Structure] *= 1.3f;
        }

        // density / size / chaos
        if (!neg && matchAny(w, {"lots", "tons", "many", "dense", "busy", "packed", "heavy", "full", "extreme", "insane", "overload*"})) {
            plan.clustering = std::min(1.f, plan.clustering + 0.25f);
            style.push_back("dense around gameplay");
        }
        if (!neg && matchAny(w, {"sparse", "subtle", "few", "calm", "airy", "spread", "scattered"})) {
            plan.clustering = std::max(0.f, plan.clustering - 0.25f);
            style.push_back("spread out");
        }
        if (!neg && matchAny(w, {"big", "huge", "giant", "large", "massive", "enormous"})) {
            plan.scaleMul *= 1.45f;
            style.push_back("bigger");
        }
        if (!neg && matchAny(w, {"small", "tiny", "mini", "little", "micro"})) {
            plan.scaleMul *= 0.65f;
            style.push_back("smaller");
        }
        if (!neg && matchAny(w, {"chaos", "chaotic", "wild", "crazy", "spin", "spinning", "messy", "random"})) {
            plan.spin = 1.f;
            style.push_back("chaotic rotation");
        }
        if (!neg && matchAny(w, {"neat", "tidy", "ordered", "straight", "symmetric", "symmetrical", "organized"})) {
            plan.spin = 0.f;
            plan.clustering = std::max(0.f, plan.clustering - 0.1f);
            style.push_back("neat");
        }
    }

    // 3) Colors from the user's words.
    if (rainbow) {
        plan.colors[0] = {255, 60, 60};
        plan.colors[1] = {80, 255, 120};
        plan.colors[2] = {80, 140, 255};
        plan.colors[4] = {255, 230, 80};
        plan.glow = glowSet ? plan.glow : true;
        plan.notes.push_back("Colors: rainbow");
    }
    if (!userColors.empty()) {
        RGB c0 = userColors[0];
        plan.colors[0] = c0;
        plan.colors[1] = userColors.size() >= 2 ? userColors[1] : mixColor(c0, {255, 255, 255}, 0.5f);
        if (userColors.size() >= 3) plan.colors[2] = userColors[2];
        else if (userColors.size() == 2) plan.colors[2] = mixColor(userColors[0], userColors[1], 0.5f);
        else plan.colors[2] = shiftHue(c0, 25.f);
        plan.colors[3] = scaleColor(c0, 0.18f);
        plan.colors[4] = mixColor(plan.colors[1], {255, 255, 255}, 0.4f);
        plan.notes.push_back("Colors: " + join(colorNames, ", ") + " -> " + toHex(plan.colors[0]) + " / " +
                             toHex(plan.colors[1]) + " / " + toHex(plan.colors[2]));
    } else if (!rainbow) {
        plan.notes.push_back("Colors: theme palette " + toHex(plan.colors[0]) + " / " + toHex(plan.colors[1]) + " / " +
                             toHex(plan.colors[2]));
    }
    if (darkMood) {
        plan.colors[3] = scaleColor(plan.colors[3], 0.5f);
        plan.opacity[3] = 0.55f;
        plan.opacity[4] = 0.3f;
        plan.notes.push_back("Mood: dark");
    }

    // 4) Sanity + summary.
    float total = 0.f;
    for (auto& w : plan.slotWeight) {
        w = std::max(0.f, w);
        total += w;
    }
    if (total <= 0.001f) {
        plan.slotWeight[(int)Slot::Floating] = 1.f;
        total = 1.f;
    }
    plan.scaleMul = std::clamp(plan.scaleMul, 0.3f, 3.f);
    plan.spin = std::clamp(plan.spin, 0.f, 1.f);
    plan.clustering = std::clamp(plan.clustering, 0.f, 1.f);

    plan.notes.push_back(std::string("Glow: ") + (plan.glow ? "on" : "off"));
    if (!boosted.empty()) {
        std::set<std::string> u(boosted.begin(), boosted.end());
        plan.notes.push_back("More: " + join(std::vector<std::string>(u.begin(), u.end()), ", "));
    }
    if (!removed.empty()) {
        std::set<std::string> u(removed.begin(), removed.end());
        plan.notes.push_back("None: " + join(std::vector<std::string>(u.begin(), u.end()), ", "));
    }
    if (!style.empty()) {
        std::set<std::string> u(style.begin(), style.end());
        plan.notes.push_back("Style: " + join(std::vector<std::string>(u.begin(), u.end()), ", "));
    }
    char mix[160];
    std::snprintf(mix, sizeof(mix), "Mix: %d%% ground, %d%% hanging, %d%% floating, %d%% sky, %d%% skyline",
                  (int)std::lround(100 * plan.slotWeight[0] / total), (int)std::lround(100 * plan.slotWeight[1] / total),
                  (int)std::lround(100 * plan.slotWeight[2] / total), (int)std::lround(100 * plan.slotWeight[3] / total),
                  (int)std::lround(100 * plan.slotWeight[4] / total));
    plan.notes.push_back(mix);
    return plan;
}

std::vector<std::string> const& ideaPrompts() {
    static std::vector<std::string> ideas = {
        "neon cyber city with pink and cyan glow",
        "spooky graveyard with chains and purple fog",
        "frozen ice cave with icicles and snow",
        "lava hell with red embers and spikes",
        "dreamy pastel sky with hearts and clouds",
        "underwater ocean with bubbles and seaweed",
        "deep space galaxy with stars and rings",
        "rave party with music notes and flashing poles",
        "golden desert temple with diamonds",
        "clean minimal white rings, no clouds",
        "enchanted forest with fireflies and bushes",
        "dark dungeon with hanging chains and torches",
        "rainbow chaos with tons of particles",
        "industrial factory with chains and yellow lights",
    };
    return ideas;
}

} // namespace aplus
