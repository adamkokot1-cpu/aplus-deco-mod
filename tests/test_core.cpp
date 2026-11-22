// Standalone tests for A+ Deco's core logic (no Geode needed).
// Build:  g++ -std=c++23 -O2 -Wall -Wextra -I../src/core ../src/core/*.cpp test_core.cpp -o test_core
#include "DecoBrain.hpp"
#include "Layout.hpp"
#include "ObjectString.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>

using namespace aplus;

static int failures = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

static std::vector<PartInfo> fakeParts() {
    std::vector<PartInfo> out;
    for (auto const& d : builtinParts()) {
        PartInfo p;
        p.id = d.id;
        p.slot = d.slot;
        p.tag = d.tag;
        p.weight = d.weight;
        p.scaleLo = d.scaleLo;
        p.scaleHi = d.scaleHi;
        // rough sprite sizes
        if (p.tag == "pole") { p.w = 10; p.h = 75; }
        else if (p.tag == "chain") { p.w = 12; p.h = 60; }
        else if (p.tag == "cloud") { p.w = 90; p.h = 45; }
        else if (p.tag == "dot" || p.tag == "bokeh") { p.w = 6; p.h = 6; }
        else if (p.tag == "spikes") { p.w = 40; p.h = 20; }
        else { p.w = 25; p.h = 22; }
        out.push_back(p);
    }
    return out;
}

static LevelGrid sampleLevel() {
    LevelGrid g;
    for (int cx = 10; cx <= 20; ++cx) g.addSolid(cx, 2);      // platform
    for (int cy = 0; cy <= 3; ++cy) g.addSolid(30, cy);       // pillar
    for (int cx = 40; cx <= 60; ++cx) g.addSolid(cx, 6);      // ceiling
    for (int cx = 40; cx <= 60; ++cx) g.addSolid(cx, 0);      // floor blocks under it
    g.addBlocked(25, 0);                                       // a spike
    g.addBlocked(35, 3);                                       // an orb
    return g;
}

static void testBrain() {
    std::printf("[brain]\n");
    struct Case {
        char const* prompt;
        char const* expectTheme; // substring
    };
    Case cases[] = {
        {"neon cyber city with pink and cyan glow", "Neon City"},
        {"spooky graveyard with chains and purple fog", "Haunted"},
        {"frozen ice cave with icicles and snow", "Frozen"},
        {"lava hell with red embers and spikes", "Inferno"},
        {"dreamy pastel sky with hearts and clouds", "Sweet Dream"},
        {"underwater ocean with bubbles and seaweed", "Deep Ocean"},
        {"deep space galaxy with stars and rings", "Cosmic"},
        {"rave party with music notes", "Party"},
        {"", "Classic"},
        {"I want it to look super cool", ""}, // improvised
    };
    for (auto const& c : cases) {
        auto plan = interpretDescription(c.prompt);
        std::printf("  \"%s\"\n", c.prompt);
        for (auto const& n : plan.notes) std::printf("      %s\n", n.c_str());
        if (*c.expectTheme) CHECK(plan.theme.find(c.expectTheme) != std::string::npos);
        float sum = 0;
        for (float w : plan.slotWeight) { CHECK(w >= 0); sum += w; }
        CHECK(sum > 0);
    }

    // colors
    auto red = interpretDescription("red and black spikes");
    CHECK(red.colors[0].r == 255 && red.colors[0].g == 40);
    CHECK(red.colors[1].r == 15);
    // negation
    auto noClouds = interpretDescription("frozen land, no clouds and no glow");
    CHECK(noClouds.tagBoost["cloud"] == 0.f);
    CHECK(!noClouds.glow);
    auto dont = interpretDescription("forest but i don't want bushes");
    CHECK(dont.tagBoost["bush"] == 0.f);
    // "start" must not count as "star"
    auto start = interpretDescription("start of the level");
    CHECK(start.tagBoost.count("star") == 0 || start.tagBoost["star"] <= 1.f);
    // size words
    auto big = interpretDescription("huge clouds");
    CHECK(big.scaleMul > 1.3f);
    CHECK(big.tagBoost["cloud"] > 2.f);
    // hex helpers
    RGB c;
    CHECK(parseHex("#FF8000", c) && c.r == 255 && c.g == 128 && c.b == 0);
    CHECK(!parseHex("#GG0000", c));
    CHECK(toHex({1, 2, 255}) == "#0102FF");
}

