#include "CustomTextEditor.hpp"
#include <algorithm>

using namespace cocos2d;

// ============================================================================
// Colors
// ============================================================================
static const ccColor3B COL_NORMAL  = {212, 212, 212};
static const ccColor3B COL_KEYWORD = {86,  156, 214};
static const ccColor3B COL_NUMBER  = {181, 206, 168};
static const ccColor3B COL_STRING  = {206, 145, 120};
static const ccColor3B COL_COMMENT = {106, 153, 85};
static const ccColor3B COL_LINENUM = {75,  75,  75};
static const ccColor3B COL_CURSOR  = {220, 220, 220};

static const std::vector<std::string> KEYWORDS = {
    "if","else","while","for","func","return","var","let","const",
    "true","false","null","and","or","not","in","break","continue",
    // GeoScript keywords
    "group","on_start","on_touch","on_death",
    "move","pulse","wait","repeat","while_true","shake",
    "trigger","delay","rotate","alpha","spawn","stop","toggle","color","on","off"
};

// ============================================================================
// Tokenizer
// ============================================================================
std::vector<SyntaxToken> CustomTextEditor::tokenizeLine(const std::string& line) const {
    std::vector<SyntaxToken> tokens;
    size_t i = 0, n = line.size();
    while (i < n) {
        unsigned char ch = (unsigned char)line[i];
        if (ch >= 0x80) {
            int len = utf8::seqLen(ch);
            tokens.emplace_back(SyntaxTokenType::Normal, line.substr(i, len));
            i += len; continue;
        }
        if (ch == '/' && i+1 < n && line[i+1] == '/') {
            tokens.emplace_back(SyntaxTokenType::Comment, line.substr(i)); break;
        }
        if (ch == '"') {
            size_t j = i+1;
            while (j < n && line[j] != '"') { if (line[j]=='\\') j++; j++; }
            if (j < n) j++;
            tokens.emplace_back(SyntaxTokenType::String, line.substr(i, j-i));
            i = j; continue;
        }
        if (ch == '\'') {
            size_t j = i+1;
            while (j < n && line[j] != '\'') { if (line[j]=='\\') j++; j++; }
            if (j < n) j++;
            tokens.emplace_back(SyntaxTokenType::String, line.substr(i, j-i));
            i = j; continue;
        }
        if (std::isdigit(ch)) {
            size_t j = i;
            while (j < n && (std::isdigit((unsigned char)line[j]) || line[j]=='.')) j++;
            tokens.emplace_back(SyntaxTokenType::Number, line.substr(i, j-i));
            i = j; continue;
        }
        if (std::isalpha(ch) || ch == '_') {
            size_t j = i;
            while (j < n && (std::isalnum((unsigned char)line[j]) || line[j]=='_')) j++;
            std::string word = line.substr(i, j-i);
            bool isKw = std::find(KEYWORDS.begin(), KEYWORDS.end(), word) != KEYWORDS.end();
            tokens.emplace_back(isKw ? SyntaxTokenType::Keyword : SyntaxTokenType::Identifier, word);
            i = j; continue;
        }
        if (std::string("=+-*/<>!&|^%~;:,(){}[]").find((char)ch) != std::string::npos) {
            tokens.emplace_back(SyntaxTokenType::Operator, std::string(1,(char)ch));
            i++; continue;
        }
        tokens.emplace_back(SyntaxTokenType::Normal, std::string(1,(char)ch));
        i++;
    }
    return tokens;
}

// ============================================================================
// Text measurement — real pixel width via CCLabelTTF
// ============================================================================
float CustomTextEditor::measureText(const std::string& text) {
    if (text.empty()) return 0.f;
    auto* lbl = CCLabelTTF::create(text.c_str(), kFont, m_fontSize);
    if (!lbl) lbl = CCLabelTTF::create(text.c_str(), kFontFallback, m_fontSize);
    if (!lbl) return (float)utf8::cpLen(text) * 5.5f;
    return lbl->getContentSize().width;
}

