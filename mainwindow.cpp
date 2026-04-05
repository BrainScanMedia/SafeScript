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
#include <QStatusBar>

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

        if (opt.state & QStyle::State_Selected)
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
    setWindowTitle("SafeScript v1.2.6");
    resize(1100, 700);

    // ── Footer status bar ─────────────────────────────────
    QStatusBar* sb = new QStatusBar(this);
    sb->setFixedHeight(24);
    setStatusBar(sb);

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
    sidebarWidget->setMaximumWidth(260);

    // ── Snippet List ─────────────────────────────────────
    searchBox = new QLineEdit;
    searchBox->setPlaceholderText("Search snippets...");
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);

    snippetList = new QListWidget;
    snippetList->setMinimumWidth(220);
    snippetList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    snippetList->setWordWrap(false);
    snippetList->setItemDelegate(new SnippetDelegate(snippetList));
    connect(snippetList, &QListWidget::currentItemChanged, this, &MainWindow::onSnippetSelected);

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
    snippetListWidget->setMaximumWidth(320);

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

    btnSave = new QPushButton("💾 Save Script");
    btnSave->setFixedHeight(36);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onSaveSnippet);

    // ── Editor splitter for code/notes ───────────────────
    QSplitter* editorSplitter = new QSplitter(Qt::Vertical);
    editorSplitter->addWidget(codeEditor);

    QWidget* notesWidget = new QWidget;
    QVBoxLayout* notesLayout = new QVBoxLayout(notesWidget);
    notesLayout->setContentsMargins(0, 6, 0, 0);
    notesLayout->addWidget(new QLabel("Notes:"));
    notesLayout->addWidget(noteEditor);
    editorSplitter->addWidget(notesWidget);
    editorSplitter->setStretchFactor(0, 3);
    editorSplitter->setStretchFactor(1, 1);

    QVBoxLayout* editorLayout = new QVBoxLayout;
    editorLayout->setContentsMargins(8, 8, 8, 8);
    editorLayout->addWidget(titleField);
    editorLayout->addWidget(descField);
    editorLayout->addWidget(new QLabel("Code:"));
    editorLayout->addWidget(editorSplitter, 1);
    editorLayout->addWidget(btnSave);

    QWidget* editorWidget = new QWidget;
    editorWidget->setLayout(editorLayout);

    // ── Splitter ─────────────────────────────────────────
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(sidebarWidget);
    splitter->addWidget(snippetListWidget);
    splitter->addWidget(editorWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);

    setCentralWidget(splitter);
}

// ── Menu Bar ──────────────────────────────────────────────────
void MainWindow::setupMenuBar() {
    setContentsMargins(0, 6, 0, 0);
    QMenu* appMenu = menuBar()->addMenu("SafeScript");
    appMenu->addAction("About SafeScript", this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("About SafeScript");
        dlg.setFixedSize(320, 200);
        QVBoxLayout* layout = new QVBoxLayout(&dlg);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(8);
        QLabel* text = new QLabel(
            "<b>SafeScript v1.2.6</b><br>"
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
        DatabaseManager::instance().deleteFolder(id);
        delete folderList->takeItem(folderList->currentRow());
        currentFolderID = -1;
        snippetList->clear();
        clearEditor();
        int total = folderList->count();
        folderCountLabel->setText(QString("%1 Folder%2").arg(total).arg(total == 1 ? "" : "s"));
    }
}

void MainWindow::onFolderSelected(QListWidgetItem* current, QListWidgetItem*) {
    if (!current) return;
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
void MainWindow::loadSnippets(int folderID) {
    snippetList->clear();
    currentSnippets = DatabaseManager::instance().fetchSnippets(folderID);
    QString filter = searchBox->text();
    for (const auto& s : currentSnippets) {
        if (!filter.isEmpty() && !s.title.contains(filter, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(s.title.isEmpty() ? "Untitled Snippet" : s.title);
        item->setData(Qt::UserRole, s.id);
        snippetList->addItem(item);
    }
    int total = currentSnippets.size();
    snippetCountLabel->setText(QString("%1 Snippet%2").arg(total).arg(total == 1 ? "" : "s"));
    if (snippetList->count() > 0)
        snippetList->setCurrentRow(0);
    else
        clearEditor();
}

void MainWindow::onNewSnippet() {
    if (currentFolderID < 0) return;
    int newID = DatabaseManager::instance().insertSnippet(currentFolderID);
    loadSnippets(currentFolderID);
    for (int i = 0; i < snippetList->count(); ++i) {
        if (snippetList->item(i)->data(Qt::UserRole).toInt() == newID) {
            snippetList->setCurrentRow(i);
            break;
        }
    }
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
        DatabaseManager::instance().deleteSnippet(item->data(Qt::UserRole).toInt());
        loadSnippets(currentFolderID);
    }
}

void MainWindow::onSnippetSelected(QListWidgetItem* current, QListWidgetItem*) {
    if (!current) { clearEditor(); return; }
    currentSnippetID = current->data(Qt::UserRole).toInt();
    for (const auto& s : currentSnippets) {
        if (s.id == currentSnippetID) { populateEditor(s); return; }
    }
}

void MainWindow::onSaveSnippet() {
    if (currentSnippetID < 0) return;
    Snippet s;
    s.id          = currentSnippetID;
    s.folderID    = currentFolderID;
    s.title       = titleField->text();
    s.description = descField->text();
    s.code        = codeEditor->toPlainText();
    s.note        = noteEditor->toPlainText();
    DatabaseManager::instance().updateSnippet(s);
    // Update in-memory snippet
    for (auto& snippet : currentSnippets) {
        if (snippet.id == s.id) {
            snippet.title       = s.title;
            snippet.description = s.description;
            snippet.code        = s.code;
            snippet.note        = s.note;
            break;
        }
    }
    if (auto* item = snippetList->currentItem())
        item->setText(s.title.isEmpty() ? "Untitled Snippet" : s.title);

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

void MainWindow::onSearchTextChanged(const QString&) {
    if (currentFolderID >= 0) loadSnippets(currentFolderID);
}

void MainWindow::clearEditor() {
    currentSnippetID = -1;
    titleField->clear();
    descField->clear();
    codeEditor->clear();
    noteEditor->clear();
}

void MainWindow::populateEditor(const Snippet& s) {
    titleField->setText(s.title);
    descField->setText(s.description);
    codeEditor->setPlainText(s.code);
    noteEditor->setText(s.note);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    DatabaseManager::instance().saveSetting("WindowSize",
                                            QString("%1x%2").arg(width()).arg(height()));
    event->accept();
}

