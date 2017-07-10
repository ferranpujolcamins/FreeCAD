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
#include "EditorView.h"
#include "PythonCode.h"
#include "PythonDebugger.h"
#include "Application.h"
#include "BitmapFactory.h"
#include "Macro.h"
#include "FileDialog.h"
#include "DlgEditorImp.h"
#include "App/PropertyContainer.h"
#include "App/PropertyContainerPy.h"
//#include "CallTips.h"


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
#include <QMessageBox>

using namespace Gui;

namespace Gui {
struct PythonEditorP
{
    bool  autoIndented;
    int   debugLine;
    QRect debugRect;
    QPixmap breakpoint;
    QPixmap breakpointDisabled;
    QPixmap debugMarker;
    QString filename;
    PythonDebugger* debugger;
    PythonCode *pythonCode;
    QHash <int, Py::ExceptionInfo> exceptions;
    //CallTipsList* callTipsList;
    PythonMatchingChars* matchingChars;
    const QColor breakPointScrollBarMarkerColor = QColor(242, 58, 82); // red
    const QColor exceptionScrollBarMarkerColor = QColor(252, 172, 0); // red-orange
    PythonEditorCodeAnalyzer *codeAnalyzer;
    PythonEditorP()
        : autoIndented(false), debugLine(-1),
//          callTipsList(nullptr),
          codeAnalyzer(nullptr)
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

// -------------------------------------------------------------------------------------

class PythonEditorCodeAnalyzerP
{
public:
    PythonEditorCodeAnalyzerP(PythonEditor *editor) :
        editor(editor),
        completerModel(nullptr),
        popupPos(0),
        reparsedAt(-1)
    {
    }
    ~PythonEditorCodeAnalyzerP() {}
    PythonEditor *editor;
    static bool isActive;
    JediScript_ptr_t currentScript;
    PythonCompleterModel *completerModel;
    int popupPos,
        reparsedAt;
};
bool PythonEditorCodeAnalyzerP::isActive = false;

// ------------------------------------------------------------------------------------

class PythonCompleterModelP
{
public:
    PythonCompleterModelP()
    { }
    ~PythonCompleterModelP()
    { }
    JediCompletion_list_t currentList;
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
    connect(dbg, SIGNAL(breakpointRemoved(int,const BreakpointLine*)),
            this, SLOT(breakpointRemoved(int,const BreakpointLine*)));
    connect(dbg, SIGNAL(exceptionFatal(const Py::ExceptionInfo*)),
            this, SLOT(exception(const Py::ExceptionInfo*)));
    connect(dbg, SIGNAL(exceptionOccured(const Py::ExceptionInfo*)),
            this, SLOT(exception(const Py::ExceptionInfo*)));
    connect(dbg, SIGNAL(started()), this, SLOT(clearAllExceptions()));
    connect(dbg, SIGNAL(clearAllExceptions()), this, SLOT(clearAllExceptions()));
    connect(dbg, SIGNAL(clearException(QString,int)), this, SLOT(clearException(QString,int)));


    d->matchingChars = new PythonMatchingChars(this);

    // tooltips on this widget must use a monospaced font to display code info correctly
    setStyleSheet(QLatin1String("QToolTip {font-size:12pt; font-family:'DejaVu Sans Mono', Courier; }"));

