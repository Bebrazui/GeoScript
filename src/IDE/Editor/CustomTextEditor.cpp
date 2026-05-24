#include "CustomTextEditor.hpp"
#include <algorithm>

using namespace cocos2d;

// ============================================================================
// Colors (VS Code Dark+ palette)
// ============================================================================
static const ccColor3B COL_NORMAL  = {212, 212, 212};
static const ccColor3B COL_KEYWORD = {86,  156, 214};
static const ccColor3B COL_NUMBER  = {181, 206, 168};
static const ccColor3B COL_STRING  = {206, 145, 120};
static const ccColor3B COL_COMMENT = {106, 153, 85};
static const ccColor3B COL_LINENUM = {80,  80,  80};
static const ccColor3B COL_CURSOR  = {220, 220, 220};

// ============================================================================
// GeoScript keywords
// ============================================================================
static const std::vector<std::string> KEYWORDS = {
    "if", "else", "while", "for", "func", "return", "var", "let", "const",
    "true", "false", "null", "and", "or", "not", "in", "break", "continue",
    "event", "group", "trigger", "delay", "move", "rotate", "alpha", "pulse",
    "spawn", "stop", "toggle", "color", "on", "off"
};

// ============================================================================
// Tokenizer (operates on UTF-8 strings; ASCII-only token detection is fine)
// ============================================================================
std::vector<SyntaxToken> CustomTextEditor::tokenizeLine(const std::string& line) const {
    std::vector<SyntaxToken> tokens;
    size_t i = 0, n = line.size();

    while (i < n) {
        unsigned char ch = (unsigned char)line[i];

        // Non-ASCII (e.g. Cyrillic) — emit as Normal token, one codepoint at a time
        if (ch >= 0x80) {
            int len = utf8::seqLen(ch);
            tokens.emplace_back(SyntaxTokenType::Normal, line.substr(i, len));
            i += len;
            continue;
        }

        // Single-line comment
        if (ch == '/' && i + 1 < n && line[i+1] == '/') {
            tokens.emplace_back(SyntaxTokenType::Comment, line.substr(i));
            break;
        }

        // String literal "..."
        if (ch == '"') {
            size_t j = i + 1;
            while (j < n && line[j] != '"') { if (line[j] == '\\') j++; j++; }
            if (j < n) j++;
            tokens.emplace_back(SyntaxTokenType::String, line.substr(i, j - i));
            i = j; continue;
        }

        // String literal '...'
        if (ch == '\'') {
            size_t j = i + 1;
            while (j < n && line[j] != '\'') { if (line[j] == '\\') j++; j++; }
            if (j < n) j++;
            tokens.emplace_back(SyntaxTokenType::String, line.substr(i, j - i));
            i = j; continue;
        }

        // Number
        if (std::isdigit(ch)) {
            size_t j = i;
            while (j < n && (std::isdigit((unsigned char)line[j]) || line[j] == '.')) j++;
            tokens.emplace_back(SyntaxTokenType::Number, line.substr(i, j - i));
            i = j; continue;
        }

        // Identifier / keyword
        if (std::isalpha(ch) || ch == '_') {
            size_t j = i;
            while (j < n && (std::isalnum((unsigned char)line[j]) || line[j] == '_')) j++;
            std::string word = line.substr(i, j - i);
            bool isKw = std::find(KEYWORDS.begin(), KEYWORDS.end(), word) != KEYWORDS.end();
            tokens.emplace_back(isKw ? SyntaxTokenType::Keyword : SyntaxTokenType::Identifier, word);
            i = j; continue;
        }

        // Operators
        if (std::string("=+-*/<>!&|^%~;:,(){}[]").find((char)ch) != std::string::npos) {
            tokens.emplace_back(SyntaxTokenType::Operator, std::string(1, (char)ch));
            i++; continue;
        }

        // Everything else (space, tab, etc.)
        tokens.emplace_back(SyntaxTokenType::Normal, std::string(1, (char)ch));
        i++;
    }
    return tokens;
}

