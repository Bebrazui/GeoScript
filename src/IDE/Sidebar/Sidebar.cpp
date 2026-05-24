#include "Sidebar.hpp"

using namespace geode::prelude;

Sidebar* Sidebar::create() {
    auto ret = new Sidebar();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool Sidebar::init() {
    if (!CCLayerColor::initWithColor({50, 50, 50, 255}, 150.f, 320.f))
        return false;

    return true;
}
