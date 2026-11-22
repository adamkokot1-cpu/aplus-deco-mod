#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include "DecoEngine.hpp"
#include "core/DecoPlan.hpp"

class DecoPopup : public geode::Popup {
protected:
    EditorUI* m_editor = nullptr;
    geode::TextInput* m_descInput = nullptr;
    geode::TextInput* m_countInput = nullptr;
    cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
    cocos2d::CCLabelBMFont* m_areaLabel = nullptr;
    CCMenuItemToggler* m_colorToggle = nullptr;
    CCMenuItemToggler* m_claudeToggle = nullptr;
    CCMenuItemSpriteExtra* m_generateBtn = nullptr;

    aplus::AreaMode m_area = aplus::AreaMode::WholeLevel;
    bool m_busy = false;
    bool m_hasPlan = false;
    aplus::DecoPlan m_lastPlan;
    std::vector<std::string> m_lastNotes;

    geode::async::TaskHolder<geode::utils::web::WebResponse> m_request;

    bool init(EditorUI* editor);

    void onGenerate(cocos2d::CCObject*);
    void onUndoLast(cocos2d::CCObject*);
    void onCountStep(cocos2d::CCObject* sender); // tag = delta
    void onCountPreset(cocos2d::CCObject* sender); // tag = value
    void onArea(cocos2d::CCObject*);
    void onIdea(cocos2d::CCObject*);
    void onInfo(cocos2d::CCObject*);
    void onToggle(cocos2d::CCObject*) {}

    int readCount();
    void setCount(int value);
    void setStatus(std::string const& text, cocos2d::ccColor3B color);
    void setBusy(bool busy);
    void updateAreaLabel();
    void placeWithPlan(aplus::DecoPlan plan, std::string const& extraNote);
    void saveInputs();
    void closeLater(float);

public:
    static DecoPopup* create(EditorUI* editor);
};
