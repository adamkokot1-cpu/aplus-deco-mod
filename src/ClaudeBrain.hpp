#pragma once
// Optional online brain: asks Claude (Anthropic API) to design the DecoPlan.
// The placement itself is always done locally by the engine.

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

#include "core/DecoPlan.hpp"

namespace aplus {

struct ClaudeContext {
    std::string description;
    int count = 300;
    int solids = 0;
    int hazards = 0;
    float width = 0.f;
    float height = 0.f;
};

char const* claudeUrl();

// Fills headers + JSON body for POST https://api.anthropic.com/v1/messages
void configureClaudeRequest(
    geode::utils::web::WebRequest& req, std::string const& apiKey, std::string const& model, ClaudeContext const& ctx
);

// Reads Claude's reply and applies it on top of `plan` (which already holds the
// built-in interpretation, so anything Claude leaves out keeps a sensible value).
// Returns false and fills `error` if the request failed or the reply was unusable.
bool applyClaudeResponse(geode::utils::web::WebResponse& res, DecoPlan& plan, std::string& error);

} // namespace aplus
