#pragma once
#include <Geode/Geode.hpp>
using namespace geode::prelude;

class CodeEditor : public CCNode, public TextInputDelegate {
protected:
    CCLabelBMFont* m_lineNumbers;
    CCTextInputNode* m_input;
    float m_editorWidth;
    float m_editorHeight;
    float m_lineNumWidth;
    float m_lineHeight;

    void updateLineNumbers(std::string const& text);

public:
    static CodeEditor* create();
    bool init() override;
    void textChanged(CCTextInputNode* input) override;
};