// ============================================================================
// Factory / init
// ============================================================================
CustomTextEditor* CustomTextEditor::create(CCSize size) {
    auto ret = new CustomTextEditor();
    if (ret && ret->init(size)) { ret->autorelease(); return ret; }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool CustomTextEditor::init(CCSize size) {
    if (!CCLayer::init()) return false;
    this->setContentSize(size);
    this->setTouchEnabled(true);
    this->setMouseEnabled(true);

    auto bg = CCLayerColor::create({18, 18, 18, 255}, size.width, size.height);
    this->addChild(bg, -1);

    auto gutterBg = CCLayerColor::create({22, 22, 22, 255}, m_lineNumWidth, size.height);
    this->addChild(gutterBg, 0);
    auto sep = CCLayerColor::create({45, 45, 45, 255}, 1.f, size.height);
    sep->setPosition({m_lineNumWidth, 0});
    this->addChild(sep, 0);

    m_textLayer = CCNode::create();
    m_textLayer->setContentSize(size);
    this->addChild(m_textLayer, 1);

    m_cursorLayer = CCNode::create();
    m_cursorLayer->setContentSize(size);
    this->addChild(m_cursorLayer, 2);

    m_cursorNode = CCLayerColor::create(
        {COL_CURSOR.r, COL_CURSOR.g, COL_CURSOR.b, 240},
        1.5f, m_lineHeight - 2.f
    );
    m_cursorLayer->addChild(m_cursorNode);

    this->schedule(schedule_selector(CustomTextEditor::blinkCursor), 0.53f);

    m_lines.push_back("");
    redraw();
    return true;
}

void CustomTextEditor::blinkCursor(float) {
    m_cursorVisible = !m_cursorVisible;
    if (m_cursorNode) m_cursorNode->setVisible(m_cursorVisible);
}

bool CustomTextEditor::ccTouchBegan(CCTouch*, CCEvent*) { return true; }

// ============================================================================
// Scroll wheel — y > 0 means wheel up = content scrolls up
// ============================================================================
void CustomTextEditor::scrollWheel(float y, float /*x*/) {
    float totalH    = (float)m_lines.size() * m_lineHeight;
    float viewH     = getContentSize().height;
    float maxScroll = std::max(0.f, totalH - viewH + m_lineHeight);
    // y > 0 = wheel up → scroll up → scrollY decreases (show earlier lines)
    m_scrollY = std::max(0.f, std::min(m_scrollY + y * 2.5f, maxScroll));
    redraw();
}

// ============================================================================
// Helpers
// ============================================================================
int CustomTextEditor::cursorByteOffset() const {
    return utf8::byteOffset(m_lines[m_cursorLine], m_cursorCol);
}

void CustomTextEditor::clampCursor() {
    if (m_cursorLine < 0) m_cursorLine = 0;
    if (m_cursorLine >= (int)m_lines.size()) m_cursorLine = (int)m_lines.size()-1;
    int len = utf8::cpLen(m_lines[m_cursorLine]);
    if (m_cursorCol < 0) m_cursorCol = 0;
    if (m_cursorCol > len) m_cursorCol = len;
}

void CustomTextEditor::scrollToCursor() {
    CCSize size = getContentSize();
    float cursorScreenY = size.height - (float)(m_cursorLine + 1) * m_lineHeight + m_scrollY;
    if (cursorScreenY < 2.f)
        m_scrollY -= cursorScreenY - 2.f;
    if (cursorScreenY + m_lineHeight > size.height)
        m_scrollY -= (cursorScreenY + m_lineHeight - size.height) + 2.f;
    float totalH    = (float)m_lines.size() * m_lineHeight;
    float maxScroll = std::max(0.f, totalH - size.height + m_lineHeight);
    m_scrollY = std::max(0.f, std::min(m_scrollY, maxScroll));
}

// ============================================================================
// getColX — returns screen X for codepoint cp in line lineIdx
// Uses m_lineColX cache built during redraw.
// Falls back to measurement if line not in cache.
// ============================================================================
float CustomTextEditor::getColX(int lineIdx, int cp) {
    auto it = m_lineColX.find(lineIdx);
    if (it != m_lineColX.end() && cp < (int)it->second.size())
        return it->second[cp];

    // Not cached — measure manually
    float codeX = m_lineNumWidth + 4.f;
    if (lineIdx < 0 || lineIdx >= (int)m_lines.size()) return codeX;
    const std::string& line = m_lines[lineIdx];
    if (cp <= 0) return codeX;

    // Measure text up to cp codepoints
    int byteEnd = utf8::byteOffset(line, cp);
    return codeX + measureText(line.substr(0, byteEnd));
}

// ============================================================================
// Editing
// ============================================================================
void CustomTextEditor::insertUTF8(const std::string& s) {
    if (m_hasSelection) deleteSelection();
    for (size_t i = 0; i < s.size(); ) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n' || ch == '\r') { insertNewline(); i++; continue; }
        if (ch == '\t') {
            for (int k = 0; k < 4; k++) {
                m_lines[m_cursorLine].insert(cursorByteOffset(), 1, ' ');
                m_cursorCol++;
            }
            i++; continue;
        }
        int len = utf8::seqLen(ch);
        m_lines[m_cursorLine].insert(cursorByteOffset(), s.substr(i, len));
        m_cursorCol++;
        i += len;
    }
    // Invalidate cache for modified line
    m_lineColX.erase(m_cursorLine);
}

