#include "searcher.h"
#include <QFile>
#include <cstring>
#include <algorithm>

static constexpr char MAGIC[] = "FILESNAP";
static constexpr uint32_t VERSION = 0x00000500;
static constexpr uint32_t NOPARPOS = 0x3FFFFFFF;

struct FileHead {
    char magic[8];
    uint32_t version;
    int32_t nEntry;
};

// Case-insensitive wildcard match (* and ?)
static bool matchWild(const char* ptn, const char* str)
{
    while (*ptn) {
        switch (*ptn) {
        case '*':
            while (ptn[1] == '*') ++ptn;
            if (!ptn[1]) return true;
            if (matchWild(ptn + 1, str)) return true;
            if (!*str) return false;
            return matchWild(ptn, str + 1);
        case '?':
            if (!*str) return false;
            ++ptn; ++str;
            break;
        default: {
            char c1 = *ptn, c2 = *str;
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) return false;
            ++ptn; ++str;
            break;
        }
        }
    }
    if (!*str) return true;
    // Allow trailing / on directories
    if (str[0] == '/' && !str[1]) return true;
    return false;
}

bool Searcher::open(const std::string& dbfile)
{
    QFile file(QString::fromUtf8(dbfile.c_str()));
    if (!file.open(QIODevice::ReadOnly)) return false;

    FileHead head{};
    if (file.read(reinterpret_cast<char*>(&head), sizeof(head)) != sizeof(head))
        return false;
    if (memcmp(head.magic, MAGIC, 8) != 0 || head.version != VERSION)
        return false;

    m_records.clear();
    m_records.reserve(head.nEntry);

    for (int i = 0; i < head.nEntry; ++i) {
        uint32_t flags;
        uint16_t mode, reserved;
        uint32_t nlen;
        if (file.read(reinterpret_cast<char*>(&flags), 4) != 4) return false;
        if (file.read(reinterpret_cast<char*>(&mode), 2) != 2) return false;
        if (file.read(reinterpret_cast<char*>(&reserved), 2) != 2) return false;
        if (file.read(reinterpret_cast<char*>(&nlen), 4) != 4) return false;

        uint32_t padded = nlen + 1;
        if (padded & 3) padded += 4 - (padded & 3);
        QByteArray buf = file.read(padded);
        if (buf.size() != (int)padded) return false;

        Record rec;
        rec.parpos = flags & NOPARPOS;
        rec.has_subdir = (flags >> 30) & 1;
        rec.is_file = (flags >> 31) & 1;
        rec.mode = mode;
        rec.name = std::string(buf.constData(), nlen);
        m_records.push_back(std::move(rec));
    }
    return true;
}

std::string Searcher::getFullPath(int idx)
{
    std::vector<int> stack;
    for (uint32_t o = idx;; o = m_records[o].parpos) {
        stack.push_back(o);
        if (m_records[o].parpos == NOPARPOS) break;
    }
    std::string result;
    for (int i = (int)stack.size() - 1; i >= 0; --i) {
        auto& r = m_records[stack[i]];
        if (i == (int)stack.size() - 1) {
            result = r.name; // root is absolute path
        } else {
            if (result.empty() || result.back() != '/')
                result += '/';
            result += r.name;
        }
    }
    if (!m_records[idx].is_file) result += '/';
    return result;
}

bool Searcher::search(const std::string& pattern, SearchCallback cb, bool autoDetect, int limit, bool execOnly)
{
    if (m_records.empty()) return true;

    // Determine match mode
    bool wholePath = false;
    if (autoDetect)
        wholePath = pattern.find('/') != std::string::npos || pattern.find('\\') != std::string::npos;

    // Uppercase pattern for matching
    std::string ptn = pattern;

    int found = 0;

    if (wholePath) {
        // Build full paths incrementally
        std::vector<std::string> paths(m_records.size());
        for (size_t i = 0; i < m_records.size(); ++i) {
            auto& r = m_records[i];
            if (r.parpos == NOPARPOS) {
                paths[i] = r.name;
            } else {
                paths[i] = paths[r.parpos];
                if (!paths[i].empty() && paths[i].back() != '/')
                    paths[i] += '/';
                paths[i] += r.name;
            }
            if (!r.is_file) paths[i] += '/';

            if (execOnly && (!r.is_file || !(r.mode & 0111))) continue;

            if (matchWild(ptn.c_str(), paths[i].c_str())) {
                if (!cb(paths[i])) return false;
                if (limit > 0 && ++found >= limit) return false;
            }
        }
    } else {
        // Match filename only
        for (size_t i = 0; i < m_records.size(); ++i) {
            auto& r = m_records[i];
            if (execOnly && (!r.is_file || !(r.mode & 0111))) continue;

            std::string matchTarget = r.name;
            if (!r.is_file) matchTarget += '/';

            if (matchWild(ptn.c_str(), matchTarget.c_str())) {
                std::string full = getFullPath(i);
                if (!cb(full)) return false;
                if (limit > 0 && ++found >= limit) return false;
            }
        }
    }
    return true;
}
