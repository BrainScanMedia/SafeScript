#include <QToolButton>
#include <QTabWidget>
#include <QWindow>
#include <QCollator>
#include <QRegularExpression>
#include <QPainterPath>
#include <QActionGroup>
#include <QStyle>
#include <QStyleOptionButton>
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
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
#include <QSizeGrip>
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

static QRegion roundedWindowRegion(const QRect& rect) {
    constexpr int radius = 12;
    QPainterPath outline;
    outline.addRoundedRect(QRectF(rect), radius, radius);
    return QRegion(outline.toFillPolygon().toPolygon());
}

class PopupDialog : public QDialog {
public:
    PopupDialog(QWidget* parent, const QString& title,
                QStyle::StandardPixmap icon = QStyle::SP_MessageBoxInformation)
        : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint) {
        setAttribute(Qt::WA_TranslucentBackground);
        setObjectName("safeScriptPopup");
        setStyleSheet("QDialog#safeScriptPopup { background: transparent; }"
                      "QWidget#popupContent { background-color: palette(window); }");
        setWindowTitle(title);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        heading = new QWidget;
        heading->setObjectName("popupHeading");
        heading->setStyleSheet("QWidget#popupHeading { background-color: palette(alternate-base); }"
                               "QLabel { background: transparent; color: palette(window-text);"
                               " font-weight: bold; }");
        auto* headerLayout = new QHBoxLayout(heading);
        headerLayout->setContentsMargins(16, 8, 16, 8);
        headerLayout->setSpacing(8);
        auto* iconLabel = new QLabel;
        iconLabel->setPixmap(style()->standardIcon(icon).pixmap(QSize(16, 16), devicePixelRatioF()));
        headerLayout->addWidget(iconLabel);
        auto* titleLabel = new QLabel(title);
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        headerLayout->addWidget(titleLabel, 1);
        layout->addWidget(heading);

        content = new QWidget;
        content->setObjectName("popupContent");
        body = new QVBoxLayout(content);
        body->setContentsMargins(16, 16, 16, 16);
        layout->addWidget(content);
    }

    QVBoxLayout* body;

protected:
    void resizeEvent(QResizeEvent* event) override {
        QDialog::resizeEvent(event);
        const QRegion shape = roundedWindowRegion(rect());
        setMask(shape);
        heading->setMask(shape);
        content->setMask(shape.translated(0, -content->y()));
    }

private:
    QWidget* heading;
    QWidget* content;
};

static QMessageBox::StandardButton showPopup(
    QWidget* parent, const QString& title, const QString& message,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
    QMessageBox::StandardButton defaultButton = QMessageBox::Ok) {
    PopupDialog dialog(parent, title, buttons == QMessageBox::Ok
                                         ? QStyle::SP_MessageBoxInformation
                                         : QStyle::SP_MessageBoxQuestion);
    dialog.setMinimumWidth(260);
    auto* label = new QLabel(message);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    dialog.body->addWidget(label);

    auto* box = new QDialogButtonBox(QDialogButtonBox::StandardButtons(int(buttons)));
    if (auto* button = box->button(QDialogButtonBox::StandardButton(defaultButton)))
        button->setDefault(true);
    QObject::connect(box, &QDialogButtonBox::clicked, &dialog, [&dialog, box](QAbstractButton* button) {
        dialog.done(box->standardButton(button));
    });
    dialog.body->addWidget(box);
    const int result = dialog.exec();
    return result == QDialog::Rejected ? QMessageBox::Cancel : QMessageBox::StandardButton(result);
}

static QString promptPopupText(QWidget* parent, const QString& title, const QString& label,
                               const QString& value = {}) {
    PopupDialog dialog(parent, title);
    dialog.setMinimumWidth(300);
    dialog.body->addWidget(new QLabel(label));
    auto* field = new QLineEdit(value);
    dialog.body->addWidget(field);
    field->setFocus();
    field->selectAll();
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.body->addWidget(box);
    return dialog.exec() == QDialog::Accepted ? field->text() : QString();
}

class EditorCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;

protected:
    void paintEvent(QPaintEvent* event) override {
        QCheckBox::paintEvent(event);
        if (palette().color(QPalette::Window).lightness() < 128) return;

        QStyleOptionButton option;
        initStyleOption(&option);
        const QRectF box = QRectF(style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, this))
                               .adjusted(0.5, 0.5, -0.5, -0.5);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QColor("#777777"));
        painter.setBrush(option.palette.color(QPalette::Base));
        painter.drawRoundedRect(box, 2, 2);

        if (isChecked()) {
            painter.setPen(QPen(option.palette.color(QPalette::Text), 1.6, Qt::SolidLine,
                                Qt::RoundCap, Qt::RoundJoin));
            painter.drawPolyline(QPolygonF{
                QPointF(box.left() + box.width() * 0.22, box.top() + box.height() * 0.52),
                QPointF(box.left() + box.width() * 0.43, box.top() + box.height() * 0.73),
                QPointF(box.left() + box.width() * 0.80, box.top() + box.height() * 0.29)
            });
        }
    }
};

class FooterResizeGrip : public QSizeGrip {
public:
    explicit FooterResizeGrip(QWidget* parent) : QSizeGrip(parent) {
        setFixedSize(24, 24);
        setToolTip("Resize window");
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        QColor color = palette().color(QPalette::WindowText);
        color.setAlpha(150);
        painter.setPen(color);
        const int right = width() - 5;
        const int bottom = height() - 5;
        for (int length : {5, 9, 13})
            painter.drawLine(right - length, bottom, right, bottom - length);
    }
};

