#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <string>

class Autocompleter {
public:
    static std::vector<std::string> getSuggestions(const std::string& input);
};
