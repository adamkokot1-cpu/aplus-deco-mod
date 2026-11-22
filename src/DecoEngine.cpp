#include "DecoEngine.hpp"

#include "core/DecoBrain.hpp"
#include "core/Layout.hpp"
#include "core/ObjectString.hpp"
#include "core/Parts.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <random>
#include <set>
#include <unordered_map>
#include <unordered_set>

using namespace geode::prelude;

namespace aplus {

char const* areaModeName(AreaMode mode) {
    switch (mode) {
        case AreaMode::WholeLevel: return "Whole level";
        case AreaMode::CurrentView: return "Current view";
        case AreaMode::Selection: return "Around selection";
    }
    return "?";
}

namespace {

// ---------------------------------------------------------------- last generation (for "Undo Last")

CCArray* s_last = nullptr;
LevelEditorLayer* s_layer = nullptr;

void setLast(CCArray* arr, LevelEditorLayer* layer) {
    if (arr) arr->retain();
    if (s_last) s_last->release();
    s_last = arr;
    s_layer = layer;
}

// ---------------------------------------------------------------- object probing

struct Probe {
    bool exists = false;
    bool deco = false;
    bool trigger = false;
    float w = 30.f, h = 30.f;
};

Probe probe(int id) {
    static std::unordered_map<int, Probe> cache;
    auto it = cache.find(id);
    if (it != cache.end()) return it->second;

    Probe p;
    auto& keys = ObjectToolbox::sharedState()->m_allKeys;
    auto k = keys.find(id);
    if (k != keys.end() && k->second.size() > 0) {
        if (auto obj = GameObject::createWithKey(id)) {
            p.exists = true;
            p.deco = obj->m_objectType == GameObjectType::Decoration;
            p.trigger = obj->m_isTrigger;
            auto size = obj->getContentSize();
            if (size.width > 0.5f) p.w = size.width;
            if (size.height > 0.5f) p.h = size.height;
        }
    }
    cache[id] = p;
    return p;
}

std::vector<PartInfo> resolveParts(bool useExtras) {
    std::vector<PartInfo> out;

    // Sanity check: do known decoration objects report as decoration?
    // If not, we don't trust type info and only use the verified catalog.
    int decoVotes = 0;
    for (int id : {1764, 48, 18, 41}) {
        if (probe(id).deco) ++decoVotes;
    }
    bool typesReliable = decoVotes >= 2;

    std::set<int> known;
    for (auto const& d : builtinParts()) {
        known.insert(d.id);
        auto pr = probe(d.id);
        if (!pr.exists) continue;
        if (!d.verified && (!typesReliable || !pr.deco || pr.trigger)) continue;
        PartInfo p;
        p.id = d.id;
        p.slot = d.slot;
        p.tag = d.tag;
        p.weight = d.weight;
        p.w = pr.w;
        p.h = pr.h;
        p.scaleLo = d.scaleLo;
        p.scaleHi = d.scaleHi;
        out.push_back(p);
    }

    // Discover more decoration from the game's own sprite names (glow, clouds, grass...).
    if (useExtras && typesReliable) {
        std::map<std::string, int> perKeyword;
        for (auto const& [id, frameRaw] : ObjectToolbox::sharedState()->m_allKeys) {
            if (known.count(id)) continue;
            std::string frame = toLowerAscii(std::string(frameRaw.c_str()));
            if (frame.empty() || frame.rfind("edit_", 0) == 0) continue;
            for (auto const& rule : extraRules()) {
                if (frame.find(rule.keyword) == std::string::npos) continue;
                int& n = perKeyword[rule.keyword];
                if (n >= 8) break;
                auto pr = probe(id);
                if (!pr.exists || !pr.deco || pr.trigger) break;
                if (pr.w < 2.f || pr.h < 2.f || pr.w > 160.f || pr.h > 160.f) break;
                PartInfo p;
                p.id = id;
                p.slot = rule.slot;
                p.tag = rule.tag;
                p.weight = 0.35f;
                p.w = pr.w;
                p.h = pr.h;
                p.scaleLo = 0.7f;
                p.scaleHi = 1.2f;
                out.push_back(p);
                ++n;
                break;
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------- level scan

struct Scan {
    LevelGrid grid;
    LevelStats stats;
    bool any = false;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
};

Scan scanLevel(LevelEditorLayer* layer) {
    Scan s;
    if (!layer || !layer->m_objects) return s;
    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!obj || obj->m_isTrigger) continue;
        auto pos = obj->getPosition();
        s.any = true;
        s.minX = std::min(s.minX, pos.x);
        s.maxX = std::max(s.maxX, pos.x);
        s.minY = std::min(s.minY, pos.y);
        s.maxY = std::max(s.maxY, pos.y);

        auto type = obj->m_objectType;
        bool blockLike = type == GameObjectType::Solid || type == GameObjectType::Slope;
        bool solid = blockLike && !obj->m_isNoTouch;
        bool deco = type == GameObjectType::Decoration || (blockLike && obj->m_isNoTouch);
        if (deco) continue; // existing deco never blocks new deco

        CCRect r = obj->getObjectRect();
        if (r.size.width < 1.f || r.size.height < 1.f) r = CCRect(pos.x - 14.f, pos.y - 14.f, 28.f, 28.f);
        int cxa = static_cast<int>(std::floor((r.getMinX() + 1.f) / CELL));
        int cxb = static_cast<int>(std::floor((r.getMaxX() - 1.f) / CELL));
        int cya = static_cast<int>(std::floor((r.getMinY() + 1.f) / CELL));
        int cyb = static_cast<int>(std::floor((r.getMaxY() - 1.f) / CELL));
        cxb = std::clamp(cxb, cxa, cxa + 64);
        cyb = std::clamp(cyb, cya, cya + 64);
        for (int cx = cxa; cx <= cxb; ++cx) {
            for (int cy = cya; cy <= cyb; ++cy) {
                if (solid) s.grid.addSolid(cx, cy);
                else s.grid.addBlocked(cx, cy);
            }
        }
        if (solid) ++s.stats.solids;
        else if (type == GameObjectType::Hazard) ++s.stats.hazards;
        else ++s.stats.others;
    }
    return s;
}

bool computeArea(EditorUI* ui, AreaMode mode, Scan const& s, Area& area, std::string& error) {
    auto layer = ui->m_editorLayer;
    switch (mode) {
        case AreaMode::WholeLevel: {
            if (s.any) {
                area.minX = std::max(0.f, s.minX - 90.f);
                area.maxX = s.maxX + 90.f;
                area.minY = 0.f;
                area.maxY = std::max(600.f, s.maxY + 300.f);
            } else {
                area = Area{0.f, 2400.f, 0.f, 600.f};
            }
            return true;
        }
        case AreaMode::CurrentView: {
            auto ol = layer->m_objectLayer;
            if (!ol) {
                error = "Couldn't read the editor view.";
                return false;
            }
            auto win = CCDirector::get()->getWinSize();
            auto a = ol->convertToNodeSpace(CCPoint(0.f, 0.f));
            auto b = ol->convertToNodeSpace(CCPoint(win.width, win.height));
            area.minX = std::max(0.f, std::min(a.x, b.x));
            area.maxX = std::max(a.x, b.x);
            area.minY = std::max(0.f, std::min(a.y, b.y));
            area.maxY = std::max(a.y, b.y);
            if (area.maxX - area.minX < 60.f || area.maxY - area.minY < 60.f) {
                error = "The visible area is too small - zoom out a bit.";
                return false;
            }
            return true;
        }
        case AreaMode::Selection: {
            auto sel = ui->getSelectedObjects();
            if (!sel || sel->count() == 0) {
                error = "Select some objects first (or change the Area).";
                return false;
            }
            float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
            for (auto obj : CCArrayExt<GameObject*>(sel)) {
                if (!obj) continue;
                auto p = obj->getPosition();
                x0 = std::min(x0, p.x);
                x1 = std::max(x1, p.x);
                y0 = std::min(y0, p.y);
                y1 = std::max(y1, p.y);
            }
            area.minX = std::max(0.f, x0 - 150.f);
            area.maxX = x1 + 150.f;
            area.minY = std::max(0.f, y0 - 120.f);
            area.maxY = y1 + 150.f;
            return true;
        }
    }
    error = "Unknown area mode.";
    return false;
}

// If A+ color triggers already exist at the level start, put the new ones 1 unit later
// so they win (GD runs triggers left to right). Nothing old is deleted.
float nextTriggerX(LevelEditorLayer* layer, float triggerY) {
    float x = 15.f;
    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!obj || obj->m_objectID != 899) continue;
        auto p = obj->getPosition();
        if (p.x >= 14.f && p.x < 75.f && p.y >= triggerY - 1.f && p.y <= triggerY + 30.f * ROLE_COUNT) {
            x = std::max(x, p.x + 1.f);
        }
    }
    return x;
}

int settingInt(char const* key, int lo, int hi) {
    auto v = Mod::get()->getSettingValue<int64_t>(key);
    return static_cast<int>(std::clamp<int64_t>(v, lo, hi));
}

} // namespace

// ---------------------------------------------------------------- public API

void DecoEngine::onEditorOpened(LevelEditorLayer* layer) {
    setLast(nullptr, layer);
}

LevelStats DecoEngine::quickStats(EditorUI* ui, AreaMode mode) {
    LevelStats st;
    if (!ui || !ui->m_editorLayer) return st;
    auto scan = scanLevel(ui->m_editorLayer);
    st = scan.stats;
    Area area;
    std::string err;
    if (computeArea(ui, mode, scan, area, err)) {
        st.width = area.maxX - area.minX;
        st.height = area.maxY - area.minY;
    }
    return st;
}

GenerateResult DecoEngine::generate(EditorUI* ui, GenerateRequest const& req) {
    GenerateResult res;
    if (!ui || !ui->m_editorLayer) {
        res.message = "Open a level in the editor first.";
        return res;
    }
    auto layer = ui->m_editorLayer;
    auto scan = scanLevel(layer);

    Area area;
    std::string err;
    if (!computeArea(ui, req.area, scan, area, err)) {
        res.message = err;
        return res;
    }

    auto parts = resolveParts(Mod::get()->getSettingValue<bool>("use-extra-objects"));
    if (parts.empty()) {
        res.message = "Couldn't find any decoration objects in this GD version.";
        return res;
    }

    int count = std::clamp(req.count, 1, settingInt("max-count", 1, 100000));
    uint32_t seed = std::random_device{}() ^
                    static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    auto lay = layoutDeco(req.plan, scan.grid, area, parts, count, seed);
    if (lay.objects.empty()) {
        res.message = lay.notes.empty() ? "Nothing to place here." : lay.notes.back();
        return res;
    }

    StringOptions opt;
    int firstChannel = settingInt("color-channel-start", 1, 995);
    for (int i = 0; i < ROLE_COUNT; ++i) opt.channels[i] = firstChannel + i;
    opt.groupId = settingInt("deco-group", 0, 9999);
    opt.colorTriggers = req.colorTriggers;
    opt.triggerX = nextTriggerX(layer, opt.triggerY);

    auto str = buildColorTriggers(req.plan, opt) + buildObjectString(lay.objects, opt);

    ui->deselectAll();
    auto created = layer->createObjectsFromString(str, true, false);
    if (!created || created->count() == 0) {
        res.message = "GD didn't create any objects (object limit reached?).";
        return res;
    }
    // One undo step for the whole generation, exactly like a paste:
    // Ctrl+Z removes every object in the array.
    layer->addToUndoList(UndoObject::createWithArray(created, UndoCommand::Paste), false);
    setLast(created, layer);
    ui->updateButtons();

    int triggers = opt.colorTriggers ? ROLE_COUNT : 0;
    res.ok = true;
    res.placed = std::max(0, static_cast<int>(created->count()) - triggers);
    res.notes = lay.notes;
    res.message = fmt::format("Placed {} objects", res.placed);
    log::info("A+ Deco: theme '{}' ({}), {} objects, area x {}..{}", req.plan.theme, req.plan.source, res.placed,
              static_cast<int>(area.minX), static_cast<int>(area.maxX));
    return res;
}

bool DecoEngine::hasLast(EditorUI* ui) {
    return ui && s_last && s_layer == ui->m_editorLayer && s_last->count() > 0;
}

int DecoEngine::removeLast(EditorUI* ui) {
    if (!hasLast(ui)) return 0;
    auto layer = ui->m_editorLayer;

    // Only objects that are still in the level (the user may have undone/deleted some).
    std::unordered_set<GameObject*> inLevel;
    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) inLevel.insert(obj);
    auto alive = CCArray::create();
    for (auto obj : CCArrayExt<GameObject*>(s_last)) {
        if (inLevel.count(obj)) alive->addObject(obj);
    }
    setLast(nullptr, layer);
    if (alive->count() == 0) return 0;

    // Use GD's own "delete selected" so it is a normal, undoable delete.
    ui->deselectAll();
    ui->selectObjects(alive, true);
    ui->onDeleteSelected(nullptr);
    ui->updateButtons();
    return static_cast<int>(alive->count());
}

} // namespace aplus
