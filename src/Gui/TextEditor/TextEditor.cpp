/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *   Copyright (c) 2019 Fredrik Johansson github.com/mumme74               *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
# include <QKeyEvent>
# include <QPainter>
# include <QShortcut>
# include <QTextCursor>
#endif

#include "TextEditor.h"
#include "SyntaxHighlighter.h"
#include "BitmapFactory.h"
#include "LangPluginBase.h"
#include "EditorView.h"

#include <QScrollBar>
#include <QStyleOptionSlider>
#include <QCompleter>
#include <QMimeDatabase>
#include <QFileInfo>
#include <QToolTip>


#include <QDebug>


#define ITERATE_CODE_PLUGINS(METHOD) \
    for (auto plugin : view()->currentPlugins()) { \
        auto codePlug = dynamic_cast<AbstractLangPluginCode*>(plugin); \
        if (codePlug && codePlug->METHOD(this)) \
            return; \
    }

using namespace Gui;

namespace Gui {
class LineMarkerAreaP
{
public:
    LineMarkerAreaP() :
        lineNumberActive(true)
    { }
    ~LineMarkerAreaP() {}
    TextEditor *textEditor;
    QPixmap bookmarkIcon;
    bool lineNumberActive;
    void loadIcons(int rowHeight)
    {
        bookmarkIcon = BitmapFactory().iconFromTheme("bookmark").pixmap(rowHeight, rowHeight);
    }
};

class AnnotatedScrollBarP
{
public:
    AnnotatedScrollBarP(TextEditor *editor) :
        editor(editor)
    {  }
    ~AnnotatedScrollBarP() {}
    QMultiHash<int, QColor> markers;
    TextEditor *editor;
};

struct TextEditorP
{
    //QMap<QString, QColor> colormap; // Color map
    QHash<QString, QList<QTextEdit::ExtraSelection> > extraSelections;
    QCompleter *completer;
    QString filename,
            mime;
    EditorViewWrapper *wrapper;
    SyntaxHighlighter* highlighter;
    LineMarkerArea* lineNumberArea;
    // TODO this should be changeable through color dialog
    const QColor bookmarkScrollBarMarkerColor = QColor(72, 108, 165); // blue-ish
    int lastSavedRevision;
    bool showIndentMarkers,
         showLongLineMarker;

    TextEditorP(TextEditor *owner)
        : completer(nullptr)
        , wrapper(nullptr)
        , highlighter(new SyntaxHighlighter(owner))
        , lastSavedRevision(0)
        , showIndentMarkers(true)
        , showLongLineMarker(false)
    {
    }
    ~TextEditorP()
    {
        highlighter->deleteLater();
    }
};

} // namespace Gui

/**
 *  Constructs a TextEditor which is a child of 'parent' and does the
 *  syntax highlighting for the Python language.
 */
TextEditor::TextEditor(QWidget* parent)
    : QPlainTextEdit(parent), WindowParameter("Editor")
    , d(new TextEditorP(this))
{
    d->lineNumberArea = new LineMarkerArea(this);

    setSyntaxHighlighter(d->highlighter);

    // replace scrollbar to a annotated one
    setVerticalScrollBar(new AnnotatedScrollBar(this));

    QFont serifFont(QLatin1String("Courier"), 10, QFont::Normal);
    setFont(serifFont);
    d->lineNumberArea->setFont(serifFont);
    d->lineNumberArea->fontSizeChanged();

    ParameterGrp::handle hPrefGrp = getWindowParameter();
    hPrefGrp->Attach( this );

    // set colors and font
    hPrefGrp->NotifyAll();

    connect(this, SIGNAL(cursorPositionChanged()),
            this, SLOT(highlightSelections()));
    connect(this, SIGNAL(blockCountChanged(int)),
            this, SLOT(updateLineNumberAreaWidth(int)));
    connect(this, SIGNAL(updateRequest(const QRect &, int)),
            this, SLOT(updateLineNumberArea(const QRect &, int)));


    connect(lineMarkerArea(), SIGNAL(contextMenuOnLine(int, QContextMenuEvent*)),
            this, SLOT(markerAreaContextMenu(int, QContextMenuEvent*)));
    updateLineNumberAreaWidth(0);
    highlightSelections();
}

/** Destroys the object and frees any allocated resources */
TextEditor::~TextEditor()
{
    getWindowParameter()->Detach(this);
    delete d;
}

void TextEditor::highlightText(int pos, int len, const QTextCharFormat format)
{
    QList<QTextEdit::ExtraSelection> selections = extraSelections();

    QTextEdit::ExtraSelection selection;
    selection.format = format;

    QTextCursor cursor = textCursor();
    cursor.setPosition(pos);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, len);
    selection.cursor = cursor;

    selections.append(selection);
    setExtraSelections(selections);
}

int TextEditor::lineNumberAreaWidth()
{
    return d->lineNumberArea->sizeHint().width();
}

