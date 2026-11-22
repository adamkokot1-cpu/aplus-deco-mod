#include "DecoPopup.hpp"

#include "ClaudeBrain.hpp"
#include "core/DecoBrain.hpp"

#include <algorithm>
#include <cstdlib>
#include <random>

using namespace geode::prelude;

namespace {
constexpr float POPUP_W = 400.f;
constexpr float POPUP_H = 290.f;

ccColor3B const COLOR_OK = {120, 255, 120};
ccColor3B const COLOR_WARN = {255, 200, 80};
ccColor3B const COLOR_ERR = {255, 100, 100};
ccColor3B const COLOR_INFO = {200, 200, 255};

CCLabelBMFont* makeLabel(char const* text, char const* font, float scale, CCPoint anchor) {
    auto label = CCLabelBMFont::create(text, font);
    label->setScale(scale);
    label->setAnchorPoint(anchor);
    return label;
}

CCMenuItemSpriteExtra* makeButton(char const* text, char const* bg, float scale, CCObject* target, SEL_MenuHandler cb) {
    auto spr = ButtonSprite::create(text, "goldFont.fnt", bg, scale);
    return CCMenuItemSpriteExtra::create(spr, target, cb);
}
} // namespace

DecoPopup* DecoPopup::create(EditorUI* editor) {
    auto ret = new DecoPopup();
    if (ret->init(editor)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool DecoPopup::init(EditorUI* editor) {
    if (!Popup::init(POPUP_W, POPUP_H)) return false;
    m_editor = editor;
    this->setTitle("A+ Deco Mod");

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    menu->setContentSize({POPUP_W, POPUP_H});
    menu->setID("aplus-deco-menu");
    m_mainLayer->addChild(menu, 5);

    // ---- description
    auto descLabel = makeLabel("Describe your deco:", "goldFont.fnt", 0.55f, {0.f, 0.5f});
    descLabel->setPosition({22.f, 238.f});
    m_mainLayer->addChild(descLabel, 5);

    auto ideaBtn = makeButton("Idea", "GJ_button_05.png", 0.5f, this, menu_selector(DecoPopup::onIdea));
    ideaBtn->setPosition({POPUP_W - 52.f, 238.f});
    menu->addChild(ideaBtn);

    m_descInput = TextInput::create(356.f, "e.g. neon city with pink glow and floating stars", "chatFont.fnt");
    m_descInput->setCommonFilter(CommonFilter::Any);
    m_descInput->setMaxCharCount(180);
    m_descInput->setPosition({POPUP_W / 2.f, 208.f});
    m_descInput->setString(Mod::get()->getSavedValue<std::string>("last-description", ""), false);
    m_mainLayer->addChild(m_descInput, 5);

    // ---- object count
    auto countLabel = makeLabel("Objects:", "goldFont.fnt", 0.55f, {0.f, 0.5f});
    countLabel->setPosition({22.f, 163.f});
    m_mainLayer->addChild(countLabel, 5);

    auto minus = makeButton("-", "GJ_button_04.png", 0.6f, this, menu_selector(DecoPopup::onCountStep));
    minus->setTag(-1);
    minus->setPosition({110.f, 163.f});
    menu->addChild(minus);

    m_countInput = TextInput::create(64.f, "300", "bigFont.fnt");
    m_countInput->setCommonFilter(CommonFilter::Uint);
    m_countInput->setMaxCharCount(5);
    m_countInput->setPosition({160.f, 163.f});
    m_mainLayer->addChild(m_countInput, 5);

    auto plus = makeButton("+", "GJ_button_04.png", 0.6f, this, menu_selector(DecoPopup::onCountStep));
    plus->setTag(1);
    plus->setPosition({210.f, 163.f});
    menu->addChild(plus);

    int presets[] = {100, 500, 2000};
    char const* presetNames[] = {"100", "500", "2K"};
    for (int i = 0; i < 3; ++i) {
        auto b = makeButton(presetNames[i], "GJ_button_02.png", 0.45f, this, menu_selector(DecoPopup::onCountPreset));
        b->setTag(presets[i]);
        b->setPosition({262.f + i * 48.f, 163.f});
        menu->addChild(b);
    }

    int savedCount = static_cast<int>(Mod::get()->getSavedValue<int64_t>(
        "last-count", Mod::get()->getSettingValue<int64_t>("default-count")
    ));
    setCount(savedCount);

    // ---- area
    auto areaBtn = makeButton("Area", "GJ_button_04.png", 0.55f, this, menu_selector(DecoPopup::onArea));
    areaBtn->setPosition({50.f, 120.f});
    menu->addChild(areaBtn);
    m_areaLabel = makeLabel("", "bigFont.fnt", 0.35f, {0.f, 0.5f});
    m_areaLabel->setPosition({84.f, 120.f});
    m_mainLayer->addChild(m_areaLabel, 5);
    updateAreaLabel();

    // ---- toggles
    m_colorToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(DecoPopup::onToggle), 0.65f);
    m_colorToggle->setPosition({240.f, 120.f});
    m_colorToggle->toggle(Mod::get()->getSettingValue<bool>("color-triggers"));
    menu->addChild(m_colorToggle);
    auto colorLabel = makeLabel("Theme colors", "bigFont.fnt", 0.35f, {0.f, 0.5f});
    colorLabel->setPosition({258.f, 120.f});
    m_mainLayer->addChild(colorLabel, 5);

    m_claudeToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(DecoPopup::onToggle), 0.65f);
    m_claudeToggle->setPosition({240.f, 86.f});
    m_claudeToggle->toggle(Mod::get()->getSettingValue<bool>("use-claude"));
    menu->addChild(m_claudeToggle);
    auto claudeLabel = makeLabel("Claude AI (online)", "bigFont.fnt", 0.33f, {0.f, 0.5f});
    claudeLabel->setPosition({258.f, 86.f});
    m_mainLayer->addChild(claudeLabel, 5);

    auto infoBtn = makeButton("AI Plan", "GJ_button_05.png", 0.55f, this, menu_selector(DecoPopup::onInfo));
    infoBtn->setPosition({70.f, 86.f});
    menu->addChild(infoBtn);

    // ---- actions
    m_generateBtn = makeButton("Generate", "GJ_button_01.png", 0.85f, this, menu_selector(DecoPopup::onGenerate));
    m_generateBtn->setPosition({145.f, 46.f});
    menu->addChild(m_generateBtn);

    auto undoBtn = makeButton("Undo Last", "GJ_button_06.png", 0.65f, this, menu_selector(DecoPopup::onUndoLast));
    undoBtn->setPosition({290.f, 46.f});
    menu->addChild(undoBtn);

    // ---- status
    m_statusLabel = makeLabel("Type a description, pick a count, press Generate!", "chatFont.fnt", 0.6f, {0.5f, 0.5f});
    m_statusLabel->setPosition({POPUP_W / 2.f, 17.f});
    m_mainLayer->addChild(m_statusLabel, 5);
    return true;
}

