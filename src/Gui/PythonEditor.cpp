/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *   Copyright (c) 2017 Fredrik Johansson github.com/mumme74               *
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
# include <QContextMenuEvent>
# include <QMenu>
# include <QPainter>
# include <QShortcut>
# include <QTextCursor>
#endif

#include "PythonEditor.h"
#include "PythonCode.h"
#include "PythonDebugger.h"
#include "Application.h"
#include "BitmapFactory.h"
#include "Macro.h"
#include "FileDialog.h"
#include "DlgEditorImp.h"
#include "CallTips.h"


#include <CXX/Objects.hxx>
#include <Base/PyObjectBase.h>
#include <Base/Interpreter.h>
#include <Base/Exception.h>
#include <Base/Parameter.h>
#include <QRegExp>
#include <QDebug>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QToolTip>

using namespace Gui;

namespace Gui {
struct PythonEditorP
{
    int   debugLine;
    QRect debugRect;
    QPixmap breakpoint;
    QPixmap breakpointDisabled;
    QPixmap debugMarker;
    QString filename;
    PythonDebugger* debugger;
    PythonCode *pythonCode;
    CallTipsList* callTipsList;
    PythonMatchingChars* matchingChars;
    const QColor breakPointScrollBarMarkerColor = QColor(242, 58, 82); // red
    PythonEditorP()
        : debugLine(-1),
          callTipsList(0)
    {
        debugger = Application::Instance->macroManager()->debugger();
        pythonCode = new PythonCode();
    }
    ~PythonEditorP()
    {
        delete pythonCode;
    }
    void loadIcons(int rowHeight)
    {
        breakpoint = BitmapFactory().iconFromTheme("breakpoint").pixmap(rowHeight, rowHeight);
        breakpointDisabled = BitmapFactory().iconFromTheme("breakpoint-disabled").pixmap(rowHeight, rowHeight);
        debugMarker = BitmapFactory().iconFromTheme("debug-marker").pixmap(rowHeight, rowHeight);
    }
};

} // namespace Gui



/* TRANSLATOR Gui::PythonEditor */

/**
 *  Constructs a PythonEditor which is a child of 'parent' and does the
 *  syntax highlighting for the Python language. 
 */
PythonEditor::PythonEditor(QWidget* parent)
  : TextEditor(parent)
{
    d = new PythonEditorP();
    d->loadIcons(fontMetrics().height());

    PythonSyntaxHighlighter *hl = new PythonSyntaxHighlighter(this);
    this->setSyntaxHighlighter(hl);            

    // set acelerators
    QShortcut* comment = new QShortcut(this);
    comment->setKey(Qt::ALT + Qt::Key_C);

    QShortcut* uncomment = new QShortcut(this);
    uncomment->setKey(Qt::ALT + Qt::Key_U);

    QShortcut* autoIndent = new QShortcut(this);
    autoIndent->setKey(Qt::CTRL + Qt::SHIFT + Qt::Key_I );

    connect(comment, SIGNAL(activated()), 
            this, SLOT(onComment()));
    connect(uncomment, SIGNAL(activated()), 
            this, SLOT(onUncomment()));
    connect(autoIndent, SIGNAL(activated()),
            this, SLOT(onAutoIndent()));

    connect(getMarkerArea(), SIGNAL(contextMenuOnLine(int,QContextMenuEvent*)),
            this, SLOT(markerAreaContextMenu(int,QContextMenuEvent*)));

    PythonDebugger *dbg = PythonDebugger::instance();
    connect(dbg, SIGNAL(stopped()), this, SLOT(hideDebugMarker()));
    connect(dbg, SIGNAL(breakpointAdded(const BreakpointLine*)),
            this, SLOT(breakpointAdded(const BreakpointLine*)));
    connect(dbg, SIGNAL(breakpointChanged(const BreakpointLine*)),
            this, SLOT(breakpointChanged(const BreakpointLine*)));
    connect(dbg, SIGNAL(breakpointRemoved(int,const BreakpointLine*)),
            this, SLOT(breakpointRemoved(int,const BreakpointLine*)));

    d->matchingChars = new PythonMatchingChars(this);

    // create the window for call tips
    d->callTipsList = new CallTipsList(this);
    d->callTipsList->setFrameStyle(QFrame::Box|QFrame::Raised);
    d->callTipsList->setLineWidth(2);
    installEventFilter(d->callTipsList);
    viewport()->installEventFilter(d->callTipsList);
    d->callTipsList->setSelectionMode( QAbstractItemView::SingleSelection );
    d->callTipsList->hide();
}