class WindowTitleBar : public QWidget {
public:
    explicit WindowTitleBar(QWidget* window) : QWidget(window), window(window) {
        setObjectName("windowTitleBar");
        auto* row = new QGridLayout(this);
        row->setContentsMargins(12, 8, 12, 8);
        row->setSpacing(0);
        constexpr int controlWidth = 24;
        constexpr int controlSpacing = 8;
        constexpr int controlsWidth = 3 * controlWidth + 2 * controlSpacing;
        row->setColumnMinimumWidth(0, controlsWidth);
        row->setColumnStretch(1, 1);
        row->setColumnMinimumWidth(2, controlsWidth);

        title = new QLabel;
        title->setAlignment(Qt::AlignCenter);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->addWidget(title, 0, 1);

        auto* controls = new QHBoxLayout;
        controls->setSpacing(controlSpacing);
        row->addLayout(controls, 0, 2);

        const auto makeButton = [controls, controlWidth](const QString& glyph, const QString& label) {
            auto* button = new QToolButton;
            button->setFixedSize(controlWidth, controlWidth);
            button->setText(glyph);
            button->setToolTip(label);
            button->setAccessibleName(label);
            controls->addWidget(button);
            return button;
        };

        auto* minimize = makeButton("−", "Minimize");
        connect(minimize, &QToolButton::clicked, window, &QWidget::showMinimized);

        maximize = makeButton("□", "Maximize");
        connect(maximize, &QToolButton::clicked, this, [this]() { toggleMaximized(); });

        auto* close = makeButton("×", "Close");
        connect(close, &QToolButton::clicked, window, &QWidget::close);

        window->installEventFilter(this);
        refresh();
    }

protected:
    bool eventFilter(QObject* object, QEvent* event) override {
        if (object == window && (event->type() == QEvent::WindowTitleChange ||
                                 event->type() == QEvent::ModifiedChange ||
                                 event->type() == QEvent::WindowStateChange))
            refresh();
        return QWidget::eventFilter(object, event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window->windowHandle() &&
            window->windowHandle()->startSystemMove()) {
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            toggleMaximized();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    void toggleMaximized() {
        if (window->isMaximized())
            window->showNormal();
        else
            window->showMaximized();
    }

    void refresh() {
        title->setText(QString(window->windowTitle()).replace("[*]", window->isWindowModified() ? "*" : ""));
        const bool maximized = window->isMaximized();
        maximize->setText(maximized ? "❐" : "□");
        const QString label = maximized ? "Restore" : "Maximize";
        maximize->setToolTip(label);
        maximize->setAccessibleName(label);
    }

    QWidget* window;
    QLabel* title;
    QToolButton* maximize;
};

class DropdownMenu : public QMenu {
public:
    DropdownMenu(const QString& title, QWidget* parent) : QMenu(title, parent) {
        setAttribute(Qt::WA_TranslucentBackground);
        connect(this, &QMenu::hovered, this, [this]() { update(); });
    }

protected:
    bool event(QEvent* event) override {
        QAction* previous = activeAction();
        const bool handled = QMenu::event(event);
        if (previous != activeAction()) update();
        return handled;
    }

    void paintEvent(QPaintEvent* event) override {
        QMenu::paintEvent(event);
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        QPainter painter(this);
        painter.setPen(dark ? palette().color(QPalette::Window).lighter(165) : QColor("#dddddd"));
        for (auto* item : actions()) {
            if (!item->isSeparator()) continue;
            const QRect separator = actionGeometry(item);
            if (separator.isEmpty()) continue;
            painter.drawLine(1, separator.center().y(), width() - 2, separator.center().y());
        }

        auto* action = activeAction();
        if (!action || !action->isEnabled() || action->isSeparator()) return;

        const QRect row = actionGeometry(action);
        const QColor highlight = dark ? QColor("#094771") : QColor("#dedede");
        painter.fillRect(QRect(1, row.top(), row.left() - 1, row.height()), highlight);
        painter.fillRect(QRect(row.right() + 1, row.top(),
                               width() - row.right() - 2, row.height()), highlight);
    }
};

static QMenu* addDropdown(QMenuBar* bar, const QString& title) {
    auto* menu = new DropdownMenu(title, bar);
    bar->addMenu(menu);
    return menu;
}

static QString convertCodeToAscii(const QString& code) {
    QString result;
    result.reserve(code.size());
    bool followsAsciiLetter = false;
    for (const char32_t character : code.normalized(QString::NormalizationForm_KD).toUcs4()) {
        if (character <= 0x7f) {
            const QChar ascii(static_cast<ushort>(character));
            result.append(ascii);
            followsAsciiLetter = ascii.isLetter();
            continue;
        }

        const auto category = QChar::category(character);
        if (followsAsciiLetter && (category == QChar::Mark_NonSpacing ||
                                   category == QChar::Mark_SpacingCombining ||
                                   category == QChar::Mark_Enclosing))
            continue;
        followsAsciiLetter = false;

        switch (character) {
        case U'‘': case U'’': result += u'\''; break;
        case U'“': case U'”': result += u'"'; break;
        case U'–': case U'—': case U'−': result += u'-'; break;
        case U'ß': result += "ss"; break;
        case U'Æ': result += "AE"; break;
        case U'æ': result += "ae"; break;
        case U'Œ': result += "OE"; break;
        case U'œ': result += "oe"; break;
        default:
            result += QStringLiteral("\\u{%1}").arg(quint32(character), 0, 16);
            break;
        }
    }
    return result;
}

static QString expandCodeTabs(const QString& code) {
    QString result;
    result.reserve(code.size());
    int column = 0;
    for (const QChar character : code) {
        if (character == u'\t') {
            const int spaces = 4 - column % 4;
            result.append(QString(spaces, u' '));
            column += spaces;
        } else {
            result.append(character);
            column = character == u'\n' ? 0 : column + 1;
        }
    }
    return result;
}

static QString removeHiddenCodeCharacters(const QString& code) {
    QString result;
    result.reserve(code.size());
    for (const QChar character : code) {
        const auto category = character.category();
        if ((category != QChar::Other_Control && category != QChar::Other_Format) ||
            character == u'\t' || character == u'\n' || character == u'\r') {
            result.append(character);
        }
    }
    return result;
}

static QString prettifyCodeIndentation(const QString& code) {
    QStringList lines = code.split(u'\n');
    QList<int> levels{0};
    for (QString& line : lines) {
        qsizetype start = 0;
        int indent = 0;
        while (start < line.size() && (line[start] == u' ' || line[start] == u'\t')) {
            indent += line[start] == u'\t' ? 4 - indent % 4 : 1;
            ++start;
        }
        if (start == line.size()) {
            line.clear();
            continue;
        }

        while (levels.last() > indent)
            levels.removeLast();
        if (levels.last() < indent)
            levels.append(indent);
        line.replace(0, start, QString(levels.size() - 1, u' '));
    }
    return lines.join(u'\n');
}

static QString titleCase(const QString& text) {
    static const QRegularExpression words(QStringLiteral(R"(\p{L}[\p{L}\p{M}\p{N}]*)"));
    QString result = text;
    auto matches = words.globalMatch(text);
    qsizetype offset = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        const QString word = match.captured();
        const QString converted = word.left(1).toUpper() + word.mid(1).toLower();
        result.replace(match.capturedStart() + offset, word.size(), converted);
        offset += converted.size() - word.size();
    }
    return result;
}

static QString swapCase(const QString& text) {
    QString result;
    result.reserve(text.size());
    for (const QChar character : text)
        result.append(character.isUpper() ? character.toLower() : character.toUpper());
    return result;
}

static QString identifierCase(const QString& text, bool capitalizeFirst) {
    static const QRegularExpression boundaries(
        QStringLiteral(R"((?<=[\p{Ll}\p{N}])(?=\p{Lu})|(?<=\p{Lu})(?=\p{Lu}\p{Ll}))"));
    static const QRegularExpression separators(QStringLiteral(R"([^\p{L}\p{M}\p{N}]+)"));
    QStringList lines = text.split(u'\n');
    for (QString& line : lines) {
        QString spaced = line;
        spaced.replace(boundaries, " ");
        QStringList words = spaced.toLower().split(separators, Qt::SkipEmptyParts);
        if (words.isEmpty()) continue;
        for (qsizetype i = capitalizeFirst ? 0 : 1; i < words.size(); ++i)
            words[i].replace(0, 1, words[i].left(1).toUpper());
        line = words.join(QString());
    }
    return lines.join(u'\n');
}

class CodeDocumentLayout : public QPlainTextDocumentLayout {
public:
    explicit CodeDocumentLayout(QTextDocument* document) : QPlainTextDocumentLayout(document) {}

    QRectF blockBoundingRect(const QTextBlock& block) const override {
        QRectF bounds = QPlainTextDocumentLayout::blockBoundingRect(block);
        if (block.isValid() && block.isVisible())
            bounds.setHeight(bounds.height() + spacing);
        return bounds;
    }

    int spacing = 2;
};

CodeEditor::CodeEditor(QWidget* parent) : QPlainTextEdit(parent) {
    auto* codeDocument = new QTextDocument(this);
    codeDocument->setDocumentLayout(new CodeDocumentLayout(codeDocument));
    setDocument(codeDocument);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        if (!editingMultiple) clearMultipleCursors();
    });
    highlightCurrentLine();
}

void CodeEditor::setLineSpacing(int pixels) {
    auto* layout = static_cast<CodeDocumentLayout*>(document()->documentLayout());
    if (layout->spacing == pixels) return;
    layout->spacing = pixels;
    document()->markContentsDirty(0, document()->characterCount());
    ensureCursorVisible();
    viewport()->update();
    if (lineNumberArea) lineNumberArea->update();
}

void CodeEditor::sortLines(Qt::CaseSensitivity sensitivity) {
    if (isReadOnly()) return;
    const QTextCursor original = textCursor();
    QTextCursor range(document());
    range.select(QTextCursor::Document);
    if (original.hasSelection()) {
        const QTextBlock first = document()->findBlock(original.selectionStart());
        const QTextBlock last = document()->findBlock(original.selectionEnd() - 1);
        range.setPosition(first.position());
        range.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
    }

    const QString text = range.selectedText().replace(QChar::ParagraphSeparator, u'\n');
    QStringList lines = text.split(u'\n');
    const bool trailingNewline = !original.hasSelection() && text.endsWith(u'\n');
    if (trailingNewline) lines.removeLast();

    QLocale locale;
    if (locale.language() == QLocale::C) locale = QLocale(QLocale::English);
    QCollator collator(locale);
    collator.setCaseSensitivity(sensitivity);
    collator.setNumericMode(true);
    std::stable_sort(lines.begin(), lines.end(), [&collator](const QString& a, const QString& b) {
        return collator.compare(a, b) < 0;
    });

    QString sorted = lines.join(u'\n');
    if (trailingNewline) sorted += u'\n';
    if (sorted == text) return;

    clearMultipleCursors();
    const int start = range.selectionStart();
    range.beginEditBlock();
    range.insertText(sorted);
    range.endEditBlock();
    if (original.hasSelection()) {
        const int end = range.position();
        range.setPosition(start);
        range.setPosition(end, QTextCursor::KeepAnchor);
    } else {
        range.setPosition(original.position());
    }
    setTextCursor(range);
}

