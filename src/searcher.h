#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Returns false from callback to stop searching
using SearchCallback = std::function<bool(const std::string& fullpath)>;

class Searcher {
public:
    bool open(const std::string& dbfile);
    // pattern: wildcard with * and ?, case-insensitive
    // wholePathMatch: match full path or filename only (auto-detect if pattern contains /)
    // execOnly: only return files with executable bit set
    bool search(const std::string& pattern, SearchCallback cb, bool autoDetect = true, int limit = 0, bool execOnly = false);

private:
    struct Record {
        uint32_t parpos;
        bool has_subdir;
        bool is_file;
        uint16_t mode;
        std::string name;
    };
    std::vector<Record> m_records;
    std::string getFullPath(int idx);
};
