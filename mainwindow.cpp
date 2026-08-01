#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QFont>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QIcon>
#include <QPixmap>
#include <QScreen>
#include <QCloseEvent>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDialog>
#include <QPalette>
#include <QTextBlock>
#include <QScrollBar>
#include <QMouseEvent>
#include <QEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDateTime>
#include <QCheckBox>
#include <QShortcut>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QTextCursor>
#include <QTextDocument>
#include <QSet>
#include <algorithm>

// Display label for a snippet in the list: a star prefix marks favorites,
// and empty titles fall back to a placeholder.
static QString snippetLabel(const Snippet& s) {
    QString t = s.title.isEmpty() ? QStringLiteral("Untitled Snippet") : s.title;
    return s.favorite ? (QStringLiteral("★ ") + t) : t;
}

// Turns a snippet title into a filesystem-safe base filename.
static QString sanitizeFileName(const QString& title) {
    QString name = title.trimmed();
    if (name.isEmpty()) name = QStringLiteral("untitled");
    for (QChar& c : name) {
        if (QStringLiteral("/\\:*?\"<>|").contains(c) || c < ' ')
            c = QChar('_');
    }
    return name;
}

// ── Custom delegate to draw ≡ drag handle ─────────────────────
class FolderDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, option, index);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QColor lineColor = option.state & QStyle::State_Selected
                               ? QColor(255, 255, 255, 180)
                               : QColor(150, 150, 150, 200);
        painter->setPen(QPen(lineColor, 1.5));
        int x1 = option.rect.left() + 6;
        int x2 = option.rect.left() + 18;
        int cy = option.rect.center().y();
        painter->drawLine(x1, cy - 4, x2, cy - 4);
        painter->drawLine(x1, cy,     x2, cy);
        painter->drawLine(x1, cy + 4, x2, cy + 4);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(qMax(s.height(), 28));
        return s;
    }
};

// ── Custom delegate to truncate long snippet names ─────────────
class SnippetDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        opt.text.clear();
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        painter->save();
        QRect textRect = opt.rect.adjusted(4, 0, -4, 0);
        QString fullText = index.data(Qt::DisplayRole).toString();
        QString elidedText = opt.fontMetrics.elidedText(fullText, Qt::ElideRight, textRect.width());

        // The text colour has to match whichever background the style paints:
        //  - Hover wins over selection (per the stylesheet), and the hover band
        //    is light in light mode / dark in dark mode — so use the normal text
        //    colour (dark-on-light, light-on-dark), never the white highlight.
        //  - A non-hovered, active selection paints the blue highlight → white.
        //  - Everything else (normal rows, unfocused/grey selection) → normal.
        const bool hovered  = opt.state & QStyle::State_MouseOver;
        const bool selected = opt.state & QStyle::State_Selected;
        const bool active   = opt.state & QStyle::State_Active;
        if (!hovered && selected && active)
            painter->setPen(opt.palette.highlightedText().color());
        else
            painter->setPen(opt.palette.text().color());

        painter->setFont(opt.font);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedText);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(qMax(s.height(), 28));
        return s;
    }
};

// ── Palettes ──────────────────────────────────────────────────
static QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window,          QColor(42,  42,  42));
    p.setColor(QPalette::WindowText,      QColor(212, 212, 212));
    p.setColor(QPalette::Base,            QColor(24,  24,  24));
    p.setColor(QPalette::AlternateBase,   QColor(36,  36,  36));
    p.setColor(QPalette::ToolTipBase,     QColor(45,  45,  45));
    p.setColor(QPalette::ToolTipText,     QColor(212, 212, 212));
    p.setColor(QPalette::Text,            QColor(212, 212, 212));
    p.setColor(QPalette::Button,          QColor(55,  55,  55));
    p.setColor(QPalette::ButtonText,      QColor(212, 212, 212));
    p.setColor(QPalette::BrightText,      Qt::white);
    p.setColor(QPalette::Link,            QColor(86,  156, 214));
    p.setColor(QPalette::Highlight,       QColor(38,  79,  120));
    p.setColor(QPalette::HighlightedText, QColor(212, 212, 212));
    p.setColor(QPalette::PlaceholderText, QColor(110, 110, 110));
    p.setColor(QPalette::Mid,             QColor(55,  55,  55));
    p.setColor(QPalette::Dark,            QColor(25,  25,  25));
    p.setColor(QPalette::Shadow,          QColor(10,  10,  10));
    return p;
}

static QPalette lightPalette() {
    QPalette p;
    p.setColor(QPalette::Window,          QColor(240, 240, 240));
    p.setColor(QPalette::WindowText,      QColor(30,  30,  30));
    p.setColor(QPalette::Base,            QColor(255, 255, 255));
    p.setColor(QPalette::AlternateBase,   QColor(245, 245, 245));
    p.setColor(QPalette::ToolTipBase,     QColor(255, 255, 220));
    p.setColor(QPalette::ToolTipText,     QColor(30,  30,  30));
    p.setColor(QPalette::Text,            QColor(30,  30,  30));
    p.setColor(QPalette::Button,          QColor(225, 225, 225));
    p.setColor(QPalette::ButtonText,      QColor(30,  30,  30));
    p.setColor(QPalette::BrightText,      Qt::black);
    p.setColor(QPalette::Link,            QColor(0,   100, 200));
    p.setColor(QPalette::Highlight,       QColor(0,   120, 215));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
    p.setColor(QPalette::Mid,             QColor(180, 180, 180));
    p.setColor(QPalette::Dark,            QColor(160, 160, 160));
    p.setColor(QPalette::Shadow,          QColor(100, 100, 100));
    return p;
}

// ── Constructor ───────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUI();
    setupMenuBar();
    loadFolders();

    // Restore wrap
    QString savedWrap = DatabaseManager::instance().getSetting("WrapCode", "off");
    wrapEnabled = (savedWrap == "on");
    wrapAction->setChecked(wrapEnabled);
    codeEditor->setLineWrapMode(wrapEnabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

    // Restore dark mode
    QString savedDark = DatabaseManager::instance().getSetting("DarkMode", "off");
    darkModeEnabled = (savedDark == "on");
    darkModeAction->setChecked(darkModeEnabled);
    qApp->setPalette(darkModeEnabled ? darkPalette() : lightPalette());
    applyThemeStyles(darkModeEnabled);
    codeEditor->darkMode = darkModeEnabled;

    // Restore editor font size
    editorFontSize = DatabaseManager::instance().getSetting("EditorFontSize", "12").toInt();
    if (editorFontSize < 8)  editorFontSize = 8;
    if (editorFontSize > 40) editorFontSize = 40;
    applyEditorFont();

    // Restore line numbers
    QString savedLines = DatabaseManager::instance().getSetting("LineNumbers", "off");
    lineNumbersEnabled = (savedLines == "on");
    lineNumberAction->setChecked(lineNumbersEnabled);
    codeEditor->showLineNumbers = lineNumbersEnabled;
    lineNumberArea->setVisible(lineNumbersEnabled);
    codeEditor->updateLineNumberAreaWidth();

    // Restore window size
    QString savedSize = DatabaseManager::instance().getSetting("WindowSize");
    if (!savedSize.isEmpty()) {
        QStringList parts = savedSize.split("x");
        if (parts.size() == 2) {
            int w = parts[0].toInt();
            int h = parts[1].toInt();
            if (w > 400 && h > 300) resize(w, h);
        }
    }

    // Restore column layout (left + middle + editor split)
    QString savedSplitter = DatabaseManager::instance().getSetting("SplitterState");
    if (!savedSplitter.isEmpty())
        mainSplitter->restoreState(QByteArray::fromBase64(savedSplitter.toLatin1()));

    // Restore the code / notes divider inside the editor pane
    QString savedEditorSplitter = DatabaseManager::instance().getSetting("EditorSplitterState");
    if (!savedEditorSplitter.isEmpty())
        editorSplitter->restoreState(QByteArray::fromBase64(savedEditorSplitter.toLatin1()));

    // Restore the folder + snippet that were open last session
    restoreLastSelection();

    // The editor now reflects the freshly loaded snippet, so nothing is unsaved yet.
    setDirty(false);
    updateStatusInfo();

    // Center on screen
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect geo = screen->availableGeometry();
        move((geo.width() - width()) / 2, (geo.height() - height()) / 2);
    }
    statusBar()->setStyleSheet(darkModeEnabled
                                   ? "QStatusBar { background-color: #2a2a2a; border-top: 2px solid #3a3a3a; }"
                                   : "QStatusBar { background-color: #cccccc; border-top: 2px solid #bbbbbb; }");
}