// ============================================================================
// Key → ASCII char (US QWERTY, for non-IME keys)
// ============================================================================
static char keyToChar(enumKeyCodes key, bool shift) {
    if (key >= KEY_A && key <= KEY_Z) {
        char base = 'a' + (key - KEY_A);
        return shift ? (char)(base - 32) : base;
    }
    if (!shift) {
        if (key >= KEY_Zero && key <= KEY_Nine)
            return '0' + (key - KEY_Zero);
    } else {
        switch (key) {
            case KEY_Zero:  return ')'; case KEY_One:   return '!';
            case KEY_Two:   return '@'; case KEY_Three: return '#';
            case KEY_Four:  return '$'; case KEY_Five:  return '%';
            case KEY_Six:   return '^'; case KEY_Seven: return '&';
            case KEY_Eight: return '*'; case KEY_Nine:  return '(';
            default: break;
        }
    }
    switch (key) {
        case KEY_Space:        return ' ';
        case KEY_OEMPeriod:    return shift ? '>' : '.';
        case KEY_OEMComma:     return shift ? '<' : ',';
        case KEY_OEMMinus:     return shift ? '_' : '-';
        case KEY_OEMPlus:      return shift ? '+' : '=';
        case KEY_Equal:        return shift ? '+' : '=';
        case KEY_OEMEqual:     return shift ? '+' : '=';
        case KEY_OEM1:         return shift ? ':' : ';';
        case KEY_Semicolon:    return shift ? ':' : ';';
        case KEY_OEM2:         return shift ? '?' : '/';
        case KEY_Slash:        return shift ? '?' : '/';
        case KEY_OEM3:         return shift ? '~' : '`';
        case KEY_GraveAccent:  return shift ? '~' : '`';
        case KEY_OEM4:         return shift ? '{' : '[';
        case KEY_LeftBracket:  return shift ? '{' : '[';
        case KEY_OEM5:         return shift ? '|' : '\\';
        case KEY_Backslash:    return shift ? '|' : '\\';
        case KEY_OEM6:          return shift ? '}' : ']';
        case KEY_RightBracket:  return shift ? '}' : ']';
        case KEY_OEM7:         return shift ? '"' : '\'';
        case KEY_Apostrophe:   return shift ? '"' : '\'';
        case KEY_Divide:   return '/'; case KEY_Multiply: return '*';
        case KEY_Subtract: return '-'; case KEY_Add:      return '+';
        case KEY_Decimal:  return '.';
        case KEY_NumPad0: return '0'; case KEY_NumPad1: return '1';
        case KEY_NumPad2: return '2'; case KEY_NumPad3: return '3';
        case KEY_NumPad4: return '4'; case KEY_NumPad5: return '5';
        case KEY_NumPad6: return '6'; case KEY_NumPad7: return '7';
        case KEY_NumPad8: return '8'; case KEY_NumPad9: return '9';
        default: return 0;
    }
}

// ============================================================================
// Factory
// ============================================================================
CustomTextEditor* CustomTextEditor::create(CCSize size) {
    auto ret = new CustomTextEditor();
    if (ret && ret->init(size)) { ret->autorelease(); return ret; }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

// ============================================================================
// init
// ============================================================================
bool CustomTextEditor::init(CCSize size) {
    if (!CCLayer::init()) return false;
    this->setContentSize(size);
    // Keyboard events come from GeoIDE via direct keyDown/keyUp calls

    // Background
    auto bg = CCLayerColor::create({18, 18, 18, 255}, size.width, size.height);
    this->addChild(bg, -1);

    // Gutter background
    auto gutterBg = CCLayerColor::create({24, 24, 24, 255}, m_lineNumWidth, size.height);
    this->addChild(gutterBg, 0);

    // Gutter separator
    auto sep = CCLayerColor::create({50, 50, 50, 255}, 1.f, size.height);
    sep->setPosition({m_lineNumWidth, 0});
    this->addChild(sep, 0);

    // Text layer (z=1)
    m_textLayer = CCNode::create();
    m_textLayer->setContentSize(size);
    this->addChild(m_textLayer, 1);

    // Cursor layer (z=2)
    m_cursorLayer = CCNode::create();
    m_cursorLayer->setContentSize(size);
    this->addChild(m_cursorLayer, 2);

    // Cursor: 2px wide bright bar
    m_cursorNode = CCLayerColor::create(
        {COL_CURSOR.r, COL_CURSOR.g, COL_CURSOR.b, 230},
        2.f, m_lineHeight - 2.f
    );
    m_cursorLayer->addChild(m_cursorNode);

    // Blink via schedule
    this->schedule(schedule_selector(CustomTextEditor::blinkCursor), 0.53f);

    m_lines.push_back("");
    redraw();
    return true;
}

void CustomTextEditor::blinkCursor(float) {
    m_cursorVisible = !m_cursorVisible;
    if (m_cursorNode) m_cursorNode->setVisible(m_cursorVisible);
}

// ============================================================================
// Cursor byte offset helper
// ============================================================================
int CustomTextEditor::cursorByteOffset() const {
    return utf8::byteOffset(m_lines[m_cursorLine], m_cursorCol);
}

// ============================================================================
// Editing helpers
// ============================================================================
void CustomTextEditor::clampCursor() {
    if (m_cursorLine < 0) m_cursorLine = 0;
    if (m_cursorLine >= (int)m_lines.size())
        m_cursorLine = (int)m_lines.size() - 1;
    int cpLen = utf8::cpLen(m_lines[m_cursorLine]);
    if (m_cursorCol < 0) m_cursorCol = 0;
    if (m_cursorCol > cpLen) m_cursorCol = cpLen;
}

void CustomTextEditor::insertUTF8(const std::string& s) {
    if (m_hasSelection) deleteSelection();
    for (size_t i = 0; i < s.size(); ) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n' || ch == '\r') {
            insertNewline();
            i++;
        } else if (ch == '\t') {
            // 4 spaces
            for (int k = 0; k < 4; k++) {
                int bytePos = cursorByteOffset();
                m_lines[m_cursorLine].insert(bytePos, 1, ' ');
                m_cursorCol++;
            }
            i++;
        } else {
            int len = utf8::seqLen(ch);
            std::string cp = s.substr(i, len);
            int bytePos = cursorByteOffset();
            m_lines[m_cursorLine].insert(bytePos, cp);
            m_cursorCol++;
            i += len;
        }
    }
}