/** Destroys the object and frees any allocated resources */
PythonEditor::~PythonEditor()
{
    getWindowParameter()->Detach( this );
    delete d;
    d = nullptr;
}

void PythonEditor::setFileName(const QString& fn)
{
    d->filename = fn;
}

int PythonEditor::findText(const QString find)
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

void PythonEditor::startDebug()
{
    if (d->debugger->start()) {
        d->debugger->runFile(d->filename);
        if (d) // if app gets closed during debugging halt, d is deleted
            d->debugger->stop();
    }
}

void PythonEditor::OnChange(Base::Subject<const char *> &rCaller, const char *rcReason)
{
    // reload breakpoint and debug icons as correct size
    TextEditor::OnChange(rCaller, rcReason);
    if (strcmp(rcReason, "FontSize") == 0 || strcmp(rcReason, "Font") == 0) {
        int rowHeight = fontMetrics().height();
        d->loadIcons(rowHeight);
    }
}

void PythonEditor::cut()
{
    // track breakpoint movements
    breakpointPasteOrCut(true);
}

void PythonEditor::paste()
{
    // track breakpoint movements
    breakpointPasteOrCut(false);
}



void PythonEditor::toggleBreakpoint()
{
    QTextCursor cursor = textCursor();
    int line = cursor.blockNumber() + 1;
    d->debugger->toggleBreakpoint(line, d->filename);
    getMarkerArea()->update();
}

void PythonEditor::showDebugMarker(int line)
{
    d->debugLine = line;
    getMarkerArea()->update();
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    int cur = cursor.blockNumber() + 1;
    if (cur > line) {
        for (int i=line; i<cur; i++)
            cursor.movePosition(QTextCursor::Up);
    }
    else if (cur < line) {
        for (int i=cur; i<line; i++)
            cursor.movePosition(QTextCursor::Down);
    }
    setTextCursor(cursor);
}

void PythonEditor::hideDebugMarker()
{
    d->debugLine = -1;
    getMarkerArea()->update();
}

void PythonEditor::drawMarker(int line, int x, int y, QPainter* p)
{
    BreakpointLine *bpl = d->debugger->getBreakpointLine(d->filename, line);
    if (bpl != nullptr) {
        if (bpl->disabled())
            p->drawPixmap(x, y, d->breakpointDisabled);
        else
            p->drawPixmap(x, y, d->breakpoint);
    }
    if (d->debugLine == line) {
        p->drawPixmap(x, y, d->debugMarker);
        d->debugRect = QRect(x, y, d->debugMarker.width(), d->debugMarker.height());
    }
}

void PythonEditor::contextMenuEvent ( QContextMenuEvent * e )
{
    QMenu* menu = createStandardContextMenu();
    if (!isReadOnly()) {
        menu->addSeparator();
        menu->addAction( tr("Comment"), this, SLOT( onComment() ), Qt::ALT + Qt::Key_C );
        menu->addAction( tr("Uncomment"), this, SLOT( onUncomment() ), Qt::ALT + Qt::Key_U );
        menu->addAction( tr("Auto indent"), this, SLOT( onAutoIndent() ), Qt::CTRL + Qt::SHIFT + Qt::Key_I );
    }

    menu->exec(e->globalPos());
    delete menu;
}

/**
 * Checks the input to make the correct indentations.
 * And code completions
 */
