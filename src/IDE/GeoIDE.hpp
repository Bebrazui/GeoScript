#pragma once
#include <Geode/Geode.hpp>

class CustomTextEditor;

class GeoIDE : public cocos2d::CCLayerColor, public cocos2d::CCKeyboardDelegate {
protected:
    CustomTextEditor* m_editor;
    cocos2d::CCNode* m_consoleNode;
    cocos2d::CCLabelBMFont* m_consoleText;

public:
    static GeoIDE* create();
    virtual bool init() override;

    void onCloseBtn(cocos2d::CCObject*);
    void onCompile(cocos2d::CCObject*);

    virtual bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

    virtual void keyDown(cocos2d::enumKeyCodes key, double) override;
    virtual void keyUp(cocos2d::enumKeyCodes key, double) override {}

    cocos2d::CCNode* createCustomCloseButton();
};