int TextEditor::findAndHighlight(const QString needle, QTextDocument::FindFlags flags)
{
    QList<QTextEdit::ExtraSelection> selections;


    QList<int> found;
    if (needle.size()) {
        QTextCharFormat format;
        format.setForeground(QColor(QLatin1String("#110059")));
        format.setBackground(QColor(QLatin1String("#f7f74f")));

        QTextDocument *doc = document();
        QTextCursor cursor = doc->find(needle, 0, flags);

        while (!cursor.isNull()) {
            QTextEdit::ExtraSelection selection;
            selection.format = format;
            selection.cursor = cursor;
            selections.append(selection);
            found.append(cursor.blockNumber());
            cursor = doc->find(needle, cursor, flags);
        }
    }

    setTextMarkers(QLatin1String("find"), selections);
    AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
    if (vBar) {
        static const QColor searchMarkerScrollbar = QColor(65, 209, 60); // green
        vBar->resetMarkers(found, searchMarkerScrollbar);

    }
    return found.size();
}

int TextEditor::findText(const QString find)
{
    if (!find.size())
        return 0;

    QTextCharFormat format;
    format.setForeground(QColor(QLatin1String("#110059")));
    format.setBackground(QColor(QLatin1String("#fff356")));

    int found = 0;
    for (QTextBlock block = document()->begin();
         block.isValid(); block = block.next())
    {
        int pos = block.text().indexOf(find);
        if (pos > -1) {
            ++found;
            highlightText(block.position() + pos, find.size(), format);
        }
    }

    return found;
}

void TextEditor::setTextMarkers(QString key, QList<QTextEdit::ExtraSelection> selections)
{
    static const QString highlightLineKey(QLatin1String("highlightCurrentLine"));

    // store the new value
    if (selections.size() == 0)
        d->extraSelections.remove(key);
    else
        d->extraSelections.insert(key, selections);

    // show the new value including the old stored ones
     QList<QTextEdit::ExtraSelection> show;
     for (QString &key : d->extraSelections.keys()) {
         if (key != highlightLineKey)
            for (QTextEdit::ExtraSelection &sel: d->extraSelections[key])
                show.append(sel);
     }

     if (d->extraSelections.keys().contains(highlightLineKey))
         show.prepend(d->extraSelections[highlightLineKey][0]);

     setExtraSelections(show);
}

void TextEditor::setFilename(const QString &fn)
{
    if (fn != d->filename) {
        QFileInfo oldFi(d->filename);

        d->filename = QFileInfo(fn).absoluteFilePath();

        QFileInfo fi(fn);
        if (!fn.isEmpty() && fi.suffix() != fi.baseName() && fi.suffix() != oldFi.suffix()) {
            QMimeDatabase db;
            QMimeType type = db.mimeTypeForFile(fn);
            if (type.isValid())
                d->mime = type.name();
        }

        auto hl = dynamic_cast<SyntaxHighlighter*>(d->highlighter);
        if (hl)
            d->highlighter->setFilepath(fn);
        Q_EMIT filenameChanged(fn);
    }
}

QString TextEditor::filename() const
{
    return d->filename;
}

bool TextEditor::setSyntax(const QString &defName)
{
    return d->highlighter->setSyntax(defName);
}

void TextEditor::cut()
{
    // let plugins know
    if (view()) {
        for (auto plugin : view()->currentPlugins())
            plugin->onCut(this);
    }
}

void TextEditor::copy()
{
    // let plugins know
    if (view()) {
        for (auto plugin : view()->currentPlugins())
            plugin->onCopy(this);
    }
}

void TextEditor::paste()
{
    // let plugins know
    if (view()) {
        for (auto plugin : view()->currentPlugins())
            plugin->onPaste(this);
    }
}

QString TextEditor::syntax() const
{
    return d->highlighter->syntax();
}

const QString &TextEditor::mimetype() const
{
    return d->mime;
}

void TextEditor::setMimetype(QString mime)
{
    d->mime = mime;
    if (d->wrapper) {
        if (d->wrapper->mimetype() != mime)
            d->wrapper->setMimetype(mime);

        if (d->wrapper->view())
            d->wrapper->view()->refreshLangPlugins();
    }
}

EditorViewWrapper *TextEditor::wrapper() const
{
    return d->wrapper;
}

void TextEditor::setWrapper(EditorViewWrapper *wrapper)
{
    d->wrapper = wrapper;
}

EditorView *TextEditor::view() const
{
    if (d->wrapper)
        return d->wrapper->view();
    return nullptr;
}

void TextEditor::setCompleter(QCompleter *completer) const
{
    d->completer = completer;
    d->completer->setWidget(const_cast<TextEditor*>(this));
}

QCompleter *TextEditor::completer() const
{
    return d->completer;
}

void TextEditor::onSave()
{
    QTextBlock block = document()->firstBlock();
    int maxRev = 0;
    while (block.isValid()) {
        if (block.revision() > maxRev)
            maxRev = block.revision();
        block = block.next();
    }

    if (maxRev > d->lastSavedRevision)
        d->lastSavedRevision = maxRev;

    repaint();
}

void TextEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void TextEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        d->lineNumberArea->scroll(0, dy);
    else
        d->lineNumberArea->update(0, rect.y(), d->lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void TextEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    d->lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void TextEditor::contextMenuEvent(QContextMenuEvent *e)
{
    auto menu = createStandardContextMenu(e->globalPos());

    auto cursor = cursorForPosition(e->globalPos());
    auto line = cursor.block().blockNumber();

    menu->addSeparator();
    if (d->highlighter) {
        // syntax selection
        auto hlActionGroup = new QActionGroup(menu);
        hlActionGroup->setExclusive(true);
        auto hlGroupMenu = menu->addMenu(QStringLiteral("Syntax"));
        QMenu *hlSubMenu = hlGroupMenu;
        QMap<QString, QMenu*> grpMenus;
        QString currentGroup;
        auto syntaxes = d->highlighter->syntaxes();
        for (const auto &name : syntaxes.keys()) {
            if (currentGroup != syntaxes[name] && syntaxes[name].size()) {
                currentGroup = syntaxes[name];
                hlSubMenu = grpMenus[currentGroup];
                if (!hlSubMenu) {
                    hlSubMenu = hlGroupMenu->addMenu(currentGroup);
                    grpMenus[currentGroup] = hlSubMenu;
                }
            }

            Q_ASSERT(hlSubMenu);
            QAction *action;
            if (syntaxes[name].size()) {
                action = hlSubMenu->addAction(name);
                hlActionGroup->addAction(action);
            } else {
                action = hlGroupMenu->addAction(name);
                hlGroupMenu->addAction(action);
            }
            action->setCheckable(true);
            action->setData(name);
            if (name == d->highlighter->syntax())
                action->setChecked(true);
        }
        connect(hlActionGroup, &QActionGroup::triggered, this, [this](QAction *action) {
            const auto name = action->data().toString();
            d->highlighter->setSyntax(name);
        });
    }

    // let plugins extend context menu
    if (d->wrapper && d->wrapper->view()) {
        for (auto plugin : d->wrapper->view()->currentPlugins()) {
            plugin->contextMenuTextArea(this, menu, d->filename, line);
        }
    }

    menu->exec(e->globalPos());
    delete menu;
}

LineMarkerArea *TextEditor::lineMarkerArea() const
{
    return d->lineNumberArea;
}

void TextEditor::highlightSelections()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly() && d->highlighter) {
        // highlight current line
        QTextEdit::ExtraSelection selection;
        QColor lineColor = d->highlighter->color(QLatin1String("color_Currentline")); //d->colormap[QLatin1String("Current line highlight")];
        int col = (lineColor.red() << 24) | (lineColor.green() << 16) | (lineColor.blue() << 8);
        ParameterGrp::handle hPrefGrp = getWindowParameter();
        unsigned long value = static_cast<unsigned long>(col);
        value = hPrefGrp->GetUnsigned( "Current line highlight", value);
        col = static_cast<int>(value);
        lineColor.setRgb((col>>24)&0xff, (col>>16)&0xff, (col>>8)&0xff);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setTextMarkers(QLatin1String("highlightCurrentLine"), extraSelections);
}

void TextEditor::markerAreaContextMenu(int line, QContextMenuEvent *event)
{
    setUpMarkerAreaContextMenu(line);
    QAction *res = m_markerAreaContextMenu.exec(event->globalPos());
    handleMarkerAreaContextMenu(res, line);
    event->accept();
}

void TextEditor::drawMarker(int line, int x, int y, QPainter* p)
{
    Q_UNUSED(line)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(p)
}

void TextEditor::setSyntaxHighlighter(SyntaxHighlighter* sh)
{
    if (sh) {
        sh->setDocument(this->document());
        if (d->highlighter && d->highlighter != sh)
            delete d->highlighter;
    }
    d->highlighter = sh;
}

SyntaxHighlighter *TextEditor::syntaxHighlighter() const
{
    return d->highlighter;
}

