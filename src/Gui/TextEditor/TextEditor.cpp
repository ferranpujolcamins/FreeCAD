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

#include <QScrollBar>
#include <QStyleOptionSlider>
#include <QCompleter>

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
    const QColor bookmarkScrollBarMarkerColor = QColor(72, 108, 165); // blue-ish
    TextEditorP() :
        completer(nullptr)
    {

    }
    ~TextEditorP()
    { }
};
} // namespace Gui

/**
 *  Constructs a TextEditor which is a child of 'parent' and does the
 *  syntax highlighting for the Python language. 
 */
TextEditor::TextEditor(QWidget* parent)
  : QPlainTextEdit(parent), WindowParameter("Editor"), highlighter(0)
{
    d = new TextEditorP();
    lineNumberArea = new LineMarkerArea(this);

    // replace scrollbar to a annotated one
    setVerticalScrollBar(new AnnotatedScrollBar(this));

    QFont serifFont(QLatin1String("Courier"), 10, QFont::Normal);
    setFont(serifFont);
    lineNumberArea->setFont(serifFont);
    lineNumberArea->fontSizeChanged();

    ParameterGrp::handle hPrefGrp = getWindowParameter();
    // set default to 4 characters
    hPrefGrp->SetInt( "TabSize", 4 );
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
    delete highlighter;
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
    return lineNumberArea->sizeHint().width();
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

void TextEditor::setCompleter(QCompleter *completer) const
{
    d->completer = completer;
    d->completer->setWidget(const_cast<TextEditor*>(this));
}

QCompleter *TextEditor::completer() const
{
    return d->completer;
}

void TextEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void TextEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void TextEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void TextEditor::highlightSelections()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly() && highlighter) {
        // highlight current line
        QTextEdit::ExtraSelection selection;
        QColor lineColor = highlighter->color(QLatin1String("color_Currentline")); //d->colormap[QLatin1String("Current line highlight")];
        unsigned int col = (lineColor.red() << 24) | (lineColor.green() << 16) | (lineColor.blue() << 8);
        ParameterGrp::handle hPrefGrp = getWindowParameter();
        unsigned long value = static_cast<unsigned long>(col);
        value = hPrefGrp->GetUnsigned( "Current line highlight", value);
        col = static_cast<unsigned int>(value);
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
    Q_UNUSED(line); 
    Q_UNUSED(x); 
    Q_UNUSED(y); 
    Q_UNUSED(p); 
}

//void TextEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
//{
//    QPainter painter(lineNumberArea);
//    //painter.fillRect(event->rect(), Qt::lightGray);

//    QTextBlock block = firstVisibleBlock();
//    int blockNumber = block.blockNumber();
//    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
//    int bottom = top + (int) blockBoundingRect(block).height();

//    while (block.isValid() && top <= event->rect().bottom()) {
//        if (block.isVisible() && bottom >= event->rect().top()) {
//            QString number = QString::number(blockNumber + 1);
//            QPalette pal = palette();
//            QColor color = pal.windowText().color();
//            painter.setPen(color);
//            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
//                             Qt::AlignRight, number);
//            drawMarker(blockNumber + 1, 1, top, &painter);
//        }

//        block = block.next();
//        top = bottom;
//        bottom = top + (int) blockBoundingRect(block).height();
//        ++blockNumber;
//    }
//}

void TextEditor::setSyntaxHighlighter(SyntaxHighlighter* sh)
{
    sh->setDocument(this->document());
    this->highlighter = sh;
}