// ---------------------------------------------------------------- helpers

int DecoPopup::readCount() {
    // getString() is gd::string on Android, std::string elsewhere: convert explicitly
    std::string text = m_countInput ? std::string(m_countInput->getString()) : std::string();
    long v = std::strtol(text.c_str(), nullptr, 10);
    int maxCount = static_cast<int>(std::clamp<int64_t>(Mod::get()->getSettingValue<int64_t>("max-count"), 1, 100000));
    if (v <= 0) v = static_cast<long>(std::max<int64_t>(1, Mod::get()->getSettingValue<int64_t>("default-count")));
    return static_cast<int>(std::min<long>(v, maxCount));
}

void DecoPopup::setCount(int value) {
    int maxCount = static_cast<int>(std::clamp<int64_t>(Mod::get()->getSettingValue<int64_t>("max-count"), 1, 100000));
    value = std::clamp(value, 1, maxCount);
    if (m_countInput) m_countInput->setString(std::to_string(value), false);
}

void DecoPopup::setStatus(std::string const& text, ccColor3B color) {
    if (!m_statusLabel) return;
    m_statusLabel->setString(text.c_str());
    m_statusLabel->setColor(color);
    m_statusLabel->limitLabelWidth(POPUP_W - 24.f, 0.6f, 0.2f);
}

void DecoPopup::setBusy(bool busy) {
    m_busy = busy;
    if (m_generateBtn) {
        m_generateBtn->setEnabled(!busy);
        if (auto spr = typeinfo_cast<ButtonSprite*>(m_generateBtn->getNormalImage())) {
            spr->setOpacity(busy ? 120 : 255);
        }
    }
}

void DecoPopup::updateAreaLabel() {
    if (m_areaLabel) m_areaLabel->setString(aplus::areaModeName(m_area));
}

void DecoPopup::saveInputs() {
    if (m_descInput) Mod::get()->setSavedValue<std::string>("last-description", std::string(m_descInput->getString()));
    Mod::get()->setSavedValue<int64_t>("last-count", readCount());
}

// ---------------------------------------------------------------- buttons

void DecoPopup::onCountStep(CCObject* sender) {
    int dir = sender->getTag() < 0 ? -1 : 1;
    int cur = readCount();
    int step = cur < 100 ? 10 : (cur < 1000 ? 50 : 250);
    setCount(cur + dir * step);
}

void DecoPopup::onCountPreset(CCObject* sender) {
    setCount(sender->getTag());
}

void DecoPopup::onArea(CCObject*) {
    m_area = static_cast<aplus::AreaMode>((static_cast<int>(m_area) + 1) % aplus::AREA_MODE_COUNT);
    updateAreaLabel();
}

