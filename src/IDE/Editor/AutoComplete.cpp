#include "AutoComplete.hpp"

using namespace geode::prelude;

AutoComplete* AutoComplete::create() {
    auto ret = new AutoComplete();
    ret->autorelease();
    return ret;
}
