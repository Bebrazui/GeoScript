#include "CustomTextEditor.hpp"

CustomTextEditor* CustomTextEditor::create(CCSize size) {
    auto ret = new CustomTextEditor();
    if (ret && ret->init(size)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool CustomTextEditor::init(CCSize size) {
    if (!CCLayer::init()) return false;
    this->setContentSize(size);
    this->setKeyboardEnabled(true);

    auto bg = CCDrawNode::create();
    bg->drawRect({0, 0}, {size.width, size.height}, {0, 0, 0, 255}, 1.f, {50, 50, 50, 255});
    this->addChild(bg, -1);

    m_input = CCTextInputNode::create(size.width, size.height, "", "chatFont.fnt");
    m_input->setDelegate(this);
    m_input->setVisible(false);
    this->addChild(m_input);

    m_lines.push_back("");

    m_cursorNode = CCNode::create();
    auto cursorDraw = CCDrawNode::create();
    cursorDraw->drawRect({0, 0}, {2, m_lineHeight}, {255, 255, 255, 255}, 0, {255, 255, 255, 255});
    m_cursorNode->addChild(cursorDraw);
    m_cursorNode->runAction(CCRepeatForever::create(CCSequence::create(CCFadeOut::create(0.5f), CCFadeIn::create(0.5f), nullptr)));
    this->addChild(m_cursorNode, 10);

    redraw();
    return true;
}

void CustomTextEditor::onEnterPressed() {
    std::string currentText = m_input->getString();
    currentText += "\n";
    m_input->setString(currentText.c_str());
    textChanged(m_input);
}

void CustomTextEditor::textChanged(CCTextInputNode* input) {
    std::string text = input->getString();
    m_lines.clear();
    std::string currentLine;
    for (char c : text) {
        if (c == '\n' || c == '\r') {
            m_lines.push_back(currentLine);
            currentLine = "";
        } else {
            currentLine += c;
        }
    }
    m_lines.push_back(currentLine);
    redraw();
}

void CustomTextEditor::redraw() {
    for (auto label : m_lineLabels) label->removeFromParent();
    m_lineLabels.clear();
    for (auto label : m_lineNumberLabels) label->removeFromParent();
    m_lineNumberLabels.clear();

    for (size_t i = 0; i < m_lines.size(); ++i) {
        auto numLabel = CCLabelBMFont::create(std::to_string(i + 1).c_str(), "chatFont.fnt");
        numLabel->setAnchorPoint({1, 1});
        numLabel->setScale(m_fontSize);
        numLabel->setColor({80, 80, 80});
        numLabel->setPosition({-5.f, getContentSize().height - (i * m_lineHeight)});
        this->addChild(numLabel);
        m_lineNumberLabels.push_back(numLabel);

        auto label = CCLabelBMFont::create(m_lines[i].c_str(), "chatFont.fnt");
        label->setAnchorPoint({0, 1});
        label->setScale(m_fontSize);
        label->setPosition({0, getContentSize().height - (i * m_lineHeight)});
        this->addChild(label);
        m_lineLabels.push_back(label);
    }

    // Position cursor based on line/col
    float charWidth = 8.f; // Approximate character width
    float xPos = m_cursorCol * charWidth;
    float yPos = getContentSize().height - (m_cursorLine * m_lineHeight) - (m_lineHeight / 2.f);
    m_cursorNode->setPosition({xPos, yPos});
    m_cursorNode->setVisible(true);
}

void CustomTextEditor::keyDown(enumKeyCodes key, double) {
    // Чтобы выводить в консоль GeoIDE, нам нужно иметь доступ к m_consoleText
    // Поскольку у нас CustomTextEditor не знает про GeoIDE напрямую,
    // давайте передадим сообщение через логирование, которое мы уже умеем читать или выводить в консоль игры.
    // Если нужно именно в консоль GeoIDE, придется добавить указатель на неё в CustomTextEditor.
    // Пока добавим отладку через geode::log, она обычно отображается в консоли Geoode (если запущена).

    geode::log::info("CustomTextEditor::keyDown key: {} (hex: {:X})", (int)key, (int)key);

    if (key == KEY_Enter || key == (enumKeyCodes)0x0D) { // 0x0D is commonly Enter
        onEnterPressed();
    } else if (key == KEY_Left || key == (enumKeyCodes)0x25) { // 0x25 Left
        if (m_cursorCol > 0) m_cursorCol--;
    } else if (key == KEY_Right || key == (enumKeyCodes)0x27) { // 0x27 Right
        if (m_cursorCol < (int)m_lines[m_cursorLine].length()) m_cursorCol++;
    } else if (key == KEY_Up || key == (enumKeyCodes)0x26) { // 0x26 Up
        if (m_cursorLine > 0) {
            m_cursorLine--;
            if (m_cursorCol > (int)m_lines[m_cursorLine].length()) m_cursorCol = (int)m_lines[m_cursorLine].length();
        }
    } else if (key == KEY_Down || key == (enumKeyCodes)0x28) { // 0x28 Down
        if (m_cursorLine < (int)m_lines.size() - 1) {
            m_cursorLine++;
            if (m_cursorCol > (int)m_lines[m_cursorLine].length()) m_cursorCol = (int)m_lines[m_cursorLine].length();
        }
    }
    redraw();
}

std::string CustomTextEditor::getText() {
    return m_input->getString();
}