void DecoPopup::onIdea(CCObject*) {
    auto const& ideas = aplus::ideaPrompts();
    static std::mt19937 rng{std::random_device{}()};
    auto idea = ideas[std::uniform_int_distribution<size_t>(0, ideas.size() - 1)(rng)];
    m_descInput->setString(idea, false);
    setStatus("Idea: \"" + idea + "\"", COLOR_INFO);
}

void DecoPopup::onInfo(CCObject*) {
    std::vector<std::string> notes;
    std::string title;
    if (m_hasPlan) {
        notes = m_lastNotes;
        title = "Last generation";
    } else {
        notes = aplus::interpretDescription(std::string(m_descInput->getString())).notes;
        title = "AI plan preview";
    }
    std::string text;
    for (auto const& n : notes) text += n + "\n";
    FLAlertLayer::create(title.c_str(), text, "OK")->show();
}

void DecoPopup::onUndoLast(CCObject*) {
    if (m_busy) return;
    int n = aplus::DecoEngine::removeLast(m_editor);
    if (n > 0) setStatus(fmt::format("Removed {} objects from the last generation (Ctrl+Z brings them back)", n), COLOR_OK);
    else setStatus("Nothing to undo from A+ in this session.", COLOR_WARN);
}

void DecoPopup::onGenerate(CCObject*) {
    if (m_busy) return;
    saveInputs();
    std::string desc = std::string(m_descInput->getString());
    auto basePlan = aplus::interpretDescription(desc);

    bool useClaude = m_claudeToggle && m_claudeToggle->isToggled();
    if (!useClaude) {
        placeWithPlan(basePlan, "");
        return;
    }

    auto key = Mod::get()->getSettingValue<std::string>("claude-api-key");
    // pasted keys often carry spaces / newlines
    auto isSpace = [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; };
    while (!key.empty() && isSpace(key.back())) key.pop_back();
    while (!key.empty() && isSpace(key.front())) key.erase(key.begin());
    if (key.empty()) {
        FLAlertLayer::create(
            "Claude AI",
            "To use <cy>Claude</c>, paste your Anthropic API key into the <cg>A+ Deco Mod settings</c> "
            "(Geode menu > A+ Deco Mod > Settings).\n\nThis time the <co>built-in AI</c> decorated your level.",
            "OK"
        )->show();
        placeWithPlan(basePlan, "No API key - used the built-in AI");
        return;
    }

    auto model = Mod::get()->getSettingValue<std::string>("claude-model");
    if (model.empty()) model = "claude-sonnet-5-5";
    auto stats = aplus::DecoEngine::quickStats(m_editor, m_area);
    aplus::ClaudeContext ctx;
    ctx.description = desc;
    ctx.count = readCount();
    ctx.solids = stats.solids;
    ctx.hazards = stats.hazards;
    ctx.width = stats.width;
    ctx.height = stats.height;

    web::WebRequest req;
    aplus::configureClaudeRequest(req, key, model, ctx);

    setBusy(true);
    setStatus("Claude is designing your deco...", COLOR_WARN);
    m_request.spawn(req.post(aplus::claudeUrl()), [this, basePlan](web::WebResponse res) {
        setBusy(false);
        auto plan = basePlan;
        std::string error;
        if (aplus::applyClaudeResponse(res, plan, error)) {
            placeWithPlan(plan, "");
        } else {
            log::warn("A+ Deco: {}", error);
            placeWithPlan(basePlan, error + " - used the built-in AI instead");
        }
    });
}

void DecoPopup::placeWithPlan(aplus::DecoPlan plan, std::string const& extraNote) {
    aplus::GenerateRequest req;
    req.plan = plan;
    req.count = readCount();
    req.area = m_area;
    req.colorTriggers = m_colorToggle ? m_colorToggle->isToggled() : true;

    auto res = aplus::DecoEngine::generate(m_editor, req);

    m_hasPlan = true;
    m_lastPlan = plan;
    m_lastNotes.clear();
    if (!extraNote.empty()) m_lastNotes.push_back("Note: " + extraNote);
    m_lastNotes.push_back("Brain: " + plan.source);
    for (auto const& n : plan.notes) m_lastNotes.push_back(n);
    for (auto const& n : res.notes) m_lastNotes.push_back(n);

    if (!res.ok) {
        setStatus(res.message, COLOR_ERR);
        return;
    }
    auto summary = fmt::format("{} - {} ({})", res.message, plan.theme, plan.source);
    if (!extraNote.empty()) {
        setStatus(summary + " | " + extraNote, COLOR_WARN);
    } else {
        setStatus(summary, COLOR_OK);
    }

    if (!Mod::get()->getSettingValue<bool>("keep-open")) {
        Notification::create("A+ Deco: " + summary, NotificationIcon::Success, 2.5f)->show();
        // close on the next frame (we may be inside a web callback owned by this popup)
        this->scheduleOnce(schedule_selector(DecoPopup::closeLater), 0.f);
    }
}

void DecoPopup::closeLater(float) {
    this->onClose(nullptr);
}