SyntaxHighlighter *TextEditor::syntaxHighlighter()
{
    return highlighter;
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

    if ( e->key() == Qt::Key_Tab ) {
        ParameterGrp::handle hPrefGrp = getWindowParameter();
        int indent = hPrefGrp->GetInt( "IndentSize", 4 );
        bool space = hPrefGrp->GetBool( "Spaces", false );
        QString ch = space ? QString(indent, QLatin1Char(' '))
                           : QString::fromLatin1("\t");

        QTextCursor cursor = textCursor();
        if (!cursor.hasSelection()) {
            // insert a single tab or several spaces
            cursor.beginEditBlock();
            // move cursor to start of text and adjust indentation
            int oldPos = cursor.position();
            int startPos = cursor.block().position();
            QString line = cursor.block().text();
            QRegExp re(QLatin1String("^(\\s*)"));
            if (re.indexIn(line) > -1 && space && indent > 0 &&
                    re.cap(1).size() + startPos >= oldPos)
            {
                // cursor are in indent whitespace
                int diff = re.cap(1).size() % indent;
                ch.remove(ch.size() - diff, diff); // resize indetation to match even indentation size
                cursor.setPosition(startPos + re.cap(1).size()); // to end of current indentation
            }

            cursor.insertText(ch);
            cursor.endEditBlock();
            setTextCursor(cursor);
        } else {
            // for each selected block insert a tab or spaces
            int selStart = cursor.selectionStart();
            int selEnd = cursor.selectionEnd();
            QTextBlock block;
            cursor.beginEditBlock();
            for (block = document()->begin(); block.isValid(); block = block.next()) {
                int pos = block.position();
                int off = block.length()-1;
                // at least one char of the block is part of the selection
                if ( pos >= selStart || pos+off >= selStart) {
                    if ( pos+1 > selEnd )
                        break; // end of selection reached
                    cursor.setPosition(block.position());
                    cursor.insertText(ch);
                        selEnd += ch.length();
                }
            }

            cursor.endEditBlock();
        }

        return;
    }
    else if (e->key() == Qt::Key_Backtab) {
        QTextCursor cursor = textCursor();
        if (!cursor.hasSelection())
            return; // Shift+Tab should not do anything
        // If some text is selected we remove a leading tab or
        // spaces from each selected block
        ParameterGrp::handle hPrefGrp = getWindowParameter();
        int indent = hPrefGrp->GetInt( "IndentSize", 4 );

        int selStart = cursor.selectionStart();
        int selEnd = cursor.selectionEnd();
        QTextBlock block;
        cursor.beginEditBlock();
        for (block = document()->begin(); block.isValid(); block = block.next()) {
            int pos = block.position();
            int off = block.length()-1;
            // at least one char of the block is part of the selection
            if ( pos >= selStart || pos+off >= selStart) {
                if ( pos+1 > selEnd )
                    break; // end of selection reached
                // if possible remove one tab or several spaces
                QString text = block.text();
                if (text.startsWith(QLatin1String("\t"))) {
                    cursor.setPosition(block.position());
                    cursor.deleteChar();
                    selEnd--;
                }
                else {
                    cursor.setPosition(block.position());
                    for (int i=0; i<indent; i++) {
                        if (!text.startsWith(QLatin1String(" ")))
                            break;
                        text = text.mid(1);
                        cursor.deleteChar();
                        selEnd--;
                    }
                }
            }
        }

        cursor.endEditBlock();
        return;
    }

    QPlainTextEdit::keyPressEvent( e );
}

/** Sets the font, font size and tab size of the editor. */  
void TextEditor::OnChange(Base::Subject<const char*> &rCaller,const char* sReason)
{
    Q_UNUSED(rCaller); 
    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (strcmp(sReason, "FontSize") == 0 || strcmp(sReason, "Font") == 0) {
#ifdef FC_OS_LINUX
        int fontSize = hPrefGrp->GetInt("FontSize", 15);
#else
        int fontSize = hPrefGrp->GetInt("FontSize", 10);
#endif
        QString fontFamily = QString::fromLatin1(hPrefGrp->GetASCII( "Font", "Courier" ).c_str());
        
        QFont font(fontFamily, fontSize);
        setFont(font);
        lineNumberArea->setFont(font);

        // resize linemarker icons
        lineNumberArea->fontSizeChanged();

    } else if (strncmp(sReason, "color_", 6) == 0) {
        if (this->highlighter)
            this->highlighter->loadSettings();
        /*
        QMap<QString, QColor>::ConstIterator it = d->colormap.find(QString::fromLatin1(sReason));
        if (it != d->colormap.end()) {
            QColor color = it.value();
            unsigned int col = (color.red() << 24) | (color.green() << 16) | (color.blue() << 8);
            unsigned long value = static_cast<unsigned long>(col);
            value = hPrefGrp->GetUnsigned(sReason, value);
            col = static_cast<unsigned int>(value);
            color.setRgb((col>>24)&0xff, (col>>16)&0xff, (col>>8)&0xff);
            if (this->highlighter)
                this->highlighter->setColor(QLatin1String(sReason), color);
        }
        */
    }

    if (strcmp(sReason, "TabSize") == 0 || strcmp(sReason, "FontSize") == 0) {
        int tabWidth = hPrefGrp->GetInt("TabSize", 4);
        QFontMetrics metric(font());
        int fontSize = metric.width(QLatin1String("0"));
        setTabStopWidth(tabWidth * fontSize);
    }

    // Enables/Disables Line number in the Macro Editor from Edit->Preferences->Editor menu.
    QRect cr = contentsRect();
    bool show = hPrefGrp->GetBool( "EnableLineNumber", true );
    if(show) {
        lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    } else {
        lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), 20, cr.height()));
    }
    lineNumberArea->setLineNumbersVisible(show);
}

