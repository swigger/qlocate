#include "indexer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <cstring>
#include <vector>

#ifndef Q_OS_WIN
#include <sys/stat.h>
#endif

static constexpr char MAGIC[] = "FILESNAP";
static constexpr uint32_t VERSION = 0x00000500;
static constexpr uint32_t NOPARPOS = 0x3FFFFFFF;

struct FileHead {
    char magic[8];
    uint32_t version;
    int32_t nEntry;
};

struct DRecord {
    uint32_t flags;
    uint16_t mode;
    uint16_t reserved = 0;
    std::string name;
};

static uint32_t makeFlags(uint32_t parpos, bool has_subdir, bool is_file)
{
    return (parpos & NOPARPOS) | (has_subdir ? (1u << 30) : 0) | (is_file ? (1u << 31) : 0);
}

static uint16_t getMode(const QFileInfo& info)
{
#ifdef Q_OS_WIN
    return info.isDir() ? 0755 : 0644;
#else
    struct stat st;
    if (stat(info.absoluteFilePath().toUtf8().constData(), &st) == 0)
        return st.st_mode & 07777;
    return info.isDir() ? 0755 : 0644;
#endif
}

static void scanDir(const QString& dir, uint32_t parpos,
                    std::vector<DRecord>& records, uint32_t& count,
                    const QSet<QString>& excludePaths,
                    const QSet<QString>& ignoreDirnames,
                    const ProgressCallback& progress)
{
    QDir d(dir);
    if (!d.exists()) return;

    auto entries = d.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden, QDir::Name);
    for (auto& info : entries) {
        if (info.isSymLink()) continue;

        QString fullPath = info.absoluteFilePath();
        std::string nameUtf8 = info.fileName().toUtf8().constData();
        bool isFile = !info.isDir();

        uint32_t myPos = count;
        DRecord rec;
        rec.flags = makeFlags(parpos, false, isFile);
        rec.mode = getMode(info);
        rec.name = nameUtf8;
        records.push_back(rec);
        ++count;

        if (progress && (count & 0x1FF) == 0)
            progress(count, fullPath.toUtf8().constData());

        if (!isFile) {
            // Ignored dir: keep entry but skip contents
            if (ignoreDirnames.contains(info.fileName())) continue;
            if (excludePaths.contains(fullPath)) continue;

            scanDir(fullPath, myPos, records, count, excludePaths, ignoreDirnames, progress);
            if (myPos + 1 < count)
                records[myPos].flags |= (1u << 30);
        }
    }
}

bool createIndex(const DatabaseConfig& db, ProgressCallback progress)
{
    if (db.filename.empty()) return false;

    QSet<QString> excludePaths;
    for (auto& p : db.exclude_paths)
        excludePaths.insert(QString::fromUtf8(p.c_str()));

    QSet<QString> ignoreDirnames;
    for (auto& d : db.ignore_dirnames)
        ignoreDirnames.insert(QString::fromUtf8(d.c_str()));

    std::vector<DRecord> records;
    records.reserve(100000);
    uint32_t count = 0;

    for (auto& inc : db.include_paths) {
        QString path = QString::fromUtf8(inc.c_str());
        while (path.endsWith('/') || path.endsWith('\\'))
            path.chop(1);
#ifdef Q_OS_WIN
        // On Windows, "D:" is drive-relative (current dir on D:), not the
        // drive root. Restore the separator so "D:\" indexes the whole drive.
        if (path.length() == 2 && path[1] == QLatin1Char(':'))
            path.append(QLatin1Char('\\'));
#endif

        QFileInfo rootInfo(path);
        DRecord rootRec;
        rootRec.flags = makeFlags(NOPARPOS, false, false);
        rootRec.mode = getMode(rootInfo);
        rootRec.name = rootInfo.absoluteFilePath().toUtf8().constData();
        uint32_t rootPos = count;
        records.push_back(rootRec);
        ++count;

        scanDir(path, rootPos, records, count, excludePaths, ignoreDirnames, progress);
        if (rootPos + 1 < count)
            records[rootPos].flags |= (1u << 30);
    }

    // Write binary file
    QString outPath = QString::fromUtf8(db.filename.c_str());
    QDir().mkpath(QFileInfo(outPath).absolutePath());
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    FileHead head{};
    memcpy(head.magic, MAGIC, 8);
    head.version = VERSION;
    head.nEntry = static_cast<int32_t>(count);
    file.write(reinterpret_cast<const char*>(&head), sizeof(head));

    for (auto& rec : records) {
        file.write(reinterpret_cast<const char*>(&rec.flags), 4);
        file.write(reinterpret_cast<const char*>(&rec.mode), 2);
        file.write(reinterpret_cast<const char*>(&rec.reserved), 2);
        uint32_t nlen = static_cast<uint32_t>(rec.name.size());
        file.write(reinterpret_cast<const char*>(&nlen), 4);
        uint32_t padded = nlen + 1;
        if (padded & 3) padded += 4 - (padded & 3);
        std::vector<char> buf(padded, 0);
        memcpy(buf.data(), rec.name.c_str(), nlen);
        file.write(buf.data(), padded);
    }

    file.close();
    return true;
}