void CustomTextEditor::insertNewline() {
    if (m_hasSelection) deleteSelection();
    int bytePos = cursorByteOffset();
    std::string tail = m_lines[m_cursorLine].substr(bytePos);
    m_lines[m_cursorLine] = m_lines[m_cursorLine].substr(0, bytePos);
    m_lines.insert(m_lines.begin() + m_cursorLine + 1, tail);
    m_cursorLine++;
    m_cursorCol = 0;
}

void CustomTextEditor::deleteCharBefore() {
    if (m_hasSelection) { deleteSelection(); return; }
    if (m_cursorCol > 0) {
        int bytePos = cursorByteOffset();
        int newBytePos = utf8::eraseCP(m_lines[m_cursorLine], bytePos);
        (void)newBytePos;
        m_cursorCol--;
    } else if (m_cursorLine > 0) {
        int prevCpLen = utf8::cpLen(m_lines[m_cursorLine - 1]);
        m_lines[m_cursorLine - 1] += m_lines[m_cursorLine];
        m_lines.erase(m_lines.begin() + m_cursorLine);
        m_cursorLine--;
        m_cursorCol = prevCpLen;
    }
}

void CustomTextEditor::deleteSelection() {
    if (!m_hasSelection) return;
    int sl = m_selStartLine, sc = m_selStartCol;
    int el = m_selEndLine,   ec = m_selEndCol;
    if (sl > el || (sl == el && sc > ec)) { std::swap(sl, el); std::swap(sc, ec); }

    int scByte = utf8::byteOffset(m_lines[sl], sc);
    int ecByte = utf8::byteOffset(m_lines[el], ec);

    if (sl == el) {
        m_lines[sl].erase(scByte, ecByte - scByte);
    } else {
        std::string merged = m_lines[sl].substr(0, scByte) + m_lines[el].substr(ecByte);
        m_lines.erase(m_lines.begin() + sl, m_lines.begin() + el + 1);
        m_lines.insert(m_lines.begin() + sl, merged);
    }
    m_cursorLine = sl;
    m_cursorCol  = sc;
    m_hasSelection = false;
}

void CustomTextEditor::selectAll() {
    m_hasSelection = true;
    m_selStartLine = 0; m_selStartCol = 0;
    m_selEndLine   = (int)m_lines.size() - 1;
    m_selEndCol    = utf8::cpLen(m_lines.back());
    m_cursorLine   = m_selEndLine;
    m_cursorCol    = m_selEndCol;
}

// ============================================================================
// TextInputDelegate — called by IME for Unicode input (Cyrillic, etc.)
// ============================================================================
// ============================================================================
// insertText / deleteBackward — called from IME hook in main.cpp
// ============================================================================
void CustomTextEditor::insertText(const std::string& text) {
    if (text.empty()) return;
    insertUTF8(text);
    redraw();
}

