#include "mainwindow.h"
#include "indexer.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QMessageBox>

MainWindow::MainWindow(const AppConfig& cfg, QWidget* parent)
    : QWidget(parent), m_cfg(cfg)
{
    setWindowTitle("qlocate");
    resize(700, 500);

    auto* vbox = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout;
    m_edit = new QLineEdit;
    m_edit->setPlaceholderText("Search pattern (* and ? wildcards)...");
    topRow->addWidget(m_edit);

    m_execOnly = new QCheckBox("Exec only");
    topRow->addWidget(m_execOnly);

    auto* btnSearch = new QPushButton("Search");
    topRow->addWidget(btnSearch);

    auto* btnUpdate = new QPushButton("Update Index");
    topRow->addWidget(btnUpdate);
    vbox->addLayout(topRow);

    m_list = new QListWidget;
    vbox->addWidget(m_list);

    m_status = new QLabel;
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    vbox->addWidget(m_status);

    connect(m_edit, &QLineEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(m_edit, &QLineEdit::returnPressed, this, &MainWindow::onSearch);
    connect(btnSearch, &QPushButton::clicked, this, &MainWindow::onSearch);
    connect(btnUpdate, &QPushButton::clicked, this, &MainWindow::onUpdate);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &MainWindow::onItemDoubleClicked);

    loadSearchers();
}

void MainWindow::loadSearchers()
{
    m_searchers.clear();
    for (auto& db : m_cfg.databases) {
        Searcher s;
        if (s.open(db.filename))
            m_searchers.push_back(std::move(s));
    }
}

void MainWindow::onTextChanged(const QString& text)
{
    if (text.length() < 2) return;
    QString ptn = text;
    if (!ptn.endsWith('*') && !ptn.contains('/'))
        ptn += '*';
    doSearch(ptn, m_cfg.file_limit_interactive);
}

void MainWindow::onSearch()
{
    QString ptn = m_edit->text();
    if (ptn.isEmpty()) return;
    doSearch(ptn, m_cfg.file_limit_search);
}

void MainWindow::doSearch(const QString& pattern, int limit)
{
    m_list->clear();
    int count = 0;
    bool execOnly = m_execOnly->isChecked();
    std::string ptn = pattern.toUtf8().constData();

    for (auto& s : m_searchers) {
        s.search(ptn, [&](const std::string& path) -> bool {
            m_list->addItem(QString::fromUtf8(path.c_str()));
            return ++count < limit;
        }, true, limit - count, execOnly);
        if (count >= limit) break;
    }
    m_status->setText(QString("%1 files found").arg(count));
}

void MainWindow::onUpdate()
{
    bool forceAll = QApplication::keyboardModifiers() & Qt::ShiftModifier;
    m_status->setText(forceAll ? "Force updating all indexes..." : "Updating index...");
    QApplication::processEvents();

    for (auto& db : m_cfg.databases) {
        if (forceAll || db.default_update) {
            createIndex(db, [this](int count, const std::string& path) {
                if ((count & 0x1FF) == 0) {
                    m_status->setText(QString("Indexing: %1").arg(QString::fromUtf8(path.c_str())));
                    QApplication::processEvents();
                }
            });
        }
    }

    loadSearchers();
    m_status->setText("Index updated.");
}

void MainWindow::onItemDoubleClicked(QListWidgetItem* item)
{
    QString path = item->text();
    if (path.endsWith('/'))
        path.chop(1);
    QFileInfo fi(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? path : fi.absolutePath()));
}
