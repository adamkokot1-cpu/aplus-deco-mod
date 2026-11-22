#include "ObjectString.hpp"

#include <cmath>
#include <cstdio>

namespace aplus {

// Object keys (GD level string):
//  1 id, 2 x, 3 y, 4 flip x, 6 rotation, 20 editor layer, 21 base color, 22 detail color,
//  24 z layer, 25 z order, 57 groups, 96 no glow, 121 no touch, 128 scale x, 129 scale y.
// Color trigger keys: 7/8/9 RGB, 10 duration, 17 blending, 23 target channel, 35 opacity.

std::string fmtNum(float v) {
    if (!std::isfinite(v)) v = 0.f;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

std::string buildObjectString(std::vector<Placed> const& objects, StringOptions const& opt) {
    std::string out;
    out.reserve(objects.size() * 72);
    for (auto const& o : objects) {
        int ch = opt.channels[static_cast<int>(o.role)];
        out += "1," + std::to_string(o.id);
        out += ",2," + fmtNum(o.x);
        out += ",3," + fmtNum(o.y);
        if (o.flipX) out += ",4,1";
        float rot = std::fmod(o.rot, 360.f);
        if (std::fabs(rot) > 0.01f) out += ",6," + fmtNum(rot);
        if (std::fabs(o.scale - 1.f) > 0.001f) {
            auto s = fmtNum(o.scale);
            out += ",128," + s + ",129," + s;
        }
        out += ",21," + std::to_string(ch) + ",22," + std::to_string(ch);
        out += ",24," + std::to_string(o.zLayer);
        out += ",25," + std::to_string(o.zOrder);
        if (o.noTouch) out += ",121,1,96,1";
        if (opt.groupId > 0) out += ",57," + std::to_string(opt.groupId);
        out += ";";
    }
    return out;
}

std::string buildColorTriggers(DecoPlan const& plan, StringOptions const& opt) {
    if (!opt.colorTriggers) return "";
    std::string out;
    for (int r = 0; r < ROLE_COUNT; ++r) {
        RGB c = plan.colors[r];
        bool blend = plan.glow && (r == static_cast<int>(Role::Secondary) || r == static_cast<int>(Role::Accent));
        out += "1,899";
        out += ",2," + fmtNum(opt.triggerX);
        out += ",3," + fmtNum(opt.triggerY + r * 30.f);
        out += ",7," + std::to_string(c.r);
        out += ",8," + std::to_string(c.g);
        out += ",9," + std::to_string(c.b);
        out += ",10,0";
        out += ",23," + std::to_string(opt.channels[r]);
        out += ",35," + fmtNum(std::fmax(0.f, std::fmin(1.f, plan.opacity[r])));
        if (blend) out += ",17,1";
        out += ";";
    }
    return out;
}

} // namespace aplus
