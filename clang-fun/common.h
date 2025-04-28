#pragma once

#include "../check_names.h"

#include <unordered_map>
#include <string>
#include <filesystem>
#include <optional>
#include <vector>

std::unordered_map<std::string, Statistics> ReadExpected(const std::filesystem::path& path);

void WriteResult(const std::unordered_map<std::string, Statistics>& map,
                 const std::filesystem::path& path);

std::vector<std::filesystem::path> GetCppFiles(const std::filesystem::path& dir);

void CheckDir(const std::filesystem::path& dir,
              const std::optional<std::filesystem::path>& dict = std::nullopt);
