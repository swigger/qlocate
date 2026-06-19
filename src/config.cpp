#include "config.h"
#include <toml.hpp>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

std::string findConfigPath()
{
    // Look next to executable first, then in home directory
    QString exeDir = QCoreApplication::applicationDirPath();
    QString candidate = exeDir + "/qlocate.toml";
    if (QFileInfo::exists(candidate))
        return candidate.toStdString();

    candidate = QDir::homePath() + "/.config/qlocate/qlocate.toml";
    if (QFileInfo::exists(candidate))
        return candidate.toStdString();

    return {};
}

AppConfig loadConfig(const std::string& path)
{
    AppConfig cfg;
    if (path.empty()) return cfg;

    auto data = toml::parse(path);

    if (data.contains("settings")) {
        auto& s = data.at("settings");
        cfg.file_limit_interactive = toml::find_or(s, "file_limit_interactive", 50);
        cfg.file_limit_search = toml::find_or(s, "file_limit_search", 2000);
    }

    if (data.contains("database")) {
        auto& dbs = data.at("database");
        // database is array of tables
        if (dbs.is_array_of_tables()) {
            for (auto& db : dbs.as_array()) {
                DatabaseConfig dc;
                dc.name = toml::find_or(db, "name", "default");
                dc.filename = toml::find_or(db, "filename", "");
                dc.include_paths = toml::find_or<std::vector<std::string>>(db, "include_paths", {});
                dc.exclude_paths = toml::find_or<std::vector<std::string>>(db, "exclude_paths", {});
                dc.ignore_dirnames = toml::find_or<std::vector<std::string>>(db, "ignore_dirnames", {});
                dc.default_update = toml::find_or(db, "default_update", true);
                cfg.databases.push_back(std::move(dc));
            }
        }
    }

    return cfg;
}
