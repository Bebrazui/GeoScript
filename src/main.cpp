#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCIMEDispatcher.hpp>
#include "IDE/IDEButton.hpp"
#include "IDE/GeoIDE.hpp"

using namespace geode::prelude;

// Helper: find GeoIDE in the running scene
static GeoIDE* findIDE() {
    auto* scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return nullptr;
    auto* children = scene->getChildren();
    if (!children) return nullptr;
    for (int i = 0; i < (int)children->count(); i++) {
        if (auto* ide = dynamic_cast<GeoIDE*>(children->objectAtIndex(i)))
            return ide;
    }
    return nullptr;
}

// ============================================================================
// Hook CCIMEDispatcher::dispatchInsertText
// This is called for ALL printable character input (letters, digits, symbols)
// including Unicode / Cyrillic from the OS IME.
// ============================================================================
class $modify(MyCCIMEDispatcher, CCIMEDispatcher) {
    void dispatchInsertText(const char* text, int len, enumKeyCodes key) {
        if (auto* ide = findIDE()) {
            // Forward text directly to the editor
            ide->insertText(std::string(text, len));
            return; // don't pass to game inputs
        }
        CCIMEDispatcher::dispatchInsertText(text, len, key);
    }

    void dispatchDeleteBackward() {
        if (auto* ide = findIDE()) {
            ide->deleteBackward();
            return;
        }
        CCIMEDispatcher::dispatchDeleteBackward();
    }
};

// ============================================================================
// Hook CCKeyboardDispatcher for special keys (arrows, enter, backspace, ctrl…)
// ============================================================================
class $modify(MyCCKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double dt) {
        if (auto* ide = findIDE()) {
            if (isKeyDown) ide->keyDown(key, dt);
            else           ide->keyUp(key, dt);
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, dt);
    }
};

// ============================================================================
// EditorUI — add IDE button
// ============================================================================
class $modify(MyEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto menu = CCMenu::create();
        this->addChild(menu);

        auto btn = IDEButton::create(menu_selector(MyEditorUI::onOpenIDE), this);
        menu->addChild(btn);

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