void PythonEditor::keyPressEvent(QKeyEvent * e)
{
    QTextCursor cursor = this->textCursor();
    QTextCursor inputLineBegin = this->textCursor();
    inputLineBegin.movePosition( QTextCursor::StartOfLine );
    static bool autoIndented = false;

    QTextBlock inputBlock = inputLineBegin.block();              //< get the last paragraph's text
    QString    inputLine  = inputBlock.text();

    // track lineNr changes before and after textChange
    int beforeLineNr = cursor.block().blockNumber();
    int afterLineNr = -1;
    int lineCountBefore = document()->blockCount();
    if (this->textCursor().hasSelection()) {
        int posEnd = qMax(cursor.selectionEnd(), cursor.selectionStart());
        int posStart = qMin(cursor.selectionEnd(), cursor.selectionStart());
        beforeLineNr = document()->findBlock(posEnd).blockNumber();
        afterLineNr = document()->findBlock(posStart).blockNumber();
    }

    switch (e->key())
    {
    case Qt::Key_Backspace:
    {
        if (autoIndented) {
            cursor.beginEditBlock();
            int oldPos = cursor.position(),
                blockPos = cursor.block().position();
            // find previous block indentation
            QRegExp re(QLatin1String("[:{]\\s*$"));
            int pos = 0, ind = 0;
            while (cursor.movePosition(QTextCursor::Up,
                                       QTextCursor::KeepAnchor, 1))
            {
                if (re.indexIn(cursor.block().text()) > -1) {
                    // get leading indent of this row
                    QRegExp rx(QString::fromLatin1("^(\\s*)"));
                    pos = rx.indexIn(cursor.block().text());
                    ind = pos > -1 ? rx.cap(1).size() : -1;
                    if (blockPos + ind < oldPos)
                        break;
                }
                pos = 0;
                ind = 0;
            }

            if (ind > -1) {
                // reposition to start chr where we want to erase
                cursor.setPosition(blockPos + ind, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
            if (ind <= 0) {
                autoIndented = false;
            }

            cursor.endEditBlock();
        } else {
            TextEditor::keyPressEvent(e);
        }

    } break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    {
        // for breakpoint move
        if (cursor.hasSelection()) {
            // add one because this Key adds a row
            afterLineNr += 1;
        } else if (cursor.atBlockEnd()) {
            // dont move breakpoint if we are at line end
            beforeLineNr += 1;
            afterLineNr = beforeLineNr +1;
        }

        // auto indent
        ParameterGrp::handle hPrefGrp = getWindowParameter();
        if (hPrefGrp->GetBool( "EnableAutoIndent", true)) {
            cursor.beginEditBlock();
            int indent = hPrefGrp->GetInt( "IndentSize", 4 );
            bool space = hPrefGrp->GetBool( "Spaces", false );
            QString ch = space ? QString(indent, QLatin1Char(' '))
                               : QString::fromLatin1("\t");

            // get leading indent
            QRegExp rx(QString::fromLatin1("^(\\s*)"));
            int pos = rx.indexIn(inputLine);
            QString ind = pos > -1 ? rx.cap(1) : QString();

            QRegExp re(QLatin1String("[:{]"));
            if (cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor) &&
                re.indexIn(cursor.selectedText()) > -1)
            {
                // a class or function
                ind += ch;
            }
            TextEditor::keyPressEvent(e);
            insertPlainText(ind);
            cursor.endEditBlock();
            autoIndented = true;
        } else {
            TextEditor::keyPressEvent(e);
        }
    }   break;

    case Qt::Key_Period:
    {
        autoIndented = false;

        // In Qt 4.8 there is a strange behaviour because when pressing ":"
        // then key is also set to 'Period' instead of 'Colon'. So we have
        // to make sure we only handle the period.
        if (e->text() == QLatin1String(".") && cursor != inputLineBegin) {
            // analyse context and show available call tips
            int contextLength = cursor.position() - inputLineBegin.position();
            TextEditor::keyPressEvent(e);
            d->callTipsList->showTips( inputLine.left( contextLength ) );
        }
        else {
            TextEditor::keyPressEvent(e);
        }
    }   break;
    case Qt::Key_Delete:
    {
        // move breakpoint if we are at line end
        if (cursor.atBlockEnd() && !cursor.hasSelection()) {
            beforeLineNr += 1;
            afterLineNr = cursor.block().blockNumber();
        }

    } // intentional fallthrough
    default:
    {
        // auto insert closing char
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        QString nextChr = cursor.selectedText();
        QString insertAfter;
        bool clearPrevious = false;

        static QString previousKeyText;

        if (e->key() == Qt::Key_ParenLeft) {
            if (nextChr != QLatin1String(")"))
                insertAfter = QLatin1String(")");
        } else if (e->key() == Qt::Key_BraceLeft) {
            if (nextChr != QLatin1String("}"))
                insertAfter = QLatin1String("}");
        } else if (e->key() == Qt::Key_BracketLeft) {
            if (nextChr != QLatin1String("]"))
                insertAfter = QLatin1String("]");
        } else if (e->key() == Qt::Key_QuoteDbl) {
            if (nextChr != QLatin1String("\""))
                insertAfter = QLatin1String("\"");
        } else if (e->key() == Qt::Key_Apostrophe) {
            if (nextChr != QLatin1String("'"))
                insertAfter = QLatin1String("'");
        }

        autoIndented = false;
        TextEditor::keyPressEvent(e);

        if (insertAfter.size()) {
            if (previousKeyText != e->text()) {
                QTextCursor cursor = textCursor();
                cursor.insertText(insertAfter);
                cursor.movePosition(QTextCursor::Left);
                setTextCursor(cursor);
            } else { // last char was autoinserted, we want to step over
                QTextCursor cursor = textCursor();
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                setTextCursor(cursor);
                clearPrevious = true; // only do this once, not on consecutive presses
            }
        }

        // need to use string instead of key as modifiers messes up otherwise
        if (clearPrevious)
            previousKeyText.clear();
        else if (e->text().size())
            previousKeyText = e->text();
    }   break;
    }

    // move breakpoints?
    if (lineCountBefore != document()->blockCount()) {
        if (afterLineNr == -1)
            afterLineNr = this->textCursor().block().blockNumber();
        if (beforeLineNr != afterLineNr) {
            int move = afterLineNr - beforeLineNr;
            BreakpointFile *bp = PythonDebugger::instance()->getBreakpointFile(d->filename);
            if (bp) {
                bp->moveLines(beforeLineNr +1, move);
            }
        }
    }

    // This can't be done in CallTipsList::eventFilter() because we must first perform
    // the event and afterwards update the list widget
    if (d->callTipsList->isVisible())
    { d->callTipsList->validateCursor(); }
}

bool PythonEditor::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        QPoint point = helpEvent->pos();
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

            QString str = d->pythonCode->findFromCurrentFrame(
                                            cursor.selectedText());
            QToolTip::showText(helpEvent->globalPos(), str);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return TextEditor::event(event);
}

