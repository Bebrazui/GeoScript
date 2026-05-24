#include "IDEButton.hpp"
#include "GeoIDE.hpp"

using namespace geode::prelude;

CCMenuItemSpriteExtra* IDEButton::create(SEL_MenuHandler callback, CCObject* target) {
    auto spr = ButtonSprite::create("IDE", "goldFont.fnt", "GJ_button_01.png", .8f);
    return CCMenuItemSpriteExtra::create(spr, target, callback);
}