void CustomTextEditor::insertNewline() {
    if (m_hasSelection) deleteSelection();
    int bp = cursorByteOffset();
    std::string tail = m_lines[m_cursorLine].substr(bp);
    m_lines[m_cursorLine] = m_lines[m_cursorLine].substr(0, bp);
    m_lines.insert(m_lines.begin() + m_cursorLine + 1, tail);
    m_lineColX.clear(); // line indices shifted
    m_cursorLine++;
    m_cursorCol = 0;
}

void CustomTextEditor::deleteCharBefore() {
    if (m_hasSelection) { deleteSelection(); return; }
    if (m_cursorCol > 0) {
        utf8::eraseCP(m_lines[m_cursorLine], cursorByteOffset());
        m_cursorCol--;
        m_lineColX.erase(m_cursorLine);
    } else if (m_cursorLine > 0) {
        int prevLen = utf8::cpLen(m_lines[m_cursorLine - 1]);
        m_lines[m_cursorLine - 1] += m_lines[m_cursorLine];
        m_lines.erase(m_lines.begin() + m_cursorLine);
        m_lineColX.clear();
        m_cursorLine--;
        m_cursorCol = prevLen;
    }
}

void CustomTextEditor::deleteSelection() {
    if (!m_hasSelection) return;
    int sl=m_selStartLine, sc=m_selStartCol, el=m_selEndLine, ec=m_selEndCol;
    if (sl>el||(sl==el&&sc>ec)){std::swap(sl,el);std::swap(sc,ec);}
    int scB=utf8::byteOffset(m_lines[sl],sc), ecB=utf8::byteOffset(m_lines[el],ec);
    if (sl==el) {
        m_lines[sl].erase(scB, ecB-scB);
    } else {
        std::string merged = m_lines[sl].substr(0,scB) + m_lines[el].substr(ecB);
        m_lines.erase(m_lines.begin()+sl, m_lines.begin()+el+1);
        m_lines.insert(m_lines.begin()+sl, merged);
    }
    m_lineColX.clear();
    m_cursorLine=sl; m_cursorCol=sc;
    m_hasSelection=false;
}

void CustomTextEditor::selectAll() {
    m_hasSelection=true;
    m_selStartLine=0; m_selStartCol=0;
    m_selEndLine=(int)m_lines.size()-1;
    m_selEndCol=utf8::cpLen(m_lines.back());
    m_cursorLine=m_selEndLine; m_cursorCol=m_selEndCol;
}

// ============================================================================
// IME entry points
// ============================================================================
void CustomTextEditor::insertText(const std::string& text) {
    if (text.empty()) return;
    insertUTF8(text);
    scrollToCursor();
    redraw();
}

void CustomTextEditor::deleteBackward() {
    deleteCharBefore();
    scrollToCursor();
    redraw();
}

// ============================================================================
// makeLabel
// ============================================================================
CCLabelTTF* CustomTextEditor::makeLabel(const std::string& text,
                                         ccColor3B color, float x, float y) {
    auto* lbl = CCLabelTTF::create(text.c_str(), kFont, m_fontSize);
    if (!lbl) lbl = CCLabelTTF::create(text.c_str(), kFontFallback, m_fontSize);
    if (!lbl) return nullptr;
    lbl->setAnchorPoint({0.f, 0.f});
    lbl->setColor(color);
    lbl->setPosition({x, y});
    return lbl;
}

