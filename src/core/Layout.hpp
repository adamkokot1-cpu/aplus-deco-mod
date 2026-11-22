#pragma once
#include "Parts.hpp"

#include <unordered_set>

namespace aplus {

constexpr float CELL = 30.f; // one GD grid block

inline int64_t cellKey(int cx, int cy) {
    return (static_cast<int64_t>(cx) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(cy));
}

// What the level looks like, as a 30x30 grid. Cell (cx, cy) covers
// x in [cx*30, cx*30+30), y in [cy*30, cy*30+30). The floor is at y = 0.
struct LevelGrid {
    std::unordered_set<int64_t> solid;   // cells with collidable blocks / slopes
    std::unordered_set<int64_t> blocked; // any gameplay object (solids, hazards, orbs, portals...)
    std::vector<std::pair<int, int>> solidCells; // same as `solid`, iterable in a stable order

    void addSolid(int cx, int cy);
    void addBlocked(int cx, int cy);
    bool isSolid(int cx, int cy) const { return solid.count(cellKey(cx, cy)) > 0; }
    bool isBlocked(int cx, int cy) const { return cy < 0 || blocked.count(cellKey(cx, cy)) > 0; }
};

struct Area {
    float minX = 0, maxX = 3000, minY = 0, maxY = 600;
};

struct Placed {
    int id = 1;
    float x = 0, y = 0;
    float rot = 0;
    float scale = 1;
    bool flipX = false;
    Role role = Role::Primary;
    int zLayer = 0; // GD z-layer value: B4=-3, B3=-1, B2=1, B1=3, T1=5
    int zOrder = 0;
    bool noTouch = false;
    Slot slot = Slot::Floating;
};

struct LayoutResult {
    std::vector<Placed> objects;
    int perSlot[SLOT_COUNT] = {0, 0, 0, 0, 0};
    std::vector<std::string> notes;
};

// Places exactly `count` objects (fewer only if there is literally nowhere to put them).
LayoutResult layoutDeco(
    DecoPlan const& plan, LevelGrid const& grid, Area const& area,
    std::vector<PartInfo> const& parts, int count, uint32_t seed
);

} // namespace aplus
