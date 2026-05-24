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
    if (!CCNode::init()) return false;
    this->setContentSize(size);

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

    for (size_t i = 0; i < m_lines.size(); ++i) {
        auto label = CCLabelBMFont::create(m_lines[i].c_str(), "chatFont.fnt");
        label->setAnchorPoint({0, 1});
        label->setScale(m_fontSize);
        label->setPosition({0, getContentSize().height - (i * m_lineHeight)});
        this->addChild(label);
        m_lineLabels.push_back(label);
    }
}

void CustomTextEditor::keyDown(enumKeyCodes key, double) {
    if (key == KEY_Enter) {
        onEnterPressed();
    }
}

std::string CustomTextEditor::getText() {
    return m_input->getString();
}