// ============================================================================
// redraw — renders visible lines and builds m_lineColX cache
// ============================================================================
void CustomTextEditor::redraw() {
    m_textLayer->removeAllChildren();

    CCSize size = getContentSize();
    float codeX = m_lineNumWidth + 4.f;

    int firstLine = std::max(0, (int)(m_scrollY / m_lineHeight));
    int lastLine  = std::min((int)m_lines.size()-1,
                             (int)((m_scrollY + size.height) / m_lineHeight) + 1);

    for (int i = firstLine; i <= lastLine; i++) {
        float y = size.height - (float)(i+1) * m_lineHeight + m_scrollY;

        // Line number
        auto* numLbl = CCLabelTTF::create(std::to_string(i+1).c_str(), kFont, m_fontSize-1.f);
        if (!numLbl) numLbl = CCLabelTTF::create(std::to_string(i+1).c_str(), kFontFallback, m_fontSize-1.f);
        if (numLbl) {
            numLbl->setAnchorPoint({1.f, 0.f});
            numLbl->setColor(COL_LINENUM);
            numLbl->setPosition({m_lineNumWidth - 3.f, y});
            m_textLayer->addChild(numLbl);
        }

        // Build colX cache for this line while rendering tokens
        auto tokens = tokenizeLine(m_lines[i]);
        float xOff = codeX;

        // colX[cp] = screen X of codepoint cp
        // We need one entry per codepoint + one for end-of-line
        std::vector<float>& colX = m_lineColX[i];
        colX.clear();
        colX.push_back(xOff); // cp=0 starts at codeX

        int cpIdx = 0; // current codepoint index in line

        for (auto& tok : tokens) {
            if (tok.text.empty()) continue;
            ccColor3B col;
            switch (tok.type) {
                case SyntaxTokenType::Keyword: col = COL_KEYWORD; break;
                case SyntaxTokenType::Number:  col = COL_NUMBER;  break;
                case SyntaxTokenType::String:  col = COL_STRING;  break;
                case SyntaxTokenType::Comment: col = COL_COMMENT; break;
                default:                       col = COL_NORMAL;  break;
            }

            auto* lbl = makeLabel(tok.text, col, xOff, y);
            if (lbl) m_textLayer->addChild(lbl);

            // For each codepoint in this token, record its X position
            // We measure prefix widths character by character
            int tokCPs = utf8::cpLen(tok.text);
            for (int c = 1; c <= tokCPs; c++) {
                int byteEnd = utf8::byteOffset(tok.text, c);
                float w = measureText(tok.text.substr(0, byteEnd));
                colX.push_back(xOff + w);
            }

            // Advance xOff by full token width
            if (lbl) xOff += lbl->getContentSize().width;
            else      xOff += measureText(tok.text);

            cpIdx += tokCPs;
        }
        // colX now has cpLen+1 entries: colX[0]..colX[cpLen]
    }

    updateCursorPos();
}

void CustomTextEditor::updateCursorPos() {
    if (!m_cursorNode) return;
    CCSize size = getContentSize();

    float x = getColX(m_cursorLine, m_cursorCol);
    float y = size.height - (float)(m_cursorLine+1) * m_lineHeight + m_scrollY + 1.f;

    m_cursorNode->setPosition({x, y});

    bool inView = (y >= -m_lineHeight && y <= size.height);
    m_cursorVisible = true;
    m_cursorNode->setVisible(inView);

    this->unschedule(schedule_selector(CustomTextEditor::blinkCursor));
    this->schedule(schedule_selector(CustomTextEditor::blinkCursor), 0.53f);
}

