#include "ClaudeBrain.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace aplus {

char const* claudeUrl() {
    return "https://api.anthropic.com/v1/messages";
}

static std::string systemPrompt() {
    return R"(You are A+ Deco, an AI that designs decoration for Geometry Dash levels.
You do not place objects yourself: you output a JSON style plan, and a placement engine places the requested number of objects around the level's existing blocks.

Reply with ONLY one JSON object (no markdown, no prose) in this exact shape:
{
  "theme": "2-4 word theme name",
  "colors": {"primary": "#RRGGBB", "secondary": "#RRGGBB", "accent": "#RRGGBB", "structure": "#RRGGBB", "sky": "#RRGGBB"},
  "opacity": {"structure": 0.0-1.0, "sky": 0.0-1.0},
  "glow": true or false,
  "mix": {"ground": 0-1, "hanging": 0-1, "floating": 0-1, "sky": 0-1, "structure": 0-1},
  "tags": {"tag": multiplier 0-5},
  "scale": 0.5-2.0,
  "spin": 0.0-1.0,
  "clustering": 0.0-1.0,
  "notes": ["up to 4 short sentences explaining your choices"]
}

Slots (mix = share of the object budget):
- ground: deco standing on block tops and the floor (tags: spikes, bush, grass, pole)
- hanging: deco hanging under blocks and ceilings (tags: chain, spikes, pole)
- floating: particles near the gameplay (tags: dot, circle, ring, heart, diamond, star, note)
- sky: clouds and big soft circles high above the gameplay (tags: cloud, bokeh)
- structure: a skyline of non-solid background pillars (tag: pillar)
Tags: 1 = normal, 0 = never use, up to 5 = use a lot. Only list tags you want to change. "extra" = other glow/light deco objects found in the game.
Color roles: primary = ground + hanging deco, secondary = floating particles, accent = pulse shapes, chains and poles, structure = background pillars, sky = clouds.
Glow turns on additive blending for secondary and accent.
spin = how much floating deco is randomly rotated. clustering = how tightly deco hugs the gameplay (high) vs spreads out (low).
Keep the gameplay readable: the structure color should be dark or low-contrast, and do not make everything bright.)";
}

void configureClaudeRequest(web::WebRequest& req, std::string const& apiKey, std::string const& model, ClaudeContext const& ctx) {
    std::string user = fmt::format(
        "Decoration description: {}\n"
        "Object budget: {}\n"
        "Level: {} solid blocks, {} hazards, decorated area about {} x {} units (one block = 30 units).",
        ctx.description.empty() ? std::string("(empty - surprise me)") : ctx.description, ctx.count, ctx.solids, ctx.hazards,
        static_cast<int>(ctx.width), static_cast<int>(ctx.height)
    );

    auto message = matjson::makeObject({
        {"role", "user"},
        {"content", user},
    });
    auto body = matjson::makeObject({
        {"model", model},
        {"max_tokens", 900},
        {"system", systemPrompt()},
        {"messages", matjson::Value(std::vector<matjson::Value>{message})},
    });

    req.header("x-api-key", apiKey);
    req.header("anthropic-version", "2023-06-01");
    req.header("content-type", "application/json");
    req.userAgent("APlusDecoMod/1.0");
    req.timeout(std::chrono::seconds(60));
    req.bodyJSON(body);
}

// ---------------------------------------------------------------- parsing helpers

static bool getNum(matjson::Value& obj, char const* key, float& out) {
    if (!obj.isObject() || !obj.contains(key)) return false;
    auto r = obj[key].asDouble();
    if (!r) return false;
    out = static_cast<float>(r.unwrap());
    return true;
}

static bool getColor(matjson::Value& obj, char const* key, RGB& out) {
    if (!obj.isObject() || !obj.contains(key)) return false;
    auto r = obj[key].asString();
    if (!r) return false;
    return parseHex(r.unwrap(), out);
}

static std::string extractJsonObject(std::string const& text) {
    auto a = text.find('{');
    auto b = text.rfind('}');
    if (a == std::string::npos || b == std::string::npos || b <= a) return "";
    return text.substr(a, b - a + 1);
}

