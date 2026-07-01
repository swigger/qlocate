#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include "config.h"
#include "searcher.h"

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(const AppConfig& cfg, QWidget* parent = nullptr);

private slots:
    void onTextChanged(const QString& text);
    void onSearch();
    void onUpdate();
    void onItemDoubleClicked(QListWidgetItem* item);
    void onContextMenu(const QPoint& pos);

private:
    void doSearch(const QString& pattern, int limit);
    void loadSearchers();
    void revealInFileManager(const QString& path);
#ifdef Q_OS_WIN
    bool showWinShellMenu(const QString& fsPath, const QString& fullText,
                          const QString& parentDir, const QString& fileName,
                          const QPoint& globalPos);
#endif

    AppConfig m_cfg;
    QLineEdit* m_edit;
    QListWidget* m_list;
    QLabel* m_status;
    QCheckBox* m_execOnly;
    std::vector<Searcher> m_searchers;
};