void CodeEditor::convertCase(QString (*convert)(const QString&)) {
    if (isReadOnly()) return;
    const bool multiple = !multipleCursors.isEmpty();
    QList<QTextCursor> selections = multiple ? multipleCursors : QList<QTextCursor>{textCursor()};
    if (!multiple && !selections.first().hasSelection())
        selections.first().select(QTextCursor::WordUnderCursor);

    editingMultiple = true;
    occurrenceText.clear();
    QTextCursor transaction(document());
    transaction.beginEditBlock();
    for (auto& cursor : selections) {
        if (!cursor.hasSelection()) continue;
        const QString original = cursor.selectedText().replace(QChar::ParagraphSeparator, u'\n');
        const QString converted = convert(original);
        if (original == converted) continue;
        const int start = cursor.selectionStart();
        cursor.insertText(converted);
        const int end = cursor.position();
        cursor.setPosition(start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
    }
    transaction.endEditBlock();
    editingMultiple = false;
    if (multiple) {
        multipleCursors = selections;
        refreshMultipleCursors();
    } else {
        setTextCursor(selections.first());
    }
}

void CodeEditor::clearMultipleCursors() {
    if (multipleCursors.isEmpty() && occurrenceText.isEmpty()) return;
    multipleCursors.clear();
    occurrenceText.clear();
    highlightCurrentLine();
}

void CodeEditor::refreshMultipleCursors() {
    setTextCursor(multipleCursors.last());
    highlightCurrentLine();
}

void CodeEditor::selectNextOccurrence() {
    if (occurrenceText.isEmpty()) {
        QTextCursor cursor = textCursor();
        const bool hadSelection = cursor.hasSelection();
        if (!hadSelection) cursor.select(QTextCursor::WordUnderCursor);
        const QString text = cursor.selectedText();
        if (text.isEmpty()) return;
        multipleCursors = {cursor};
        occurrenceText = text;
        if (!hadSelection) {
            refreshMultipleCursors();
            return;
        }
    }

    const QString code = toPlainText();
    const QString needle = QString(occurrenceText).replace(QChar::ParagraphSeparator, u'\n');
    const qsizetype start = multipleCursors.last().selectionEnd();
    for (const auto& range : {qMakePair(start, code.size()), qMakePair(qsizetype(0), start)}) {
        qsizetype position = code.indexOf(needle, range.first);
        while (position >= 0 && position < range.second) {
            const qsizetype end = position + needle.size();
            const bool overlaps = std::any_of(multipleCursors.cbegin(), multipleCursors.cend(),
                                              [position, end](const QTextCursor& cursor) {
                return position < cursor.selectionEnd() && end > cursor.selectionStart();
            });
            if (!overlaps) {
                QTextCursor match(document());
                match.setPosition(int(position));
                match.setPosition(int(end), QTextCursor::KeepAnchor);
                multipleCursors.append(match);
                refreshMultipleCursors();
                return;
            }
            position = code.indexOf(needle, position + 1);
        }
    }
}

void CodeEditor::splitSelectionIntoLines() {
    const QTextCursor selection = textCursor();
    if (!selection.hasSelection()) return;

    clearMultipleCursors();
    const int end = selection.selectionEnd();
    for (QTextBlock block = document()->findBlock(selection.selectionStart());
         block.isValid() && block.position() < end; block = block.next()) {
        QTextCursor cursor(block);
        cursor.setPosition(qMin(end, block.position() + block.length() - 1));
        multipleCursors.append(cursor);
    }
    refreshMultipleCursors();
}

void CodeEditor::editMultipleCursors(const QString& text, QTextCursor::MoveOperation deletion) {
    occurrenceText.clear();
    editingMultiple = true;
    QList<QTextCursor> edits = multipleCursors;
    for (auto& cursor : edits) {
        if (!cursor.hasSelection() && deletion != QTextCursor::NoMove)
            cursor.movePosition(deletion, QTextCursor::KeepAnchor);
    }
    std::sort(edits.begin(), edits.end(), [](const QTextCursor& left, const QTextCursor& right) {
        return left.selectionStart() < right.selectionStart();
    });
    for (qsizetype i = 1; i < edits.size();) {
        const int start = edits[i - 1].selectionStart();
        const int end = edits[i - 1].selectionEnd();
        if (edits[i].selectionStart() < end || edits[i].selectionStart() == start) {
            const int combinedEnd = qMax(end, edits[i].selectionEnd());
            edits[i - 1].setPosition(start);
            edits[i - 1].setPosition(combinedEnd, QTextCursor::KeepAnchor);
            edits.removeAt(i);
        } else {
            ++i;
        }
    }
    QTextCursor transaction(document());
    transaction.beginEditBlock();
    for (auto it = edits.rbegin(); it != edits.rend(); ++it)
        it->insertText(text);
    transaction.endEditBlock();

    multipleCursors = edits;
    editingMultiple = false;
    refreshMultipleCursors();
}

void CodeEditor::keyPressEvent(QKeyEvent* event) {
    if (multipleCursors.isEmpty()) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        clearMultipleCursors();
        QTextCursor cursor = textCursor();
        cursor.clearSelection();
        setTextCursor(cursor);
        return;
    }

    QTextCursor::MoveOperation movement = QTextCursor::NoMove;
    const bool control = event->modifiers().testFlag(Qt::ControlModifier);
    switch (event->key()) {
    case Qt::Key_Home: movement = control ? QTextCursor::Start : QTextCursor::StartOfBlock; break;
    case Qt::Key_End: movement = control ? QTextCursor::End : QTextCursor::EndOfBlock; break;
    case Qt::Key_Left: movement = control ? QTextCursor::PreviousWord : QTextCursor::Left; break;
    case Qt::Key_Right: movement = control ? QTextCursor::NextWord : QTextCursor::Right; break;
    case Qt::Key_Up: movement = QTextCursor::Up; break;
    case Qt::Key_Down: movement = QTextCursor::Down; break;
    }
    if (movement != QTextCursor::NoMove) {
        occurrenceText.clear();
        const bool extend = event->modifiers().testFlag(Qt::ShiftModifier);
        for (auto& cursor : multipleCursors) {
            if (!extend && cursor.hasSelection() &&
                (movement == QTextCursor::Left || movement == QTextCursor::Right)) {
                cursor.setPosition(movement == QTextCursor::Left ? cursor.selectionStart()
                                                                  : cursor.selectionEnd());
            } else {
                cursor.movePosition(movement, extend ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
            }
        }
        refreshMultipleCursors();
        return;
    }

    if (!isReadOnly() && !control && !event->modifiers().testFlag(Qt::AltModifier)) {
        switch (event->key()) {
        case Qt::Key_Backspace: editMultipleCursors({}, QTextCursor::PreviousCharacter); return;
        case Qt::Key_Delete: editMultipleCursors({}, QTextCursor::NextCharacter); return;
        case Qt::Key_Return:
        case Qt::Key_Enter: editMultipleCursors(QStringLiteral("\n"), QTextCursor::NoMove); return;
        case Qt::Key_Tab: editMultipleCursors(QStringLiteral("\t"), QTextCursor::NoMove); return;
        default: break;
        }
        if (!event->text().isEmpty() && event->text().front().category() != QChar::Other_Control) {
            editMultipleCursors(event->text(), QTextCursor::NoMove);
            return;
        }
    }

    if (event->matches(QKeySequence::Undo) || event->matches(QKeySequence::Redo) ||
        event->matches(QKeySequence::SelectAll))
        clearMultipleCursors();
    QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::insertFromMimeData(const QMimeData* source) {
    if (!multipleCursors.isEmpty() && source->hasText() && !isReadOnly()) {
        editMultipleCursors(source->text(), QTextCursor::NoMove);
        return;
    }
    QPlainTextEdit::insertFromMimeData(source);
}

void CodeEditor::mousePressEvent(QMouseEvent* event) {
    clearMultipleCursors();
    QPlainTextEdit::mousePressEvent(event);
}

void CodeEditor::paintEvent(QPaintEvent* event) {
    QPlainTextEdit::paintEvent(event);
    if (multipleCursors.size() < 2 || !hasFocus()) return;
    QPainter painter(viewport());
    painter.setPen(palette().text().color());
    for (qsizetype i = 0; i < multipleCursors.size() - 1; ++i) {
        const auto& cursor = multipleCursors[i];
        if (cursor.hasSelection()) continue;
        const QRect caret = cursorRect(cursor);
        painter.drawLine(caret.topLeft(), caret.bottomLeft());
    }
}

static QIcon menuIcon(const QString& name, QStyle::StandardPixmap fallback) {
    return QIcon::fromTheme(name, QApplication::style()->standardIcon(fallback));
}

static QIcon eyeMenuIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor ink("#818b99");
    painter.setPen(QPen(ink, 2.5));
    QPainterPath eye(QPointF(3, 16));
    eye.quadTo(16, 1, 29, 16);
    eye.quadTo(16, 31, 3, 16);
    painter.drawPath(eye);
    painter.setBrush(ink);
    painter.drawEllipse(QPointF(16, 16), 3, 3);
    painter.end();
    return QIcon(pixmap);
}

static QIcon favoriteMenuIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#9a6400"));
    painter.setBrush(QColor("#ffcc33"));
    painter.drawPolygon(QPolygon{
        {16, 2}, {20, 11}, {30, 12}, {23, 19}, {25, 29},
        {16, 24}, {7, 29}, {9, 19}, {2, 12}, {12, 11}
    });
    painter.end();
    return QIcon(pixmap);
}

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

    static QRect handleRect(const QRect& row) {
        return QRect(row.left() + 6, row.center().y() - 4, 13, 9);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, option, index);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QColor lineColor = option.state & QStyle::State_Selected
                               ? QColor(255, 255, 255, 180)
                               : QColor(150, 150, 150, 200);
        painter->setPen(QPen(lineColor, 1.5));
        const QRect handle = handleRect(option.rect);
        const int x1 = handle.left();
        const int x2 = handle.right();
        const int cy = handle.center().y();
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
struct ColorScheme {
    const char* name;
    const char* editor;
    const char* panel;
    const char* text;
    const char* border;
    const char* selection;
};