// ── Restore last opened folder / snippet ──────────────────────
// Called once at startup, after loadFolders() has populated the sidebar.
// Silently does nothing if the saved IDs no longer exist (deleted, or a
// different database was imported), leaving the default first-row selection.
void MainWindow::restoreLastSelection() {
    int lastFolder = DatabaseManager::instance().getSetting("LastFolderID", "-1").toInt();
    if (lastFolder < 0) return;

    bool folderFound = false;
    for (int i = 0; i < folderList->count(); ++i) {
        if (folderList->item(i)->data(Qt::UserRole).toInt() == lastFolder) {
            // Selecting the row fires onFolderSelected(), which loads its snippets.
            folderList->setCurrentRow(i);
            folderFound = true;
            break;
        }
    }
    if (!folderFound) return;

    int lastSnippet = DatabaseManager::instance().getSetting("LastSnippetID", "-1").toInt();
    if (lastSnippet < 0) return;

    for (int i = 0; i < snippetList->count(); ++i) {
        if (snippetList->item(i)->data(Qt::UserRole).toInt() == lastSnippet) {
            snippetList->setCurrentRow(i);
            snippetList->scrollToItem(snippetList->item(i));
            break;
        }
    }
}

// ── Theme ─────────────────────────────────────────────────────
void MainWindow::applyThemeStyles(bool dark) {
    if (dark) {
        setStyleSheet(
            "QMainWindow { background-color: #1e1e1e; }"
            "QSplitter { background-color: #1e1e1e; }"
            "QSplitter::handle { background-color: #3a3a3a; }"
            "QWidget { background-color: #2a2a2a; color: #d4d4d4; }"
            "QListWidget { background-color: #2a2a2a; border: none; color: #d4d4d4; }"
            "QListWidget::item { padding-top: 5px; padding-bottom: 5px;"
            "  border-bottom: 1px solid #3a3a3a; color: #d4d4d4; }"
            "QListWidget::item:selected { background-color: #094771; color: #ffffff; }"
            "QListWidget::item:hover { background-color: #3a3a3a; color: #ffffff; }"
            "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3a3a3a; }"
            "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3a3a3a; }"
            "QLineEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3a3a3a; padding: 3px; }"
            "QPushButton { background-color: #3a3a3a; color: #d4d4d4; border: 1px solid #555555; padding: 4px 10px; }"
            "QPushButton:hover { background-color: #4a4a4a; }"
            "QPushButton:pressed { background-color: #094771; }"
            "QMenuBar { background-color: #1a1a1a; color: #d4d4d4; padding: 2px 8px; }"
            "QMenuBar::item:selected { background-color: #094771; border-radius: 4px; }"
            "QMenu { background-color: #2a2a2a; color: #d4d4d4; border: 1px solid #3a3a3a; }"
            "QMenu::item:selected { background-color: #094771; }"
            "QLabel { background-color: transparent; color: #d4d4d4; }"
            "QScrollBar:vertical { background-color: #2a2a2a; width: 12px; }"
            "QScrollBar::handle:vertical { background-color: #555555; border-radius: 4px; }"
            "QScrollBar:horizontal { background-color: #2a2a2a; height: 12px; }"
            "QScrollBar::handle:horizontal { background-color: #555555; border-radius: 4px; }"
            );
        statusBar()->setStyleSheet("QStatusBar { background-color: #2a2a2a; border-top: 2px solid #3a3a3a; }");
        folderList->setStyleSheet(
            "QListWidget { background-color: #2a2a2a; border: none; color: #d4d4d4; }"
            "QListWidget::item { padding-left: 22px; padding-top: 5px; padding-bottom: 5px;"
            "  border-bottom: 1px solid #3a3a3a; color: #d4d4d4; }"
            "QListWidget::item:selected { background-color: #094771; color: #ffffff; }"
            "QListWidget::item:hover { background-color: #3a3a3a; color: #ffffff; }"
            );
        codeEditor->darkMode = true;
        codeEditor->highlightCurrentLine();
        lineNumberArea->update();
    } else {
        setStyleSheet(
            "QMainWindow { background-color: #f0f0f0; }"
            "QSplitter { background-color: #f0f0f0; }"
            "QSplitter::handle { background-color: #cccccc; }"
            "QWidget { background-color: #f5f5f5; color: #1e1e1e; }"
            "QListWidget { background-color: #f5f5f5; border: none; color: #1e1e1e; }"
            "QListWidget::item { padding-top: 5px; padding-bottom: 5px;"
            "  border-bottom: 1px solid #e0e0e0; color: #1e1e1e; }"
            "QListWidget::item:selected { background-color: #0078d4; color: #ffffff; }"
            "QListWidget::item:hover { background-color: #e8e8e8; color: #000000; }"
            "QPlainTextEdit { background-color: #ffffff; color: #1e1e1e; border: 1px solid #cccccc; }"
            "QTextEdit { background-color: #ffffff; color: #1e1e1e; border: 1px solid #cccccc; }"
            "QLineEdit { background-color: #ffffff; color: #1e1e1e; border: 1px solid #cccccc; padding: 3px; }"
            "QPushButton { background-color: #e8e8e8; color: #1e1e1e; border: 1px solid #bbbbbb; padding: 4px 10px; }"
            "QPushButton:hover { background-color: #d8d8d8; color: #000000; }"
            "QPushButton:pressed { background-color: #0078d4; color: #ffffff; }"
            "QMenuBar { background-color: #CCCCCC; color: #000000; padding: 2px 8px; }"
            "QMenuBar::item:selected { background-color: #0078d4; color: #FFFFFF; border-radius: 4px; }"
            "QMenu { background-color: #f5f5f5; color: #1e1e1e; border: 1px solid #cccccc; }"
            "QMenu::item:selected { background-color: #0078d4; color: #ffffff; }"
            "QLabel { background-color: transparent; color: #1e1e1e; }"
            "QScrollBar:vertical { background-color: #f0f0f0; width: 12px; }"
            "QScrollBar::handle:vertical { background-color: #bbbbbb; border-radius: 4px; }"
            "QScrollBar:horizontal { background-color: #f0f0f0; height: 12px; }"
            "QScrollBar::handle:horizontal { background-color: #bbbbbb; border-radius: 4px; }"
            );
        statusBar()->setStyleSheet("QStatusBar { background-color: #cccccc; border-top: 2px solid #bbbbbb; }");
        folderList->setStyleSheet(
            "QListWidget { background-color: #f5f5f5; border: none; color: #1e1e1e; }"
            "QListWidget::item { padding-left: 22px; padding-top: 5px; padding-bottom: 5px;"
            "  border-bottom: 1px solid #e0e0e0; color: #1e1e1e; }"
            "QListWidget::item:selected { background-color: #0078d4; color: #ffffff; }"
            "QListWidget::item:hover { background-color: #e8e8e8; color: #000000; }"
            );
        codeEditor->darkMode = false;
        codeEditor->highlightCurrentLine();
        lineNumberArea->update();
    }
}

// ── Line number slots ─────────────────────────────────────────
void MainWindow::onBlockCountChanged() {
    codeEditor->updateLineNumberAreaWidth();
}

void MainWindow::onUpdateRequest(const QRect& rect, int dy) {
    if (!lineNumbersEnabled) return;
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    if (rect.contains(codeEditor->viewport()->rect()))
        codeEditor->updateLineNumberAreaWidth();
}