    // code analyzer
    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (hPrefGrp->GetBool("EnableCodeAnalyzer", true))
        d->codeAnalyzer = new PythonEditorCodeAnalyzer(this);

//    // create the window for call tips
//    d->callTipsList = new CallTipsList(this);
//    d->callTipsList->setFrameStyle(QFrame::Box|QFrame::Raised);
//    d->callTipsList->setLineWidth(2);
//    installEventFilter(d->callTipsList);
//    viewport()->installEventFilter(d->callTipsList);
//    d->callTipsList->setSelectionMode( QAbstractItemView::SingleSelection );
//    d->callTipsList->hide();
}

/** Destroys the object and frees any allocated resources */
PythonEditor::~PythonEditor()
{
    getWindowParameter()->Detach( this );
    if (d->codeAnalyzer)
        delete d->codeAnalyzer;

    delete d;
}

const QString &PythonEditor::fileName() const
{
    return d->filename;
}

PythonEditorCodeAnalyzer *PythonEditor::codeAnalyzer() const
{
    return d->codeAnalyzer;
}

void PythonEditor::setCodeAnalyzer(PythonEditorCodeAnalyzer *analyzer)
{
    if (d->codeAnalyzer)
        delete d->codeAnalyzer;

    d->codeAnalyzer = analyzer;
}

void PythonEditor::setFileName(const QString& fn)
{
    if (fn != d->filename) {
        d->filename = fn;
        Q_EMIT fileNameChanged(fn);
    }
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

    d->debugger->stop();

    if (d->debugger->start()) {
        d->debugger->runFile(d->filename);
        //if (d) // if app gets closed during debugging halt, d is deleted
        //    d->debugger->stop();
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
    if (d->exceptions.contains(line)) {
        const Py::ExceptionInfo *exp = &d->exceptions[line];
        QIcon icon = BitmapFactory().iconFromTheme(exp->iconName());
        p->drawPixmap(x +d->breakpoint.width(), y,
                      icon.pixmap(d->breakpoint.height()));
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

    bool popupShown = completer() && completer()->popup()->isVisible();
    if (popupShown)
        d->autoIndented = false;

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
        if (d->autoIndented) {
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
                d->autoIndented = false;
            }

            cursor.endEditBlock();
        } else {
            TextEditor::keyPressEvent(e);
        }

    } break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    {
        if (popupShown) {
            e->ignore();
            break;
        }
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
            d->autoIndented = true;
        } else {
            TextEditor::keyPressEvent(e);
        }
    }   break;

    case Qt::Key_Period:
    {
        d->autoIndented = false;

        // In Qt 4.8 there is a strange behaviour because when pressing ":"
        // then key is also set to 'Period' instead of 'Colon'. So we have
        // to make sure we only handle the period.
        if (e->text() == QLatin1String(".") && cursor != inputLineBegin) {
//            // analyse context and show available call tips
//            int contextLength = cursor.position() - inputLineBegin.position();
            TextEditor::keyPressEvent(e);
//            d->callTipsList->showTips( inputLine.left( contextLength ) );
        }
        else {
            TextEditor::keyPressEvent(e);
        }
    }   break;
    case Qt::Key_Tab:
        if (popupShown) {
            e->ignore();
            break;
        }
        TextEditor::keyPressEvent(e);
        break;
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

        d->autoIndented = false;
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

    // calltips
    if (d->codeAnalyzer)
        d->codeAnalyzer->keyPressed(e);
}

bool PythonEditor::editorToolTipEvent(QPoint pos, const QString &textUnderPos)
{
    if (textUnderPos.isEmpty()) {
        QToolTip::hideText();
    } else {
        QString str = d->pythonCode->findFromCurrentFrame(textUnderPos);
        QToolTip::showText(pos, str, this);
    }
    return true;
}

bool PythonEditor::lineMarkerAreaToolTipEvent(QPoint pos, int line)
{
    if (d->exceptions.contains(line)) {
        QString srcText = d->exceptions[line].text();
        int offset = d->exceptions[line].offset();
        if (offset > 0) {
            if (!srcText.endsWith(QLatin1String("\n")))
                srcText += QLatin1String("\n");
            for (int i = 0; i < offset -1; ++i) {
                srcText += QLatin1Char('-');
            }
            srcText += QLatin1Char('^');
        }

        QString txt = QString((tr("%1 on line %2\nreason: '%4'\n\n%5")))
                                .arg(d->exceptions[line].typeString())
                                .arg(QString::number(d->exceptions[line].lineNr()))
                                .arg(d->exceptions[line].message())
                                .arg(srcText);
        QToolTip::showText(pos, txt, this);
    } else {
        QToolTip::hideText();
    }
    return true;
}

void PythonEditor::markerAreaContextMenu(int line, QContextMenuEvent *event)
{
    QMenu menu;
    QAction *clearException = nullptr;

    if (d->exceptions.contains(line)) {
        clearException = new QAction(BitmapFactory().iconFromTheme(d->exceptions[line].iconName()),
                               tr("Clear exception"), &menu);
        menu.addAction(clearException);
        menu.addSeparator();
    }

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
        } else if (res == clearException) {
            PythonDebugger::instance()->sendClearException(d->filename, line);
        }

    } else {
        QAction create(BitmapFactory().iconFromTheme("breakpoint"),
                       tr("Add breakpoint"), this);
        menu.addAction(&create);
        QAction *res = menu.exec(event->globalPos());
        if (res == &create)
            d->debugger->setBreakpoint(d->filename, line);
        else if (res == clearException)
            PythonDebugger::instance()->sendClearException(d->filename, line);
    }

    if (clearException)
        delete clearException;

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

