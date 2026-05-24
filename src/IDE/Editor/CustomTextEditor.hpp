#include <Geode/Geode.hpp>
#include <vector>
#include <string>

using namespace geode::prelude;

class CustomTextEditor : public cocos2d::CCLayer, public CCKeyboardDelegate, public TextInputDelegate {
protected:
    std::vector<std::string> m_lines;
    int m_cursorLine = 0;
    int m_cursorCol = 0;
    std::string m_clipboard;
    float m_lineHeight = 16.f;
    float m_fontSize = 0.5f;
    std::vector<CCLabelBMFont*> m_lineLabels;
    std::vector<CCLabelBMFont*> m_lineNumberLabels;
    CCNode* m_cursorNode;
    CCTextInputNode* m_input;

public:
    static CustomTextEditor* create(CCSize size);
    bool init(CCSize size);
    void redraw();
    void keyDown(enumKeyCodes key, double) override;
    void keyUp(enumKeyCodes key, double) override {}
    void textChanged(CCTextInputNode* input) override;
    std::string getText();
    void onEnterPressed();
};