// ── Event Filter ──────────────────────────────────────────────
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == folderList->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            QListWidgetItem* item = folderList->itemAt(me->pos());
            folderList->viewport()->setCursor(item ? Qt::OpenHandCursor : Qt::ArrowCursor);
        } else if (event->type() == QEvent::Leave) {
            folderList->viewport()->setCursor(Qt::ArrowCursor);
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// ── Setup UI ──────────────────────────────────────────────────
void MainWindow::setupUI() {
    // The "[*]" placeholder is shown as "*" whenever there are unsaved changes.
    setWindowTitle("SafeScript v1.2.9[*]");
    resize(1100, 700);

    // Accept files dropped from the file manager to create snippets.
    setAcceptDrops(true);

    // ── Footer status bar ─────────────────────────────────
    QStatusBar* sb = new QStatusBar(this);
    sb->setFixedHeight(24);
    setStatusBar(sb);

    // Permanent right-aligned info line (lines / chars / modified). It survives
    // transient showMessage() notifications, which appear on the left.
    statusInfoLabel = new QLabel;
    statusInfoLabel->setContentsMargins(0, 0, 8, 0);
    sb->addPermanentWidget(statusInfoLabel);

    // ── Sidebar ──────────────────────────────────────────
    folderList = new QListWidget;
    folderList->setMinimumWidth(180);
    folderList->setDragDropMode(QAbstractItemView::InternalMove);
    folderList->setDefaultDropAction(Qt::MoveAction);
    folderList->setItemDelegate(new FolderDelegate(folderList));
    folderList->setMouseTracking(true);
    folderList->viewport()->setMouseTracking(true);
    folderList->viewport()->installEventFilter(this);

    connect(folderList, &QListWidget::currentItemChanged, this, &MainWindow::onFolderSelected);
    connect(folderList, &QListWidget::itemDoubleClicked,  this, &MainWindow::onRenameFolder);
    connect(folderList->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
        for (int i = 0; i < folderList->count(); ++i) {
            int folderID = folderList->item(i)->data(Qt::UserRole).toInt();
            DatabaseManager::instance().updateFolderSortOrder(folderID, i + 1);
        }
    });

    btnNewFolder    = new QPushButton("+ New");
    btnDeleteFolder = new QPushButton("🗑 Delete");
    connect(btnNewFolder,    &QPushButton::clicked, this, &MainWindow::onNewFolder);
    connect(btnDeleteFolder, &QPushButton::clicked, this, &MainWindow::onDeleteFolder);

    QHBoxLayout* folderBtnLayout = new QHBoxLayout;
    folderBtnLayout->addWidget(btnNewFolder);
    folderBtnLayout->addWidget(btnDeleteFolder);

    folderCountLabel = new QLabel("0 Folders");
    folderCountLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout* sidebarLayout = new QVBoxLayout;
    sidebarLayout->setContentsMargins(4, 4, 4, 4);
    sidebarLayout->addLayout(folderBtnLayout);
    sidebarLayout->addWidget(folderList);
    sidebarLayout->addWidget(folderCountLabel);

    sidebarWidget = new QWidget;
    sidebarWidget->setLayout(sidebarLayout);
    sidebarWidget->setMinimumWidth(180);

    // ── Snippet List ─────────────────────────────────────
    searchBox = new QLineEdit;
    searchBox->setPlaceholderText("Search title, code & notes...");
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);

    snippetList = new QListWidget;
    snippetList->setMinimumWidth(220);
    snippetList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    snippetList->setWordWrap(false);
    snippetList->setItemDelegate(new SnippetDelegate(snippetList));
    connect(snippetList, &QListWidget::currentItemChanged, this, &MainWindow::onSnippetSelected);

    // Right-click a snippet for favorite / clone / move / export / delete.
    snippetList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(snippetList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onSnippetContextMenu);

    btnNewSnippet    = new QPushButton("+ New");
    btnDeleteSnippet = new QPushButton("🗑 Delete");
    connect(btnNewSnippet,    &QPushButton::clicked, this, &MainWindow::onNewSnippet);
    connect(btnDeleteSnippet, &QPushButton::clicked, this, &MainWindow::onDeleteSnippet);

    QHBoxLayout* snippetBtnLayout = new QHBoxLayout;
    snippetBtnLayout->addWidget(btnNewSnippet);
    snippetBtnLayout->addWidget(btnDeleteSnippet);

    snippetCountLabel = new QLabel("0 Snippets");
    snippetCountLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout* snippetListLayout = new QVBoxLayout;
    snippetListLayout->setContentsMargins(4, 4, 4, 4);
    snippetListLayout->addLayout(snippetBtnLayout);
    snippetListLayout->addWidget(searchBox);
    snippetListLayout->addWidget(snippetList);
    snippetListLayout->addWidget(snippetCountLabel);

    snippetListWidget = new QWidget;
    snippetListWidget->setLayout(snippetListLayout);
    snippetListWidget->setMinimumWidth(220);

    // ── Editor ───────────────────────────────────────────
    QFont monoFont("Monospace", 12);
    monoFont.setStyleHint(QFont::Monospace);

    titleField = new QLineEdit;
    titleField->setPlaceholderText("Title");
    titleField->setFont(monoFont);

    descField = new QLineEdit;
    descField->setPlaceholderText("Description");
    descField->setFont(monoFont);

    codeEditor = new CodeEditor;
    codeEditor->setFont(monoFont);
    codeEditor->setPlaceholderText("Code goes here...");
    codeEditor->setLineWrapMode(QPlainTextEdit::NoWrap);

    lineNumberArea = new LineNumberArea(codeEditor);
    codeEditor->lineNumberArea = lineNumberArea;
    lineNumberArea->setVisible(false);

    connect(codeEditor, &QPlainTextEdit::blockCountChanged,
            this, &MainWindow::onBlockCountChanged);
    connect(codeEditor, &QPlainTextEdit::updateRequest,
            this, &MainWindow::onUpdateRequest);

    noteEditor = new QTextEdit;
    noteEditor->setFont(monoFont);
    noteEditor->setPlaceholderText("Notes...");

    // Any user edit to a field marks the snippet as having unsaved changes.
    // textEdited fires only on user input (not on programmatic setText), while
    // the plain/rich text editors use textChanged guarded by loadingSnippet.
    connect(titleField, &QLineEdit::textEdited,        this, &MainWindow::markDirty);
    connect(descField,  &QLineEdit::textEdited,        this, &MainWindow::markDirty);
    connect(codeEditor, &QPlainTextEdit::textChanged,  this, &MainWindow::markDirty);
    connect(noteEditor, &QTextEdit::textChanged,       this, &MainWindow::markDirty);

    // Keep the status line's line/char counts current as the code changes.
    connect(codeEditor, &QPlainTextEdit::textChanged,  this, &MainWindow::updateStatusInfo);

    // ── Find / replace bar (hidden until Ctrl+F) ──────────
    findBar = new QWidget;
    findField = new QLineEdit;
    findField->setPlaceholderText("Find");
    replaceField = new QLineEdit;
    replaceField->setPlaceholderText("Replace with");
    findCaseCheck = new QCheckBox("Aa");
    findCaseCheck->setToolTip("Match case");
    QPushButton* findNextBtn   = new QPushButton("Next");
    QPushButton* replaceBtn    = new QPushButton("Replace");
    QPushButton* replaceAllBtn = new QPushButton("All");
    QPushButton* findCloseBtn  = new QPushButton("✕");
    findCloseBtn->setFixedWidth(28);
    findCloseBtn->setToolTip("Close (Esc)");

    QHBoxLayout* findLayout = new QHBoxLayout(findBar);
    findLayout->setContentsMargins(0, 4, 0, 0);
    findLayout->setSpacing(4);
    findLayout->addWidget(findField, 2);
    findLayout->addWidget(replaceField, 2);
    findLayout->addWidget(findCaseCheck);
    findLayout->addWidget(findNextBtn);
    findLayout->addWidget(replaceBtn);
    findLayout->addWidget(replaceAllBtn);
    findLayout->addWidget(findCloseBtn);
    findBar->setVisible(false);

    connect(findField,    &QLineEdit::returnPressed, this, &MainWindow::onFindNext);
    connect(findNextBtn,  &QPushButton::clicked,     this, &MainWindow::onFindNext);
    connect(replaceBtn,   &QPushButton::clicked,     this, &MainWindow::onReplaceOne);
    connect(replaceAllBtn,&QPushButton::clicked,     this, &MainWindow::onReplaceAll);
    connect(findCloseBtn, &QPushButton::clicked,     this, &MainWindow::onHideFindBar);

    // Esc closes the find bar when focus is inside it.
    QShortcut* escFind = new QShortcut(QKeySequence(Qt::Key_Escape), findBar);
    escFind->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escFind, &QShortcut::activated, this, &MainWindow::onHideFindBar);

    btnCopyCode = new QPushButton("📋 Copy Code");
    btnCopyCode->setFixedHeight(36);
    btnCopyCode->setToolTip("Copy this snippet's code to the clipboard");
    connect(btnCopyCode, &QPushButton::clicked, this, &MainWindow::onCopyCode);

    btnSave = new QPushButton("💾 Save Script");
    btnSave->setFixedHeight(36);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onSaveSnippet);

    // ── Editor splitter for code/notes ───────────────────
    editorSplitter = new QSplitter(Qt::Vertical);
    editorSplitter->addWidget(codeEditor);

    QWidget* notesWidget = new QWidget;
    QVBoxLayout* notesLayout = new QVBoxLayout(notesWidget);
    notesLayout->setContentsMargins(0, 6, 0, 0);
    notesLayout->addWidget(new QLabel("Notes:"));
    notesLayout->addWidget(noteEditor);
    editorSplitter->addWidget(notesWidget);
    editorSplitter->setStretchFactor(0, 3);
    editorSplitter->setStretchFactor(1, 1);
    editorSplitter->setChildrenCollapsible(false);   // code/notes can't be dragged to zero

    // Copy sits beside Save; Save stretches to remain the primary action.
    QHBoxLayout* actionRow = new QHBoxLayout;
    actionRow->setSpacing(8);
    actionRow->addWidget(btnCopyCode);
    actionRow->addWidget(btnSave, 1);

    QVBoxLayout* editorLayout = new QVBoxLayout;
    editorLayout->setContentsMargins(8, 8, 8, 8);
    editorLayout->addWidget(titleField);
    editorLayout->addWidget(descField);
    editorLayout->addWidget(new QLabel("Code:"));
    editorLayout->addWidget(editorSplitter, 1);
    editorLayout->addWidget(findBar);
    editorLayout->addLayout(actionRow);

    QWidget* editorWidget = new QWidget;
    editorWidget->setLayout(editorLayout);
    editorWidget->setMinimumWidth(250);

    // ── Splitter ─────────────────────────────────────────
    mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->addWidget(sidebarWidget);
    mainSplitter->addWidget(snippetListWidget);
    mainSplitter->addWidget(editorWidget);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 0);
    mainSplitter->setStretchFactor(2, 1);
    mainSplitter->setChildrenCollapsible(false);   // panes can't be dragged to zero
    mainSplitter->setSizes({200, 260, 640});        // first-run default; saved state overrides

    setCentralWidget(mainSplitter);
}