void CustomTextEditor::deleteBackward() {
    deleteCharBefore();
    redraw();
}

// ============================================================================
// makeLabel
// ============================================================================
CCLabelTTF* CustomTextEditor::makeLabel(const std::string& text,
                                         ccColor3B color,
                                         float x, float y) {
    auto* lbl = CCLabelTTF::create(text.c_str(), kFont, m_fontSize);
    if (!lbl || std::string(lbl->getString()).empty() && !text.empty()) {
        lbl = CCLabelTTF::create(text.c_str(), "Courier New", m_fontSize);
    }
    if (!lbl) return nullptr;
    lbl->setAnchorPoint({0.f, 0.f});
    lbl->setColor(color);
    lbl->setPosition({x, y});
    return lbl;
}

// ============================================================================
// redraw
// ============================================================================
void CustomTextEditor::redraw() {
    m_textLayer->removeAllChildren();

    CCSize size = getContentSize();
    float codeX = m_lineNumWidth + 6.f;

    for (size_t i = 0; i < m_lines.size(); ++i) {
        float y = size.height - (float)(i + 1) * m_lineHeight;

        // Line number label
        auto* numLbl = CCLabelTTF::create(
            std::to_string(i + 1).c_str(), kFont, m_fontSize - 1.f);
        if (!numLbl)
            numLbl = CCLabelTTF::create(
                std::to_string(i + 1).c_str(), "Courier New", m_fontSize - 1.f);
        if (numLbl) {
            numLbl->setAnchorPoint({1.f, 0.f});
            numLbl->setColor(COL_LINENUM);
            numLbl->setPosition({m_lineNumWidth - 4.f, y});
            m_textLayer->addChild(numLbl);
        }

        // Tokenize and render
        auto tokens = tokenizeLine(m_lines[i]);
        float xOff = codeX;
        for (auto& tok : tokens) {
            if (tok.text.empty()) continue;
            ccColor3B col;
            switch (tok.type) {
                case SyntaxTokenType::Keyword: col = COL_KEYWORD; break;
                case SyntaxTokenType::Number:  col = COL_NUMBER;  break;
                case SyntaxTokenType::String:  col = COL_STRING;  break;
                case SyntaxTokenType::Comment: col = COL_COMMENT; break;
                default:                 col = COL_NORMAL;  break;
            }
            auto* lbl = makeLabel(tok.text, col, xOff, y);
            if (lbl) m_textLayer->addChild(lbl);
            // Advance by codepoint count (each codepoint ≈ m_charWidth)
            xOff += (float)utf8::cpLen(tok.text) * m_charWidth;
        }
    }

    updateCursorPos();
}

void CustomTextEditor::updateCursorPos() {
    if (!m_cursorNode) return;
    CCSize size = getContentSize();
    float x = m_lineNumWidth + 6.f + (float)m_cursorCol * m_charWidth;
    float y = size.height - (float)(m_cursorLine + 1) * m_lineHeight + 1.f;
    m_cursorNode->setPosition({x, y});
    // Show cursor immediately on any action
    m_cursorVisible = true;
    m_cursorNode->setVisible(true);
    // Restart blink timer
    this->unschedule(schedule_selector(CustomTextEditor::blinkCursor));
    this->schedule(schedule_selector(CustomTextEditor::blinkCursor), 0.53f);
}

