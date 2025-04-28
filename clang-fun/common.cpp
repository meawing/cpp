#include "common.h"

#include <unordered_map>
#include <string>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

std::unordered_map<std::string, Statistics> ReadExpected(const std::filesystem::path& path) {
    std::ifstream in{path};
    std::unordered_map<std::string, Statistics> map;
    std::string file;

    size_t n;
    for (in >> n; n; --n) {
        in >> file;
        auto& [bad_names, mistakes] = map.emplace(file, Statistics{}).first->second;

        size_t k;
        for (in >> k; k; --k) {
            auto& bad_name = bad_names.emplace_back();
            int entity;
            in >> bad_name.file >> bad_name.name >> entity >> bad_name.line;
            bad_name.entity = static_cast<Entity>(entity);
        }

        for (in >> k; k; --k) {
            auto& mistake = mistakes.emplace_back();
            in >> mistake.file >> mistake.name;
            in >> mistake.wrong_word >> mistake.ok_word >> mistake.line;
        }
    }
    return map;
}

void WriteResult(const std::unordered_map<std::string, Statistics>& map,
                 const std::filesystem::path& path) {
    std::ofstream out{path};
    out << map.size() << '\n';
    for (const auto& [file, stats] : map) {
        out << file << '\n';
        const auto& [bad_names, mistakes] = stats;

        out << bad_names.size() << '\n';
        for (const auto& bad_name : bad_names) {
            out << bad_name.file << ' ' << bad_name.name << ' ';
            out << static_cast<int>(bad_name.entity) << ' ' << bad_name.line << '\n';
        }

        out << mistakes.size() << '\n';
        for (const auto& mistake : mistakes) {
            out << mistake.file << ' ' << mistake.name << ' ' << mistake.wrong_word << ' ';
            out << mistake.ok_word << ' ' << mistake.line << '\n';
        }
    }
}

std::vector<std::filesystem::path> GetCppFiles(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator{dir}) {
        if (const auto& path = entry.path(); path.extension() == ".cpp") {
            result.push_back(path);
        }
    }
    return result;
}

void CheckDir(const std::filesystem::path& dir, const std::optional<std::filesystem::path>& dict) {
    auto files = GetCppFiles(dir);
    REQUIRE_FALSE(files.empty());

    std::vector args = {"./test_check_names", "-p", "."};
    if (dict) {
        args.push_back("-dict");
        args.push_back(dict->c_str());
    }
    for (const auto& file : files) {
        args.push_back(file.c_str());
    }

    auto result = CheckNames(args.size(), args.data());
    auto expected = ReadExpected(dir / "expected.txt");
    CHECK(result == expected);
}
