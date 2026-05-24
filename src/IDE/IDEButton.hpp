#pragma once
#include <Geode/Geode.hpp>
using namespace geode::prelude;

class IDEButton {
public:
    static CCMenuItemSpriteExtra* create(SEL_MenuHandler callback, CCObject* target);
};