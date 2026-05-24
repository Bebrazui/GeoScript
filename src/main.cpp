#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCIMEDispatcher.hpp>
#include <unordered_set>
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
            // Use the key parameter to reject navigation/special keys
            // that GD sends as text (arrows send 'a','b','c','d' etc.)
            static const std::unordered_set<int> BLOCKED_KEYS = {
                (int)KEY_Left,  (int)KEY_Right, (int)KEY_Up,    (int)KEY_Down,
                (int)KEY_Home,  (int)KEY_End,   (int)KEY_PageUp,(int)KEY_PageDown,
                (int)KEY_Enter, (int)KEY_NumEnter,
                (int)KEY_Backspace, (int)KEY_Delete,
                (int)KEY_Escape, (int)KEY_Tab,
                (int)KEY_Shift, (int)KEY_LeftShift,  (int)KEY_RightShift,
                (int)KEY_Control,(int)KEY_LeftControl,(int)KEY_RightContol,
                (int)KEY_Alt,   (int)KEY_F1, (int)KEY_F2, (int)KEY_F3,
                (int)KEY_F4,    (int)KEY_F5, (int)KEY_F6, (int)KEY_F7,
                (int)KEY_F8,    (int)KEY_F9, (int)KEY_F10,(int)KEY_F11,(int)KEY_F12,
            };
            if (BLOCKED_KEYS.count((int)key)) return;

            // Also filter raw control bytes just in case
            std::string s(text, len);
            std::string filtered;
            for (size_t i = 0; i < s.size(); ) {
                unsigned char ch = (unsigned char)s[i];
                int sl = (ch >= 0xF0) ? 4 : (ch >= 0xE0) ? 3 : (ch >= 0xC0) ? 2 : 1;
                if ((ch < 0x20 && ch != '\t') || ch == 0x7F) { i += sl; continue; }
                filtered += s.substr(i, sl);
                i += sl;
            }
            if (!filtered.empty()) {
                geode::log::debug("IME insert: key=0x{:X} text='{}'", (int)key, filtered);
                ide->insertText(filtered);
            }
            return;
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