void TextEditor::keyPressEvent (QKeyEvent * e)
{
    // prevent these events from entering when showing suggestions
    if (d->completer && d->completer->popup()->isVisible()) {
        int key = e->key();
        if (key == Qt::Key_Enter || key == Qt::Key_Return || key == Qt::Key_Escape ||
            key == Qt::Key_Tab   || key == Qt::Key_Backtab)
        {
            e->ignore();
            return;
        }
    }

    // let our plugins know, view handles plugins
    if (view()) {
        switch (e->key()){
        case Qt::Key_Tab:
            ITERATE_CODE_PLUGINS(onTabPressed) break;
        case Qt::Key_Backtab:
            ITERATE_CODE_PLUGINS(onBacktabPressed) break;
        case Qt::Key_Delete:
            ITERATE_CODE_PLUGINS(onDelPressed) break;
        case Qt::Key_Backspace:
            ITERATE_CODE_PLUGINS(onBackspacePressed) break;
        case Qt::Key_Space:
            ITERATE_CODE_PLUGINS(onSpacePressed) break;
        case Qt::Key_Enter: case Qt::Key_Return:
            ITERATE_CODE_PLUGINS(onEnterPressed) break;
        case Qt::Key::Key_Escape:
            ITERATE_CODE_PLUGINS(onEscPressed) break;
        case Qt::Key_Period:
            ITERATE_CODE_PLUGINS(onPeriodPressed) break;
        case Qt::Key_ParenLeft:
            ITERATE_CODE_PLUGINS(onParenLeftPressed) break;
        case Qt::Key_ParenRight:
            ITERATE_CODE_PLUGINS(onParenRightPressed) break;
        case Qt::Key_BraceLeft:
            ITERATE_CODE_PLUGINS(onBraceLeftPressed) break;
        case Qt::Key_BraceRight:
            ITERATE_CODE_PLUGINS(onBraceRightPressed) break;
        case Qt::Key_BracketLeft:
            ITERATE_CODE_PLUGINS(onBracketLeftPressed) break;
        case Qt::Key_BracketRight:
            ITERATE_CODE_PLUGINS(onBracketRightPressed) break;
        case Qt::Key_At:
            ITERATE_CODE_PLUGINS(onAtPressed) break;
        case Qt::Key_QuoteDbl:
            ITERATE_CODE_PLUGINS(onQuoteDblPressed) break;
        case Qt::Key_Apostrophe:
            ITERATE_CODE_PLUGINS(onQuoteSglPressed) break;
        case Qt::Key_Ampersand:
            ITERATE_CODE_PLUGINS(onAmpersandPressed) break;
        case Qt::Key_Comma:
            ITERATE_CODE_PLUGINS(onCommaPressed) break;
        case Qt::Key_Colon:
            ITERATE_CODE_PLUGINS(onColonPressed) break;
        case Qt::Key_Semicolon:
            ITERATE_CODE_PLUGINS(onSemiColonPressed) break;
        default: ;
            if (view()) {
                for (auto plugin : view()->currentPlugins()) {
                    auto plug = dynamic_cast<AbstractLangPluginCode*>(plugin);
                    if (plug && plug->onKeyPress(this, e))
                        return;
                }
            }
        }
    }
    // if we get here all plugins have rejected this event
    QPlainTextEdit::keyPressEvent( e );
}

/** Sets the font, font size and tab size of the editor. */  
void TextEditor::OnChange(Base::Subject<const char*> &rCaller,const char* sReason)
{
    Q_UNUSED(rCaller)
    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (strcmp(sReason, "FontSize") == 0 || strcmp(sReason, "Font") == 0) {
        int fontSize = static_cast<int>(hPrefGrp->GetInt("FontSize", 10));
        QString fontFamily = QString::fromLatin1(hPrefGrp->GetASCII( "Font", "Courier" ).c_str());
        
        QFont font(fontFamily, fontSize);
        setFont(font);
        d->lineNumberArea->setFont(font);

        // resize linemarker icons
        d->lineNumberArea->fontSizeChanged();

    } else if (strncmp(sReason, "color_", 6) == 0) {
        if (d->highlighter)
            d->highlighter->loadSettings();

    } else if (strcmp(sReason, "TabSize") == 0 || strcmp(sReason, "FontSize") == 0) {
        int tabWidth = static_cast<int>(hPrefGrp->GetInt("TabSize", 4));
        QFontMetrics metric(font());
        int fontSize = metric.width(QLatin1String("0"));
        setTabStopWidth(tabWidth * fontSize);

    } else if (strncmp(sReason, "EnableIndentMarkers", 30) == 0) {
        d->showIndentMarkers = hPrefGrp->GetBool("EnableIndentMarkers", true);
        repaint();

    } else if (strncmp(sReason, "EnableLongLineMarker", 30) == 0) {
        d->showLongLineMarker = hPrefGrp->GetBool("EnableLongLineMarker", false);
        repaint();

    } else if (strncmp(sReason, "EnableLineWrap", 25) == 0) {
        // enable/disable linewrap mode in Editor from Edit->Preferences->Editor menu.
        QPlainTextEdit::LineWrapMode wrapMode;
        bool wrap = hPrefGrp->GetBool("EnableLineWrap", false);
        wrapMode = wrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap;
        setLineWrapMode(wrapMode);
        repaint();

    } else if (strncmp(sReason, "EnableLineNumber", 25) == 0) {
        // Enables/Disables Line number in the Macro Editor from Edit->Preferences->Editor menu.
        QRect cr = contentsRect();
        bool show = hPrefGrp->GetBool( "EnableLineNumber", true );
        if(show) {
            d->lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
        } else {
            d->lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), 20, cr.height()));
        }
        d->lineNumberArea->setLineNumbersVisible(show);
    }

    // let our plugins know
    if (view()) {
        for (auto plugin : view()->currentPlugins())
            plugin->OnChange(this, rCaller, sReason);

    }
}

