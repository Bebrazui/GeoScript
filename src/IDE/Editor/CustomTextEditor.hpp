#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <Geode/Geode.hpp>

using namespace geode::prelude;

enum class SyntaxTokenType {
    Normal, Keyword, Number, String, Comment, Operator, Identifier,
};

struct SyntaxToken {
    SyntaxTokenType type;
    std::string text;
    SyntaxToken(SyntaxTokenType t, std::string s) : type(t), text(std::move(s)) {}
};

namespace utf8 {
    inline int seqLen(unsigned char c) {
        if (c < 0x80) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 1;
    }
    inline int cpLen(const std::string& s) {
        int n = 0;
        for (size_t i = 0; i < s.size(); ) { i += seqLen((unsigned char)s[i]); n++; }
        return n;
    }
    inline int byteOffset(const std::string& s, int cp) {
        int off = 0;
        for (int i = 0; i < cp && off < (int)s.size(); i++)
            off += seqLen((unsigned char)s[off]);
        return off;
    }
    inline int eraseCP(std::string& s, int bytePos) {
        if (bytePos <= 0) return 0;
        int p = bytePos - 1;
        while (p > 0 && ((unsigned char)s[p] & 0xC0) == 0x80) p--;
        s.erase(p, bytePos - p);
        return p;
    }
}

class CustomTextEditor : public cocos2d::CCLayer, public CCKeyboardDelegate {
protected:
    std::vector<std::string> m_lines;
    int   m_cursorLine = 0;
    int   m_cursorCol  = 0;

    // Visual
    float m_lineHeight   = 11.f;
    float m_fontSize     = 9.f;
    float m_lineNumWidth = 26.f;
    float m_scrollY      = 0.f;

    // Per-line cursor X positions (codepoint index → screen X)
    // Rebuilt during redraw for visible lines
    std::unordered_map<int, std::vector<float>> m_lineColX;

    CCNode*       m_textLayer   = nullptr;
    CCNode*       m_cursorLayer = nullptr;
    CCLayerColor* m_cursorNode  = nullptr;
    bool          m_cursorVisible = true;

    bool m_shiftHeld = false;
    bool m_ctrlHeld  = false;

    bool m_hasSelection = false;
    int  m_selStartLine = 0, m_selStartCol = 0;
    int  m_selEndLine   = 0, m_selEndCol   = 0;

    static constexpr const char* kFont = "GoogleSans-Regular.ttf";
    static constexpr const char* kFontFallback = "Courier New";

public:
    static CustomTextEditor* create(CCSize size);
    bool init(CCSize size);

    void redraw();
    void updateCursorPos();
    void blinkCursor(float dt);
    void scrollWheel(float y, float x) override;

    void keyDown(enumKeyCodes key, double) override;
    void keyUp(enumKeyCodes key, double) override;

    void insertText(const std::string& text);
    void deleteBackward();

    std::string getText() const;
    void setText(const std::string& text);

    bool ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*) override;

private:
    void insertUTF8(const std::string& s);
    void insertNewline();
    void deleteCharBefore();
    void deleteSelection();
    void selectAll();
    void clampCursor();
    void scrollToCursor();

    int cursorByteOffset() const;

    // Returns X position of codepoint cp in line lineIdx (uses cache)
    float getColX(int lineIdx, int cp);

    std::vector<SyntaxToken> tokenizeLine(const std::string& line) const;
    CCLabelTTF* makeLabel(const std::string& text, ccColor3B color, float x, float y);

    // Measure text width using a temporary label
    float measureText(const std::string& text);
};