void PythonEditor::exception(const Py::ExceptionInfo *exc)
{
    if (exc->fileName() != d->filename)
        return;

    if (d->exceptions.contains(exc->lineNr()))
        return; // already set

    d->exceptions[exc->lineNr()] = *exc;
    renderExceptionExtraSelections();
    getMarkerArea()->update();

    AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
    if (vBar)
        vBar->setMarker(exc->lineNr(), d->exceptionScrollBarMarkerColor);

    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (hPrefGrp->GetBool("EnableScrollToExceptionLine", true)) {
        // scroll into view on exceptions
        PythonEditorView *editView = PythonEditorView::setAsActive();
        if (!editView)
            return;

        editView->open(d->filename);

        // scroll to view
        QTextCursor cursor(editView->editor()->document()->
                           findBlockByLineNumber(exc->lineNr() - 1)); // ln-1 because line number starts from 0
        editView->editor()->setTextCursor(cursor);
    }
}

void PythonEditor::clearAllExceptions()
{
    for (int i : d->exceptions.keys()) {
        Py::ExceptionInfo exc = d->exceptions.take(i);
        AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
        if (vBar)
            vBar->setMarker(exc.lineNr(), d->exceptionScrollBarMarkerColor);
    }

    renderExceptionExtraSelections();
    getMarkerArea()->update();
}

void PythonEditor::clearException(const QString &fn, int line)
{
    if (fn == d->filename && d->exceptions.contains(line)) {
        d->exceptions.remove(line);
        renderExceptionExtraSelections();
        getMarkerArea()->update();

        AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
        if (vBar)
            vBar->setMarker(line, d->exceptionScrollBarMarkerColor);
    }
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

// render the underlining for syntax errors in editor
void PythonEditor::renderExceptionExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections;

    for (Py::ExceptionInfo &exc : d->exceptions) {
        if (exc.offset() > 0) {
            // highlight text up to error in editor
            QTextEdit::ExtraSelection sel;
            sel.cursor = textCursor();
            QTextBlock block = document()->findBlockByLineNumber(exc.lineNr() -1);
            sel.cursor.setPosition(block.position());
            sel.cursor.setPosition(sel.cursor.position() + exc.offset(),
                               QTextCursor::KeepAnchor);

            sel.format.setBackground(QColor("#e5e5de")); // lightgray
            sel.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            sel.format.setUnderlineColor(QColor("#ff1635")); // red

            selections.append(sel);
        }
    }

    setTextMarkers(QLatin1String("exceptionStyling"), selections);
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


PythonEditorCodeAnalyzer::PythonEditorCodeAnalyzer(PythonEditor *editor) :
    QObject(editor), WindowParameter("Editor"),
    d( new PythonEditorCodeAnalyzerP(editor))
{
    ParameterGrp::handle hPrefGrp = getWindowParameter();
    bool enable = hPrefGrp->GetBool("EnableCodeAnalyzer", true);
    static bool checkedForJedi = false;

    if (enable) {
        if (!checkedForJedi) {
            Base::Subject<const char *> caller;
            OnChange(caller, "EnableCodeAnalyzer");
            checkedForJedi = true;
        }

        if (d->isActive) {
            createCompleter();
        }
    }

    if (d->isActive) {
        connect(editor, SIGNAL(fileNameChanged(QString)), this, SLOT(parseDocument()));

    }
}

PythonEditorCodeAnalyzer::~PythonEditorCodeAnalyzer()
{
    getWindowParameter()->Detach(this);
    delete d;
}

void PythonEditorCodeAnalyzer::OnChange(Base::Subject<const char *> &rCaller,
                                        const char *rcReason)
{
    Q_UNUSED(rCaller);
    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (strcmp(rcReason, "EnableCodeAnalyzer") == 0) {
        if (hPrefGrp->GetBool("EnableCodeAnalyzer", true)) {
            d->isActive = JediInterpreter::instance()->isValid();
            if (!d->isActive) {
                QMessageBox::warning(d->editor, tr("Python editor"),
                                     tr("<h1>Jedi doesnt work!</h1>Are you sure it's installed correctly?"
                                        "<br/> &nbsp;&nbsp; run:<i><b>pip install jedi</b></i> on command line"));

            } else {
                createCompleter();
                // we need to be sure that we only have one filter installed
                d->editor->removeEventFilter(this);
                d->editor->installEventFilter(this);
            }

        } else {
            d->isActive = false;
            d->editor->removeEventFilter(this);
        }
    }
}

void PythonEditorCodeAnalyzer::parseDocument()
{
    if (!d->isActive)
        return;

    QTextDocument *doc = d->editor->document();
    QTextCursor cursor = d->editor->textCursor();
    QTextBlock block = doc->findBlock(cursor.position());
    int lineNr = block.firstLineNumber() +1;
    int column = cursor.position() - block.position();
    d->currentScript.reset(new JediScriptObj(doc->toPlainText(), lineNr,
                                             column, d->editor->fileName()));
}

bool PythonEditorCodeAnalyzer::eventFilter(QObject *obj, QEvent *event)
{
    // This is a trick to avoid to hide the tooltip window after the defined time span
    // of 10 seconds. We just filter out all timer events to keep the label visible.
    QLabel* label = qobject_cast<QLabel*>(obj);

    // Ignore the timer events to prevent from being closed
    if (label && label->windowFlags() & Qt::ToolTip && event->type() == QEvent::Timer)
        return true;

    if  (obj == d->editor) {
        // Ot doesnt let me snatch keypress originating from Completer
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *e = static_cast<QKeyEvent*>(event);
            if (e->text().isEmpty() && e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))
            {
                // popup on shift + space
                keyPressed(e);
                return true;
            }
        }

        // other editor filters here

    } else if  (obj == d->editor->completer()->popup()) {

        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *e = static_cast<QKeyEvent*>(event);

            if (e->text().isEmpty() && e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))
            {
//                // stop event here
//                //popupChoiceSelected(d->editor->completer()->currentIndex());
//                //d->editor->completer()->popup()->hide();
                event->ignore();
                return true; // prevent new line when we select from completer popup
            }
        }
        // popup events here
    }

    return false; // default is to allow events through
}

