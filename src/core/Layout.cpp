#include "Layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <random>
#include <unordered_map>

namespace aplus {

void LevelGrid::addSolid(int cx, int cy) {
    if (solid.insert(cellKey(cx, cy)).second) solidCells.push_back({cx, cy});
    blocked.insert(cellKey(cx, cy));
}

void LevelGrid::addBlocked(int cx, int cy) {
    blocked.insert(cellKey(cx, cy));
}

namespace {

struct Rng {
    std::mt19937 gen;
    explicit Rng(uint32_t s) : gen(s) {}
    float f() { return std::uniform_real_distribution<float>(0.f, 1.f)(gen); }
    float range(float a, float b) { return a + (b - a) * f(); }
    int irange(int a, int b) {
        if (b < a) std::swap(a, b);
        return std::uniform_int_distribution<int>(a, b)(gen);
    }
};

struct Site {
    float x, y;
    float room; // free space (units) above for ground sites, below for hanging sites
    float weight;
};

struct WeightedPicker {
    std::vector<float> cum;
    void build(std::vector<float> const& w) {
        cum.clear();
        float t = 0.f;
        for (float v : w) {
            t += std::max(0.f, v);
            cum.push_back(t);
        }
    }
    bool empty() const { return cum.empty() || cum.back() <= 0.f; }
    size_t pick(Rng& r) const {
        float x = r.f() * cum.back();
        auto it = std::upper_bound(cum.begin(), cum.end(), x);
        size_t i = static_cast<size_t>(it - cum.begin());
        return std::min(i, cum.size() - 1);
    }
};

float tagBoost(DecoPlan const& plan, std::string const& tag) {
    auto it = plan.tagBoost.find(tag);
    return it == plan.tagBoost.end() ? 1.f : std::max(0.f, it->second);
}

void decode(int64_t key, int& cx, int& cy) {
    cx = static_cast<int32_t>(key >> 32);
    cy = static_cast<int32_t>(static_cast<uint32_t>(key & 0xffffffffLL));
}

bool siteLess(Site const& a, Site const& b) {
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
}

} // namespace

LayoutResult layoutDeco(
    DecoPlan const& plan, LevelGrid const& grid, Area const& area,
    std::vector<PartInfo> const& parts, int count, uint32_t seed
) {
    LayoutResult out;
    if (count <= 0) return out;
    Rng rng(seed);

    // ---------------------------------------------------------------- area
    float minX = std::max(0.f, std::min(area.minX, area.maxX));
    float maxX = std::max(area.minX, area.maxX);
    float minY = std::max(0.f, std::min(area.minY, area.maxY));
    float maxY = std::max(area.minY, area.maxY);
    if (maxX - minX < CELL) maxX = minX + CELL;
    if (maxY - minY < CELL * 4) maxY = minY + CELL * 4;

    int cx0 = static_cast<int>(std::floor(minX / CELL));
    int cx1 = static_cast<int>(std::floor((maxX - 0.01f) / CELL));
    int cy0 = std::max(0, static_cast<int>(std::floor(minY / CELL)));
    int cy1 = static_cast<int>(std::floor((maxY - 0.01f) / CELL));
    auto inX = [&](int cx) { return cx >= cx0 && cx <= cx1; };
    auto inY = [&](int cy) { return cy >= cy0 && cy <= cy1; };

    // ---------------------------------------------------------------- parts per slot
    std::vector<PartInfo const*> slotParts[SLOT_COUNT];
    WeightedPicker slotPicker[SLOT_COUNT];
    for (int s = 0; s < SLOT_COUNT; ++s) {
        std::vector<float> w;
        for (auto const& p : parts) {
            if (static_cast<int>(p.slot) != s) continue;
            float pw = p.weight * tagBoost(plan, p.tag);
            if (pw <= 0.f) continue;
            slotParts[s].push_back(&p);
            w.push_back(pw);
        }
        slotPicker[s].build(w);
    }

    // ---------------------------------------------------------------- sites
    auto roomUp = [&](int cx, int cy, int maxCells) {
        int n = 0;
        while (n < maxCells && !grid.isBlocked(cx, cy + n)) ++n;
        return n;
    };
    auto roomDown = [&](int cx, int cy, int maxCells) {
        int n = 0;
        while (n < maxCells && cy - n >= 0 && !grid.isBlocked(cx, cy - n)) ++n;
        return n;
    };

    std::vector<Site> ground, hanging, floating;

    // the floor
    if (cy0 == 0) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            if (!grid.isBlocked(cx, 0)) {
                ground.push_back({cx * CELL + CELL / 2, 0.f, roomUp(cx, 0, 4) * CELL, 1.f});
            }
        }
    }

    // block tops and undersides
    auto cells = grid.solidCells;
    std::sort(cells.begin(), cells.end());
    int topCell = -1;
    for (auto [cx, cy] : cells) {
        if (!inX(cx)) continue;
        topCell = std::max(topCell, cy);
        if (inY(cy + 1) && !grid.isBlocked(cx, cy + 1)) {
            ground.push_back({cx * CELL + CELL / 2, (cy + 1) * CELL, roomUp(cx, cy + 1, 4) * CELL, 1.5f});
        }
        if (cy >= 2 && inY(cy - 1) && !grid.isBlocked(cx, cy - 1)) {
            int room = roomDown(cx, cy - 1, 5);
            if (room >= 2) hanging.push_back({cx * CELL + CELL / 2, cy * CELL, room * CELL, 1.f});
        }
    }

    // floating: empty cells within R cells of gameplay (multi-source BFS)
    int R = static_cast<int>(std::lround(2.f + (1.f - plan.clustering) * 6.f));
    {
        std::vector<std::pair<int, int>> seeds;
        for (auto key : grid.blocked) {
            int cx, cy;
            decode(key, cx, cy);
            if (cx < cx0 - R || cx > cx1 + R || cy > cy1 + R || cy < 0) continue;
            seeds.push_back({cx, cy});
            if (inX(cx)) topCell = std::max(topCell, cy);
        }
        std::sort(seeds.begin(), seeds.end());
        std::unordered_map<int64_t, int> dist;
        std::deque<std::pair<int, int>> q;
        for (auto [cx, cy] : seeds) {
            dist[cellKey(cx, cy)] = 0;
            q.push_back({cx, cy});
        }
        while (!q.empty()) {
            auto [cx, cy] = q.front();
            q.pop_front();
            int d = dist[cellKey(cx, cy)];
            if (d >= R) continue;
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (!dx && !dy) continue;
                    int nx = cx + dx, ny = cy + dy;
                    if (ny < 0 || nx < cx0 - R || nx > cx1 + R || ny > cy1 + R) continue;
                    auto k = cellKey(nx, ny);
                    if (dist.count(k) || grid.isBlocked(nx, ny)) continue;
                    dist[k] = d + 1;
                    q.push_back({nx, ny});
                }
            }
        }
        for (auto const& [k, d] : dist) {
            if (d < 1) continue;
            int cx, cy;
            decode(k, cx, cy);
            if (!inX(cx) || !inY(cy) || cy < 1) continue;
            float w = std::exp(-(d - 1) * plan.clustering * 0.7f);
            // inside a tunnel (solid ceiling close above): keep the player's path readable
            for (int up = 1; up <= 5; ++up) {
                if (grid.isSolid(cx, cy + up)) {
                    w *= 0.35f;
                    break;
                }
            }
            floating.push_back({cx * CELL + CELL / 2, cy * CELL + CELL / 2, 0.f, w});
        }
    }
    if (floating.empty()) {
        // empty level / empty area: fill a band above the floor
        int lo = std::max(cy0, 2), hi = std::min(cy1, std::max(lo, 12));
        for (int cx = cx0; cx <= cx1; ++cx) {
            for (int cy = lo; cy <= hi; ++cy) {
                if (!grid.isBlocked(cx, cy)) floating.push_back({cx * CELL + CELL / 2, cy * CELL + CELL / 2, 0.f, 1.f});
            }
        }
    }
    std::sort(ground.begin(), ground.end(), siteLess);
    std::sort(hanging.begin(), hanging.end(), siteLess);
    std::sort(floating.begin(), floating.end(), siteLess);

    auto buildSitePicker = [](std::vector<Site> const& sites) {
        WeightedPicker p;
        std::vector<float> w;
        w.reserve(sites.size());
        for (auto const& s : sites) w.push_back(s.weight);
        p.build(w);
        return p;
    };
    WeightedPicker groundPick = buildSitePicker(ground);
    WeightedPicker hangPick = buildSitePicker(hanging);
    WeightedPicker floatPick = buildSitePicker(floating);

    // sky band: above the gameplay, inside the area
    float skyLo = topCell >= 0 ? (topCell + 3) * CELL : 150.f;
    skyLo = std::max(skyLo, minY);
    float skyHi = maxY;
    if (skyHi - skyLo < CELL * 3) {
        skyLo = minY + (maxY - minY) * 0.55f;
        skyHi = maxY;
    }

    // ---------------------------------------------------------------- budgets
    bool ok[SLOT_COUNT] = {
        !ground.empty() && !slotPicker[0].empty(),
        !hanging.empty() && !slotPicker[1].empty(),
        !floating.empty() && !slotPicker[2].empty(),
        !slotPicker[3].empty(),
        !slotPicker[4].empty(),
    };
    float w[SLOT_COUNT];
    float total = 0.f;
    for (int s = 0; s < SLOT_COUNT; ++s) {
        w[s] = ok[s] ? std::max(0.f, plan.slotWeight[s]) : 0.f;
        total += w[s];
    }
    if (total <= 0.f) {
        for (int s = 0; s < SLOT_COUNT; ++s) {
            w[s] = ok[s] ? 1.f : 0.f;
            total += w[s];
        }
        if (total <= 0.f) {
            out.notes.push_back("Nothing could be placed (no usable decoration objects).");
            return out;
        }
    }
    int budget[SLOT_COUNT];
    int assigned = 0;
    std::vector<std::pair<float, int>> rema;
    for (int s = 0; s < SLOT_COUNT; ++s) {
        float exact = count * w[s] / total;
        budget[s] = static_cast<int>(std::floor(exact));
        assigned += budget[s];
        if (w[s] > 0.f) rema.push_back({exact - budget[s], s});
    }
    std::sort(rema.begin(), rema.end(), [](auto& a, auto& b) { return a.first > b.first; });
    for (int i = 0; assigned < count && !rema.empty(); ++i, ++assigned) {
        budget[rema[i % rema.size()].second]++;
    }

    // ---------------------------------------------------------------- placement
    std::unordered_map<int64_t, int> used; // de-duplicate near-identical spots
    auto spotFree = [&](float x, float y, float q) {
        auto k = cellKey(static_cast<int>(std::floor(x / q)), static_cast<int>(std::floor(y / q)));
        return used[k]++ == 0;
    };
    auto emit = [&](Placed p) {
        out.perSlot[static_cast<int>(p.slot)]++;
        out.objects.push_back(p);
    };
    auto pickPart = [&](int s) -> PartInfo const& { return *slotParts[s][slotPicker[s].pick(rng)]; };

    int carry = 0; // objects a slot couldn't place, handed to the next slot

    // Structure: background skyline pillars of no-touch blocks
    {
        int want = budget[4];
        int left = want;
        if (left > 0 && ok[4]) {
            int widthCells = cx1 - cx0 + 1;
            float avgH = 6.f * std::clamp(plan.scaleMul, 0.5f, 2.f);
            int nPillars = std::max(1, static_cast<int>(std::lround(left / (avgH * 1.4f))));
            nPillars = std::min(nPillars, std::max(1, widthCells / 2));
            float spacing = widthCells / static_cast<float>(nPillars);
            std::unordered_map<int64_t, bool> usedCells;
            auto addPillar = [&](int center, int pw, int ph) {
                PartInfo const& p = pickPart(4);
                int zo = rng.irange(-45, -35);
                for (int dy = 0; dy < ph && left > 0; ++dy) {
                    for (int dx = 0; dx < pw && left > 0; ++dx) {
                        int cx = center + dx, cy = dy;
                        if (!inX(cx) || grid.isBlocked(cx, cy)) continue;
                        auto k = cellKey(cx, cy);
                        if (usedCells[k]) continue;
                        usedCells[k] = true;
                        Placed o;
                        o.id = p.id;
                        o.x = cx * CELL + CELL / 2;
                        o.y = cy * CELL + CELL / 2;
                        o.role = Role::Structure;
                        o.zLayer = -3; // B4
                        o.zOrder = zo;
                        o.noTouch = true;
                        o.slot = Slot::Structure;
                        emit(o);
                        --left;
                    }
                }
            };
            for (int i = 0; i < nPillars && left > 0; ++i) {
                int center = cx0 + static_cast<int>(spacing * (i + 0.5f) + rng.range(-spacing * 0.3f, spacing * 0.3f));
                int pw = (rng.f() < 0.35f ? 2 : 1) + (plan.scaleMul > 1.3f ? 1 : 0);
                int ph = std::clamp(static_cast<int>(std::lround(rng.range(0.4f, 1.6f) * avgH)), 2, 18);
                int fair = left / (nPillars - i);
                ph = std::min(ph, std::max(2, fair / pw + 1));
                addPillar(center, pw, ph);
            }
            // leftover blocks: grow random pillars upward
            for (int tries = 0; left > 0 && tries < want * 4 + 20; ++tries) {
                addPillar(rng.irange(cx0, cx1), 1, rng.irange(2, 20));
            }
        }
        carry += left;
    }

    // Ground
    {
        int want = budget[0];
        int placed = 0;
        for (int i = 0; i < want && ok[0]; ++i) {
            bool done = false;
            for (int attempt = 0; attempt < 6 && !done; ++attempt) {
                Site const& s = ground[groundPick.pick(rng)];
                PartInfo const& p = pickPart(0);
                float sc = rng.range(p.scaleLo, p.scaleHi) * plan.scaleMul;
                float h = p.h * sc;
                if (s.room > 0.f && h > s.room * 0.95f) {
                    sc *= (s.room * 0.95f) / h;
                    h = p.h * sc;
                }
                if (sc < 0.25f) continue;
                float x = s.x + rng.range(-12.f, 12.f);
                if (!spotFree(x, s.y, 5.f) && attempt < 5) continue;
                Placed o;
                o.id = p.id;
                o.x = x;
                o.y = s.y + h / 2.f - 2.f; // sink a little so it looks rooted
                o.rot = plan.spin > 0.5f ? rng.range(-6.f, 6.f) * plan.spin : 0.f;
                o.scale = sc;
                o.flipX = rng.f() < 0.5f;
                o.role = p.tag == "pole" ? Role::Accent : Role::Primary;
                o.zLayer = 3; // B1: just behind gameplay, so the bottom hides behind blocks
                o.zOrder = rng.irange(-2, 2);
                o.slot = Slot::Ground;
                emit(o);
                done = true;
                ++placed;
            }
        }
        carry += budget[0] - placed;
    }

    // Hanging
    {
        int placed = 0;
        for (int i = 0; i < budget[1] && ok[1]; ++i) {
            bool done = false;
            for (int attempt = 0; attempt < 6 && !done; ++attempt) {
                Site const& s = hanging[hangPick.pick(rng)];
                PartInfo const& p = pickPart(1);
                float sc = rng.range(p.scaleLo, p.scaleHi) * plan.scaleMul;
                float h = p.h * sc;
                float maxH = s.room - CELL * 0.5f;
                if (h > maxH) {
                    sc *= maxH / h;
                    h = p.h * sc;
                }
                if (sc < 0.25f) continue;
                float x = s.x + rng.range(-10.f, 10.f);
                if (!spotFree(x, s.y, 6.f) && attempt < 5) continue;
                Placed o;
                o.id = p.id;
                o.x = x;
                o.y = s.y - h / 2.f + 2.f;
                o.rot = (p.tag == "spikes" || p.tag == "pole") ? 180.f : 0.f;
                o.scale = sc;
                o.flipX = rng.f() < 0.5f;
                o.role = p.tag == "spikes" ? Role::Primary : Role::Accent;
                o.zLayer = 3;
                o.zOrder = rng.irange(-2, 2);
                o.slot = Slot::Hanging;
                emit(o);
                done = true;
                ++placed;
            }
        }
        carry += budget[1] - placed;
    }

    // Floating (also absorbs what other slots couldn't place)
    {
        int want = budget[2];
        if (ok[2]) {
            want += carry;
            carry = 0;
        }
        int placed = 0;
        for (int i = 0; i < want && ok[2]; ++i) {
            for (int attempt = 0; attempt < 6; ++attempt) {
                Site const& s = floating[floatPick.pick(rng)];
                PartInfo const& p = pickPart(2);
                float x = s.x + rng.range(-13.f, 13.f);
                float y = s.y + rng.range(-13.f, 13.f);
                if (!spotFree(x, y, 6.f) && attempt < 5) continue;
                Placed o;
                o.id = p.id;
                o.x = x;
                o.y = y;
                o.scale = rng.range(p.scaleLo, p.scaleHi) * plan.scaleMul;
                o.rot = plan.spin * rng.range(0.f, 360.f);
                o.role = p.tag == "dot" ? Role::Secondary : (rng.f() < 0.65f ? Role::Accent : Role::Secondary);
                o.zLayer = 1; // B2
                o.zOrder = rng.irange(-3, 3);
                o.slot = Slot::Floating;
                emit(o);
                ++placed;
                break;
            }
        }
        carry += want - placed;
    }

    // Sky (absorbs anything still left)
    {
        int want = budget[3];
        if (ok[3]) {
            want += carry;
            carry = 0;
        }
        for (int i = 0; i < want && ok[3]; ++i) {
            PartInfo const& p = pickPart(3);
            float x = rng.range(minX, maxX);
            float y = rng.range(skyLo, skyHi);
            spotFree(x, y, 8.f);
            Placed o;
            o.id = p.id;
            o.x = x;
            o.y = y;
            o.scale = rng.range(p.scaleLo, p.scaleHi) * plan.scaleMul;
            bool cloud = p.tag == "cloud";
            o.rot = cloud ? 0.f : rng.range(0.f, 360.f);
            o.flipX = cloud && rng.f() < 0.5f;
            o.role = Role::Sky;
            o.zLayer = -3; // B4
            o.zOrder = rng.irange(-25, -15);
            o.slot = Slot::Sky;
            emit(o);
        }
    }

    char buf[200];
    std::snprintf(buf, sizeof(buf), "Placed %d: %d ground, %d hanging, %d floating, %d sky, %d skyline",
                  static_cast<int>(out.objects.size()), out.perSlot[0], out.perSlot[1], out.perSlot[2], out.perSlot[3],
                  out.perSlot[4]);
    out.notes.push_back(buf);
    if (carry > 0) {
        std::snprintf(buf, sizeof(buf), "%d objects had no free spot in this area.", carry);
        out.notes.push_back(buf);
    }
    return out;
}

} // namespace aplus