// ── Menu Bar ──────────────────────────────────────────────────
void MainWindow::setupMenuBar() {
    setContentsMargins(0, 6, 0, 0);
    QMenu* appMenu = menuBar()->addMenu("SafeScript");
    appMenu->addAction("About SafeScript", this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("About SafeScript");
        dlg.setFixedSize(320, 280);
        QVBoxLayout* layout = new QVBoxLayout(&dlg);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(8);

        QLabel* iconLabel = new QLabel;
        iconLabel->setPixmap(QIcon(":/safescript.png").pixmap(72, 72));
        iconLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(iconLabel);

        QLabel* text = new QLabel(
            "<b>SafeScript v1.2.9</b><br>"
            "Designed &amp; Programmed By<br>"
            "Thomas J. Allen<br>"
            "Copyright 2025<br>"
            "BrainScanMedia.com, Inc."
            );
        text->setAlignment(Qt::AlignCenter);
        layout->addWidget(text);
        layout->addSpacing(8);
        QPushButton* ok = new QPushButton("OK");
        ok->setFixedWidth(80);
        connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
        QHBoxLayout* btnRow = new QHBoxLayout;
        btnRow->addStretch();
        btnRow->addWidget(ok);
        btnRow->addStretch();
        layout->addLayout(btnRow);
        dlg.exec();
    });
    appMenu->addSeparator();
    QAction* quitAction = new QAction("Quit", this);
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    appMenu->addAction(quitAction);

    QMenu* editMenu = menuBar()->addMenu("Edit");

    QAction* cutAction = editMenu->addAction("Cut");
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, [this]() {
        if (auto* w = qobject_cast<QLineEdit*>(focusWidget()))           w->cut();
        else if (auto* w = qobject_cast<QPlainTextEdit*>(focusWidget())) w->cut();
        else if (auto* w = qobject_cast<QTextEdit*>(focusWidget()))      w->cut();
    });

    QAction* copyAction = editMenu->addAction("Copy");
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (auto* w = qobject_cast<QLineEdit*>(focusWidget()))           w->copy();
        else if (auto* w = qobject_cast<QPlainTextEdit*>(focusWidget())) w->copy();
        else if (auto* w = qobject_cast<QTextEdit*>(focusWidget()))      w->copy();
    });

    QAction* pasteAction = editMenu->addAction("Paste");
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, [this]() {
        if (auto* w = qobject_cast<QLineEdit*>(focusWidget()))           w->paste();
        else if (auto* w = qobject_cast<QPlainTextEdit*>(focusWidget())) w->paste();
        else if (auto* w = qobject_cast<QTextEdit*>(focusWidget()))      w->paste();
    });

    QAction* selectAllAction = editMenu->addAction("Select All");
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this]() {
        if (auto* w = qobject_cast<QLineEdit*>(focusWidget()))           w->selectAll();
        else if (auto* w = qobject_cast<QPlainTextEdit*>(focusWidget())) w->selectAll();
        else if (auto* w = qobject_cast<QTextEdit*>(focusWidget()))      w->selectAll();
    });

    editMenu->addSeparator();
    QAction* newSnippetAction = new QAction("New Snippet", this);
    newSnippetAction->setShortcut(QKeySequence::New);   // Ctrl+N
    connect(newSnippetAction, &QAction::triggered, this, &MainWindow::onNewSnippet);
    editMenu->addAction(newSnippetAction);

    QAction* saveAction = new QAction("Save Snippet", this);
    saveAction->setShortcut(QKeySequence::Save);   // Ctrl+S
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveSnippet);
    editMenu->addAction(saveAction);

    QAction* copyCodeAction = new QAction("Copy Code", this);
    copyCodeAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    connect(copyCodeAction, &QAction::triggered, this, &MainWindow::onCopyCode);
    editMenu->addAction(copyCodeAction);

    editMenu->addSeparator();
    QAction* findAction = new QAction("Find / Replace", this);
    findAction->setShortcut(QKeySequence::Find);   // Ctrl+F
    connect(findAction, &QAction::triggered, this, &MainWindow::onShowFindBar);
    editMenu->addAction(findAction);

    QAction* cloneAction = new QAction("Clone Snippet", this);
    cloneAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
    connect(cloneAction, &QAction::triggered, this, &MainWindow::onCloneSnippet);
    editMenu->addAction(cloneAction);

    QAction* favAction = new QAction("Toggle Favorite", this);
    favAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(favAction, &QAction::triggered, this, &MainWindow::onToggleFavorite);
    editMenu->addAction(favAction);

    // ── Database menu (backup / import / export) ─────────
    QMenu* databaseMenu = menuBar()->addMenu("Database");

    QAction* backupAction = databaseMenu->addAction("Backup Database…");
    connect(backupAction, &QAction::triggered, this, &MainWindow::onBackupDatabase);

    QAction* importAction = databaseMenu->addAction("Import Database…");
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportDatabase);

    databaseMenu->addSeparator();
    QAction* exportSnippetAction = databaseMenu->addAction("Export Snippet…");
    connect(exportSnippetAction, &QAction::triggered, this, &MainWindow::onExportSnippet);

    QAction* exportFolderAction = databaseMenu->addAction("Export Folder…");
    connect(exportFolderAction, &QAction::triggered, this, &MainWindow::onExportFolder);

    QMenu* optionsMenu = menuBar()->addMenu("Options");

    wrapAction = new QAction("Wrap Code", this);
    wrapAction->setCheckable(true);
    connect(wrapAction, &QAction::triggered, this, &MainWindow::onToggleWrap);
    optionsMenu->addAction(wrapAction);

    lineNumberAction = new QAction("Line Numbers", this);
    lineNumberAction->setCheckable(true);
    connect(lineNumberAction, &QAction::triggered, this, &MainWindow::onToggleLineNumbers);
    optionsMenu->addAction(lineNumberAction);

    darkModeAction = new QAction("Dark Mode", this);
    darkModeAction->setCheckable(true);
    connect(darkModeAction, &QAction::triggered, this, &MainWindow::onToggleDarkMode);
    optionsMenu->addAction(darkModeAction);

    optionsMenu->addSeparator();
    QAction* incFontAction = new QAction("Increase Font Size", this);
    incFontAction->setShortcut(QKeySequence::ZoomIn);    // Ctrl++
    connect(incFontAction, &QAction::triggered, this, &MainWindow::onIncreaseFont);
    optionsMenu->addAction(incFontAction);

    QAction* decFontAction = new QAction("Decrease Font Size", this);
    decFontAction->setShortcut(QKeySequence::ZoomOut);   // Ctrl+-
    connect(decFontAction, &QAction::triggered, this, &MainWindow::onDecreaseFont);
    optionsMenu->addAction(decFontAction);

    QAction* resetFontAction = new QAction("Reset Font Size", this);
    resetFontAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetFontAction, &QAction::triggered, this, &MainWindow::onResetFont);
    optionsMenu->addAction(resetFontAction);

    optionsMenu->addSeparator();
    QAction* resetLayoutAction = new QAction("Reset Window Size", this);
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::onResetWindowSize);
    optionsMenu->addAction(resetLayoutAction);

    QMenu* helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("Storage Location", this, [this](){
        QDialog dlg(this);
        dlg.setWindowTitle("Data Storage");
        dlg.setFixedSize(600, 320);
        QVBoxLayout* layout = new QVBoxLayout(&dlg);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(8);
        QLabel* title = new QLabel("<b>Data Storage</b>");
        layout->addWidget(title);
        QLabel* desc = new QLabel("Snippets are saved locally depending on how you installed SafeScript:");
        desc->setWordWrap(true);
        layout->addWidget(desc);
        QTextEdit* info = new QTextEdit;
        info->setReadOnly(true);
        info->setPlainText(
            "Flatpak:\n"
            "~/.var/app/com.brainscanmedia.SafeScript/data/BrainScanMedia/SafeScript/storage.sqlite3\n\n"
            "Build from source:\n"
            "~/.local/share/BrainScanMedia/SafeScript/storage.sqlite3"
            );
        layout->addWidget(info);
        QPushButton* ok = new QPushButton("OK");
        ok->setFixedWidth(80);
        connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
        QHBoxLayout* btnRow = new QHBoxLayout;
        btnRow->addStretch();
        btnRow->addWidget(ok);
        layout->addLayout(btnRow);
        dlg.exec();
    });
    helpMenu->addSeparator();
    helpMenu->addAction("Visit Our Website", this, [](){
        QDesktopServices::openUrl(QUrl("https://www.brainscanmedia.com"));
    });
}