bool PythonEditorCodeAnalyzer::keyPressed(const QKeyEvent *e)
{
    if (!d->isActive || !d->completerModel)
        return false; // allow event to reach editor

    int key = e->key();
    bool forcePopup = e->modifiers() & Qt::ShiftModifier && key == Qt::Key_Space;

    // do nothing if ctrl or shift on ther own
    if (!forcePopup && (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) &&
        e->text().isEmpty())
        return false;

    if (!forcePopup && e->text().isEmpty())
        return false;


    // find out what we have beneath cursor,
    // if its a word (2 wordchars or more) and popup not shown -> parse and popup
    // if its not and we are popup-ed, hide popup
    QTextCursor cursor = d->editor->textCursor();

    // find out how many ch it is in current word from cursor position
    // we cant use findWordUnderCursor exacliy because of it selects the whole word
    int startPos = cursor.position();
    if (!e->text().isEmpty())
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor); // select just typed chr
    cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
    QString prefix = cursor.selectedText();
    int pos = cursor.position();
    int chrTyped = prefix.size();
    QChar startChar;
    if (chrTyped > 0)
        startChar = prefix[chrTyped -1];

    // get completions when we type '.'
    if (!forcePopup)
        forcePopup = e->text() == QLatin1String(".");

    // optimization, only open on the second ch
    static const int autoPopupCnt = 2;


    if (startChar.isLetterOrNumber() || forcePopup) {
        /*if (forcePopup || // force by Ctrl + Space
            (chrTyped == autoPopupCnt && startPos >= d->popupPos) ||  // initial open
            (shown && startPos < d->popupPos)) // popup open but we have erased some of the starting chars
        {
            // rationale here is to parse on initial popup
            // reparse each 4th char, as Jedi skips list if its to big
            // if we step back we need to reparse each char to fill up the list again
            if (d->popupPos == 0) { // initial opening
//                parseDocument();// runs in python, potentially slow?
                d->popupPos = startPos;
//                d->completerModel->setCompletions(d->currentScript->completions());

            } else if ((startPos > d->reparsedAt && chrTyped % 4 == 0) ||
                       (startPos < d->reparsedAt)) {
                // type forward backward with popup shown
//                parseDocument(); // runs in python, potentially slow?
                d->reparsedAt = startPos;
//                d->completerModel->setCompletions(d->currentScript->completions());
            }
        }*/
        parseDocument();// runs in python, potentially slow?
        d->completerModel->setCompletions(d->currentScript->completions());


        // TODO arguments suggestions?

        // set prefix
        cursor.setPosition(pos+2);
        cursor.setPosition(startPos+1, QTextCursor::KeepAnchor);
        QString completionPrefix = cursor.selectedText() + startChar;
        d->editor->completer()->setCompletionPrefix(completionPrefix);

        std::cout << /*"rowsProxy " << d->proxyModel->rowCount() <<*/ " prefix:" <<
                      completionPrefix.toStdString() << " [0]:" <<
                     /*d->currentScript->completions()[0]->name().toStdString()<<*/  std::endl;

        std::cout << "rowCompleter:" << d->editor->completer()->completionCount() << " model:"<<d->completerModel->rowCount();

        complete();

    } else {
        hide();
    }

    return false;
}

