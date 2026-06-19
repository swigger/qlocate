#include "config.h"
#include "indexer.h"
#include "searcher.h"
#include "mainwindow.h"
#include <QApplication>
#include <iostream>
#include <cstring>

static void printUsage(const char* argv0)
{
    std::cerr << "Usage:\n"
              << "  " << argv0 << " --update              Update all indexes\n"
              << "  " << argv0 << " [-x] pattern [...]    Search files (CLI)\n"
              << "  " << argv0 << "                       Launch GUI\n"
              << "\nOptions:\n"
              << "  -x, --exec   Only show executable files\n"
              << "  -c file      Specify config file path\n";
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("qlocate");

    // Parse args
    std::string configPath;
    bool doUpdate = false;
    bool execOnly = false;
    std::vector<std::string> patterns;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--update") == 0 || strcmp(argv[i], "-u") == 0) {
            doUpdate = true;
        } else if (strcmp(argv[i], "--exec") == 0 || strcmp(argv[i], "-x") == 0) {
            execOnly = true;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            configPath = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            patterns.push_back(argv[i]);
        }
    }

    if (configPath.empty())
        configPath = findConfigPath();
    AppConfig cfg;
    if (!configPath.empty()) {
        try { cfg = loadConfig(configPath); }
        catch (std::exception& e) {
            std::cerr << "Config error: " << e.what() << "\n";
            return 1;
        }
    }

    // CLI: update mode
    if (doUpdate) {
        for (auto& db : cfg.databases) {
            std::cout << "Updating: " << db.name << " -> " << db.filename << "\n";
            createIndex(db, [](int count, const std::string& path) {
                if ((count & 0x1FF) == 0)
                    std::cout << "  [" << count << "] " << path << "\n";
            });
        }
        std::cout << "Done.\n";
        return 0;
    }

    // CLI: search mode
    if (!patterns.empty()) {
        std::vector<Searcher> searchers;
        for (auto& db : cfg.databases) {
            Searcher s;
            if (s.open(db.filename))
                searchers.push_back(std::move(s));
        }
        if (searchers.empty()) {
            std::cerr << "No index files found. Run with --update first.\n";
            return 1;
        }
        for (auto& ptn : patterns) {
            for (auto& s : searchers) {
                s.search(ptn, [](const std::string& path) -> bool {
                    std::cout << path << "\n";
                    return true;
                }, true, 0, execOnly);
            }
        }
        return 0;
    }

    // GUI mode
    MainWindow w(cfg);
    w.show();
    return app.exec();
}
