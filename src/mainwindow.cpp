#include "mainwindow.h"
#include "indexer.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

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
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    vbox->addWidget(m_list);

    m_status = new QLabel;
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    vbox->addWidget(m_status);

    connect(m_edit, &QLineEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(m_edit, &QLineEdit::returnPressed, this, &MainWindow::onSearch);
    connect(btnSearch, &QPushButton::clicked, this, &MainWindow::onSearch);
    connect(btnUpdate, &QPushButton::clicked, this, &MainWindow::onUpdate);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &MainWindow::onContextMenu);

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
    revealInFileManager(path);
}

void MainWindow::onContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_list->itemAt(pos);
    if (!item) return;

    QString path = item->text();
    // Normalize trailing slash on directory entries for path computations.
    QString normalized = path;
    if (normalized.endsWith('/') && normalized.length() > 1)
        normalized.chop(1);
    QFileInfo fi(normalized);
    QString parent = fi.absolutePath();
    QString name = fi.fileName();
    QPoint globalPos = m_list->viewport()->mapToGlobal(pos);

#ifdef Q_OS_WIN
    // On Windows, show the full native Explorer context menu (Open, Cut,
    // Copy, Properties, shell extensions, ...) with our custom items merged
    // on top. Falls back to the Qt menu below if the shell call fails.
    if (showWinShellMenu(normalized, path, parent, name, globalPos))
        return;
#endif

    QMenu menu(this);
    QAction* copyPath = menu.addAction("Copy Path");
    QAction* copyParent = menu.addAction("Copy Parent Directory");
    QAction* copyName = menu.addAction("Copy Filename");
    menu.addSeparator();
    QAction* openMgr = menu.addAction("Open in File Manager");

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    QClipboard* clip = QGuiApplication::clipboard();
    if (chosen == copyPath) {
        clip->setText(path);
        m_status->setText("Copied path: " + path);
    } else if (chosen == copyParent) {
        clip->setText(parent);
        m_status->setText("Copied parent directory: " + parent);
    } else if (chosen == copyName) {
        clip->setText(name);
        m_status->setText("Copied filename: " + name);
    } else if (chosen == openMgr) {
        revealInFileManager(normalized);
    }
}

void MainWindow::revealInFileManager(const QString& path)
{
    QFileInfo fi(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? path : fi.absolutePath()));
}

#ifdef Q_OS_WIN
namespace {
    enum {
        CMD_COPY_PATH   = 1,
        CMD_COPY_PARENT = 2,
        CMD_COPY_NAME   = 3,
        CMD_OPEN_MGR    = 4,
        CMD_SHELL_FIRST = 1000,   // shell verbs get IDs >= this
        CMD_SHELL_LAST  = 0x7FFF
    };
}

bool MainWindow::showWinShellMenu(const QString& fsPath, const QString& fullText,
                                  const QString& parentDir, const QString& fileName,
                                  const QPoint& globalPos)
{
    HWND hwnd = reinterpret_cast<HWND>(window()->winId());
    std::wstring wpath = QDir::toNativeSeparators(fsPath).toStdWString();

    LPITEMIDLIST pidl = nullptr;
    HRESULT hr = SHParseDisplayName(wpath.c_str(), nullptr, &pidl, 0, nullptr);
    if (FAILED(hr) || !pidl)
        return false;

    IShellFolder* parentFolder = nullptr;
    LPCITEMIDLIST childPidl = nullptr;
    hr = SHBindToParent(pidl, IID_IShellFolder,
                        reinterpret_cast<void**>(&parentFolder), &childPidl);
    if (FAILED(hr) || !parentFolder) {
        CoTaskMemFree(pidl);
        return false;
    }

    IContextMenu* ctxMenu = nullptr;
    hr = parentFolder->GetUIObjectOf(hwnd, 1, &childPidl, IID_IContextMenu,
                                     nullptr, reinterpret_cast<void**>(&ctxMenu));
    if (FAILED(hr) || !ctxMenu) {
        parentFolder->Release();
        CoTaskMemFree(pidl);
        return false;
    }

    HMENU hmenu = CreatePopupMenu();
    // Our custom items on top.
    AppendMenuW(hmenu, MF_STRING, CMD_COPY_PATH,   L"Copy Path");
    AppendMenuW(hmenu, MF_STRING, CMD_COPY_PARENT, L"Copy Parent Directory");
    AppendMenuW(hmenu, MF_STRING, CMD_COPY_NAME,   L"Copy Filename");
    AppendMenuW(hmenu, MF_STRING, CMD_OPEN_MGR,    L"Open in File Manager");
    AppendMenuW(hmenu, MF_SEPARATOR, 0, nullptr);

    // Native Explorer verbs appended after our items.
    ctxMenu->QueryContextMenu(hmenu, GetMenuItemCount(hmenu),
                              CMD_SHELL_FIRST, CMD_SHELL_LAST,
                              CMF_NORMAL | CMF_EXPLORE);

    UINT cmd = TrackPopupMenuEx(hmenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                globalPos.x(), globalPos.y(), hwnd, nullptr);

    if (cmd >= CMD_SHELL_FIRST) {
        CMINVOKECOMMANDINFOEX info = {};
        info.cbSize = sizeof(info);
        info.fMask  = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
        info.hwnd   = hwnd;
        info.lpVerb  = MAKEINTRESOURCEA(cmd - CMD_SHELL_FIRST);
        info.lpVerbW = MAKEINTRESOURCEW(cmd - CMD_SHELL_FIRST);
        info.nShow  = SW_SHOWNORMAL;
        info.ptInvoke = { globalPos.x(), globalPos.y() };
        ctxMenu->InvokeCommand(reinterpret_cast<LPCMINVOKECOMMANDINFO>(&info));
    } else if (cmd != 0) {
        QClipboard* clip = QGuiApplication::clipboard();
        switch (cmd) {
        case CMD_COPY_PATH:
            clip->setText(fullText);
            m_status->setText("Copied path: " + fullText);
            break;
        case CMD_COPY_PARENT:
            clip->setText(parentDir);
            m_status->setText("Copied parent directory: " + parentDir);
            break;
        case CMD_COPY_NAME:
            clip->setText(fileName);
            m_status->setText("Copied filename: " + fileName);
            break;
        case CMD_OPEN_MGR:
            revealInFileManager(fsPath);
            break;
        }
    }

    DestroyMenu(hmenu);
    ctxMenu->Release();
    parentFolder->Release();
    CoTaskMemFree(pidl);
    return true;
}
#endif