// ── Database Slots ────────────────────────────────────────────
void MainWindow::onBackupDatabase() {
    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.isEmpty())
        startDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    QString defaultName = QString("safescript-backup-%1.sqlite3")
                              .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-HHmmss"));

    QString dest = QFileDialog::getSaveFileName(
        this, "Backup Database",
        startDir + "/" + defaultName,
        "SQLite Database (*.sqlite3 *.db);;All Files (*)");
    if (dest.isEmpty()) return;

    QString err;
    if (DatabaseManager::instance().backupTo(dest, &err)) {
        QMessageBox::information(this, "Backup Complete",
                                 "Your database was backed up successfully to:\n\n" + dest);
    } else {
        QMessageBox::warning(this, "Backup Failed",
                             err.isEmpty() ? "The backup could not be completed." : err);
    }
}

void MainWindow::onImportDatabase() {
    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.isEmpty())
        startDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    QString src = QFileDialog::getOpenFileName(
        this, "Import Database",
        startDir,
        "SQLite Database (*.sqlite3 *.db);;All Files (*)");
    if (src.isEmpty()) return;

    // Importing overwrites everything — confirm first.
    QMessageBox confirm(this);
    confirm.setWindowTitle("Import Database");
    confirm.setIcon(QMessageBox::Warning);
    confirm.setText("Importing will overwrite your current database.");
    confirm.setInformativeText(
        "All folders and snippets currently in SafeScript will be replaced by "
        "the contents of the selected file. This cannot be undone.\n\n"
        "Consider backing up your current database first.\n\nContinue?");
    confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirm.setDefaultButton(QMessageBox::Cancel);
    if (confirm.exec() != QMessageBox::Yes) return;

    QString err;
    if (DatabaseManager::instance().importFrom(src, &err)) {
        // The whole database was replaced, so any pending edits belong to data
        // that no longer exists — drop the dirty state before reloading the UI.
        currentFolderID  = -1;
        currentSnippetID = -1;
        setDirty(false);
        snippetList->clear();
        clearEditor();
        loadFolders();
        QMessageBox::information(this, "Import Complete",
                                 "The database was imported successfully.");
    } else {
        QMessageBox::warning(this, "Import Failed",
                             err.isEmpty() ? "The database could not be imported." : err);
    }
}

// ── Option Slots ──────────────────────────────────────────────
void MainWindow::onToggleWrap() {
    wrapEnabled = wrapAction->isChecked();
    codeEditor->setLineWrapMode(wrapEnabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    DatabaseManager::instance().saveSetting("WrapCode", wrapEnabled ? "on" : "off");
}

void MainWindow::onToggleDarkMode() {
    darkModeEnabled = darkModeAction->isChecked();
    qApp->setPalette(darkModeEnabled ? darkPalette() : lightPalette());
    applyThemeStyles(darkModeEnabled);
    DatabaseManager::instance().saveSetting("DarkMode", darkModeEnabled ? "on" : "off");
}

void MainWindow::onToggleLineNumbers() {
    lineNumbersEnabled = lineNumberAction->isChecked();
    codeEditor->showLineNumbers = lineNumbersEnabled;
    lineNumberArea->setVisible(lineNumbersEnabled);
    codeEditor->updateLineNumberAreaWidth();
    lineNumberArea->update();
    DatabaseManager::instance().saveSetting("LineNumbers", lineNumbersEnabled ? "on" : "off");
}

// Restores the default window size, column widths and code/notes divider,
// and clears the saved layout so nothing stale comes back on next launch.
// Does not touch snippets, folders, or the Wrap/Dark/Line Numbers options.
void MainWindow::onResetWindowSize() {
    // Default window size (matches setupUI)
    resize(1100, 700);

    // Default column widths (matches setupUI)
    mainSplitter->setSizes({200, 260, 640});

    // Default code / notes divider — 3:1, same ratio as the stretch factors
    int h = editorSplitter->height();
    if (h < 100) h = 500;            // fallback if not laid out yet
    editorSplitter->setSizes({ h * 3 / 4, h / 4 });

    // Clear the stored layout values
    DatabaseManager::instance().saveSetting("WindowSize", "");
    DatabaseManager::instance().saveSetting("SplitterState", "");
    DatabaseManager::instance().saveSetting("EditorSplitterState", "");

    // Re-center on screen
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect geo = screen->availableGeometry();
        move((geo.width() - width()) / 2, (geo.height() - height()) / 2);
    }
}

// ── Folder Slots ──────────────────────────────────────────────
void MainWindow::loadFolders() {
    folderList->clear();
    auto folders = DatabaseManager::instance().fetchFolders();
    for (const auto& f : folders) {
        auto* item = new QListWidgetItem(f.name);
        item->setData(Qt::UserRole, f.id);
        folderList->addItem(item);
    }
    int total = folderList->count();
    folderCountLabel->setText(QString("%1 Folder%2").arg(total).arg(total == 1 ? "" : "s"));
    if (total > 0)
        folderList->setCurrentRow(0);
}

void MainWindow::onNewFolder() {
    bool ok;
    QString name = QInputDialog::getText(this, "New Folder", "Folder Name:",
                                         QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        int newID = DatabaseManager::instance().insertFolder(name);
        auto* item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, newID);
        folderList->addItem(item);
        folderList->setCurrentItem(item);
        int total = folderList->count();
        folderCountLabel->setText(QString("%1 Folder%2").arg(total).arg(total == 1 ? "" : "s"));
    }
}