void PythonEditorCodeAnalyzer::popupChoiceSelected(const QModelIndex &idx)
{
    QTextCursor cursor(d->editor->textCursor());
    JediBaseDefinitionObj *def = d->completerModel->getCompletion(idx.row()).get();
    QString txt = def->name();
    int pos = cursor.position();
    if (pos > 0) {
        QChar triggerCh = d->editor->document()->characterAt(pos -1);
        if (!triggerCh.isLetterOrNumber())
            txt.prepend(triggerCh); // such as '.' completion
    }
    int insertLen = txt.size() - d->editor->completer()->completionPrefix().size();
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    cursor.setPosition(pos);
    cursor.insertText(txt.right(insertLen));

    QString type = def->type();
    if (type == QLatin1String("function")) {
        cursor.insertText(QLatin1String("()"));
        cursor.movePosition(QTextCursor::Left);
        d->editor->setTextCursor(cursor); // must to move backward
        // completion for call signatures
        parseDocument();
        d->completerModel->setCompletions(d->currentScript->completions());
        complete();
    }
}

void PythonEditorCodeAnalyzer::popupChoiceHighlighted(const QModelIndex &idx)
{
    // tooltips
    if (!idx.isValid()) {
        QToolTip::hideText();
        return;
    }

    JediBaseDefinition_ptr_t def = d->completerModel->getCompletion(idx.row());
    if (!def)
        return;

    QPoint pos = d->editor->completer()->popup()->pos();
    pos.rx() += d->editor->completer()->popup()->width();

    // for debug only, popup lock out all events so we cant debug
    //d->editor->completer()->popup()->hide();

    QToolTip::showText(pos, buildToolTipText(def), d->editor->completer()->popup());
}

void PythonEditorCodeAnalyzer::createCompleter()
{
    if (d->editor->completer() == nullptr) {
        // create a new completer
        QCompleter *completer = new QCompleter(d->editor);
//        completer->setWidget(d->editor);
        d->completerModel = new PythonCompleterModel(completer);
        completer->setModel(d->completerModel);
        d->editor->setCompleter(completer);
        completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        completer->setModelSorting(QCompleter::UnsortedModel); // Jedi sorts it?

        completer->popup()->installEventFilter(this);
        d->editor->removeEventFilter(this);
        d->editor->installEventFilter(this);

        connect(completer, SIGNAL(activated(const QModelIndex&)),
                this, SLOT(popupChoiceSelected(const QModelIndex&)));
        connect(completer, SIGNAL(highlighted(const QModelIndex&)),
                this, SLOT(popupChoiceHighlighted(const QModelIndex)));
    }
}

void PythonEditorCodeAnalyzer::complete()
{
    // set location in widget
    QRect completerRect = d->editor->cursorRect();
    completerRect.setWidth(d->editor->completer()->popup()->sizeHintForColumn(0) +
                           d->editor->completer()->popup()->verticalScrollBar()->sizeHint().width());

    // popup and search within internal data for prefix
    d->editor->completer()->complete(completerRect);

    if(!d->editor->completer()->popup()->isVisible()) {
        // install this object to filter timer events for the tooltip label
        qApp->installEventFilter(this);
    }
}