// ============================================================================
// keyDown
// ============================================================================
void CustomTextEditor::keyDown(enumKeyCodes key, double) {
    switch (key) {
        case KEY_Shift: case KEY_LeftShift: case KEY_RightShift:
            m_shiftHeld = true; return;
        case KEY_Control: case KEY_LeftControl: case KEY_RightContol:
            m_ctrlHeld = true; return;
        case KEY_Alt: return;

        case KEY_A:
            if (m_ctrlHeld) { selectAll(); redraw(); } return;
        case KEY_V:
            if (m_ctrlHeld) {
                auto clip = geode::utils::clipboard::read();
                if (!clip.empty()) { insertUTF8(clip); scrollToCursor(); redraw(); }
            } return;
        case KEY_C:
            if (m_ctrlHeld && m_hasSelection) {
                int sl=m_selStartLine,sc=m_selStartCol,el=m_selEndLine,ec=m_selEndCol;
                if(sl>el||(sl==el&&sc>ec)){std::swap(sl,el);std::swap(sc,ec);}
                int scB=utf8::byteOffset(m_lines[sl],sc),ecB=utf8::byteOffset(m_lines[el],ec);
                std::string copied;
                if(sl==el){copied=m_lines[sl].substr(scB,ecB-scB);}
                else{
                    copied=m_lines[sl].substr(scB);
                    for(int li=sl+1;li<el;li++) copied+='\n'+m_lines[li];
                    copied+='\n'+m_lines[el].substr(0,ecB);
                }
                geode::utils::clipboard::write(copied);
            } return;
        case KEY_Z: return;

        case KEY_Enter: case KEY_NumEnter:
            insertNewline(); scrollToCursor(); break;

        // Backspace handled ONLY via deleteBackward() (IME hook) — not here
        // to avoid double-delete. Do NOT add KEY_Backspace here.

        case KEY_Delete:
            if (m_hasSelection) { deleteSelection(); }
            else if (m_cursorCol < utf8::cpLen(m_lines[m_cursorLine])) {
                int bp = cursorByteOffset();
                m_lines[m_cursorLine].erase(bp, utf8::seqLen((unsigned char)m_lines[m_cursorLine][bp]));
                m_lineColX.erase(m_cursorLine);
            } else if (m_cursorLine < (int)m_lines.size()-1) {
                m_lines[m_cursorLine] += m_lines[m_cursorLine+1];
                m_lines.erase(m_lines.begin()+m_cursorLine+1);
                m_lineColX.clear();
            }
            break;

        case KEY_Left:
            m_hasSelection=false;
            if(m_cursorCol>0) m_cursorCol--;
            else if(m_cursorLine>0){m_cursorLine--;m_cursorCol=utf8::cpLen(m_lines[m_cursorLine]);}
            scrollToCursor(); break;
        case KEY_Right:
            m_hasSelection=false;
            if(m_cursorCol<utf8::cpLen(m_lines[m_cursorLine])) m_cursorCol++;
            else if(m_cursorLine<(int)m_lines.size()-1){m_cursorLine++;m_cursorCol=0;}
            scrollToCursor(); break;
        case KEY_Up:
            m_hasSelection=false;
            if(m_cursorLine>0){m_cursorLine--;clampCursor();}
            scrollToCursor(); break;
        case KEY_Down:
            m_hasSelection=false;
            if(m_cursorLine<(int)m_lines.size()-1){m_cursorLine++;clampCursor();}
            scrollToCursor(); break;
        case KEY_Home:
            m_hasSelection=false; m_cursorCol=0; break;
        case KEY_End:
            m_hasSelection=false; m_cursorCol=utf8::cpLen(m_lines[m_cursorLine]); break;
        case KEY_PageUp:
            m_hasSelection=false;
            m_cursorLine=std::max(0,m_cursorLine-10);
            clampCursor(); scrollToCursor(); break;
        case KEY_PageDown:
            m_hasSelection=false;
            m_cursorLine=std::min((int)m_lines.size()-1,m_cursorLine+10);
            clampCursor(); scrollToCursor(); break;

        case KEY_Tab:
            insertUTF8("    "); scrollToCursor(); break;
        case KEY_Escape:
            m_hasSelection=false; break;

        default: return;
    }
    redraw();
}

void CustomTextEditor::keyUp(enumKeyCodes key, double) {
    switch (key) {
        case KEY_Shift: case KEY_LeftShift: case KEY_RightShift:
            m_shiftHeld=false; break;
        case KEY_Control: case KEY_LeftControl: case KEY_RightContol:
            m_ctrlHeld=false; break;
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
    m_lineColX.clear();
    std::string cur;
    for (size_t i = 0; i < text.size(); ) {
        unsigned char ch = (unsigned char)text[i];
        if (ch=='\n'||ch=='\r'){m_lines.push_back(cur);cur.clear();i++;}
        else{int len=utf8::seqLen(ch);cur+=text.substr(i,len);i+=len;}
    }
    m_lines.push_back(cur);
    m_cursorLine=0; m_cursorCol=0;
    m_hasSelection=false; m_scrollY=0.f;
    redraw();
}