void MainWindow::onDeleteFolder() {
    auto* item = folderList->currentItem();
    if (!item) return;
    int id = item->data(Qt::UserRole).toInt();

    QDialog dlg(this);
    dlg.setWindowTitle("Delete Folder");
    dlg.setFixedSize(360, 130);
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    QLabel* msg = new QLabel("Deleting this folder will also remove\nall associated snippets.");
    msg->setAlignment(Qt::AlignLeft);
    layout->addWidget(msg);
    layout->addSpacing(8);
    QHBoxLayout* btnRow = new QHBoxLayout;
    QPushButton* cancel = new QPushButton("Cancel");
    QPushButton* yes    = new QPushButton("Yes");
    cancel->setFixedWidth(80);
    yes->setFixedWidth(80);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(yes,    &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addStretch();
    btnRow->addWidget(cancel);
    btnRow->addWidget(yes);
    layout->addLayout(btnRow);

    if (dlg.exec() == QDialog::Accepted) {
        // The snippet loaded in the editor lives in the folder being removed,
        // so any unsaved changes are intentionally discarded. Clearing the dirty
        // flag first also stops the selection-change guard from prompting while
        // the folder row is taken out below.
        setDirty(false);
        DatabaseManager::instance().deleteFolder(id);
        delete folderList->takeItem(folderList->currentRow());
        currentFolderID = -1;
        snippetList->clear();
        clearEditor();
        int total = folderList->count();
        folderCountLabel->setText(QString("%1 Folder%2").arg(total).arg(total == 1 ? "" : "s"));
    }
}

void MainWindow::onFolderSelected(QListWidgetItem* current, QListWidgetItem* previous) {
    if (!current) return;

    // Guard against losing edits when the user switches folders. If they cancel,
    // put the selection back on the folder they were in (with signals blocked so
    // this handler doesn't re-enter).
    if (!maybeSave()) {
        QSignalBlocker blocker(folderList);
        if (previous) folderList->setCurrentItem(previous);
        return;
    }

    currentFolderID = current->data(Qt::UserRole).toInt();
    loadSnippets(currentFolderID);
}

void MainWindow::onRenameFolder(QListWidgetItem* item) {
    if (!item) return;
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Folder", "New Name:",
                                            QLineEdit::Normal, item->text(), &ok);
    if (ok && !newName.isEmpty()) {
        int id = item->data(Qt::UserRole).toInt();
        DatabaseManager::instance().renameFolder(id, newName);
        item->setText(newName);
    }
}

// ── Snippet Slots ─────────────────────────────────────────────
void MainWindow::loadSnippets(int folderID, int selectSnippetID) {
    // This is a programmatic rebuild: the currentItemChanged signals it fires
    // must not trigger the unsaved-changes guard.
    rebuildingList = true;

    snippetList->clear();
    currentSnippets = DatabaseManager::instance().fetchSnippets(folderID);
    const QString filter = searchBox->text();

    int shown = 0;
    int rowToSelect = -1;
    for (const auto& s : currentSnippets) {
        // Search matches the title, description, code, or notes — not just the title.
        if (!filter.isEmpty() &&
            !s.title.contains(filter,       Qt::CaseInsensitive) &&
            !s.description.contains(filter, Qt::CaseInsensitive) &&
            !s.code.contains(filter,        Qt::CaseInsensitive) &&
            !s.note.contains(filter,        Qt::CaseInsensitive))
            continue;

        auto* item = new QListWidgetItem(snippetLabel(s));
        item->setData(Qt::UserRole, s.id);
        snippetList->addItem(item);
        if (s.id == selectSnippetID) rowToSelect = shown;
        ++shown;
    }

    int total = currentSnippets.size();
    // Show "3 of 5" while a search filter is narrowing the list.
    if (!filter.isEmpty() && shown != total)
        snippetCountLabel->setText(QString("%1 of %2 Snippet%3")
                                       .arg(shown).arg(total).arg(total == 1 ? "" : "s"));
    else
        snippetCountLabel->setText(QString("%1 Snippet%2").arg(total).arg(total == 1 ? "" : "s"));

    if (snippetList->count() > 0)
        snippetList->setCurrentRow(rowToSelect >= 0 ? rowToSelect : 0);
    else
        clearEditor();

    rebuildingList = false;

    // Whatever ended up loaded above is a fresh, unmodified snippet.
    setDirty(false);
}

// Re-orders the visible list to match the database ordering (favorites first,
// then title, case-insensitive) using the in-memory records — without reloading
// from disk. Selection signals are blocked so the editor, caret, and any unsaved
// edits are left completely untouched; the current snippet stays selected. Call
// this after actions that change sort-relevant data (favoriting, saving a
// new/renamed snippet). It must not be called from inside a selection handler
// that still holds QListWidgetItem pointers, since it clears the list.
void MainWindow::resortSnippetList() {
    if (currentFolderID < 0) return;

    std::sort(currentSnippets.begin(), currentSnippets.end(),
              [](const Snippet& a, const Snippet& b) {
                  if (a.favorite != b.favorite) return a.favorite;   // favorites first
                  return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
              });

    const QString filter = searchBox->text();

    QSignalBlocker blocker(snippetList);
    snippetList->clear();
    int shown = 0, rowToSelect = -1;
    for (const auto& s : currentSnippets) {
        if (!filter.isEmpty() &&
            !s.title.contains(filter,       Qt::CaseInsensitive) &&
            !s.description.contains(filter, Qt::CaseInsensitive) &&
            !s.code.contains(filter,        Qt::CaseInsensitive) &&
            !s.note.contains(filter,        Qt::CaseInsensitive))
            continue;
        auto* item = new QListWidgetItem(snippetLabel(s));
        item->setData(Qt::UserRole, s.id);
        snippetList->addItem(item);
        if (s.id == currentSnippetID) rowToSelect = shown;
        ++shown;
    }
    if (rowToSelect >= 0)
        snippetList->setCurrentRow(rowToSelect);

    int total = currentSnippets.size();
    if (!filter.isEmpty() && shown != total)
        snippetCountLabel->setText(QString("%1 of %2 Snippet%3")
                                       .arg(shown).arg(total).arg(total == 1 ? "" : "s"));
    else
        snippetCountLabel->setText(QString("%1 Snippet%2").arg(total).arg(total == 1 ? "" : "s"));
}

void MainWindow::onNewSnippet() {
    if (currentFolderID < 0) return;
    // Don't lose edits to the currently open snippet.
    if (!maybeSave()) return;

    int newID = DatabaseManager::instance().insertSnippet(currentFolderID);
    loadSnippets(currentFolderID);

    // Select the newly created snippet. Wrapped in rebuildingList so the
    // selection change loads it without engaging the guard.
    rebuildingList = true;
    for (int i = 0; i < snippetList->count(); ++i) {
        if (snippetList->item(i)->data(Qt::UserRole).toInt() == newID) {
            snippetList->setCurrentRow(i);
            break;
        }
    }
    rebuildingList = false;
    setDirty(false);
}

