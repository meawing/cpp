#pragma once

#include <string>
#include <vector>
#include <unordered_map>

enum class Entity { kVariable, kField, kType, kConst, kFunction };

struct BadName {
    std::string file;
    std::string name;
    Entity entity;
    size_t line;

    bool operator==(const BadName&) const = default;
};

struct Mistake {
    std::string file;
    std::string name;
    std::string wrong_word;
    std::string ok_word;
    size_t line;

    bool operator==(const Mistake&) const = default;
};

struct Statistics {
    std::vector<BadName> bad_names;
    std::vector<Mistake> mistakes;

    bool operator==(const Statistics&) const = default;
};

std::unordered_map<std::string, Statistics> CheckNames(int argc, const char* argv[]);