void PythonEditor::markerAreaContextMenu(int line, QContextMenuEvent *event)
{
    QMenu menu;
    BreakpointLine *bpl = d->debugger->getBreakpointLine(d->filename, line);
    if (bpl != nullptr) {
        QAction disable(BitmapFactory().iconFromTheme("breakpoint-disabled"),
                        tr("Disable breakpoint"), &menu);
        QAction enable(BitmapFactory().iconFromTheme("breakpoint"),
                       tr("Enable breakpoint"), &menu);
        QAction edit(BitmapFactory().iconFromTheme("preferences-general"),
                        tr("Edit breakpoint"), &menu);
        QAction del(BitmapFactory().iconFromTheme("delete"),
                       tr("Delete breakpoint"), &menu);

        if (bpl->disabled())
            menu.addAction(&enable);
        else
            menu.addAction(&disable);
        menu.addAction(&edit);
        menu.addAction(&del);

        QAction *res = menu.exec(event->globalPos());
        if (res == &disable) {
            bpl->setDisabled(true);
        } else if(res == &enable) {
            bpl->setDisabled(false);
        } else if (res == &edit) {
            PythonEditorBreakpointDlg dlg(this, bpl);
            dlg.exec();
        } else if (res == &del) {
            d->debugger->deleteBreakpoint(d->filename, line);
        }

    } else {
        QAction create(BitmapFactory().iconFromTheme("breakpoint"),
                       tr("Add breakpoint"), this);
        menu.addAction(&create);
        QAction *res = menu.exec(event->globalPos());
        if (res == &create)
            d->debugger->setBreakpoint(d->filename, line);
    }

    event->accept();
}

void PythonEditor::breakpointAdded(const BreakpointLine *bpl)
{
    if (bpl->parent()->fileName() != d->filename)
        return;

    AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
    if (vBar)
        vBar->setMarker(bpl->lineNr(), d->breakPointScrollBarMarkerColor);

    getMarkerArea()->update();
}