void MainWindow::onDeleteSnippet() {
    auto* item = snippetList->currentItem();
    if (!item) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Delete Snippet");
    dlg.setFixedSize(360, 130);
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    QLabel* msg = new QLabel("Are you sure you want to delete this snippet?\nThis cannot be undone.");
    msg->setAlignment(Qt::AlignLeft);
    layout->addWidget(msg);
    layout->addSpacing(8);
    QHBoxLayout* btnRow = new QHBoxLayout;
    QPushButton* cancel = new QPushButton("Cancel");
    QPushButton* yes    = new QPushButton("Yes");
    cancel->setFixedWidth(80);
    yes->setFixedWidth(80);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(yes,    &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addStretch();
    btnRow->addWidget(cancel);
    btnRow->addWidget(yes);
    layout->addLayout(btnRow);

    if (dlg.exec() == QDialog::Accepted) {
        // Deleting the open snippet discards its unsaved edits by design.
        setDirty(false);
        DatabaseManager::instance().deleteSnippet(item->data(Qt::UserRole).toInt());
        loadSnippets(currentFolderID);
    }
}

void MainWindow::onSnippetSelected(QListWidgetItem* current, QListWidgetItem* previous) {
    // Programmatic rebuilds just load the selected snippet — no guard.
    if (rebuildingList) {
        if (!current) { clearEditor(); return; }
        currentSnippetID = current->data(Qt::UserRole).toInt();
        for (const auto& s : currentSnippets)
            if (s.id == currentSnippetID) { populateEditor(s); return; }
        return;
    }

    // User clicked a different snippet — protect unsaved edits on the one we're
    // leaving. On cancel, revert the selection to the previous snippet.
    if (dirty && currentSnippetID >= 0) {
        int choice = promptUnsaved();
        if (choice == QMessageBox::Cancel) {
            QSignalBlocker blocker(snippetList);
            if (previous) snippetList->setCurrentItem(previous);
            return;
        }
        if (choice == QMessageBox::Save) saveCurrentSnippet();
        setDirty(false);
    }

    if (!current) { clearEditor(); return; }
    currentSnippetID = current->data(Qt::UserRole).toInt();
    for (const auto& s : currentSnippets) {
        if (s.id == currentSnippetID) { populateEditor(s); return; }
    }
}

// Writes the editor contents to the database without any confirmation dialog.
// Used by the save-on-navigate guard and by the explicit Save button.
void MainWindow::saveCurrentSnippet() {
    if (currentSnippetID < 0) return;

    Snippet s;
    s.id          = currentSnippetID;
    s.folderID    = currentFolderID;
    s.title       = titleField->text();
    s.description = descField->text();
    s.code        = codeEditor->toPlainText();
    s.note        = noteEditor->toPlainText();
    DatabaseManager::instance().updateSnippet(s);

    // Keep the in-memory copy in sync (including the fresh modified stamp), and
    // reuse its favorite flag when relabelling the list row below.
    const QString nowIso = QDateTime::currentDateTime().toString(Qt::ISODate);
    bool favorite = false;
    for (auto& snippet : currentSnippets) {
        if (snippet.id == s.id) {
            snippet.title       = s.title;
            snippet.description = s.description;
            snippet.code        = s.code;
            snippet.note        = s.note;
            snippet.modified    = nowIso;
            favorite            = snippet.favorite;
            break;
        }
    }

    // Update the list row for this snippet by ID (the current selection may have
    // already moved on when saving during a navigation), keeping the ★ prefix.
    Snippet display;
    display.title = s.title;
    display.favorite = favorite;
    const QString label = snippetLabel(display);
    for (int i = 0; i < snippetList->count(); ++i) {
        auto* it = snippetList->item(i);
        if (it->data(Qt::UserRole).toInt() == s.id) { it->setText(label); break; }
    }

    setDirty(false);
    updateStatusInfo();
}

void MainWindow::onSaveSnippet() {
    if (currentSnippetID < 0) return;
    saveCurrentSnippet();
    resortSnippetList();   // a changed title (or a newly-titled snippet) may re-order the list

    QDialog dlg(this);
    dlg.setWindowTitle("Saved");
    dlg.setFixedSize(260, 110);
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    QLabel* msg = new QLabel("Snippet saved successfully.");
    msg->setAlignment(Qt::AlignLeft);
    layout->addWidget(msg);
    layout->addSpacing(8);
    QPushButton* ok = new QPushButton("OK");
    ok->setFixedWidth(80);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(ok);
    layout->addLayout(btnRow);
    dlg.exec();
}

// Copies the current snippet's code to the system clipboard, with brief
// feedback in the status bar. Works on the live editor text (including any
// unsaved edits), which is what the user sees.
void MainWindow::onCopyCode() {
    const QString code = codeEditor->toPlainText();
    if (code.isEmpty()) {
        statusBar()->showMessage("Nothing to copy — the code area is empty.", 2000);
        return;
    }
    QApplication::clipboard()->setText(code);
    statusBar()->showMessage("Code copied to clipboard.", 2000);
}

void MainWindow::onSearchTextChanged(const QString&) {
    if (currentFolderID >= 0) loadSnippets(currentFolderID);
}

void MainWindow::clearEditor() {
    loadingSnippet = true;
    currentSnippetID = -1;
    titleField->clear();
    descField->clear();
    codeEditor->clear();
    noteEditor->clear();
    loadingSnippet = false;
    setDirty(false);
    updateStatusInfo();
}

void MainWindow::populateEditor(const Snippet& s) {
    loadingSnippet = true;
    titleField->setText(s.title);
    descField->setText(s.description);
    codeEditor->setPlainText(s.code);
    noteEditor->setText(s.note);
    loadingSnippet = false;
    setDirty(false);
    updateStatusInfo();
}

// ── Unsaved-changes helpers ───────────────────────────────────
void MainWindow::setDirty(bool d) {
    dirty = d;
    // Drives the "*" shown in place of the "[*]" placeholder in the title.
    setWindowModified(d);
}

void MainWindow::markDirty() {
    // Ignore programmatic population and the case where no snippet is loaded.
    if (loadingSnippet) return;
    if (currentSnippetID < 0) return;
    if (!dirty) setDirty(true);
}

int MainWindow::promptUnsaved() {
    return QMessageBox::question(
        this, "Unsaved Changes",
        "This snippet has unsaved changes.\n\nDo you want to save them?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
}

// Returns true if the caller may proceed (either nothing was dirty, the user
// saved, or the user chose to discard). Returns false only if the user cancels.
bool MainWindow::maybeSave() {
    if (!dirty || currentSnippetID < 0) return true;

    int choice = promptUnsaved();
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save)   saveCurrentSnippet();
    setDirty(false);
    return true;
}

// ── Snippet lookup helper ─────────────────────────────────────
const Snippet* MainWindow::currentSnippet() const {
    for (const auto& s : currentSnippets)
        if (s.id == currentSnippetID) return &s;
    return nullptr;
}

// ── Status line ───────────────────────────────────────────────
QString MainWindow::relativeTime(const QString& iso) const {
    QDateTime t = QDateTime::fromString(iso, Qt::ISODate);
    if (!t.isValid()) return QString();
    qint64 secs = t.secsTo(QDateTime::currentDateTime());
    if (secs < 0) secs = 0;
    if (secs < 60)  return "just now";
    qint64 mins = secs / 60;
    if (mins < 60)  return QString("%1m ago").arg(mins);
    qint64 hours = mins / 60;
    if (hours < 24) return QString("%1h ago").arg(hours);
    qint64 days = hours / 24;
    if (days < 7)   return QString("%1d ago").arg(days);
    return t.date().toString("yyyy-MM-dd");
}

void MainWindow::updateStatusInfo() {
    if (currentSnippetID < 0) { statusInfoLabel->clear(); return; }
    const int lines = codeEditor->blockCount();
    const int chars = codeEditor->toPlainText().length();
    QString info = QString("%1 line%2 · %3 char%4")
                       .arg(lines).arg(lines == 1 ? "" : "s")
                       .arg(chars).arg(chars == 1 ? "" : "s");
    // Language will be added here once the per-snippet language field lands.
    if (const Snippet* s = currentSnippet()) {
        if (!s->modified.isEmpty()) {
            const QString rel = relativeTime(s->modified);
            if (!rel.isEmpty()) info += " · modified " + rel;
        }
    }
    statusInfoLabel->setText(info);
}

// ── Font size ─────────────────────────────────────────────────
void MainWindow::applyEditorFont() {
    QFont f("Monospace", editorFontSize);
    f.setStyleHint(QFont::Monospace);
    codeEditor->setFont(f);
    noteEditor->setFont(f);
    codeEditor->updateLineNumberAreaWidth();
    lineNumberArea->update();
}

void MainWindow::onIncreaseFont() {
    if (editorFontSize >= 40) return;
    ++editorFontSize;
    applyEditorFont();
    DatabaseManager::instance().saveSetting("EditorFontSize", QString::number(editorFontSize));
}

void MainWindow::onDecreaseFont() {
    if (editorFontSize <= 8) return;
    --editorFontSize;
    applyEditorFont();
    DatabaseManager::instance().saveSetting("EditorFontSize", QString::number(editorFontSize));
}

void MainWindow::onResetFont() {
    editorFontSize = 12;
    applyEditorFont();
    DatabaseManager::instance().saveSetting("EditorFontSize", "12");
}

// ── Find / replace ────────────────────────────────────────────
void MainWindow::onShowFindBar() {
    findBar->setVisible(true);
    // Seed the find field from a single-line selection, if any.
    const QString sel = codeEditor->textCursor().selectedText();
    if (!sel.isEmpty() && !sel.contains(QChar(0x2029)))
        findField->setText(sel);
    findField->setFocus();
    findField->selectAll();
}

void MainWindow::onHideFindBar() {
    findBar->setVisible(false);
    codeEditor->setFocus();
}

void MainWindow::onFindNext() {
    const QString term = findField->text();
    if (term.isEmpty()) return;
    QTextDocument::FindFlags flags;
    if (findCaseCheck->isChecked()) flags |= QTextDocument::FindCaseSensitively;

    if (!codeEditor->find(term, flags)) {
        // Wrap around to the top and try once more.
        QTextCursor c = codeEditor->textCursor();
        c.movePosition(QTextCursor::Start);
        codeEditor->setTextCursor(c);
        if (!codeEditor->find(term, flags))
            statusBar()->showMessage("Not found: " + term, 2000);
    }
}

void MainWindow::onReplaceOne() {
    const QString term = findField->text();
    if (term.isEmpty()) return;
    QTextCursor c = codeEditor->textCursor();
    const bool caseSensitive = findCaseCheck->isChecked();
    const bool matches = c.hasSelection() &&
        (caseSensitive ? c.selectedText() == term
                       : c.selectedText().compare(term, Qt::CaseInsensitive) == 0);
    if (matches)
        c.insertText(replaceField->text());
    onFindNext();
}

void MainWindow::onReplaceAll() {
    const QString term = findField->text();
    if (term.isEmpty()) return;
    QTextDocument::FindFlags flags;
    if (findCaseCheck->isChecked()) flags |= QTextDocument::FindCaseSensitively;

    QTextDocument* doc = codeEditor->document();
    QTextCursor editCursor(doc);
    editCursor.beginEditBlock();          // group every replacement into one undo step
    int count = 0;
    QTextCursor found = doc->find(term, 0, flags);
    while (!found.isNull()) {
        found.insertText(replaceField->text());
        found = doc->find(term, found, flags);
        ++count;
    }
    editCursor.endEditBlock();

    statusBar()->showMessage(
        QString("Replaced %1 occurrence%2").arg(count).arg(count == 1 ? "" : "s"), 2000);
}

// ── Clone / favorite / move ───────────────────────────────────
void MainWindow::onCloneSnippet() {
    if (currentSnippetID < 0) return;
    if (!maybeSave()) return;
    int newID = DatabaseManager::instance().cloneSnippet(currentSnippetID);
    if (newID < 0) return;
    loadSnippets(currentFolderID, newID);
    statusBar()->showMessage("Snippet cloned.", 2000);
}

void MainWindow::onToggleFavorite() {
    if (currentSnippetID < 0) return;

    // Update the DB and the in-memory record, then re-sort so the (un)favorited
    // snippet moves to its correct place. resortSnippetList() leaves the editor
    // and any in-progress edits untouched, so no save prompt is needed.
    bool newFav = false;
    for (auto& s : currentSnippets) {
        if (s.id == currentSnippetID) {
            s.favorite = !s.favorite;
            newFav = s.favorite;
            break;
        }
    }
    DatabaseManager::instance().setSnippetFavorite(currentSnippetID, newFav);
    resortSnippetList();
    statusBar()->showMessage(newFav ? "Added to favorites." : "Removed from favorites.", 2000);
}

void MainWindow::moveSnippetToFolder(int targetFolderID) {
    if (currentSnippetID < 0 || targetFolderID == currentFolderID) return;
    if (!maybeSave()) return;
    DatabaseManager::instance().moveSnippet(currentSnippetID, targetFolderID);
    // The snippet leaves the current folder, so reload the current folder.
    loadSnippets(currentFolderID);
    statusBar()->showMessage("Snippet moved.", 2000);
}

void MainWindow::onSnippetContextMenu(const QPoint& pos) {
    QListWidgetItem* item = snippetList->itemAt(pos);
    if (!item) return;

    // Make sure the right-clicked snippet is the current one. This may prompt to
    // save the snippet being left, which is the intended guard behavior.
    if (snippetList->currentItem() != item)
        snippetList->setCurrentItem(item);
    if (currentSnippetID < 0) return;

    const Snippet* s = currentSnippet();
    if (!s) return;

    QMenu menu(this);
    QAction* favAct    = menu.addAction(s->favorite ? "Unfavorite" : "Favorite ★");
    QAction* cloneAct  = menu.addAction("Clone");

    QMenu* moveMenu = menu.addMenu("Move to Folder");
    QList<QAction*> moveActions;
    for (int i = 0; i < folderList->count(); ++i) {
        int fid = folderList->item(i)->data(Qt::UserRole).toInt();
        if (fid == currentFolderID) continue;
        QAction* a = moveMenu->addAction(folderList->item(i)->text());
        a->setData(fid);
        moveActions.append(a);
    }
    if (moveActions.isEmpty()) {
        QAction* none = moveMenu->addAction("(no other folders)");
        none->setEnabled(false);
    }

    QAction* exportAct = menu.addAction("Export Snippet…");
    menu.addSeparator();
    QAction* delAct = menu.addAction("Delete");

    QAction* chosen = menu.exec(snippetList->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if      (chosen == favAct)    onToggleFavorite();
    else if (chosen == cloneAct)  onCloneSnippet();
    else if (chosen == exportAct) onExportSnippet();
    else if (chosen == delAct)    onDeleteSnippet();
    else if (moveActions.contains(chosen)) moveSnippetToFolder(chosen->data().toInt());
}

// ── Export ────────────────────────────────────────────────────
bool MainWindow::exportSnippetToPath(const QString& code, const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    out << code;
    f.close();
    return true;
}

void MainWindow::onExportSnippet() {
    if (currentSnippetID < 0) return;
    const QString title = titleField->text();
    const QString code  = codeEditor->toPlainText();

    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.isEmpty())
        startDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString defaultPath =
        startDir + "/" + sanitizeFileName(title.isEmpty() ? "snippet" : title) + ".txt";

    QString dest = QFileDialog::getSaveFileName(this, "Export Snippet", defaultPath,
                                                "Text Files (*.txt);;All Files (*)");
    if (dest.isEmpty()) return;

    if (exportSnippetToPath(code, dest))
        statusBar()->showMessage("Snippet exported.", 2000);
    else
        QMessageBox::warning(this, "Export Failed", "Could not write to:\n" + dest);
}

void MainWindow::onExportFolder() {
    if (currentFolderID < 0) {
        QMessageBox::information(this, "Export Folder", "Select a folder first.");
        return;
    }
    if (currentSnippets.isEmpty()) {
        QMessageBox::information(this, "Export Folder", "This folder has no snippets to export.");
        return;
    }

    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.isEmpty())
        startDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    QString dir = QFileDialog::getExistingDirectory(this, "Export Folder to Directory", startDir);
    if (dir.isEmpty()) return;

    int ok = 0;
    QSet<QString> used;
    for (const auto& s : currentSnippets) {
        QString base = sanitizeFileName(s.title.isEmpty() ? "untitled" : s.title);
        QString name = base + ".txt";
        int n = 2;
        while (used.contains(name)) name = QString("%1 (%2).txt").arg(base).arg(n++);
        used.insert(name);
        if (exportSnippetToPath(s.code, dir + "/" + name)) ++ok;
    }
    statusBar()->showMessage(
        QString("Exported %1 snippet%2 to %3").arg(ok).arg(ok == 1 ? "" : "s").arg(dir), 4000);
}

