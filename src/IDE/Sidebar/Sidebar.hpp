#pragma once
#include <Geode/Geode.hpp>

class Sidebar : public cocos2d::CCLayerColor {
public:
    static Sidebar* create();
    virtual bool init() override;
};
