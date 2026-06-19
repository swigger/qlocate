#pragma once
#include "config.h"
#include <cstdint>
#include <functional>

// Binary format v5: FILESNAP header + records
// Record: { uint32_t flags (parpos:30, has_subdir:1, is_file:1), uint16_t mode, uint16_t reserved, uint32_t namelen, char name[padded] }

using ProgressCallback = std::function<void(int count, const std::string& path)>;

bool createIndex(const DatabaseConfig& db, ProgressCallback progress = nullptr);
