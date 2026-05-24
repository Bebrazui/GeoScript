#include "CodeEditor.hpp"
using namespace geode::prelude;

CodeEditor* CodeEditor::create() {
    auto ret = new CodeEditor();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool CodeEditor::init() {
    if (!CCNode::init()) return false;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    m_editorWidth = winSize.width - 160.f;
    m_editorHeight = winSize.height;
    m_lineNumWidth = 45.f;
    m_lineHeight = 18.f;

    this->setContentSize({m_editorWidth, m_editorHeight});

    // Фон редактора
    auto bgMain = CCLayerColor::create({18, 18, 18, 255}, m_editorWidth, m_editorHeight);
    this->addChild(bgMain, 0);

    // Фон панели номеров
    auto bgLines = CCLayerColor::create({25, 25, 25, 255}, m_lineNumWidth, m_editorHeight);
    this->addChild(bgLines, 1);

    // Разделитель между номерами и кодом
    auto divider = CCLayerColor::create({50, 50, 50, 255}, 1.f, m_editorHeight);
    divider->setPosition({m_lineNumWidth, 0});
    this->addChild(divider, 2);

    // Нумерация строк
    m_lineNumbers = CCLabelBMFont::create("", "chatFont.fnt");
    m_lineNumbers->setAnchorPoint({1.f, 1.f});
    m_lineNumbers->setPosition({m_lineNumWidth - 8.f, m_editorHeight - 5.f});
    m_lineNumbers->setScale(0.5f);
    m_lineNumbers->setColor({75, 75, 75});
    this->addChild(m_lineNumbers, 3);

    // Поле ввода
    float inputWidth = m_editorWidth - m_lineNumWidth - 20.f;
    m_input = CCTextInputNode::create(
        inputWidth,
        m_editorHeight - 16.f,
        "Enter GeoScript...",
        "chatFont.fnt"
    );
    m_input->setAnchorPoint({0, 0.5f});
    m_input->setPosition({m_lineNumWidth + 10.f, m_editorHeight / 2});
    m_input->setDelegate(this);
    m_input->m_textColor = {212, 212, 212};
    this->addChild(m_input, 3);

    updateLineNumbers("");
    return true;
}

void CodeEditor::textChanged(CCTextInputNode* input) {
    updateLineNumbers(input->getString());
}

void CodeEditor::updateLineNumbers(std::string const& text) {
    int lines = 1;
    for (char c : text) if (c == '\n') lines++;
    // минимум 30 строк видно сразу
    lines = std::max(lines, 30);

    std::string nums;
    for (int i = 1; i <= lines; i++) {
        nums += std::to_string(i);
        if (i < lines) nums += "\n";
    }
    m_lineNumbers->setString(nums.c_str());
}