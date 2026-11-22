#pragma once
#include "DecoPlan.hpp"

namespace aplus {

// The built-in, offline "AI". It reads the description (themes, colors,
// objects, negations like "no clouds", size/density/chaos words) and designs
// a DecoPlan. Works with no internet and no API key.
DecoPlan interpretDescription(std::string const& description);

// Example prompts for the dice button.
std::vector<std::string> const& ideaPrompts();

std::string toLowerAscii(std::string s);

// Small color helpers (also used by the Claude parser).
RGB mixColor(RGB a, RGB b, float t);
RGB shiftHue(RGB c, float degrees);
RGB scaleColor(RGB c, float factor);

} // namespace aplus