// ── Drag & drop: create snippets from dropped files ───────────
void MainWindow::createSnippetFromFile(const QString& path) {
    if (currentFolderID < 0) return;
    QFileInfo fi(path);
    if (!fi.isFile()) return;
    if (fi.size() > 5 * 1024 * 1024) {          // skip very large files
        statusBar()->showMessage("Skipped (too large): " + fi.fileName(), 3000);
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    const QString text = in.readAll();
    f.close();

    int newID = DatabaseManager::instance().insertSnippet(currentFolderID, fi.fileName(), text);
    loadSnippets(currentFolderID, newID);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& u : event->mimeData()->urls())
            if (u.isLocalFile()) { event->acceptProposedAction(); return; }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) return;
    if (currentFolderID < 0) {
        QMessageBox::information(this, "Drop File",
            "Select a folder first, then drop files to add them as snippets.");
        return;
    }
    // Protect any unsaved edits before we start replacing the list selection.
    if (!maybeSave()) return;

    int created = 0;
    for (const QUrl& u : event->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        QFileInfo fi(u.toLocalFile());
        if (!fi.isFile()) continue;
        createSnippetFromFile(u.toLocalFile());
        ++created;
    }
    if (created > 0) {
        statusBar()->showMessage(
            QString("Created %1 snippet%2 from dropped file%2")
                .arg(created).arg(created == 1 ? "" : "s"), 3000);
        event->acceptProposedAction();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Give the user a chance to save before the window closes.
    if (!maybeSave()) {
        event->ignore();
        return;
    }

    DatabaseManager::instance().saveSetting("WindowSize",
                                            QString("%1x%2").arg(width()).arg(height()));
    DatabaseManager::instance().saveSetting("SplitterState",
                                            QString::fromLatin1(mainSplitter->saveState().toBase64()));
    DatabaseManager::instance().saveSetting("EditorSplitterState",
                                            QString::fromLatin1(editorSplitter->saveState().toBase64()));
    // Remember which folder / snippet was open so we can restore it next launch
    DatabaseManager::instance().saveSetting("LastFolderID",  QString::number(currentFolderID));
    DatabaseManager::instance().saveSetting("LastSnippetID", QString::number(currentSnippetID));
    event->accept();
}
