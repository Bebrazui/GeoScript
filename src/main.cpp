#include <Geode/modify/EditorUI.hpp>
#include "IDE/IDEButton.hpp"
#include "IDE/GeoIDE.hpp"

using namespace geode::prelude;

class $modify(MyEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto menu = CCMenu::create();
        this->addChild(menu);

        auto btn = IDEButton::create(menu_selector(MyEditorUI::onOpenIDE), this);
        menu->addChild(btn);

        // Позиционируем справа чуть выше середины
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        menu->setPosition({ winSize.width - 50, winSize.height / 2 + 50 });

        return true;
    }

    void onOpenIDE(CCObject* sender) {
        geode::log::info("IDE button clicked!");
        auto ide = GeoIDE::create();
        CCDirector::sharedDirector()->getRunningScene()->addChild(ide, 100);
    }
};