static const ColorScheme colorSchemes[] = {
    {"Midnight Blue", "#121b29", "#1d2a3b", "#e1ebf7", "#364c66", "#31587f"},
    {"Forest",        "#15241e", "#20342a", "#e0efe7", "#3e594a", "#32644e"},
    {"Plum",          "#23182c", "#31213e", "#f0e3fa", "#513b62", "#684681"},
    {"Amber Terminal","#211b13", "#30271c", "#f4e5cb", "#594532", "#745028"},
    {"Nord",          "#242933", "#303846", "#e5e9f0", "#49576b", "#426078"},
    {"Obsidian",      "#191a20", "#24252e", "#edeaf5", "#464658", "#514477"},
    {"Deep Teal",     "#102021", "#193032", "#e0f1ee", "#345558", "#286760"},
    {"Paper",         "#fffdf6", "#f3eddf", "#332b20", "#c9bca7", "#775437"},
    {"Glacier",       "#f7fcff", "#e7f1f7", "#1d3142", "#adc4d3", "#345e7d"},
};

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
    wrapCheck->setChecked(wrapEnabled);
    codeEditor->setLineWrapMode(wrapEnabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

    // Restore dark mode
    QString savedDark = DatabaseManager::instance().getSetting("DarkMode", "off");
    darkModeEnabled = (savedDark == "on");
    const QString savedScheme = DatabaseManager::instance().getSetting("ColorScheme", "");
    for (auto* action : colorSchemeActions->actions())
        action->setChecked(action->text() == savedScheme);
    applyThemeStyles();

    // Restore editor font size
    editorFontSize = DatabaseManager::instance().getSetting("EditorFontSize", "12").toInt();
    if (editorFontSize < 8)  editorFontSize = 8;
    if (editorFontSize > 40) editorFontSize = 40;
    applyEditorFont();

    // Restore line numbers
    QString savedLines = DatabaseManager::instance().getSetting("LineNumbers", "off");
    lineNumbersEnabled = (savedLines == "on");
    lineNumberCheck->setChecked(lineNumbersEnabled);
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
void MainWindow::applyThemeStyles() {
    const auto* selected = colorSchemeActions->checkedAction();
    const auto* scheme = selected ? &colorSchemes[selected->data().toInt()] : nullptr;
    const bool dark = scheme ? QColor(scheme->panel).lightness() < 128 : darkModeEnabled;
    darkModeAction->setText(dark ? "Light Mode" : "Dark Mode");
    darkModeAction->setIcon(menuIcon(dark ? "weather-clear" : "weather-clear-night",
                                     QStyle::SP_DesktopIcon));

    QPalette palette = dark ? darkPalette() : lightPalette();
    if (scheme) {
        palette.setColor(QPalette::Window, QColor(scheme->panel));
        palette.setColor(QPalette::Base, QColor(scheme->editor));
        palette.setColor(QPalette::AlternateBase, QColor(scheme->border));
        palette.setColor(QPalette::Button, QColor(scheme->panel));
        palette.setColor(QPalette::Highlight, QColor(scheme->selection));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
            palette.setColor(role, QColor(scheme->text));
    }
    qApp->setPalette(palette);
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
            "QMenu { background-color: #2a2a2a; color: #d4d4d4; border: 1px solid #3a3a3a; border-radius: 8px; padding: 10px 14px; }"
            "QMenu::separator { height: 1px; margin: 2px 0; background: transparent; }"
            "QMenu::item { padding: 5px 10px; border-radius: 4px; }"
            "QMenu::item:selected { border-radius: 0; background-color: #094771; }"
            "QLabel { background-color: transparent; color: #d4d4d4; }"
            );
        statusBar()->setStyleSheet("QStatusBar { background-color: #1a1a1a; border-top: 2px solid #3a3a3a; }");
        folderList->setStyleSheet(
            "QListWidget { background-color: #2a2a2a; border: none; color: #d4d4d4; }"
            "QListWidget::item { padding-left: 22px; padding-top: 5px; padding-bottom: 5px;"
            "  border-bottom: 1px solid #3a3a3a; color: #d4d4d4; }"
            "QListWidget::item:selected { background-color: #094771; color: #ffffff; }"
            "QListWidget::item:hover { background-color: #3a3a3a; color: #ffffff; }"
            );
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
            "QMenu { background-color: #f5f5f5; color: #1e1e1e; border: 1px solid #cccccc; border-radius: 8px; padding: 10px 14px; }"
            "QMenu::separator { height: 1px; margin: 2px 0; background: transparent; }"
            "QMenu::item { padding: 5px 10px; border-radius: 4px; }"
            "QMenu::item:selected { border-radius: 0; background-color: #dedede; color: #1e1e1e; }"
            "QLabel { background-color: transparent; color: #1e1e1e; }"
            );
        statusBar()->setStyleSheet("QStatusBar { background-color: #cccccc; border-top: 2px solid #bbbbbb; }");
        folderList->setStyleSheet(
            "QListWidget { background-color: #f5f5f5; border: none; color: #1e1e1e; }"
            "QListWidget::item { padding-left: 22px; padding-top: 5px; padding-bottom: 5px;"
            "  border-bottom: 1px solid #e0e0e0; color: #1e1e1e; }"
            "QListWidget::item:selected { background-color: #0078d4; color: #ffffff; }"
            "QListWidget::item:hover { background-color: #e8e8e8; color: #000000; }"
            );
    }

    codeEditor->darkMode = dark;
    codeEditor->currentLineColor = dark ? QColor(45, 45, 45) : QColor(236, 236, 236);
    codeEditor->gutterBackground = dark ? QColor(40, 40, 40) : QColor(220, 220, 220);
    codeEditor->gutterForeground = dark ? QColor(130, 130, 130) : QColor(100, 100, 100);
    sidebarWidget->setStyleSheet(QString());

    const QColor chrome = scheme ? QColor(scheme->panel).darker(dark ? 145 : 112)
                                 : QColor(dark ? "#1a1a1a" : "#cccccc");
    const QColor titleBackground = chrome.darker(dark ? 130 : 110);
    const QColor chromeText = palette.color(QPalette::WindowText);
    QString chromeStyle = QStringLiteral(
        "QMainWindow#safeScriptWindow, QWidget#windowChrome { background: transparent; }"
        "QWidget#windowTitleBar { background-color: %1; color: %2; }"
        "QWidget#windowTitleBar QLabel { font-weight: bold; }"
        "QWidget#windowTitleBar QToolButton { background-color: %3; color: %2;"
        " border: none; border-radius: 12px; font-size: 15px; font-weight: bold; padding: 0; }"
        "QWidget#windowTitleBar QToolButton:hover { background-color: %4; }"
        "QWidget#windowTitleBar QToolButton:pressed { background-color: %5; }"
    ).arg(titleBackground.name(), chromeText.name(),
          (dark ? chrome.lighter(145) : titleBackground.lighter(118)).name(),
          (dark ? chrome.lighter(180) : titleBackground.lighter(128)).name(),
          (dark ? chrome.lighter(210) : titleBackground.lighter(138)).name());

    if (scheme) {
        const QString panels = QStringLiteral(
            "QWidget, QListWidget, QMenu, QMenuBar, QStatusBar { background-color: %1; color: %2; }"
            "QMainWindow, QSplitter, QPlainTextEdit, QTextEdit, QLineEdit { background-color: %3; color: %2; }"
            "QPlainTextEdit, QTextEdit, QLineEdit, QMenu, QPushButton { border-color: %4; }"
            "QSplitter::handle { background-color: %4; }"
            "QPushButton { background-color: %1; color: %2; }"
            "QPushButton:hover, QListWidget::item:hover { background-color: %4; color: %2; }"
            "QPushButton:pressed, QListWidget::item:selected, QMenuBar::item:selected { background-color: %5; color: white; }"
            "QListWidget::item { border-bottom-color: %4; color: %2; }"
            "QLabel { background-color: transparent; color: %2; }"
        ).arg(scheme->panel, scheme->text, scheme->editor, scheme->border, scheme->selection);
        setStyleSheet(styleSheet() + panels);
        const QColor sidebarColor = QColor(scheme->panel).darker(dark ? 125 : 108);
        sidebarWidget->setStyleSheet(QStringLiteral("QWidget#categorySidebar { background-color: %1; }")
                                         .arg(sidebarColor.name()));
        folderList->setStyleSheet(folderList->styleSheet() + panels +
                                  QStringLiteral("QListWidget { background-color: %1; }")
                                      .arg(sidebarColor.name()));
        const QColor editorColor(scheme->editor);
        codeEditor->currentLineColor = editorColor.darker(dark ? 130 : 108);
        codeEditor->gutterBackground = editorColor.darker(dark ? 150 : 114);
        const QColor textColor(scheme->text);
        codeEditor->gutterForeground = dark ? textColor.darker(140) : textColor;
        chromeStyle += QStringLiteral(
            "QMenuBar { background-color: %1; }"
            "QMenu { background-color: %2; }"
        ).arg(chrome.name(), titleBackground.name());
        statusBar()->setStyleSheet(QStringLiteral("QStatusBar { background-color: %1; border-top: 2px solid %2; }")
                                      .arg(chrome.name(), scheme->border));
    }
    const QColor scrollThumb = scheme ? QColor(scheme->border)
                                      : QColor(dark ? "#505050" : "#999999");
    chromeStyle += QStringLiteral(
        "QScrollBar:vertical { background-color: %1; width: 12px; }"
        "QScrollBar:horizontal { background-color: %1; height: 12px; }"
        "QScrollBar::handle { background-color: %2; border-radius: 4px; }"
        "QScrollBar::handle:hover { background-color: %3; }"
        "QScrollBar::handle:vertical { min-height: 20px; }"
        "QScrollBar::handle:horizontal { min-width: 20px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
        "QAbstractScrollArea::corner { background-color: %1; }"
    ).arg(chrome.name(), scrollThumb.name(),
          (dark ? scrollThumb.lighter(125) : scrollThumb.darker(115)).name());
    setStyleSheet(styleSheet() + chromeStyle);
    codeEditor->highlightCurrentLine();
    lineNumberArea->update();
}

