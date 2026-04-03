#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextDocument>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QCloseEvent>
#include <QScreen>
#include <QAction>
#include <QWidget>
#include <QPainter>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QEvent>
#include "databasemanager.h"

// ── CodeEditor subclass exposes protected methods for line numbers ──
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget* parent = nullptr) : QPlainTextEdit(parent) {}

    int lineNumberAreaWidth() {
        int digits = 1;
        int max = qMax(1, blockCount());
        while (max >= 10) { max /= 10; ++digits; }
        return fontMetrics().horizontalAdvance(QLatin1Char('9')) * (digits + 1) + 10;
    }

    void updateLineNumberAreaWidth() {
        if (showLineNumbers)
            setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
        else
            setViewportMargins(0, 0, 0, 0);
    }

    void lineNumberAreaPaintEvent(QPaintEvent* event) {
        QColor gutterBg   = darkMode ? QColor(40,  40,  40)  : QColor(220, 220, 220);
        QColor gutterText = darkMode ? QColor(130, 130, 130)  : QColor(100, 100, 100);

        QPainter painter(lineNumberArea);
        painter.fillRect(event->rect(), gutterBg);

        QTextBlock block = firstVisibleBlock();
        int blockNumber  = block.blockNumber();
        int top    = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());

        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                painter.setPen(gutterText);
                painter.drawText(0, top,
                                 lineNumberArea->width() - 6,
                                 fontMetrics().height(),
                                 Qt::AlignRight,
                                 QString::number(blockNumber + 1));
            }
            block  = block.next();
            top    = bottom;
            bottom = top + qRound(blockBoundingRect(block).height());
            ++blockNumber;
        }
    }

    QWidget* lineNumberArea  = nullptr;
    bool     showLineNumbers = false;
    bool     darkMode        = false;

protected:
    void resizeEvent(QResizeEvent* e) override {
        QPlainTextEdit::resizeEvent(e);
        if (lineNumberArea && showLineNumbers) {
            QRect cr = contentsRect();
            lineNumberArea->setGeometry(cr.left(), cr.top(),
                                        lineNumberAreaWidth(), cr.height());
        }
    }
};

// ── Gutter widget ──────────────────────────────────────────────
class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor* editor) : QWidget(editor), codeEditor(editor) {}
    QSize sizeHint() const override {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        codeEditor->lineNumberAreaPaintEvent(event);
    }
private:
    CodeEditor* codeEditor;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onNewFolder();
    void onDeleteFolder();
    void onFolderSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void onRenameFolder(QListWidgetItem* item);
    void onNewSnippet();
    void onDeleteSnippet();
    void onSnippetSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void onSaveSnippet();
    void onSearchTextChanged(const QString& text);
    void onToggleWrap();
    void onToggleDarkMode();
    void onToggleLineNumbers();
    void onBlockCountChanged();
    void onUpdateRequest(const QRect& rect, int dy);

private:
    void setupUI();
    void setupMenuBar();
    void loadFolders();
    void loadSnippets(int folderID);
    void clearEditor();
    void populateEditor(const Snippet& s);
    void applyThemeStyles(bool dark);

    // Sidebar
    QListWidget*    folderList;
    QPushButton*    btnNewFolder;
    QPushButton*    btnDeleteFolder;
    QLabel*         folderCountLabel;

    // Snippet list
    QListWidget*    snippetList;
    QPushButton*    btnNewSnippet;
    QPushButton*    btnDeleteSnippet;
    QLineEdit*      searchBox;
    QLabel*         snippetCountLabel;

    // Editor
    QLineEdit*      titleField;
    QLineEdit*      descField;
    CodeEditor*     codeEditor;
    LineNumberArea* lineNumberArea;
    QTextEdit*      noteEditor;
    QPushButton*    btnSave;

    // Options
    QAction*        wrapAction;
    QAction*        darkModeAction;
    QAction*        lineNumberAction;
    bool            wrapEnabled        = false;
    bool            darkModeEnabled    = false;
    bool            lineNumbersEnabled = false;

    // Panels
    QWidget*        sidebarWidget;
    QWidget*        snippetListWidget;

    int currentFolderID  = -1;
    int currentSnippetID = -1;
    QList<Snippet> currentSnippets;
};