void TextEditor::paintEvent(QPaintEvent *e)
{
    QPlainTextEdit::paintEvent(e);

    if (d->showLongLineMarker)
        paintTextWidthMarker(e);


    // TODO merge above into this algorithm, draw function called on line basis
    QPainter painter(viewport());
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    auto blockRectF = blockBoundingGeometry(block);
    auto offsetTopLeft = contentOffset();
    auto docMargins = static_cast<int>(document()->documentMargin());
    QRect rowRect = blockRectF.translated(offsetTopLeft).toRect();
    rowRect.moveLeft(viewport()->contentsMargins().left() + docMargins);
    rowRect.setRight(rowRect.right() - docMargins*2);
    int top = static_cast<int>(blockRectF.translated(offsetTopLeft).top());
    int bottom = top + static_cast<int>(blockRectF.height());

    while (block.isValid() && top <= e->rect().bottom()) {
        if (bottom >= e->rect().top()) {
            rowRect.moveTop(top);

            if (block.isVisible()) {
                if (d->showIndentMarkers)
                    paintIndentMarkers(&painter, block, rowRect);

                paintFoldedTextMarker(&painter, block, rowRect);

                // let plugins render there own stuff
                if (view()) {
                    painter.save();
                    for (auto plugin : view()->currentPlugins())
                        plugin->paintEventTextArea(this, &painter, block, rowRect);
                    painter.restore();
                }
            }
        }

        // advance  top-bottom visible boundaries for next row
        if (block.isVisible() || block.next().isVisible()) {
            blockRectF = blockBoundingRect(block.next());
            top = bottom;
            bottom = top + static_cast<int>(blockRectF.height());
        }

        block = block.next();
        ++blockNumber;
    }
}

bool TextEditor::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        QPoint point = helpEvent->pos();

        QString text; // this becomes the toolTip text if set
        bool ret;

        int line = d->lineNumberArea->lineFromPos(point);

        if (point.rx() < lineNumberAreaWidth()) {
            // on linenumberarea
            ret = lineMarkerAreaToolTipEvent(helpEvent->globalPos(), line, text);

        } else {

            // in editor window
            point.rx() -= lineNumberAreaWidth();
            QTextCursor cursor = cursorForPosition(point);
            cursor.select(QTextCursor::WordUnderCursor);
            if (cursor.hasSelection()) {
                int endPos = cursor.selectionEnd();
                int startPos = cursor.selectionStart();
                int pos = startPos;
                QChar ch;
                QTextDocument *doc = document();


                // find the root (leftmost char)
                do {
                    --pos;
                    ch = doc->characterAt(pos);
                } while(ch.isLetterOrNumber() ||
                        ch == QLatin1Char('.') ||
                        ch == QLatin1Char('_'));
                startPos = pos+1;

                // find the end (rightmost char)
                do {
                    ++pos;
                    ch = doc->characterAt(pos);
                } while(pos < endPos &&
                        (ch.isLetterOrNumber() ||
                        ch == QLatin1Char('.') ||
                        ch == QLatin1Char('_')));

                endPos = pos;
                cursor.setPosition(startPos);
                cursor.setPosition(endPos, QTextCursor::KeepAnchor);
                QPoint evtPos = helpEvent->pos();
                evtPos.rx() -= lineMarkerArea()->width();
                ret = editorToolTipEvent(evtPos, line, text, cursor.selectedText());

            } else {
                ret = editorToolTipEvent(helpEvent->globalPos(), line, text, QString());
            }
        }

        if (!text.isEmpty())
            QToolTip::showText(helpEvent->globalPos(), text, this);

        return ret;
    }

    return QPlainTextEdit::event(event);
}

bool TextEditor::editorToolTipEvent(QPoint pos, int line, QString &toolTipTxt,
                                    const QString &textUnderPos)
{
    Q_UNUSED(textUnderPos)
    bool ret = false;
    if (view()) {
        for (auto plugin : view()->currentPlugins()) {
            auto plug = dynamic_cast<AbstractLangPluginDbg*>(plugin);
            if (plug)
                ret = plug->textAreaToolTipEvent(this, pos, line, toolTipTxt);
        }
    }
    return ret;
}

bool TextEditor::lineMarkerAreaToolTipEvent(QPoint pos, int line, QString &toolTipTxt)
{
    Q_UNUSED(pos)
    Q_UNUSED(line)
    if (view()) {
        for (auto plugin : view()->currentPlugins()) {
            auto plug = dynamic_cast<AbstractLangPluginDbg*>(plugin);
            if (plug)
                plug->lineNrToolTipEvent(this, pos, line, toolTipTxt);
        }
    }
    return false;
}

void TextEditor::setUpMarkerAreaContextMenu(int line)
{
    m_markerAreaContextMenu.clear();
    QString bookmarkTxt = tr("Set bookmark");

    QTextBlock block = document()->findBlockByLineNumber(line -1);
    if (block.isValid()) {
        TextEditBlockData *userData = dynamic_cast<TextEditBlockData*>(block.userData());
        if (userData && userData->bookmark())
            bookmarkTxt = tr("Clear bookmark");
    }

    QAction *bookmark = new QAction(BitmapFactory().iconFromTheme("bookmark"), bookmarkTxt, &m_markerAreaContextMenu);
    bookmark->setData(Bookmark);
    m_markerAreaContextMenu.addAction(bookmark);

    // let plugins extend context menu
    if (view()) {
        for (auto plugin : view()->currentPlugins()) {
            plugin->contextMenuLineNr(this, &m_markerAreaContextMenu, d->filename, line);
        }
    }
}