static void testLayout() {
    std::printf("[layout]\n");
    auto parts = fakeParts();
    auto grid = sampleLevel();
    Area area{0, 70 * 30, 0, 600};
    for (auto prompt : {"neon city", "nature forest", "space galaxy", "dungeon with chains", "minimal"}) {
        auto plan = interpretDescription(prompt);
        for (int count : {1, 37, 300, 2500}) {
            auto res = layoutDeco(plan, grid, area, parts, count, 1234u + count);
            CHECK(static_cast<int>(res.objects.size()) == count);
            for (auto const& o : res.objects) {
                CHECK(std::isfinite(o.x) && std::isfinite(o.y));
                CHECK(o.y >= -1.f);
                CHECK(o.x >= -15.f && o.x <= area.maxX + 15.f);
                if (o.slot == Slot::Floating || o.slot == Slot::Structure) {
                    int cx = static_cast<int>(std::floor(o.x / 30.f));
                    int cy = static_cast<int>(std::floor(o.y / 30.f));
                    if (o.slot == Slot::Structure) CHECK(!grid.isBlocked(cx, cy));
                    if (o.slot == Slot::Floating) CHECK(!grid.isBlocked(cx, cy));
                }
                if (o.slot == Slot::Structure) CHECK(o.noTouch && o.id == 1);
                if (o.slot != Slot::Structure) CHECK(!o.noTouch && o.id != 1);
            }
            if (count == 300) {
                std::printf("  %-22s %s\n", prompt, res.notes.front().c_str());
            }
        }
    }
    // tag removal really removes
    auto plan = interpretDescription("no clouds, no hearts, lots of stars");
    auto res = layoutDeco(plan, grid, area, parts, 1000, 7);
    for (auto const& o : res.objects) {
        CHECK(o.id != 48 && o.id != 49 && o.id != 131 && o.id != 52);
    }
    // empty level still works
    LevelGrid empty;
    auto r2 = layoutDeco(interpretDescription("party"), empty, Area{0, 1500, 0, 500}, parts, 400, 3);
    CHECK(r2.objects.size() == 400);
    std::printf("  empty level            %s\n", r2.notes.front().c_str());
    // deterministic for same seed
    auto a = layoutDeco(plan, grid, area, parts, 200, 99);
    auto b = layoutDeco(plan, grid, area, parts, 200, 99);
    CHECK(a.objects.size() == b.objects.size());
    for (size_t i = 0; i < a.objects.size(); ++i) CHECK(a.objects[i].x == b.objects[i].x && a.objects[i].id == b.objects[i].id);
}

static void testStrings() {
    std::printf("[strings]\n");
    auto plan = interpretDescription("neon pink glow");
    auto res = layoutDeco(plan, sampleLevel(), Area{0, 2000, 0, 600}, fakeParts(), 50, 5);
    StringOptions opt;
    opt.groupId = 77;
    auto s = buildObjectString(res.objects, opt);
    auto t = buildColorTriggers(plan, opt);
    // every object is "k,v,k,v...;" with an even number of fields
    size_t objects = 0, start = 0;
    std::string all = t + s;
    while (start < all.size()) {
        auto end = all.find(';', start);
        CHECK(end != std::string::npos);
        auto obj = all.substr(start, end - start);
        int commas = 0;
        for (char ch : obj) commas += ch == ',';
        CHECK(commas % 2 == 1);
        CHECK(obj.rfind("1,", 0) == 0);
        CHECK(obj.find("nan") == std::string::npos && obj.find("inf") == std::string::npos);
        ++objects;
        start = end + 1;
    }
    CHECK(objects == 55);
    CHECK(s.find(",57,77") != std::string::npos);
    CHECK(t.find("1,899,") == 0);
    CHECK(t.find(",17,1") != std::string::npos); // glow -> blending
    std::printf("  sample object: %s\n", s.substr(0, s.find(';') + 1).c_str());
    std::printf("  sample trigger: %s\n", t.substr(0, t.find(';') + 1).c_str());
    CHECK(fmtNum(1.f) == "1");
    CHECK(fmtNum(1.5f) == "1.5");
    CHECK(fmtNum(-0.001f) == "0");
}

int main() {
    testBrain();
    testLayout();
    testStrings();
    std::printf(failures ? "\n%d FAILURES\n" : "\nALL TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