// ── Line number slots ─────────────────────────────────────────
void MainWindow::onBlockCountChanged() {
    codeEditor->updateLineNumberAreaWidth();
}

void MainWindow::onUpdateRequest(const QRect& rect, int dy) {
    if (!lineNumbersEnabled) return;
    if (dy)
        lineNumberArea->update();
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    if (rect.contains(codeEditor->viewport()->rect()))
        codeEditor->updateLineNumberAreaWidth();
}

// ── Event Filter ──────────────────────────────────────────────
static bool hasLocalFiles(const QMimeData* mimeData) {
    for (const QUrl& url : mimeData->urls()) {
        if (url.isLocalFile()) return true;
    }
    return false;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove ||
        event->type() == QEvent::Drop) {
        auto* drop = static_cast<QDropEvent*>(event);
        if (hasLocalFiles(drop->mimeData())) {
            if (event->type() == QEvent::Drop)
                dropEvent(drop);
            else
                drop->acceptProposedAction();
            return true;
        }
    }

    if (obj == folderList->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            QListWidgetItem* item = folderList->itemAt(me->pos());
            const bool overHandle = item && FolderDelegate::handleRect(folderList->visualItemRect(item))
                                                .adjusted(-1, -1, 1, 1).contains(me->pos());
            folderList->viewport()->setCursor(overHandle ? Qt::OpenHandCursor : Qt::ArrowCursor);
        } else if (event->type() == QEvent::Leave) {
            folderList->viewport()->setCursor(Qt::ArrowCursor);
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// ── Setup UI ──────────────────────────────────────────────────
void MainWindow::setupUI() {
    // The "[*]" placeholder is shown as "*" whenever there are unsaved changes.
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName("safeScriptWindow");
    setWindowTitle("SafeScript v1.3.0[*]");
    resize(1100, 700);

    // Accept files dropped from the file manager to create snippets.
    setAcceptDrops(true);

    // ── Footer status bar ─────────────────────────────────
    QStatusBar* sb = new QStatusBar(this);
    sb->setFixedHeight(32);
    sb->setSizeGripEnabled(false);
    setStatusBar(sb);

    // Permanent right-aligned info line (lines / chars / modified). It survives
    // transient showMessage() notifications, which appear on the left.
    statusInfoLabel = new QLabel;
    statusInfoLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusInfoLabel->setContentsMargins(0, 0, 6, 0);
    sb->addWidget(statusInfoLabel, 1);
    sb->addPermanentWidget(new FooterResizeGrip(sb));

    // ── Sidebar ──────────────────────────────────────────
    folderList = new QListWidget;
    folderList->setMinimumWidth(180);
    folderList->setDragDropMode(QAbstractItemView::InternalMove);
    folderList->setDefaultDropAction(Qt::MoveAction);
    folderList->setItemDelegate(new FolderDelegate(folderList));
    folderList->setMouseTracking(true);
    folderList->viewport()->setMouseTracking(true);

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
    sidebarWidget->setObjectName("categorySidebar");
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
    findCaseCheck = new EditorCheckBox("Aa");
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

    btnFavorite = new QPushButton("☆ Favorite");
    btnFavorite->setFixedHeight(36);
    btnFavorite->setEnabled(false);
    updateFavoriteButton(false);
    connect(btnFavorite, &QPushButton::clicked, this, &MainWindow::onToggleFavorite);

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

    wrapCheck = new EditorCheckBox("Wrap Lines");
    connect(wrapCheck, &QCheckBox::clicked, this, &MainWindow::onToggleWrap);

    lineNumberCheck = new EditorCheckBox("Line Numbers");
    connect(lineNumberCheck, &QCheckBox::clicked, this, &MainWindow::onToggleLineNumbers);

    auto* decreaseFontButton = new QToolButton;
    decreaseFontButton->setText("A−");
    decreaseFontButton->setToolTip("Decrease editor font size");
    connect(decreaseFontButton, &QToolButton::clicked, this, &MainWindow::onDecreaseFont);

    fontSizeButton = new QToolButton;
    fontSizeButton->setToolTip("Reset editor font size");
    connect(fontSizeButton, &QToolButton::clicked, this, &MainWindow::onResetFont);

    auto* increaseFontButton = new QToolButton;
    increaseFontButton->setText("A+");
    increaseFontButton->setToolTip("Increase editor font size");
    connect(increaseFontButton, &QToolButton::clicked, this, &MainWindow::onIncreaseFont);

    auto* fontSizeLayout = new QHBoxLayout;
    fontSizeLayout->setSpacing(2);
    fontSizeLayout->addWidget(decreaseFontButton);
    fontSizeLayout->addWidget(fontSizeButton);
    fontSizeLayout->addWidget(increaseFontButton);

    auto* lineSpacingLayout = new QHBoxLayout;
    lineSpacingLayout->setSpacing(2);
    const auto addSpacingButton = [this, lineSpacingLayout](const QString& label, const QString& tip) {
        auto* button = new QToolButton;
        button->setText(label);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        lineSpacingLayout->addWidget(button);
        return button;
    };
    auto* decreaseSpacing = addSpacingButton("↕−", "Decrease code line height");
    lineSpacingButton = addSpacingButton(QString(), "Reset code line height");
    auto* increaseSpacing = addSpacingButton("↕+", "Increase code line height");
    connect(decreaseSpacing, &QToolButton::clicked, this, [this]() { setCodeLineSpacing(codeLineSpacing - 2); });
    connect(lineSpacingButton, &QToolButton::clicked, this, [this]() { setCodeLineSpacing(2); });
    connect(increaseSpacing, &QToolButton::clicked, this, [this]() { setCodeLineSpacing(codeLineSpacing + 2); });

    QHBoxLayout* actionRow = new QHBoxLayout;
    actionRow->setSpacing(8);
    actionRow->addWidget(wrapCheck);
    actionRow->addWidget(lineNumberCheck);
    actionRow->addLayout(fontSizeLayout);
    actionRow->addLayout(lineSpacingLayout);
    actionRow->addStretch();
    actionRow->addWidget(btnFavorite);
    actionRow->addWidget(btnCopyCode);
    actionRow->addWidget(btnSave);

    QVBoxLayout* editorLayout = new QVBoxLayout;
    editorLayout->setContentsMargins(8, 8, 8, 8);
    auto* titleLabel = new QLabel("Title:");
    titleLabel->setBuddy(titleField);
    editorLayout->addWidget(titleLabel);
    editorLayout->addWidget(titleField);

    auto* descriptionPanel = new QWidget;
    auto* descriptionLayout = new QVBoxLayout(descriptionPanel);
    descriptionLayout->setContentsMargins(0, 0, 0, 0);
    auto* descriptionLabel = new QLabel("Description:");
    descriptionLabel->setBuddy(descField);
    descriptionLayout->addWidget(descriptionLabel);
    descriptionLayout->addWidget(descField);
    editorLayout->addWidget(descriptionPanel);
    editorLayout->addWidget(new QLabel("Code:"));
    editorLayout->addWidget(findBar);
    editorLayout->addWidget(editorSplitter, 1);
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

    // Scroll-area viewports receive drops before the main window.
    for (QWidget* viewport : {folderList->viewport(), snippetList->viewport(),
                              codeEditor->viewport(), noteEditor->viewport()}) {
        viewport->setAcceptDrops(true);
        viewport->installEventFilter(this);
    }
}

// ── Menu Bar ──────────────────────────────────────────────────
void MainWindow::setupMenuBar() {
    auto* chrome = new QWidget(this);
    chrome->setObjectName("windowChrome");
    auto* layout = new QVBoxLayout(chrome);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(new WindowTitleBar(this));
    auto* menuBar = new QMenuBar(chrome);
    layout->addWidget(menuBar);
    setMenuWidget(chrome);

    QMenu* appMenu = addDropdown(menuBar, "SafeScript");
    QAction* quitAction = new QAction(menuIcon("application-exit", QStyle::SP_DialogCloseButton), "Quit", this);
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    appMenu->addAction(quitAction);

    QMenu* editMenu = addDropdown(menuBar, "Edit");

    QAction* cutAction = editMenu->addAction(menuIcon("edit-cut", QStyle::SP_DialogDiscardButton), "Cut");
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, [this]() {
        if (auto* w = qobject_cast<QLineEdit*>(focusWidget()))           w->cut();
        else if (auto* w = qobject_cast<QPlainTextEdit*>(focusWidget())) w->cut();
        else if (auto* w = qobject_cast<QTextEdit*>(focusWidget()))      w->cut();
    });

    QAction* copyAction = editMenu->addAction(menuIcon("edit-copy", QStyle::SP_FileIcon), "Copy");
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (auto* w = qobject_cast<QLineEdit*>(focusWidget()))           w->copy();
        else if (auto* w = qobject_cast<QPlainTextEdit*>(focusWidget())) w->copy();
        else if (auto* w = qobject_cast<QTextEdit*>(focusWidget()))      w->copy();
    });

    QAction* pasteAction = editMenu->addAction(menuIcon("edit-paste", QStyle::SP_FileDialogContentsView), "Paste");
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, [this]() {
        if (auto* w = qobject_cast<QLineEdit*>(focusWidget()))           w->paste();
        else if (auto* w = qobject_cast<QPlainTextEdit*>(focusWidget())) w->paste();
        else if (auto* w = qobject_cast<QTextEdit*>(focusWidget()))      w->paste();
    });

    QAction* selectAllAction = editMenu->addAction(menuIcon("edit-select-all", QStyle::SP_FileDialogListView), "Select All");
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this]() {
        if (auto* w = qobject_cast<QLineEdit*>(focusWidget()))           w->selectAll();
        else if (auto* w = qobject_cast<QPlainTextEdit*>(focusWidget())) w->selectAll();
        else if (auto* w = qobject_cast<QTextEdit*>(focusWidget()))      w->selectAll();
    });

    editMenu->addSeparator();
    QAction* copyCodeAction = new QAction(menuIcon("edit-copy", QStyle::SP_FileIcon), "Copy Code", this);
    copyCodeAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    connect(copyCodeAction, &QAction::triggered, this, &MainWindow::onCopyCode);
    editMenu->addAction(copyCodeAction);

    editMenu->addSeparator();
    QAction* findAction = new QAction(menuIcon("edit-find-replace", QStyle::SP_FileDialogDetailedView), "Find / Replace", this);
    findAction->setShortcut(QKeySequence::Find);   // Ctrl+F
    connect(findAction, &QAction::triggered, this, &MainWindow::onShowFindBar);
    editMenu->addAction(findAction);

    auto* caseMenu = new DropdownMenu("Convert Case", editMenu);
    caseMenu->setIcon(menuIcon("format-text-bold", QStyle::SP_FileIcon));
    editMenu->addMenu(caseMenu);
    const auto addCaseAction = [this, caseMenu](const QString& name,
                                                QString (*convert)(const QString&)) {
        caseMenu->addAction(name, codeEditor, [this, convert]() {
            codeEditor->convertCase(convert);
        });
    };
    addCaseAction("Title Case", titleCase);
    addCaseAction("Upper Case", [](const QString& text) { return text.toUpper(); });
    addCaseAction("Lower Case", [](const QString& text) { return text.toLower(); });
    addCaseAction("Swap Case", swapCase);
    addCaseAction("lowerCamelCase", [](const QString& text) { return identifierCase(text, false); });
    addCaseAction("UpperCamelCase", [](const QString& text) { return identifierCase(text, true); });

    editMenu->addSeparator();
    auto* occurrenceAction = editMenu->addAction(
        menuIcon("edit-find", QStyle::SP_FileDialogDetailedView), "Select Next Occurrence",
        codeEditor, &CodeEditor::selectNextOccurrence);
    occurrenceAction->setShortcut(QKeySequence("Ctrl+D"));

    auto* splitSelectionAction = editMenu->addAction(
        menuIcon("edit-select-all", QStyle::SP_FileDialogListView), "Split Selection into Lines",
        codeEditor, &CodeEditor::splitSelectionIntoLines);
    splitSelectionAction->setShortcut(QKeySequence("Ctrl+Shift+L"));
    codeEditor->addAction(occurrenceAction);
    codeEditor->addAction(splitSelectionAction);
    occurrenceAction->setShortcutContext(Qt::WidgetShortcut);
    splitSelectionAction->setShortcutContext(Qt::WidgetShortcut);

    editMenu->addSeparator();
    const auto addSortAction = [this, editMenu](const QString& label, const QKeySequence& shortcut,
                                               Qt::CaseSensitivity sensitivity) {
        auto* action = editMenu->addAction(menuIcon("view-sort-ascending", QStyle::SP_ArrowUp),
                                           label, codeEditor, [this, sensitivity]() {
            codeEditor->sortLines(sensitivity);
        });
        action->setShortcut(shortcut);
        action->setShortcutContext(Qt::WidgetShortcut);
        codeEditor->addAction(action);
    };
    addSortAction("Sort Lines", QKeySequence("F9"), Qt::CaseInsensitive);
    addSortAction("Sort Lines (Case Sensitive)", QKeySequence("Ctrl+F9"), Qt::CaseSensitive);

    QMenu* snippetMenu = addDropdown(menuBar, "Snippet");
    QAction* newSnippetAction = new QAction(menuIcon("document-new", QStyle::SP_FileIcon), "New Snippet", this);
    newSnippetAction->setShortcut(QKeySequence::New);   // Ctrl+N
    connect(newSnippetAction, &QAction::triggered, this, &MainWindow::onNewSnippet);
    snippetMenu->addAction(newSnippetAction);

    QAction* saveAction = new QAction(menuIcon("document-save", QStyle::SP_DialogSaveButton), "Save Snippet", this);
    saveAction->setShortcut(QKeySequence::Save);   // Ctrl+S
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveSnippet);
    snippetMenu->addAction(saveAction);

    QAction* cloneAction = new QAction(menuIcon("edit-copy", QStyle::SP_FileIcon), "Clone Snippet", this);
    cloneAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
    connect(cloneAction, &QAction::triggered, this, &MainWindow::onCloneSnippet);
    snippetMenu->addAction(cloneAction);

    snippetMenu->addSeparator();
    snippetMenu->addAction(menuIcon("document-import", QStyle::SP_ArrowDown), "Import Snippet…", this, &MainWindow::onImportSnippet);
    snippetMenu->addAction(menuIcon("document-export", QStyle::SP_ArrowUp), "Export Snippet…", this, &MainWindow::onExportSnippet);
    snippetMenu->addAction(menuIcon("folder-download", QStyle::SP_DirIcon), "Export Folder…", this, &MainWindow::onExportFolder);

    snippetMenu->addSeparator();
    QAction* favAction = new QAction(favoriteMenuIcon(), "Toggle Favorite", this);
    connect(favAction, &QAction::triggered, this, &MainWindow::onToggleFavorite);
    snippetMenu->addAction(favAction);

    QMenu* toolsMenu = addDropdown(menuBar, "Tools");
    const auto addCodeTool = [this, toolsMenu](const QIcon& icon, const QString& name,
                                               QString (*transform)(const QString&),
                                               QString (*report)(const QString&, const QString&)) {
        toolsMenu->addAction(icon, name, this, [this, name, transform, report]() {
            if (currentSnippetID < 0) {
                showPopup(this, name, "Select a snippet first.");
                return;
            }
            const QString original = codeEditor->toPlainText();
            const QString updated = transform(original);
            QString message = report(original, updated);
            if (updated != original) {
                QTextCursor cursor = codeEditor->textCursor();
                const int position = cursor.position();
                cursor.beginEditBlock();
                cursor.select(QTextCursor::Document);
                cursor.insertText(updated);
                cursor.endEditBlock();
                cursor.setPosition(qMin(position, int(updated.size())));
                codeEditor->setTextCursor(cursor);
                message += "\n\nReview the code, then click Save Script to keep the changes. "
                           "Use Ctrl+Z to undo them.";
            }
            showPopup(this, name, message);
        });
    };
    addCodeTool(menuIcon("document-edit", QStyle::SP_FileIcon),
                "Prettify Code", prettifyCodeIndentation,
                [](const QString& original, const QString& updated) {
                    return original == updated ? QString("The indentation already matches the format.")
                                               : QString("Code formatting completed successfully.");
                });
    addCodeTool(menuIcon("accessories-character-map", QStyle::SP_FileIcon),
                "Convert Non-ASCII to ASCII", convertCodeToAscii,
                [](const QString& original, const QString&) {
                    const auto characters = original.toUcs4();
                    const auto count = std::count_if(characters.cbegin(), characters.cend(),
                                                     [](char32_t c) { return c > 0x7f; });
                    return count == 1 ? QString("Converted 1 non-ASCII character.")
                                      : QString("Converted %1 non-ASCII characters.").arg(count);
                });
    addCodeTool(menuIcon("format-indent-more", QStyle::SP_ArrowRight),
                "Tab to Spaces", expandCodeTabs,
                [](const QString& original, const QString&) {
                    const auto count = original.count(u'\t');
                    return count == 1 ? QString("Replaced 1 tab with spaces.")
                                      : QString("Replaced %1 tabs with spaces.").arg(count);
                });
    addCodeTool(menuIcon("edit-clear", QStyle::SP_DialogResetButton),
                "Remove Hidden Characters", removeHiddenCodeCharacters,
                [](const QString& original, const QString& updated) {
                    const auto count = original.size() - updated.size();
                    return count == 1 ? QString("Removed 1 hidden character.")
                                      : QString("Removed %1 hidden characters.").arg(count);
                });

    QMenu* optionsMenu = addDropdown(menuBar, "Options");

    darkModeAction = new QAction(this);
    connect(darkModeAction, &QAction::triggered, this, &MainWindow::onToggleDarkMode);
    optionsMenu->addAction(darkModeAction);

    auto* schemeMenu = new DropdownMenu("Color Schemes", optionsMenu);
    schemeMenu->setIcon(menuIcon("preferences-desktop-theme", QStyle::SP_DesktopIcon));
    optionsMenu->addMenu(schemeMenu);
    colorSchemeActions = new QActionGroup(this);
    for (int i = 0; i < int(std::size(colorSchemes)); ++i) {
        auto* action = schemeMenu->addAction(colorSchemes[i].name);
        action->setCheckable(true);
        action->setData(i);
        colorSchemeActions->addAction(action);
    }
    connect(colorSchemeActions, &QActionGroup::triggered, this, [this](QAction* action) {
        DatabaseManager::instance().saveSetting("ColorScheme", action->text());
        applyThemeStyles();
    });

    QAction* incFontAction = new QAction("Increase Font Size", this);
    incFontAction->setShortcut(QKeySequence::ZoomIn);    // Ctrl++
    connect(incFontAction, &QAction::triggered, this, &MainWindow::onIncreaseFont);
    addAction(incFontAction);

    QAction* decFontAction = new QAction("Decrease Font Size", this);
    decFontAction->setShortcut(QKeySequence::ZoomOut);   // Ctrl+-
    connect(decFontAction, &QAction::triggered, this, &MainWindow::onDecreaseFont);
    addAction(decFontAction);

    QAction* resetFontAction = new QAction("Reset Font Size", this);
    resetFontAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetFontAction, &QAction::triggered, this, &MainWindow::onResetFont);
    addAction(resetFontAction);

    auto* fontMenu = new DropdownMenu("Font Style", optionsMenu);
    fontMenu->setIcon(menuIcon("preferences-desktop-font", QStyle::SP_FileIcon));
    optionsMenu->addMenu(fontMenu);

    auto* fonts = new QActionGroup(fontMenu);
    QFont monospace("Monospace");
    monospace.setStyleHint(QFont::Monospace);
    const QString defaultFamily = QFontInfo(monospace).family();
    auto* defaultFont = fontMenu->addAction(QStringLiteral("Default — %1").arg(defaultFamily));
    defaultFont->setCheckable(true);
    defaultFont->setData("Monospace");
    fonts->addAction(defaultFont);
    defaultFont->setChecked(true);

    const QString savedFont = DatabaseManager::instance().getSetting("EditorFontFamily", "Monospace");
    const QStringList installedFonts = QFontDatabase::families();
    const QStringList codingFonts = {
        "Cascadia Mono", "DejaVu Sans Mono", "Fira Code", "Hack", "IBM Plex Mono",
        "Inconsolata", "JetBrains Mono", "Liberation Mono", "Noto Sans Mono", "Source Code Pro"
    };
    for (const auto& family : codingFonts) {
        if (family == defaultFamily || !installedFonts.contains(family) ||
            !QFontDatabase::isFixedPitch(family))
            continue;
        auto* action = fontMenu->addAction(family);
        action->setCheckable(true);
        action->setData(family);
        fonts->addAction(action);
        if (family == savedFont) {
            editorFontFamily = family;
            action->setChecked(true);
        }
    }
    connect(fonts, &QActionGroup::triggered, this, [this](QAction* action) {
        editorFontFamily = action->data().toString();
        DatabaseManager::instance().saveSetting("EditorFontFamily", editorFontFamily);
        applyEditorFont();
    });

    fontMenu->addSeparator();
    fontMenu->addAction(menuIcon("edit-undo", QStyle::SP_DialogResetButton), "Reset Font",
                        this, [this, defaultFont]() {
        editorFontFamily = defaultFont->data().toString();
        defaultFont->setChecked(true);
        DatabaseManager::instance().saveSetting("EditorFontFamily", editorFontFamily);
        onResetFont();
    });

    auto* fontSizeMenu = new DropdownMenu("Font Size", optionsMenu);
    fontSizeMenu->setIcon(menuIcon("format-font-size", QStyle::SP_FileIcon));
    optionsMenu->addMenu(fontSizeMenu);
    fontSizeMenu->addAction(menuIcon("list-add", QStyle::SP_ArrowUp), "Increase", this, &MainWindow::onIncreaseFont);
    fontSizeMenu->addAction(menuIcon("edit-undo", QStyle::SP_DialogResetButton), "Reset", this, &MainWindow::onResetFont);
    fontSizeMenu->addAction(menuIcon("list-remove", QStyle::SP_ArrowDown), "Decrease", this, &MainWindow::onDecreaseFont);

    auto* lineHeightMenu = new DropdownMenu("Code Line Height", optionsMenu);
    lineHeightMenu->setIcon(menuIcon("format-line-spacing", QStyle::SP_FileDialogListView));
    optionsMenu->addMenu(lineHeightMenu);
    codeLineSpacing = qBound(0, DatabaseManager::instance().getSetting("CodeLineSpacing", "2").toInt(), 16);
    codeEditor->setLineSpacing(codeLineSpacing);
    lineSpacingButton->setText(QStringLiteral("%1 px").arg(codeLineSpacing));
    lineHeightMenu->addAction(menuIcon("list-add", QStyle::SP_ArrowUp), "Increase", this,
                              [this]() { setCodeLineSpacing(codeLineSpacing + 2); });
    lineHeightMenu->addAction(menuIcon("edit-undo", QStyle::SP_DialogResetButton), "Reset", this,
                              [this]() { setCodeLineSpacing(2); });
    lineHeightMenu->addAction(menuIcon("list-remove", QStyle::SP_ArrowDown), "Decrease", this,
                              [this]() { setCodeLineSpacing(codeLineSpacing - 2); });

    optionsMenu->addSeparator();
    const QString previousVisibility = DatabaseManager::instance().getSetting("ShowSnippetDetails", "on");
    const QIcon eyeIcon = eyeMenuIcon();
    const auto addVisibilityAction = [this, optionsMenu, eyeIcon, previousVisibility](
                                         QWidget* panel, const QString& name, const QString& setting) {
        auto* action = optionsMenu->addAction(eyeIcon, QString());
        const auto applyVisibility = [panel, action, name](bool visible) {
            panel->setVisible(visible);
            action->setText((visible ? "Hide " : "View ") + name);
        };
        applyVisibility(DatabaseManager::instance().getSetting(setting, previousVisibility) == "on");
        connect(action, &QAction::triggered, this, [panel, setting, applyVisibility]() {
            const bool visible = panel->isHidden();
            applyVisibility(visible);
            DatabaseManager::instance().saveSetting(setting, visible ? "on" : "off");
        });
    };
    addVisibilityAction(descField->parentWidget(), "Description", "ShowSnippetDescription");
    addVisibilityAction(noteEditor->parentWidget(), "Notes", "ShowSnippetNotes");

    optionsMenu->addSeparator();
    QAction* resetLayoutAction = new QAction(menuIcon("view-restore", QStyle::SP_TitleBarNormalButton), "Reset Window Size", this);
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::onResetWindowSize);
    optionsMenu->addAction(resetLayoutAction);

    optionsMenu->addSeparator();
    auto* databaseMenu = new DropdownMenu("Database", optionsMenu);
    databaseMenu->setIcon(menuIcon("drive-harddisk", QStyle::SP_DriveHDIcon));
    optionsMenu->addMenu(databaseMenu);
    databaseMenu->addAction(menuIcon("document-save-as", QStyle::SP_DialogSaveButton), "Backup Database…", this, &MainWindow::onBackupDatabase);
    databaseMenu->addAction(menuIcon("document-import", QStyle::SP_ArrowDown), "Import Database…", this, &MainWindow::onImportDatabase);

    QMenu* helpMenu = addDropdown(menuBar, "Help");
    helpMenu->addAction(menuIcon("drive-harddisk", QStyle::SP_DriveHDIcon), "Storage Location", this, [this](){
        PopupDialog dlg(this, "Data Storage");
        dlg.setMinimumWidth(600);
        dlg.resize(600, 320);
        QVBoxLayout* layout = dlg.body;
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(8);
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
    helpMenu->addAction(menuIcon("image-x-generic", QStyle::SP_FileIcon), "Screenshots…", this, [this]() {
        PopupDialog dialog(this, "SafeScript Screenshots");
        auto* tabs = new QTabWidget;
        const QSize available = screen()->availableGeometry().size();
        const QSize previewSize(qMax(1, qMin(1100, available.width() - 72)),
                                qMax(1, available.height() - 140));
        const auto addPreview = [tabs, previewSize](const QString& title, const QString& path) {
            auto* preview = new QLabel;
            preview->setAlignment(Qt::AlignCenter);
            preview->setAccessibleName(title + " screenshot");
            preview->setPixmap(QPixmap(path).scaled(previewSize, Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));
            tabs->addTab(preview, title);
        };
        addPreview("Light Mode", ":/screenshot-light.png");
        addPreview("Dark Mode", ":/screenshot-dark.png");
        addPreview("Midnight Blue", ":/screenshot-midnightblue.png");
        addPreview("Forest", ":/screenshot-forest.png");
        addPreview("Nord", ":/screenshot-nord.png");
        tabs->setCurrentIndex(darkModeEnabled ? 1 : 0);
        dialog.body->addWidget(tabs);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.body->addWidget(buttons);
        dialog.exec();
    });
    helpMenu->addSeparator();
    helpMenu->addAction(menuIcon("internet-web-browser", QStyle::SP_DriveNetIcon), "Project Website", this, []() {
        QDesktopServices::openUrl(QUrl("https://www.brainscanmedia.com/safescript.php"));
    });
    helpMenu->addAction(menuIcon("flag", QStyle::SP_MessageBoxWarning), "Report an Issue", this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/BrainScanMedia/SafeScript/issues"));
    });
    helpMenu->addAction(menuIcon("text-x-script", QStyle::SP_FileIcon), "Browse Source Code", this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/BrainScanMedia/SafeScript"));
    });
    helpMenu->addAction(menuIcon("system-software-install", QStyle::SP_DriveNetIcon), "Flathub Website", this, []() {
        QDesktopServices::openUrl(QUrl("https://flathub.org/en/apps/com.brainscanmedia.SafeScript"));
    });
    helpMenu->addSeparator();
    helpMenu->addAction(menuIcon("internet-web-browser", QStyle::SP_DriveNetIcon),
                        "Visit Author's Website", this, []() {
        QDesktopServices::openUrl(QUrl("https://www.brainscanmedia.com/"));
    });
    helpMenu->addSeparator();
    helpMenu->addAction(menuIcon("help-about", QStyle::SP_MessageBoxInformation), "About SafeScript", this, [this]() {
        PopupDialog dlg(this, "About SafeScript");
        dlg.setMinimumWidth(320);
        dlg.resize(320, 280);
        QVBoxLayout* layout = dlg.body;
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(8);

        QLabel* iconLabel = new QLabel;
        iconLabel->setPixmap(QIcon(":/safescript.png").pixmap(72, 72));
        iconLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(iconLabel);

        QLabel* text = new QLabel(
            "<b>SafeScript v1.3.0</b><br>"
            "Designed &amp; Programmed By<br>"
            "Thomas J. Allen<br>"
            "Copyright 2026<br>"
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
        showPopup(this, "Backup Complete",
                                 "Your database was backed up successfully to:\n\n" + dest);
    } else {
        showPopup(this, "Backup Failed",
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
    if (showPopup(this, "Import Database",
                  "Importing will overwrite your current database. All folders and snippets "
                  "will be replaced by the contents of the selected file. This cannot be undone.\n\n"
                  "Consider backing up your current database first.\n\nContinue?",
                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;

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
        showPopup(this, "Import Complete",
                                 "The database was imported successfully.");
    } else {
        showPopup(this, "Import Failed",
                             err.isEmpty() ? "The database could not be imported." : err);
    }
}

// ── Option Slots ──────────────────────────────────────────────
void MainWindow::onToggleWrap() {
    wrapEnabled = wrapCheck->isChecked();
    codeEditor->setLineWrapMode(wrapEnabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    DatabaseManager::instance().saveSetting("WrapCode", wrapEnabled ? "on" : "off");
}

void MainWindow::onToggleDarkMode() {
    darkModeEnabled = !codeEditor->darkMode;
    if (auto* action = colorSchemeActions->checkedAction())
        action->setChecked(false);
    DatabaseManager::instance().saveSetting("ColorScheme", "");
    applyThemeStyles();
    DatabaseManager::instance().saveSetting("DarkMode", darkModeEnabled ? "on" : "off");
}

void MainWindow::onToggleLineNumbers() {
    lineNumbersEnabled = lineNumberCheck->isChecked();
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
    const QString name = promptPopupText(this, "New Folder", "Folder Name:", "");
    if (!name.isEmpty()) {
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

    PopupDialog dlg(this, "Delete Folder", QStyle::SP_MessageBoxQuestion);
    dlg.setMinimumWidth(360);
    dlg.resize(360, 130);
    QVBoxLayout* layout = dlg.body;
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
    const QString newName = promptPopupText(this, "Rename Folder", "New Name:", item->text());
    if (!newName.isEmpty()) {
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

    PopupDialog dlg(this, "Delete Snippet", QStyle::SP_MessageBoxQuestion);
    dlg.setMinimumWidth(360);
    dlg.resize(360, 130);
    QVBoxLayout* layout = dlg.body;
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

    showPopup(this, "Saved", "Snippet saved successfully.");
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
    codeEditor->clearMultipleCursors();
    btnFavorite->setEnabled(false);
    updateFavoriteButton(false);
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
    codeEditor->clearMultipleCursors();
    btnFavorite->setEnabled(true);
    updateFavoriteButton(s.favorite);
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
    return showPopup(
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
void MainWindow::setCodeLineSpacing(int pixels) {
    codeLineSpacing = qBound(0, pixels, 16);
    codeEditor->setLineSpacing(codeLineSpacing);
    lineSpacingButton->setText(QStringLiteral("%1 px").arg(codeLineSpacing));
    DatabaseManager::instance().saveSetting("CodeLineSpacing", QString::number(codeLineSpacing));
}

void MainWindow::applyEditorFont() {
    fontSizeButton->setText(QString::number(editorFontSize));
    QFont f(editorFontFamily, editorFontSize);
    f.setStyleHint(QFont::Monospace);
    titleField->setFont(f);
    descField->setFont(f);
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

void MainWindow::updateFavoriteButton(bool favorite) {
    btnFavorite->setText(favorite ? "★ Favorite" : "☆ Favorite");
    btnFavorite->setToolTip(favorite ? "Remove from favorites" : "Add to favorites");
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
    updateFavoriteButton(newFav);
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

    DropdownMenu menu(QString(), this);
    QAction* favAct    = menu.addAction(favoriteMenuIcon(), s->favorite ? "Unfavorite" : "Favorite ★");
    QAction* cloneAct  = menu.addAction(menuIcon("edit-copy", QStyle::SP_FileIcon), "Clone");

    auto* moveMenu = new DropdownMenu("Move To", &menu);
    moveMenu->setIcon(menuIcon("folder-move", QStyle::SP_DirIcon));
    menu.addMenu(moveMenu);
    QList<QAction*> moveActions;
    for (int i = 0; i < folderList->count(); ++i) {
        int fid = folderList->item(i)->data(Qt::UserRole).toInt();
        if (fid == currentFolderID) continue;
        QAction* a = moveMenu->addAction(menuIcon("folder", QStyle::SP_DirIcon), folderList->item(i)->text());
        a->setData(fid);
        moveActions.append(a);
    }
    if (moveActions.isEmpty()) {
        QAction* none = moveMenu->addAction(menuIcon("folder", QStyle::SP_DirIcon), "(no other folders)");
        none->setEnabled(false);
    }

    QAction* exportAct = menu.addAction(menuIcon("document-export", QStyle::SP_ArrowUp), "Export Snippet…");
    menu.addSeparator();
    QAction* delAct = menu.addAction(menuIcon("edit-delete", QStyle::SP_TrashIcon), "Delete");

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
        showPopup(this, "Export Failed", "Could not write to:\n" + dest);
}

void MainWindow::onExportFolder() {
    if (currentFolderID < 0) {
        showPopup(this, "Export Folder", "Select a folder first.");
        return;
    }
    if (currentSnippets.isEmpty()) {
        showPopup(this, "Export Folder", "This folder has no snippets to export.");
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
int MainWindow::createSnippetFromFile(const QString& path) {
    QFileInfo fileInfo(path);
    if (!fileInfo.isFile() || fileInfo.size() > 5 * 1024 * 1024)
        return -1;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return -1;
    QTextStream stream(&file);
    const QString text = stream.readAll();
    if (file.error() != QFileDevice::NoError) return -1;

    return DatabaseManager::instance().insertSnippet(currentFolderID, fileInfo.fileName(), text);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (hasLocalFiles(event->mimeData()))
        event->acceptProposedAction();
}

void MainWindow::onImportSnippet() {
    if (currentFolderID < 0) {
        showPopup(this, "Import Snippet",
                                 "Select a category before importing snippets.");
        return;
    }

    const QStringList paths = QFileDialog::getOpenFileNames(this, "Import Snippet");
    importSnippetFiles(paths);
}

void MainWindow::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    if (importSnippetFiles(paths))
        event->acceptProposedAction();
}

bool MainWindow::importSnippetFiles(const QStringList& paths) {
    if (paths.isEmpty()) return false;
    if (currentFolderID < 0) {
        showPopup(this, "Import Snippet",
                                 "Select a category before importing snippets.");
        return false;
    }

    // Protect any unsaved edits before we start replacing the list selection.
    if (!maybeSave()) return false;

    int created = 0;
    int lastSnippetID = -1;
    QStringList failed;
    for (const QString& path : paths) {
        const int id = createSnippetFromFile(path);
        if (id < 0) {
            failed.append(QFileInfo(path).fileName());
            continue;
        }
        lastSnippetID = id;
        ++created;
    }
    if (created > 0)
        loadSnippets(currentFolderID, lastSnippetID);

    QString message = QString("Imported %1 snippet(s)").arg(created);
    if (!failed.isEmpty())
        message += ". Could not import: " + failed.join(", ");
    statusBar()->showMessage(message, 5000);
    return created > 0;
}

void MainWindow::updateWindowCorners() {
    QWidget* header = menuWidget();
    QStatusBar* footer = statusBar();
    if (isMaximized() || isFullScreen()) {
        clearMask();
        if (header) header->clearMask();
        footer->clearMask();
        return;
    }

    const QRegion shape = roundedWindowRegion(rect());
    setMask(shape);

    // Clip the painted header and footer as well as the top-level window.
    if (header) header->setMask(shape);
    footer->setMask(shape.translated(0, footer->height() - height()));
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateWindowCorners();
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        updateWindowCorners();
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
