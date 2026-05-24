#pragma once
#include <vector>
#include <string>
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// Token types for syntax highlighting
enum class SyntaxTokenType {
    Normal,
    Keyword,
    Number,
    String,
    Comment,
    Operator,
    Identifier,
};

struct SyntaxToken {
    SyntaxTokenType type;
    std::string text; // UTF-8
    SyntaxToken(SyntaxTokenType t, std::string s) : type(t), text(std::move(s)) {}
};

// UTF-8 helpers
namespace utf8 {
    // Returns byte length of the codepoint starting at s[i]
    inline int seqLen(unsigned char c) {
        if (c < 0x80) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 1; // continuation byte — treat as 1
    }
    // Count codepoints in a UTF-8 string
    inline int cpLen(const std::string& s) {
        int n = 0;
        for (size_t i = 0; i < s.size(); ) {
            i += seqLen((unsigned char)s[i]);
            n++;
        }
        return n;
    }
    // Byte offset of the n-th codepoint
    inline int byteOffset(const std::string& s, int cp) {
        int off = 0;
        for (int i = 0; i < cp && off < (int)s.size(); i++)
            off += seqLen((unsigned char)s[off]);
        return off;
    }
    // Erase one codepoint before byte position pos, returns new pos
    inline int eraseCP(std::string& s, int bytePos) {
        if (bytePos <= 0) return 0;
        // Walk back to find start of previous codepoint
        int p = bytePos - 1;
        while (p > 0 && ((unsigned char)s[p] & 0xC0) == 0x80) p--;
        s.erase(p, bytePos - p);
        return p;
    }
}

class CustomTextEditor : public cocos2d::CCLayer,
                         public CCKeyboardDelegate {
protected:
    // Lines stored as UTF-8 strings
    std::vector<std::string> m_lines;
    // Cursor position in CODEPOINTS (not bytes)
    int m_cursorLine = 0;
    int m_cursorCol  = 0; // codepoint index

    // Visual settings
    float m_lineHeight   = 18.f;
    float m_fontSize     = 14.f;
    float m_charWidth    = 8.4f;   // approx width per codepoint at m_fontSize
    float m_lineNumWidth = 36.f;

    // Node layers
    CCNode*       m_textLayer   = nullptr;
    CCNode*       m_cursorLayer = nullptr;
    CCLayerColor* m_cursorNode  = nullptr;
    bool          m_cursorVisible = true;

    // Hidden input node for IME / Unicode input — REMOVED, now using global hook

    // Keyboard state
    bool m_shiftHeld = false;
    bool m_ctrlHeld  = false;

    // Selection (for Ctrl+A)
    bool m_hasSelection = false;
    int  m_selStartLine = 0, m_selStartCol = 0;
    int  m_selEndLine   = 0, m_selEndCol   = 0;

    static constexpr const char* kFont = "GoogleSans-Regular.ttf";

public:
    static CustomTextEditor* create(CCSize size);
    bool init(CCSize size);

    void redraw();
    void updateCursorPos();
    void blinkCursor(float dt);

    // CCKeyboardDelegate — special keys (arrows, enter, ctrl combos)
    void keyDown(enumKeyCodes key, double) override;
    void keyUp(enumKeyCodes key, double) override;

    // Called from IME hook — printable text input (all chars incl. Cyrillic)
    void insertText(const std::string& text);
    void deleteBackward();

    std::string getText() const;
    void setText(const std::string& text);

private:
    // Insert a UTF-8 string at cursor
    void insertUTF8(const std::string& s);
    void insertNewline();
    void deleteCharBefore();   // delete one codepoint before cursor
    void deleteSelection();
    void selectAll();
    void clampCursor();

    // Byte offset of cursor in current line
    int cursorByteOffset() const;

    // Tokenize a single line for syntax highlighting
    std::vector<SyntaxToken> tokenizeLine(const std::string& line) const;

    CCLabelTTF* makeLabel(const std::string& text, ccColor3B color, float x, float y);
};