void TextEditor::paintEvent (QPaintEvent * e)
{
    QPlainTextEdit::paintEvent( e );
}

bool TextEditor::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        QPoint point = helpEvent->pos();
        if (point.rx() < lineNumberAreaWidth()) {
            // on linenumberarea
            QTextBlock block = firstVisibleBlock();
            QRectF rowSize = blockBoundingGeometry(block);
            int line = 0;
            if (rowSize.height() > 0)
                line = ((point.ry()) / rowSize.height()) + block.blockNumber() + 1;

            return lineMarkerAreaToolTipEvent(helpEvent->globalPos(), line);
        }

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
            return editorToolTipEvent(evtPos, cursor.selectedText());

        } else {
            return editorToolTipEvent(helpEvent->globalPos(), QString());
        }
    }

    return QPlainTextEdit::event(event);
}

bool TextEditor::editorToolTipEvent(QPoint pos, const QString &textUnderPos)
{
    Q_UNUSED(pos);
    Q_UNUSED(textUnderPos);
    return false;
}

bool TextEditor::lineMarkerAreaToolTipEvent(QPoint pos, int line)
{
    Q_UNUSED(pos);
    Q_UNUSED(line);
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
}

void TextEditor::handleMarkerAreaContextMenu(QAction *res, int line)
{
    if (res->data().canConvert(QMetaType::Int)) {
        ContextEvtType type = static_cast<ContextEvtType>(res->data().toInt());
        switch (type) {
        case Bookmark: {
            QTextBlock block = document()->findBlockByLineNumber(line -1);
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
    } else {
        // custom data
        //QSting actionStr = res->data().toString();
        //if (actionStr == QLatin1String("special")) {
        //    // do something
        //}
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
        fill = qMax(w.size(), 4);
    } else
        fill = 1; // at least allow for breakpoints to be shown

    QString measureStr;
    measureStr.fill(QLatin1Char('0'), fill);
    return QSize(d->textEditor->fontMetrics().width(measureStr) +10, 0);
}

bool LineMarkerArea::lineNumbersVisible() const
{
    return d->lineNumberActive;
}

void LineMarkerArea::fontSizeChanged()
{
    d->loadIcons(d->textEditor->fontMetrics().height());
}

void LineMarkerArea::setLineNumbersVisible(bool active)
{
    d->lineNumberActive = active;
}

void LineMarkerArea::paintEvent(QPaintEvent* event)
{
    //textEditor->lineNumberAreaPaintEvent(e);
    QPainter painter(this);
    //painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = d->textEditor->firstVisibleBlock();
    int curBlockNr = d->textEditor->textCursor().block().blockNumber();
    int blockNumber = block.blockNumber();
    int top = (int) d->textEditor->blockBoundingGeometry(block).translated(
                                    d->textEditor->contentOffset()).top();
    int bottom = top + (int) d->textEditor->blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (d->lineNumberActive) {
                // we should point linenumbers
                QString number = QString::number(blockNumber + 1);
                QPalette pal = palette();
                QFont font = d->textEditor->font();
                QColor color = pal.buttonText().color();
                if (blockNumber == curBlockNr) {
                    color = pal.text().color();
                    font.setWeight(QFont::Bold);
                }
                painter.setFont(font);
                painter.setPen(color);
                painter.drawText(0, top, width(), fontMetrics().height(),
                                 Qt::AlignRight, number);
            }

            if (block.userData()) {
                TextEditBlockData *userData = dynamic_cast<TextEditBlockData*>(block.userData());
                if (userData && userData->bookmark()) {
                    painter.drawPixmap(1, top, d->bookmarkIcon);
                }
            }

            d->textEditor->drawMarker(blockNumber + 1, 1, top, &painter);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int) d->textEditor->blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void LineMarkerArea::mouseReleaseEvent(QMouseEvent *event)
{
    QTextBlock block = d->textEditor->firstVisibleBlock();
    QRectF rowSize = d->textEditor->blockBoundingGeometry(block);
    if (rowSize.height() > 0) {
        int line = ((event->y()) / rowSize.height()) + block.blockNumber() + 1;
        Q_EMIT clickedOnLine(line, event);
    }
}

void LineMarkerArea::wheelEvent(QWheelEvent *event)
{
    event->ignore();
    d->textEditor->wheelEvent(event);
}

void LineMarkerArea::contextMenuEvent(QContextMenuEvent *event)
{
    QTextBlock block = d->textEditor->firstVisibleBlock();
    QRectF rowSize = d->textEditor->blockBoundingGeometry(block);
    if (rowSize.height() > 0) {
        int line = ((event->y()) / rowSize.height()) + block.blockNumber() + 1;
        Q_EMIT contextMenuOnLine(line, event);
    }

}

// ----------------------------------------------------------------------------

TextEditBlockData::TextEditBlockData(QTextBlock block) :
    m_block(block),
    m_scanInfo(nullptr),
    m_bookmarkSet(false)
{
}

TextEditBlockData::TextEditBlockData(const TextEditBlockData &other):
    m_block(other.m_block),
    m_scanInfo(other.m_scanInfo),
    m_bookmarkSet(other.m_bookmarkSet)
{
}

TextEditBlockData::~TextEditBlockData()
{
}

TextEditBlockData *TextEditBlockData::blockDataFromCursor(const QTextCursor &cursor)
{
    QTextBlock block = cursor.block();
    if (!block.isValid())
        return nullptr;

    return dynamic_cast<TextEditBlockData*>(block.userData());
}

const QTextBlock &TextEditBlockData::block() const
{
    return m_block;
}

TextEditBlockData *TextEditBlockData::next() const
{
    QTextBlock nextBlock = block().next();
    if (!nextBlock.isValid())
        return nullptr;
    return dynamic_cast<TextEditBlockData*>(nextBlock.userData());
}

TextEditBlockData *TextEditBlockData::previous() const
{
    QTextBlock nextBlock = block().previous();
    if (!nextBlock.isValid())
        return nullptr;
    return dynamic_cast<TextEditBlockData*>(nextBlock.userData());
}

// ------------------------------------------------------------------------------------


TextEditBlockScanInfo::TextEditBlockScanInfo()
{
}

TextEditBlockScanInfo::~TextEditBlockScanInfo()
{
}

void TextEditBlockScanInfo::setParseMessage(int startPos, int endPos, QString message,
                                              MsgType type)
{
    ParseMsg msg(message, startPos, endPos, type);
    m_parseMessages.push_back(msg);
}

const TextEditBlockScanInfo::ParseMsg
*TextEditBlockScanInfo::getParseMessage(int startPos, int endPos,
                                          TextEditBlockScanInfo::MsgType type) const
{
    for (const ParseMsg &msg : m_parseMessages) {
        if (msg.startPos <= startPos && msg.endPos >= endPos) {
            if (type == AllMsgTypes)
                return &msg;
            else if (msg.type == type)
                return &msg;
            break;
        }
    }
    return nullptr;
}

QString TextEditBlockScanInfo::parseMessage(int col, MsgType type) const
{
    const ParseMsg *parseMsg = getParseMessage(col, col, type);
    if (parseMsg)
        return parseMsg->message;
    return QString();
}

void TextEditBlockScanInfo::clearParseMessage(int col)
{
    int idx = -1;
    for (ParseMsg &msg : m_parseMessages) {
        ++idx;
        if (msg.startPos <= col && msg.endPos >= col)
            m_parseMessages.removeAt(idx);
    }
}

void TextEditBlockScanInfo::clearParseMessages()
{
    m_parseMessages.clear();
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

    float yScale =  (float)groove.height() / (float)d->editor->document()->lineCount();
    int x1 = groove.x() +4,
        x2 = groove.x() + groove.width() -4;
    QColor color;
    QMultiHash<int, QColor>::iterator it;
    for (it = d->markers.begin(); it != d->markers.end(); ++it) {
        if (it.value() != color) {
            color = it.value();
            painter.setBrush(color);
            painter.setPen(QPen(color, 2.0));
        }

        int y = (it.key() * yScale) + groove.y();
        painter.drawLine(x1, y, x2, y);
    }
}

// ------------------------------------------------------------------------------


#include "moc_TextEditor.cpp"