void PythonEditor::breakpointChanged(const BreakpointLine *bpl)
{
    if (bpl->parent()->fileName() != d->filename)
        return;

    getMarkerArea()->update();
}

void PythonEditor::breakpointRemoved(int idx, const BreakpointLine *bpl)
{
    Q_UNUSED(idx);

    if (bpl->parent()->fileName() != d->filename)
        return;

    AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
    if (vBar)
        vBar->clearMarker(bpl->lineNr(), d->breakPointScrollBarMarkerColor);

    getMarkerArea()->update();
}


void PythonEditor::breakpointPasteOrCut(bool doCut)
{
    // track breakpoint movements
    QTextCursor cursor = textCursor();
    int beforeLineNr = cursor.block().blockNumber();
    if (cursor.hasSelection()) {
        int posEnd = qMax(cursor.selectionEnd(), cursor.selectionStart());
        beforeLineNr = document()->findBlock(posEnd).blockNumber();
    }

    if (doCut)
        TextEditor::cut();
    else
        TextEditor::paste();

    int currentLineNr = textCursor().block().blockNumber();
    if (beforeLineNr != currentLineNr) {
        int move = beforeLineNr - currentLineNr;
        BreakpointFile *bp = PythonDebugger::instance()->getBreakpointFile(d->filename);
        if (bp)
            bp->moveLines(beforeLineNr, move);
    }
}

void PythonEditor::onComment()
{
    QTextCursor cursor = textCursor();
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
            cursor.insertText(QLatin1String("#"));
                selEnd++;
        }
    }

    cursor.endEditBlock();
}

void PythonEditor::onUncomment()
{
    QTextCursor cursor = textCursor();
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
            if (block.text().startsWith(QLatin1String("#"))) {
                cursor.setPosition(block.position());
                cursor.deleteChar();
                selEnd--;
            }
        }
    }

    cursor.endEditBlock();
}

void PythonEditor::onAutoIndent()
{
    QTextCursor cursor = textCursor();
    int selStart = cursor.selectionStart();
    int selEnd = cursor.selectionEnd();
    QList<int> indentLevel;// level of indentation at different indentaions

    QTextBlock block;
    QRegExp reNewBlock(QLatin1String("[:{]\\s*#*.*$"));
    QChar mCommentChr(0);
    int mCommentIndents = 0;

    ParameterGrp::handle hPrefGrp = getWindowParameter();
    int indentSize = hPrefGrp->GetInt( "IndentSize", 4 );
    bool useSpaces = hPrefGrp->GetBool( "Spaces", false );
    QString space = useSpaces ? QString(indentSize, QLatin1Char(' '))
                       : QString::fromLatin1("\t");

    cursor.beginEditBlock();
    for (block = document()->begin(); block.isValid(); block = block.next()) {
        // get this rows indent
        int chrCount = 0, blockIndent = 0;
        bool textRow = false, mayIndent = false;
        for (int i = 0; i < block.text().size(); ++i) {
            QChar ch = block.text()[i];
            if (ch == QLatin1Char(' ')) {
                ++chrCount;
            } else if (ch == QLatin1Char('\t')) {
                chrCount += indentSize;
            } else if (ch == QLatin1Char('#')) {
                textRow = true;
                break; // comment row
            } else if (ch == QLatin1Char('\'') || ch == QLatin1Char('"')) {
                if (block.text().size() > i + 2 &&
                    block.text()[i +1] == ch && block.text()[i +2] == ch)
                {
                    if (mCommentChr == 0) {
                        mCommentChr = ch;// start a multiline comment
                    } else if (mCommentChr == ch) {
                        mCommentChr = 0;// end multiline comment
                        while (mCommentIndents > 0 && indentLevel.size()) {
                            indentLevel.removeLast();
                            --mCommentIndents;
                        }
                    }
                    textRow = true;
                    break;
                }
            } /*else if (mCommentChr != QChar(0)) {
                textRow = true;
                break; // inside multiline comment
            }*/ else if (ch.isLetterOrNumber()) {
                mayIndent = true;
                textRow = true;
                break; // text started
            }
        }
        if (!textRow) continue;

        // cancel a indent block
        while (indentLevel.size() > 0 && chrCount <= indentLevel.last()){
            // stop current indent block
            indentLevel.removeLast();
            if (mCommentChr != 0)
                --mCommentIndents;
        }

        // start a new indent block?
        QString txt = block.text();
        if (mayIndent && reNewBlock.indexIn(txt) > -1) {
            indentLevel.append(chrCount);
            blockIndent = -1; // dont indent this row
            if (mCommentChr != 0)
                ++mCommentIndents;
        }

        int pos = block.position();
        int off = block.length()-1;
        // at least one char of the block is part of the selection
        if ( pos >= selStart || pos+off >= selStart) {
            if ( pos+1 > selEnd )
                break; // end of selection reached
            int i = 0;
            for (QChar ch : block.text()) {
                if (!ch.isSpace())
                    break;
                cursor.setPosition(block.position());
                cursor.deleteChar();
                ++i;
            }
            for (i = 0; i < indentLevel.size() + blockIndent; ++i) {
                cursor.setPosition(block.position());
                cursor.insertText(space);
            }
        }
    }
    cursor.endEditBlock();
}