bool applyClaudeResponse(web::WebResponse& res, DecoPlan& plan, std::string& error) {
    auto jsonRes = res.json();
    if (!res.ok()) {
        std::string msg;
        if (jsonRes) {
            auto json = jsonRes.unwrap();
            if (json.contains("error") && json["error"].contains("message")) {
                msg = json["error"]["message"].asString().unwrapOr("");
            }
        }
        if (msg.empty()) msg = res.string().unwrapOr("no response");
        if (msg.size() > 120) msg = msg.substr(0, 120) + "...";
        error = fmt::format("Claude error {}: {}", res.code(), msg);
        return false;
    }
    if (!jsonRes) {
        error = "Claude sent something that isn't JSON";
        return false;
    }

    auto json = jsonRes.unwrap();
    std::string text;
    if (json.contains("content") && json["content"].isArray()) {
        for (auto& part : json["content"].asArray().unwrap()) {
            if (part.isObject() && part.contains("text")) text += part["text"].asString().unwrapOr("");
        }
    }
    auto planText = extractJsonObject(text);
    if (planText.empty()) {
        error = "Claude didn't return a plan";
        return false;
    }
    auto parsed = matjson::parse(planText);
    if (!parsed) {
        error = "Claude's plan wasn't valid JSON";
        return false;
    }
    auto p = parsed.unwrap();
    if (!p.isObject()) {
        error = "Claude's plan had the wrong shape";
        return false;
    }

    plan.source = "Claude";
    if (p.contains("theme")) {
        auto t = p["theme"].asString();
        if (t && !t.unwrap().empty()) plan.theme = t.unwrap().substr(0, 40);
    }
    if (p.contains("colors")) {
        auto& c = p["colors"];
        getColor(c, "primary", plan.colors[(int)Role::Primary]);
        getColor(c, "secondary", plan.colors[(int)Role::Secondary]);
        getColor(c, "accent", plan.colors[(int)Role::Accent]);
        getColor(c, "structure", plan.colors[(int)Role::Structure]);
        getColor(c, "sky", plan.colors[(int)Role::Sky]);
    }
    if (p.contains("opacity")) {
        auto& o = p["opacity"];
        float v;
        if (getNum(o, "structure", v)) plan.opacity[(int)Role::Structure] = std::clamp(v, 0.05f, 1.f);
        if (getNum(o, "sky", v)) plan.opacity[(int)Role::Sky] = std::clamp(v, 0.05f, 1.f);
    }
    if (p.contains("glow")) {
        auto g = p["glow"].asBool();
        if (g) plan.glow = g.unwrap();
    }
    if (p.contains("mix")) {
        auto& m = p["mix"];
        char const* names[SLOT_COUNT] = {"ground", "hanging", "floating", "sky", "structure"};
        float vals[SLOT_COUNT];
        float total = 0.f;
        bool all = true;
        for (int i = 0; i < SLOT_COUNT; ++i) {
            if (!getNum(m, names[i], vals[i])) {
                all = false;
                break;
            }
            vals[i] = std::max(0.f, vals[i]);
            total += vals[i];
        }
        if (all && total > 0.001f) {
            for (int i = 0; i < SLOT_COUNT; ++i) plan.slotWeight[i] = vals[i];
        }
    }
    if (p.contains("tags") && p["tags"].isObject()) {
        auto& t = p["tags"];
        for (auto const& tag : knownTags()) {
            float v;
            if (getNum(t, tag.c_str(), v)) plan.tagBoost[tag] = std::clamp(v, 0.f, 5.f);
        }
    }
    float v;
    if (getNum(p, "scale", v)) plan.scaleMul = std::clamp(v, 0.3f, 3.f);
    if (getNum(p, "spin", v)) plan.spin = std::clamp(v, 0.f, 1.f);
    if (getNum(p, "clustering", v)) plan.clustering = std::clamp(v, 0.f, 1.f);

    plan.notes.clear();
    plan.notes.push_back("Theme: " + plan.theme + " (designed by Claude)");
    if (p.contains("notes") && p["notes"].isArray()) {
        int n = 0;
        for (auto& note : p["notes"].asArray().unwrap()) {
            auto s = note.asString();
            if (s && n < 4) {
                plan.notes.push_back(s.unwrap().substr(0, 140));
                ++n;
            }
        }
    }
    plan.notes.push_back("Colors: " + toHex(plan.colors[0]) + " / " + toHex(plan.colors[1]) + " / " + toHex(plan.colors[2]));
    plan.notes.push_back(std::string("Glow: ") + (plan.glow ? "on" : "off"));
    return true;
}

} // namespace aplus
