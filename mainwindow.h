#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextLayout>
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
#include <QKeyEvent>
#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "databasemanager.h"

// ── CodeEditor subclass exposes protected methods for line numbers ──
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget* parent = nullptr);
    void setLineSpacing(int pixels);

    void sortLines(Qt::CaseSensitivity sensitivity);
    void convertCase(QString (*convert)(const QString&));
    void selectNextOccurrence();
    void splitSelectionIntoLines();
    void clearMultipleCursors();

    // Paints a full-width tint behind the line the caret is on. In dark mode the
    // tint is a touch lighter than the near-black background; in light mode it is
    // a touch darker than white.
    void highlightCurrentLine() {
        QList<QTextEdit::ExtraSelection> extras;
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(currentLineColor);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        extras.append(sel);
        for (const auto& cursor : multipleCursors) {
            if (!cursor.hasSelection()) continue;
            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format.setBackground(palette().highlight());
            selection.format.setForeground(palette().highlightedText());
            extras.append(selection);
        }
        setExtraSelections(extras);
        viewport()->update();
    }

    int lineNumberAreaWidth() {
        int digits = 1;
        int max = qMax(1, blockCount());
        while (max >= 10) { max /= 10; ++digits; }
        return fontMetrics().horizontalAdvance(QLatin1Char('9')) * (digits + 1) + 10;
    }

    void updateLineNumberAreaWidth() {
        const int width = showLineNumbers ? lineNumberAreaWidth() : 0;
        if (viewportMargins().left() != width)
            setViewportMargins(width, 0, 0, 0);
        if (lineNumberArea && showLineNumbers) {
            const QRect view = viewport()->geometry();
            lineNumberArea->setGeometry(view.left() - width, view.top(), width, view.height());
        }
    }

    void lineNumberAreaPaintEvent(QPaintEvent* event) {
        QPainter painter(lineNumberArea);
        painter.fillRect(event->rect(), gutterBackground);
        painter.setFont(font());
        painter.setPen(gutterForeground);
        const QFontMetricsF metrics(painter.font());
        const int right = lineNumberArea->width() - 8;

        for (QTextBlock block = firstVisibleBlock(); block.isValid(); block = block.next()) {
            const QRectF bounds = blockBoundingGeometry(block).translated(contentOffset());
            if (bounds.top() > event->rect().bottom()) break;
            if (!block.isVisible() || bounds.bottom() < event->rect().top()) continue;

            const QTextLine firstLine = block.layout()->lineAt(0);
            if (!firstLine.isValid()) continue;
            const QString number = QString::number(block.blockNumber() + 1);
            const qreal baseline = bounds.top() + firstLine.y() + firstLine.ascent();
            painter.drawText(QPointF(right - metrics.horizontalAdvance(number), baseline), number);
        }
    }

    QWidget* lineNumberArea  = nullptr;
    bool     showLineNumbers = false;
    bool     darkMode        = false;
    QColor   currentLineColor = QColor(236, 236, 236);
    QColor   gutterBackground = QColor(220, 220, 220);
    QColor   gutterForeground = QColor(100, 100, 100);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void insertFromMimeData(const QMimeData* source) override;

    void resizeEvent(QResizeEvent* e) override {
        QPlainTextEdit::resizeEvent(e);
        updateLineNumberAreaWidth();
    }

private:
    void refreshMultipleCursors();
    void editMultipleCursors(const QString& text, QTextCursor::MoveOperation deletion);

    QList<QTextCursor> multipleCursors;
    QString occurrenceText;
    bool editingMultiple = false;
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

class QToolButton;
class QActionGroup;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onNewFolder();
    void onDeleteFolder();
    void onFolderSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void onRenameFolder(QListWidgetItem* item);
    void onNewSnippet();
    void onDeleteSnippet();
    void onSnippetSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void onSaveSnippet();
    void onCopyCode();
    void onSearchTextChanged(const QString& text);
    void onToggleWrap();
    void onToggleDarkMode();
    void onToggleLineNumbers();
    void onBlockCountChanged();
    void onUpdateRequest(const QRect& rect, int dy);
    void onBackupDatabase();
    void onImportDatabase();
    void onResetWindowSize();
    void markDirty();

    // Snippet actions
    void onCloneSnippet();
    void onToggleFavorite();
    void onSnippetContextMenu(const QPoint& pos);

    // Export
    void onImportSnippet();
    void onExportSnippet();
    void onExportFolder();

    // Font size
    void onIncreaseFont();
    void onDecreaseFont();
    void onResetFont();

    // Find / replace
    void onShowFindBar();
    void onHideFindBar();
    void onFindNext();
    void onReplaceOne();
    void onReplaceAll();

    // Status line
    void updateStatusInfo();

private:
    void updateWindowCorners();
    void setupUI();
    void setupMenuBar();
    void loadFolders();
    void loadSnippets(int folderID, int selectSnippetID = -1);
    void updateFavoriteButton(bool favorite);
    void resortSnippetList();   // re-orders the list in place (favorites first, then title) without touching the editor
    void clearEditor();
    void populateEditor(const Snippet& s);
    void applyThemeStyles();
    void restoreLastSelection();

    // Feature helpers
    void applyEditorFont();
    void setCodeLineSpacing(int pixels);
    void moveSnippetToFolder(int targetFolderID);
    bool importSnippetFiles(const QStringList& paths);
    int createSnippetFromFile(const QString& path);
    bool exportSnippetToPath(const QString& code, const QString& path);
    QString relativeTime(const QString& iso) const;
    const Snippet* currentSnippet() const;   // in-memory record for currentSnippetID, or nullptr

    // Unsaved-changes handling
    void setDirty(bool dirty);
    void saveCurrentSnippet();   // writes the editor to the DB with no confirmation dialog
    bool maybeSave();            // returns false only if the user cancels the pending action
    int  promptUnsaved();        // returns a QMessageBox::StandardButton value (Save/Discard/Cancel)

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
    QPushButton*    btnFavorite;
    QPushButton*    btnCopyCode;
    QPushButton*    btnSave;

    // Find / replace bar
    QWidget*        findBar;
    QLineEdit*      findField;
    QLineEdit*      replaceField;
    QCheckBox*      findCaseCheck;

    // Status line
    QLabel*         statusInfoLabel;

    // Options
    QCheckBox*      wrapCheck;
    QToolButton*    fontSizeButton;
    QToolButton*    lineSpacingButton;
    QAction*        darkModeAction;
    QActionGroup*   colorSchemeActions;
    QCheckBox*      lineNumberCheck;
    bool            wrapEnabled        = false;
    bool            darkModeEnabled    = false;
    bool            lineNumbersEnabled = false;

    // Panels
    QWidget*        sidebarWidget;
    QWidget*        snippetListWidget;
    QSplitter*      mainSplitter;
    QSplitter*      editorSplitter;

    int currentFolderID  = -1;
    int currentSnippetID = -1;
    QList<Snippet> currentSnippets;

    // Editor state flags
    bool dirty          = false;   // editor has unsaved changes for the current snippet
    bool loadingSnippet = false;   // suppresses dirty-marking while populating fields programmatically
    bool rebuildingList = false;   // suppresses the unsaved-changes guard during programmatic list rebuilds

    QString editorFontFamily = QStringLiteral("Monospace");
    int editorFontSize = 12;
    int codeLineSpacing = 2;
};