void TextEditor::handleMarkerAreaContextMenu(QAction *res, int line)
{
    if (res && res->data().canConvert(QVariant::Int)) {
        ContextEvtType type = static_cast<ContextEvtType>(res->data().toInt());
        switch (type) {
        case Bookmark: {
            QTextBlock block = document()->findBlockByNumber(line -1);
            if (block.isValid()) {
                TextEditBlockData *userData = dynamic_cast<TextEditBlockData*>(block.userData());
                if (!userData) {
                    // insert new userData if not yet here
                    userData = new TextEditBlockData(block);
                    block.setUserData(userData);
                }

                // toggle bookmark
                userData->setBookmark(!userData->bookmark());

                // paint marker at our scrollbar
                AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
                if (vBar) {
                    if (userData->bookmark()) {
                        vBar->setMarker(line, d->bookmarkScrollBarMarkerColor);
                    } else {
                        vBar->clearMarker(line, d->bookmarkScrollBarMarkerColor);
                    }
                }

            }
        }   break;
        default:
            return;
        }
    }
}

void TextEditor::paintIndentMarkers(QPainter *painter, QTextBlock &block, QRect &rowRect)
{    
    // paint indentmarkers
    const int indentSize = static_cast<int>(getWindowParameter()->GetInt( "IndentSize", 4 ));
    const int tabSize = static_cast<int>(getWindowParameter()->GetInt( "TabSize", 4 ));

    QLatin1Char iCh(' ');
    auto fm = fontMetrics();
    int indentWidth = fm.width(QString().fill(iCh, indentSize));
    QString indentStr;

    for (const auto &ch : block.text()) {
        if (ch == iCh)
            indentStr += ch;
        else if (ch == QLatin1Char('\t'))
            indentStr += QString().fill(iCh, tabSize);
        else
            break;
    }

    int leftPos =  rowRect.left();
    int maxRight = fm.width(indentStr) + leftPos;
    painter->setPen(QColor(171, 172, 165));

    for (int i = leftPos + indentWidth, p = block.position() + indentSize;
         i < maxRight;
         i += indentWidth, p += indentSize)
    {
        if (p != textCursor().position())
            painter->drawLine(i, rowRect.top(), i, rowRect.bottom());
    }
}

void TextEditor::paintTextWidthMarker(QPaintEvent *e)
{
    // paint textwidth marker
    QPainter painter(viewport());
    painter.setClipRect(e->rect());
    painter.setPen(QColor(171, 130, 165));
    painter.setBrush(QColor(0xEE, 0xEE, 0xEE, 50));
    painter.setFont(font());

    QPointF offset(contentOffset());

    const QRect viewportRect = viewport()->rect();
    const int margin = static_cast<int>(document()->documentMargin());
    const int textWidth = static_cast<int>(getWindowParameter()->GetInt("LongLineWidth", 80));
    QString textWidthBlock;
    textWidthBlock = textWidthBlock.leftJustified(textWidth, QLatin1Char(' '));
    QRect fontRect = fontMetrics().boundingRect(e->rect(), Qt::AlignLeft, textWidthBlock);

    const QRect rect = QRectF(blockBoundingRect(firstVisibleBlock()).translated(offset)).toRect();
    const int x0 = rect.x() + fontRect.width() + margin;
    QRect overWidthRect(x0, rect.y(), static_cast<int>(document()->size().width()) - x0, viewportRect.height());
    painter.drawLine(x0, overWidthRect.top(),
                     x0, overWidthRect.bottom());
    painter.setPen(Qt::NoPen);
    painter.drawRect(overWidthRect);
}

void TextEditor::paintFoldedTextMarker(QPainter *painter, QTextBlock& block, QRect& rowRect)
{
    // paint indentmarkers
    const int rightOfText = 10;

    if (block.isVisible() && !block.next().isVisible()) {
        // find the last block not visible (should be the closing one)
        auto lastBlock = block.next();
        while (lastBlock.isValid() && !lastBlock.isVisible())
            lastBlock = lastBlock.next();

        QString dotText = QLatin1String(" ...");

        // find the first non whiteSpace
        if (lastBlock.isValid()) {
            auto re = QRegExp(QLatin1String("^\\s*([\\]\\}\\)])"));
            if (re.indexIn(lastBlock.previous().text()) > -1)
                dotText += re.cap(1);
        }

        dotText += QLatin1Char(' ');

        auto fm = fontMetrics();
        int textWidth = fm.width(block.text());
        qreal radius = rowRect.height() / 4;
        QRect rect(textWidth + rightOfText + rowRect.left(), rowRect.top(),
                   fm.width(dotText) + 6, rowRect.height());

        QPen pen(QColor(136, 167, 140));
        pen.setWidth(2);
        painter->setPen(pen);
        QBrush brush(QColor(242, 239, 155));
        painter->setBrush(brush);
        painter->drawRoundedRect(rect, radius, radius);

        pen.setColor(QColor(65,75,55));
        pen.setWidth(1);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        auto textPnt = rect.topLeft();
        textPnt.ry() += fm.ascent();
        painter->drawText(textPnt, dotText);
  }
}

// ------------------------------------------------------------------------------

