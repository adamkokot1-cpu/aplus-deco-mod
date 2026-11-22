#pragma once
// Connects the core logic to the real GD editor: reads the level, places objects,
// handles undo.

#include <Geode/Geode.hpp>

#include "core/DecoPlan.hpp"

namespace aplus {

enum class AreaMode : int { WholeLevel = 0, CurrentView = 1, Selection = 2 };
constexpr int AREA_MODE_COUNT = 3;
char const* areaModeName(AreaMode mode);

struct LevelStats {
    int solids = 0;
    int hazards = 0;
    int others = 0;
    float width = 0.f;
    float height = 0.f;
};

struct GenerateRequest {
    DecoPlan plan;
    int count = 300;
    AreaMode area = AreaMode::WholeLevel;
    bool colorTriggers = true;
};

struct GenerateResult {
    bool ok = false;
    int placed = 0;
    std::string message;
    std::vector<std::string> notes;
};

class DecoEngine {
public:
    static void onEditorOpened(LevelEditorLayer* layer);
    static GenerateResult generate(EditorUI* ui, GenerateRequest const& req);
    // Deletes the objects from the last generation (as one normal, undoable delete).
    static int removeLast(EditorUI* ui);
    static bool hasLast(EditorUI* ui);
    static LevelStats quickStats(EditorUI* ui, AreaMode mode);
};

} // namespace aplus