void PythonEditorCodeAnalyzer::hide()
{
    d->popupPos = 0;
    d->reparsedAt = -1;

    if (d->editor->completer()->popup()->isVisible()) {
        d->editor->completer()->popup()->hide();
        qApp->removeEventFilter(this);
    }
}

const QString PythonEditorCodeAnalyzer::buildToolTipText(JediBaseDefinition_ptr_t def)
{
    if (!def)
        return QString();

    bool isBuiltIn = def->in_builtin_module();

    // try several different ways to get a docstring
    QString docStr = def->docstring();

    if (docStr.isEmpty()) {
        // get docstring from definitions in script
        QString name = def->name();
        for (JediDefinition_ptr_t df :  d->currentScript->goto_definitions()) {
            if (name == df->name()){
                QString str = df->docstring();
                if (!str.isEmpty()) {
                    docStr = str;
                    break;
                }
            }
        }
    }

    if (docStr.isEmpty() && isBuiltIn) {
        // maybe a frecad object?, get doc from interpreter
        JediInterpreter::SwapIn jedi;

        try {
            // first get the obj names up to root
            QStringList names;
            JediBaseDefinition_ptr_t obj = def;
            QString name = def->name();

            while(obj && !name.isEmpty()) {
                names.prepend(name);
                obj = obj->parent();
                if (obj->pyObj().isNone())
                    break;
                name = obj->name();
            }

            // get getobject from Py in reversed order
            Py::Object parentObj = Py::Module("__main__");
            Py::Object foundObj;
            for (const QString &n : names) {
                Py::String attr(n.toStdString());
                if (!parentObj.hasAttr(attr))
                    break;
                foundObj = parentObj.getAttr(attr);
                if (foundObj.isNull())
                    break;
                parentObj = foundObj;
            }

            // it seems to throw here on mro objects?
            if (!foundObj.isNull() && foundObj.hasAttr("__doc__"))
            {
                Py::Object doc(foundObj.getAttr("__doc__"));
                if (doc.isString())
                    docStr += QString::fromStdString(Py::String(doc).as_std_string());
            }

        } catch(Py::Exception) { /* quiet */
            PyErr_Print();
            PyErr_Clear();
        }
    }

    if (docStr.isEmpty())
        docStr = def->docstring(false, false); // try again without fast

    if (docStr.isEmpty())
        docStr = def->docstring(true, false); // take it raw

    QString typeStr = QString(QLatin1String("%1: %2\nin module '%3'")).arg(def->type())
                                                          .arg(def->description())
                                                          .arg(def->module_name());
    if (isBuiltIn)
        typeStr += QLatin1String(" [built-in]");

    QString txt = QString(QLatin1String("%1\n%2")).arg(typeStr)
                                                  .arg(docStr);
    return txt;
}


// ------------------------------------------------------------------------


PythonCompleterModel::PythonCompleterModel(QObject *parent) :
    QAbstractListModel(parent), d(new PythonCompleterModelP)
{
}

PythonCompleterModel::~PythonCompleterModel()
{
    delete d;
}

void PythonCompleterModel::setCompletions(JediCompletion_list_t newList)
{
    if (!d->currentList.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, d->currentList.size());
        d->currentList.clear();
        endRemoveRows();
    }

    if (!newList.isEmpty()) {
        beginInsertRows(QModelIndex(), 0, newList.size());
        d->currentList = newList;
        endInsertRows();
    }
}

QVariant PythonCompleterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() > 0 ||
        index.row() > d->currentList.size() -1)
    {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        JediBaseDefinitionObj *def = d->currentList.at(index.row()).get();
        return def->name();

    } else if (role == Qt::DecorationRole) {
        return QVariant(); // TODO implement
    }

    return QVariant();
}

int PythonCompleterModel::rowCount(const QModelIndex &parent) const
{
    if (d->currentList.isEmpty() || parent.isValid())
        return 0;

    return d->currentList.size();
}

void PythonCompleterModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, d->currentList.size() -1);
    d->currentList.clear();
    endRemoveRows();
}

JediBaseDefinition_ptr_t PythonCompleterModel::getCompletion(int at)
{
    if (at < 0 || d->currentList.size() < at)
        return JediCompletion_ptr_t();
    return d->currentList.at(at);
}


// -------------------------------------------------------------------------


#include "moc_PythonEditor.cpp"