// ============================================================================
// keyDown
// ============================================================================
void CustomTextEditor::keyDown(enumKeyCodes key, double) {
    switch (key) {
        case KEY_Shift:
        case KEY_LeftShift:
        case KEY_RightShift:
            m_shiftHeld = true;
            return;
        case KEY_Control:
        case KEY_LeftControl:
        case KEY_RightContol:
            m_ctrlHeld = true;
            return;
        case KEY_Alt:
            return; // ignore alt

        // Ctrl combos
        case KEY_A:
            if (m_ctrlHeld) { selectAll(); redraw(); return; }
            return; // printable 'a' comes via insertText
        case KEY_V:
            if (m_ctrlHeld) {
                std::string clip = geode::utils::clipboard::read();
                if (!clip.empty()) insertUTF8(clip);
                redraw();
            }
            return;
        case KEY_C:
            if (m_ctrlHeld) {
                if (m_hasSelection) {
                    int sl = m_selStartLine, sc = m_selStartCol;
                    int el = m_selEndLine,   ec = m_selEndCol;
                    if (sl > el || (sl == el && sc > ec)) { std::swap(sl, el); std::swap(sc, ec); }
                    std::string copied;
                    int scB = utf8::byteOffset(m_lines[sl], sc);
                    int ecB = utf8::byteOffset(m_lines[el], ec);
                    if (sl == el) {
                        copied = m_lines[sl].substr(scB, ecB - scB);
                    } else {
                        copied = m_lines[sl].substr(scB);
                        for (int li = sl + 1; li < el; li++) copied += '\n' + m_lines[li];
                        copied += '\n' + m_lines[el].substr(0, ecB);
                    }
                    geode::utils::clipboard::write(copied);
                }
            }
            return;
        case KEY_Z:
            return; // undo not implemented; printable 'z' via insertText

        // Enter
        case KEY_Enter:
        case KEY_NumEnter:
            insertNewline();
            break;

        // Backspace — also handled via dispatchDeleteBackward, but keep as fallback
        case KEY_Backspace:
            deleteCharBefore();
            break;

        // Delete
        case KEY_Delete:
            if (m_hasSelection) {
                deleteSelection();
            } else if (m_cursorCol < utf8::cpLen(m_lines[m_cursorLine])) {
                int bytePos = cursorByteOffset();
                int len = utf8::seqLen((unsigned char)m_lines[m_cursorLine][bytePos]);
                m_lines[m_cursorLine].erase(bytePos, len);
            } else if (m_cursorLine < (int)m_lines.size() - 1) {
                m_lines[m_cursorLine] += m_lines[m_cursorLine + 1];
                m_lines.erase(m_lines.begin() + m_cursorLine + 1);
            }
            break;

        // Navigation
        case KEY_Left:
            m_hasSelection = false;
            if (m_cursorCol > 0) m_cursorCol--;
            else if (m_cursorLine > 0) {
                m_cursorLine--;
                m_cursorCol = utf8::cpLen(m_lines[m_cursorLine]);
            }
            break;
        case KEY_Right:
            m_hasSelection = false;
            if (m_cursorCol < utf8::cpLen(m_lines[m_cursorLine])) m_cursorCol++;
            else if (m_cursorLine < (int)m_lines.size() - 1) { m_cursorLine++; m_cursorCol = 0; }
            break;
        case KEY_Up:
            m_hasSelection = false;
            if (m_cursorLine > 0) { m_cursorLine--; clampCursor(); }
            break;
        case KEY_Down:
            m_hasSelection = false;
            if (m_cursorLine < (int)m_lines.size() - 1) { m_cursorLine++; clampCursor(); }
            break;
        case KEY_Home:
            m_hasSelection = false;
            m_cursorCol = 0;
            break;
        case KEY_End:
            m_hasSelection = false;
            m_cursorCol = utf8::cpLen(m_lines[m_cursorLine]);
            break;

        // Tab → 4 spaces
        case KEY_Tab:
            insertUTF8("    ");
            break;

        case KEY_Escape:
            m_hasSelection = false;
            break;

        default:
            return; // all printable chars come via insertText/IME hook
    }

    redraw();
}

void CustomTextEditor::keyUp(enumKeyCodes key, double) {
    switch (key) {
        case KEY_Shift:
        case KEY_LeftShift:
        case KEY_RightShift:
            m_shiftHeld = false; break;
        case KEY_Control:
        case KEY_LeftControl:
        case KEY_RightContol:
            m_ctrlHeld = false; break;
        default: break;
    }
}

// ============================================================================
// getText / setText
// ============================================================================
std::string CustomTextEditor::getText() const {
    std::string result;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (i > 0) result += '\n';
        result += m_lines[i];
    }
    return result;
}

void CustomTextEditor::setText(const std::string& text) {
    m_lines.clear();
    std::string cur;
    for (size_t i = 0; i < text.size(); ) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n' || ch == '\r') {
            m_lines.push_back(cur);
            cur.clear();
            i++;
        } else {
            int len = utf8::seqLen(ch);
            cur += text.substr(i, len);
            i += len;
        }
    }
    m_lines.push_back(cur);
    m_cursorLine = 0;
    m_cursorCol  = 0;
    m_hasSelection = false;
    redraw();
}