// ------------------------------------------------------------------------

PythonEditorBreakpointDlg::PythonEditorBreakpointDlg(QWidget *parent,
                                                      BreakpointLine *bp):
    QDialog(parent), m_bpl(bp)
{
    QLabel *lblMsg   = new QLabel(this);
    QLabel *lblEnable = new QLabel(this);
    m_enabled        = new QCheckBox(this);
    m_ignoreFromHits = new QSpinBox(this);
    m_ignoreToHits   = new QSpinBox(this);
    m_condition      = new QLineEdit(this);
    QLabel *ignoreToLbl   = new QLabel(this);
    QLabel *ignoreFromLbl = new QLabel(this);
    QLabel *conditionLbl   = new QLabel(this);
    QGridLayout *layout    = new QGridLayout;
    QFrame *separator      = new QFrame();

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                       QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));


    lblMsg->setText(tr("Breakpoint on line: %1").arg(bp->lineNr()));
    lblEnable->setText(tr("Enable breakpoint"));
    ignoreFromLbl->setText(tr("Ignore after hits"));
    ignoreFromLbl->setToolTip(tr("0 = Disabled"));
    m_ignoreFromHits->setToolTip(ignoreFromLbl->toolTip());
    ignoreToLbl->setText(tr("Ignore hits up to"));
    conditionLbl->setText(tr("Condition"));
    conditionLbl->setToolTip(tr("Python code that evaluates to true triggers breakpoint"));
    m_condition->setToolTip(conditionLbl->toolTip());

    layout->addWidget(lblMsg, 0, 0, 1, 2);
    layout->addWidget(separator, 1, 0, 1, 2);
    layout->addWidget(lblEnable, 2, 0);
    layout->addWidget(m_enabled, 2, 1);
    layout->addWidget(conditionLbl, 3, 0, 1, 2);
    layout->addWidget(m_condition, 4, 0, 1, 2);
    layout->addWidget(ignoreToLbl, 5, 0);
    layout->addWidget(m_ignoreToHits, 5, 1);
    layout->addWidget(ignoreFromLbl, 6, 0);
    layout->addWidget(m_ignoreFromHits, 6, 1);
    layout->addWidget(buttonBox, 7, 0, 1, 2);
    setLayout(layout);

    m_enabled->setChecked(!m_bpl->disabled());
    m_ignoreFromHits->setValue(m_bpl->ignoreFrom());
    m_ignoreToHits->setValue(m_bpl->ignoreTo());
    m_condition->setText(m_bpl->condition());
}

PythonEditorBreakpointDlg::~PythonEditorBreakpointDlg()
{
}

void PythonEditorBreakpointDlg::accept()
{
    m_bpl->setDisabled(!m_enabled->isChecked());
    m_bpl->setCondition(m_condition->text());
    m_bpl->setIgnoreTo(m_ignoreToHits->value());
    m_bpl->setIgnoreFrom(m_ignoreFromHits->value());

    QDialog::accept();
}

// ------------------------------------------------------------------------

#include "moc_PythonEditor.cpp"