LineMarkerArea::LineMarkerArea(TextEditor* editor) :
    QWidget(editor),
    d(new LineMarkerAreaP)
{
    d->textEditor = editor;
}

LineMarkerArea::~LineMarkerArea()
{
    delete d;
}

QSize LineMarkerArea::sizeHint() const
{
    // at least the width is 4 '0'
    int fill;
    if (d->lineNumberActive) {
        QString w = QString::number(d->textEditor->document()->blockCount());
        fill = qMax(w.size(), 5);
    } else
        fill = 1; // at least allow for breakpoints to be shown

    QString measureStr;
    measureStr.fill(QLatin1Char('0'), fill);
    int foldWidth  = 0;
    if (d->textEditor->getWindowParameter()->GetBool("EnableFolding", true))
        foldWidth = 10;

    return QSize(d->textEditor->fontMetrics().width(measureStr) + foldWidth, 0);
}

bool LineMarkerArea::lineNumbersVisible() const
{
    return d->lineNumberActive;
}

void LineMarkerArea::fontSizeChanged()
{
    d->loadIcons(d->textEditor->fontMetrics().height());
}

int LineMarkerArea::lineFromPos(const QPoint &pos)
{
    QTextBlock block = d->textEditor->firstVisibleBlock();

    int line =  block.blockNumber(),
        y = static_cast<int>(d->textEditor->contentOffset().y());

    while (block.isValid()) {
        QRectF rowSize = d->textEditor->blockBoundingGeometry(block);
        y += static_cast<int>(rowSize.height());
        ++line;
        if (pos.y() <= y) {
            //qDebug() << QLatin1String("line:") << line << QLatin1String(" revision:") << block.revision() << endl;
            return line;
        }

        block = block.next();
    }

    return 0;
}

void LineMarkerArea::setLineNumbersVisible(bool active)
{
    d->lineNumberActive = active;
}

void LineMarkerArea::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    const int foldingWidth = fontMetrics().height() +2;
    const auto lineHeight = fontMetrics().lineSpacing() +1;
    const int textWidth = width() - foldingWidth;
    const int fontHeight = fontMetrics().height();
    const bool paintFold = d->textEditor->getWindowParameter()->GetBool("EnableFolding", true);

    QTextBlock block = d->textEditor->firstVisibleBlock();
    int curBlockNr = d->textEditor->textCursor().block().blockNumber();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(d->textEditor->blockBoundingGeometry(block)
                                .translated(d->textEditor->contentOffset()).top());
    int bottom = top + static_cast<int>(d->textEditor->blockBoundingRect(block).height());
    QRect rowRect(0, 0,
                    (d->lineNumberActive ? textWidth : 0) +
                    (paintFold ? foldingWidth : 0),
                  lineHeight);



    while (block.isValid() && top <= event->rect().bottom()) {
        if (bottom >= event->rect().top()) {
            rowRect.moveTop(top);
            if (d->lineNumberActive && block.isVisible()) {
                // we should paint linenumbers
                QString number = QString::number(blockNumber + 1);
                QFont font = d->textEditor->font();
                QColor color = palette().buttonText().color();
                if (blockNumber == curBlockNr) {
                    color = palette().text().color();
                    font.setWeight(QFont::Bold);
                }
                painter.setFont(font);
                painter.setPen(color);
                painter.drawText(0, top, textWidth, fontHeight,
                                 Qt::AlignRight, number);
            }

            // draw revision marker
            if (block.isVisible() && block.revision() > 1) {
                QColor color = block.revision() > d->textEditor->d->lastSavedRevision ?
                                        QColor(Qt::red) : QColor(Qt::darkGreen);
                int height = static_cast<int>(d->textEditor->blockBoundingRect(block).height());
                QRect revRect(textWidth + 1, top, 1, height);
                painter.setPen(color);
                painter.setBrush(color);
                painter.drawRect(revRect);
            }

            // draw folding and bookmark
            if (block.userData()) {
                TextEditBlockData *userData = dynamic_cast<TextEditBlockData*>(block.userData());
                if (userData) {
                    // draw block arrow if row is visible
                    if (paintFold && block.isVisible() && userData->foldingBeginID()) {
                        QPolygonF polygon;

                        if (block.next().isVisible()) {
                            polygon << QPointF(lineHeight * 0.25, lineHeight * 0.4);
                            polygon << QPointF(lineHeight * 0.75, lineHeight * 0.4);
                            polygon << QPointF(lineHeight * 0.5, lineHeight * 0.8);
                        } else {
                            polygon << QPointF(lineHeight * 0.4, lineHeight * 0.25);
                            polygon << QPointF(lineHeight * 0.4, lineHeight * 0.75);
                            polygon << QPointF(lineHeight * 0.8, lineHeight * 0.5);
                        }

                        QColor color = QColor(114, 116, 117);
                        painter.save();
                        painter.setPen(color);
                        painter.setBrush(color);
                        painter.setRenderHint(QPainter::Antialiasing);
                        painter.translate(width() - lineHeight, top);
                        painter.setPen(Qt::NoPen);
                        painter.drawPolygon(polygon);
                        painter.restore();
                    }

                    // draw bookmark
                    if (userData->bookmark() && block.isVisible()) {
                        painter.drawPixmap(1, top, d->bookmarkIcon);
                    }
                }
            }

            if (block.isVisible()) {
                // let plugins render there own stuff
                if (d->textEditor->view()) {
                    painter.save();

                    for (auto plugin : d->textEditor->view()->currentPlugins())
                        plugin->paintEventLineNumberArea(d->textEditor, &painter, block, rowRect);

                    painter.restore();
                }
                // let special things in editor render it's stuff, ie. debugmarker breakpoints etc.
                d->textEditor->drawMarker(blockNumber + 1, 1, top, &painter);
            }
        }

        // advance  top-bottom visible boundaries for next row
        if (block.isVisible() || block.next().isVisible()) {
            top = bottom;
            bottom = top + static_cast<int>(d->textEditor->
                                            blockBoundingRect(block.next()).height());
        }

        block = block.next();
        ++blockNumber;
    }
}

