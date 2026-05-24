#include "SyntaxHighlighter.hpp"

using namespace geode::prelude;

SyntaxHighlighter* SyntaxHighlighter::create() {
    auto ret = new SyntaxHighlighter();
    ret->autorelease();
    return ret;
}
