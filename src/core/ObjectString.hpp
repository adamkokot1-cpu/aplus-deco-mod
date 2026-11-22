#pragma once
#include "Layout.hpp"

namespace aplus {

struct StringOptions {
    int channels[ROLE_COUNT] = {50, 51, 52, 53, 54}; // GD color channel per role
    int groupId = 0;                                  // 0 = no group
    bool colorTriggers = true;
    float triggerX = 15.f;   // color triggers go at the very start of the level
    float triggerY = 315.f;  // first trigger height, stacked upwards by 30
};

// GD level-string format: "1,<id>,2,<x>,3,<y>,...;" per object.
std::string buildObjectString(std::vector<Placed> const& objects, StringOptions const& opt);

// One color trigger (ID 899) per role, duration 0, so the theme colors apply instantly.
std::string buildColorTriggers(DecoPlan const& plan, StringOptions const& opt);

std::string fmtNum(float v);

} // namespace aplus
