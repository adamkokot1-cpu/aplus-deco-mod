// A+ Deco Mod — an AI that decorates your Geometry Dash levels.
// Adds an "A+" button to the level editor. Type a description, choose how many
// objects, press Generate.

#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

#include "DecoEngine.hpp"
#include "DecoPopup.hpp"

using namespace geode::prelude;

class $modify(APlusEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        aplus::DecoEngine::onEditorOpened(editorLayer);

        CCNode* sprite = CCSprite::create("aplus_button.png"_spr);
        if (!sprite) sprite = ButtonSprite::create("A+", "bigFont.fnt", "GJ_button_04.png", 0.6f);

        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(APlusEditorUI::onAPlusDeco));
        button->setID("aplus-deco-button"_spr);

        // Prefer the editor's own button column (provided by the node-ids mod);
        // otherwise use our own small menu under the undo/redo buttons.
        if (auto editMenu = this->getChildByID("editor-buttons-menu")) {
            sprite->setScale(0.9f);
            editMenu->addChild(button);
            editMenu->updateLayout();
        } else {
            auto winSize = CCDirector::get()->getWinSize();
            auto menu = CCMenu::create();
            menu->setID("aplus-deco-menu"_spr);
            menu->setPosition({30.f, winSize.height - 72.f});
            menu->addChild(button);
            this->addChild(menu, 100);
        }
        return true;
    }

    void onAPlusDeco(CCObject*) {
        if (auto popup = DecoPopup::create(this)) popup->show();
    }
};
