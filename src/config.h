#pragma once
#include <string>
#include <vector>

struct DatabaseConfig {
    std::string name;
    std::string filename;
    std::vector<std::string> include_paths;
    std::vector<std::string> exclude_paths;
    std::vector<std::string> ignore_dirnames; // e.g. node_modules, .git
    bool default_update = true;
};

struct AppConfig {
    int file_limit_interactive = 50;
    int file_limit_search = 2000;
    std::vector<DatabaseConfig> databases;
};

AppConfig loadConfig(const std::string& path);
std::string findConfigPath();