void LineMarkerArea::mouseReleaseEvent(QMouseEvent *event)
{
    int line = lineFromPos(event->pos());
    if (line > 0) {

        // determine if we clicked on folding area or linenr?
        const int foldingWidth = fontMetrics().height() + 2;
        const int textWidth = width() - foldingWidth;
        if (event->pos().x() <= textWidth)
            Q_EMIT clickedOnLine(line, event);
        else
            foldingClicked(line);
    }
}

void LineMarkerArea::wheelEvent(QWheelEvent *event)
{
    event->ignore();
    d->textEditor->wheelEvent(event);
}

void LineMarkerArea::contextMenuEvent(QContextMenuEvent *event)
{
    int line = lineFromPos(event->pos());
    if (line > 0) {
        Q_EMIT contextMenuOnLine(line, event);
    }

}

void LineMarkerArea::foldingClicked(int line)
{
    // hide or show lines below that was previously folded
    auto startBlock = d->textEditor->document()->findBlockByNumber(line -1);
    if (startBlock.isValid() && startBlock.isVisible()) {
        auto userData = dynamic_cast<TextEditBlockData*>(startBlock.userData());
        if (userData) {
            auto foldId = userData->foldingBeginID();
            if (!foldId)
                return;

            // we have a blockstarter line!
            auto endBlockNr = userData->foldingEndBlock(userData).blockNumber();
            auto block = startBlock.next();
            bool show = !block.isVisible();

            while(block.blockNumber() <= endBlockNr && block.isValid()) {
                block.setVisible(show);
                block = block.next();
            }

            // redraw document
            auto doc = d->textEditor->document();
            doc->markContentsDirty(startBlock.position(),
                                   block.position() - startBlock.position() + 1);

            // update scrollbars
            Q_EMIT doc->documentLayout()->
                        documentSizeChanged(doc->documentLayout()->documentSize());
        }
    }

}

// -----------------------------------------------------------------------------

AnnotatedScrollBar::AnnotatedScrollBar(TextEditor *parent):
    QScrollBar(parent), d(new AnnotatedScrollBarP(parent))
{
}

AnnotatedScrollBar::~AnnotatedScrollBar()
{
    delete d;
}

void AnnotatedScrollBar::setMarker(int line, QColor color)
{
    d->markers.insertMulti(line, color);

    repaint();
}

void AnnotatedScrollBar::resetMarkers(QList<int> newMarkers, QColor color)
{
    clearMarkers(color);
    for (int line : newMarkers) {
        d->markers.insert(line, color);
    }

    repaint();
}

void AnnotatedScrollBar::clearMarkers()
{
    d->markers.clear();
    repaint();
}

void AnnotatedScrollBar::clearMarkers(QColor color)
{
    // segfaults if we try to clear in one pass
    QList<int> found;
    QMultiHash<int, QColor>::iterator it;
    for (it = d->markers.begin(); it != d->markers.end(); ++it) {
        if (it.value() == color)
            found.append(it.key());
    }

    for (int i : found)
        d->markers.remove(i, color);

    repaint();
}

void AnnotatedScrollBar::clearMarker(int line, QColor color)
{
    d->markers.remove(line, color);
    repaint();
}

void AnnotatedScrollBar::paintEvent(QPaintEvent *e)
{
    // render the scrollbar
    QScrollBar::paintEvent(e);

    QPainter painter(this);
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect groove = style()->subControlRect(QStyle::CC_ScrollBar, &opt,
                                           QStyle::SC_ScrollBarGroove, this);

    float yScale =  static_cast<float>(groove.height()) /
                    static_cast<float>(d->editor->document()->lineCount());
    int x1 = groove.x() + 4,
        x2 = groove.x() + groove.width() -4;
    QColor color;
    QMultiHash<int, QColor>::iterator it;
    for (it = d->markers.begin(); it != d->markers.end(); ++it) {
        if (it.value() != color) {
            color = it.value();
            painter.setBrush(color);
            painter.setPen(QPen(color, 2.0));
        }

        int y = static_cast<int>(it.key() * yScale) + groove.y();
        painter.drawLine(x1, y, x2, y);
    }
}

// ------------------------------------------------------------------------------


#include "moc_TextEditor.cpp"
