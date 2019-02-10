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
#include <QTimer>

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
    QHash <int, Base::PyExceptionInfo> exceptions;
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
        reparsedAt(-1),
        callSignatureWgt(nullptr),
        taskAfterReparse(Nothing),
        parseTimeoutMs(300)
    {
    }
    ~PythonEditorCodeAnalyzerP() {
        if (callSignatureWgt != nullptr)
            delete callSignatureWgt;
    }
    enum DoAfterParse { Nothing, Completer, CallSignature, Both };
    PythonEditor *editor;
    static bool isActive;
    JediScript_ptr_t currentScript;
    PythonCompleterModel *completerModel;
    int popupPos,
        reparsedAt;
    QTimer parseTimer;
    PythonCallSignatureWidget *callSignatureWgt;
    DoAfterParse taskAfterReparse;
    std::unique_ptr<QKeyEvent> keyEvent;
    int parseTimeoutMs;
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
    PythonEditorCodeAnalyzer *analyzer;
};

// -----------------------------------------------------------------------------------

class PythonCallSignatureWidgetP
{
public:
    PythonCallSignatureWidgetP(PythonEditorCodeAnalyzer *analyzer):
        analyzer(analyzer), candidateIdx(-1),
        currentParamIdx(-1), userSelectedSignature(-1),
        lineNr(0)
    {
    }
    ~PythonCallSignatureWidgetP()
    {
    }
    JediCallSignature_list_t signatures;
    PythonEditorCodeAnalyzer *analyzer;
    int candidateIdx;
    int currentParamIdx;
    int userSelectedSignature;
    int lineNr;
    QString functionName;
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
    hl->loadSettings();
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

    connect(lineMarkerArea(), SIGNAL(contextMenuOnLine(int,QContextMenuEvent*)),
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
    connect(dbg, SIGNAL(exceptionFatal(Base::PyExceptionInfo*)),
            this, SLOT(exception(Base::PyExceptionInfo*)));
    connect(dbg, SIGNAL(exceptionOccured(Base::PyExceptionInfo*)),
            this, SLOT(exception(Base::PyExceptionInfo*)));
    connect(dbg, SIGNAL(started()), this, SLOT(clearAllExceptions()));
    connect(dbg, SIGNAL(clearAllExceptions()), this, SLOT(clearAllExceptions()));
    connect(dbg, SIGNAL(clearException(QString,int)), this, SLOT(clearException(QString,int)));

    MacroManager *macroMgr = Application::Instance->macroManager();
    connect(macroMgr, SIGNAL(exceptionFatal(Base::PyExceptionInfo*)),
            this, SLOT(exception(Base::PyExceptionInfo*)));

    d->matchingChars = new PythonMatchingChars(this);

    // tooltips on this widget must use a monospaced font to display code info correctly
    setStyleSheet(QLatin1String("QToolTip {font-size:12pt; font-family:'DejaVu Sans Mono', Courier; }"));

    // code analyzer
    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (hPrefGrp->GetBool("EnableCodeAnalyzer", true))
        d->codeAnalyzer = new PythonEditorCodeAnalyzer(this);
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
    } else {
        // probably a color change
        syntaxHighlighter()->loadSettings();
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
    lineMarkerArea()->update();
}

void PythonEditor::showDebugMarker(int line)
{
    d->debugLine = line;
    lineMarkerArea()->update();

    QTextCursor cursor = textCursor();
    cursor.setPosition(document()->findBlockByNumber(line - 1).position());
    setTextCursor(cursor);

    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (hPrefGrp->GetBool("EnableCenterDebugmarker", true)) {
        // debugmarker stays centered text jumps around
        centerCursor();
    } else {
        // scroll into view, line jumps around, text stays (if possible)
        ensureCursorVisible();
    }
}

void PythonEditor::hideDebugMarker()
{
    d->debugLine = -1;
    lineMarkerArea()->update();
}

void PythonEditor::drawMarker(int line, int x, int y, QPainter* p)
{
    BreakpointLine *bpl = d->debugger->getBreakpointLine(d->filename, line);
    bool hadBreakpoint = false;
    if (bpl != nullptr) {
        if (bpl->disabled())
            p->drawPixmap(x, y, d->breakpointDisabled);
        else
            p->drawPixmap(x, y, d->breakpoint);
        hadBreakpoint = true;
    }
    if (d->exceptions.contains(line)) {
        Base::PyExceptionInfo *exc = &d->exceptions[line];
        PyExceptionInfoGui excGui(exc);
        QIcon icon = BitmapFactory().iconFromTheme(excGui.iconName());
        int excX = x;
        if (lineMarkerArea()->lineNumbersVisible() && hadBreakpoint)
            excX += d->breakpoint.width();
        p->drawPixmap(excX, y, icon.pixmap(d->breakpoint.height()));
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
            // don't move breakpoint if we are at line end
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
        bool block = false;

        static QString previousKeyText;

        if (e->key() == Qt::Key_ParenLeft) {
            if (nextChr != QLatin1String(")"))
                insertAfter = QLatin1String(")");
        } else if (e->key() == Qt::Key_ParenRight) {
            if (previousKeyText == QLatin1String("("))
                block = true;
        } else if (e->key() == Qt::Key_BraceLeft) {
            if (nextChr != QLatin1String("}"))
                insertAfter = QLatin1String("}");
        } else if (e->key() == Qt::Key_BraceRight) {
            if (previousKeyText == QLatin1String("["))
                block = true;
        } else if (e->key() == Qt::Key_BracketLeft) {
            if (nextChr != QLatin1String("]"))
                insertAfter = QLatin1String("]");
        } else if (e->key() == Qt::Key_BracketRight) {
            if (previousKeyText == QLatin1String("{"))
                block = true;
        } else if (e->key() == Qt::Key_QuoteDbl) {
            if (previousKeyText == QLatin1String("\"")) {
                block = true;
                clearPrevious = true;
            } else if (nextChr != QLatin1String("\""))
                insertAfter = QLatin1String("\"");
        } else if (e->key() == Qt::Key_Apostrophe) {
            if (previousKeyText == QLatin1String("'")) {
                block = true;
                clearPrevious = true;
            } else if (nextChr != QLatin1String("'"))
                insertAfter = QLatin1String("'");
        }

        d->autoIndented = false;
        if (block) {
            cursor = textCursor();
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
        } else
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
    } else if (d->debugger->isHalted()) {
        QTextCursor cursor = cursorForPosition(pos);
        PythonTextBlockData *textData = PythonTextBlockData::blockDataFromCursor(cursor);
        if (!textData || !textData->isCodeLine())
            return false;

        QString name = textData->tokenAtAsString(cursor);
        if (name.isEmpty())
            return false;

        // TODO figure out how to extract attribute / item chain for objects, dicts and lists
        // using tokens from syntaxhighlighter

        QString str = d->pythonCode->findFromCurrentFrame(textUnderPos);
        QToolTip::showText(pos, str, this);
    }
    return true;
}

bool PythonEditor::lineMarkerAreaToolTipEvent(QPoint pos, int line)
{
    if (d->exceptions.contains(line)) {
        QString srcText = QString::fromStdWString(d->exceptions[line].text());
        int offset = d->exceptions[line].getOffset();
        if (offset > 0) {
            if (!srcText.endsWith(QLatin1String("\n")))
                srcText += QLatin1String("\n");
            for (int i = 0; i < offset -1; ++i) {
                srcText += QLatin1Char('-');
            }
            srcText += QLatin1Char('^');
        }

        QString txt = QString((tr("%1 on line %2\nreason: '%4'\n\n%5")))
                                .arg(QString::fromStdString(d->exceptions[line].getErrorType(true)))
                                .arg(QString::number(d->exceptions[line].getLine()))
                                .arg(QString::fromStdString(d->exceptions[line].getMessage()))
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
        Base::PyExceptionInfo exc(d->exceptions[line]);
        PyExceptionInfoGui excGui(&exc);
        clearException = new QAction(BitmapFactory().iconFromTheme(excGui.iconName()),
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

    lineMarkerArea()->update();
}

void PythonEditor::breakpointChanged(const BreakpointLine *bpl)
{
    if (bpl->parent()->fileName() != d->filename)
        return;

    lineMarkerArea()->update();
}

void PythonEditor::breakpointRemoved(int idx, const BreakpointLine *bpl)
{
    Q_UNUSED(idx);

    if (bpl->parent()->fileName() != d->filename)
        return;

    AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
    if (vBar)
        vBar->clearMarker(bpl->lineNr(), d->breakPointScrollBarMarkerColor);

    lineMarkerArea()->update();
}

void PythonEditor::exception(Base::PyExceptionInfo *exc)
{
    if (exc->getFile() != d->filename.toStdString())
        return;

    int linenr = exc->getLine();
    if (d->exceptions.contains(linenr))
        return; // already set

    d->exceptions[linenr] = *exc;
    renderExceptionExtraSelections();
    lineMarkerArea()->update();

    AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
    if (vBar)
        vBar->setMarker(linenr, d->exceptionScrollBarMarkerColor);

    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (hPrefGrp->GetBool("EnableScrollToExceptionLine", true)) {
        // scroll into view on exceptions
        PythonEditorView *editView = PythonEditorView::setAsActive();
        if (!editView)
            return;

        editView->open(d->filename);

        // scroll to view
        QTextCursor cursor(editView->getEditor()->document()->
                           findBlockByLineNumber(linenr - 1)); // ln-1 because line number starts from 0
        editView->getEditor()->setTextCursor(cursor);
    }
}

void PythonEditor::clearAllExceptions()
{
    for (int i : d->exceptions.keys()) {
        Base::PyExceptionInfo exc = d->exceptions.take(i);
        AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
        if (vBar)
            vBar->clearMarker(exc.getLine(), d->exceptionScrollBarMarkerColor);
    }

    renderExceptionExtraSelections();
    lineMarkerArea()->update();
}

void PythonEditor::clearException(const QString &fn, int line)
{
    if (fn == d->filename && d->exceptions.contains(line)) {
        d->exceptions.remove(line);
        renderExceptionExtraSelections();
        lineMarkerArea()->update();

        AnnotatedScrollBar *vBar = qobject_cast<AnnotatedScrollBar*>(verticalScrollBar());
        if (vBar)
            vBar->clearMarker(line, d->exceptionScrollBarMarkerColor);
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

    for (Base::PyExceptionInfo &exc : d->exceptions) {
        if (exc.getOffset() > 0) {
            // highlight text up to error in editor
            QTextEdit::ExtraSelection sel;
            sel.cursor = textCursor();
            QTextBlock block = document()->findBlockByLineNumber(exc.getLine() -1);
            sel.cursor.setPosition(block.position());
            sel.cursor.setPosition(sel.cursor.position() + exc.getOffset(),
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
            blockIndent = -1; // don't indent this row
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
            createWidgets();
        }
    }

    if (d->isActive) {
        d->parseTimer.start(d->parseTimeoutMs);
    }
}

PythonEditorCodeAnalyzer::~PythonEditorCodeAnalyzer()
{
    getWindowParameter()->Detach(this);
    delete d;
}

JediScript_ptr_t PythonEditorCodeAnalyzer::currentScriptObj() const
{
    return d->currentScript;
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
                                     tr("<h1>Jedi doesn't work!</h1>Are you sure it's installed correctly?"
                                        "<br/> &nbsp;&nbsp; run:<i><b>pip install jedi</b></i> on command line"
                                        "<br/><a href='https://pypi.python.org/pypi/jedi/'>https://pypi.python.org/pypi/jedi</a>"));

            } else {
                createWidgets();
                // we need to be sure that we only have one filter installed
                d->editor->removeEventFilter(this);
                d->editor->installEventFilter(this);
            }

        } else {
            d->isActive = false;
            d->editor->removeEventFilter(this);
        }
    } else if(strcmp(rcReason, "PopupTimeoutTime") == 0) {
        d->parseTimeoutMs = hPrefGrp->GetInt("PopupTimeoutTime", 300);
    }
}

QIcon PythonEditorCodeAnalyzer::getIconForDefinition(JediBaseDefinition_ptr_t def,
                                                     bool allowMarkers,
                                                     int recursionGuard)
{
    QSize iconSize(64, 64);
    if (!def && recursionGuard > 10)
        return QPixmap(iconSize);

    JediBaseDefinitionObj *obj = def.get();

    QString name = obj->name();
    QString type = obj->type();
    QString module_name = obj->module_name();

    QStringList imgLayers;
    if(type == QLatin1String("statement")) {
        JediDefinition_list_t lookups;

        if (obj->isCompletionObj())
            lookups = d->currentScript->goto_definitions();
        else
            lookups = obj->goto_definitions();

        for (JediBaseDefinition_ptr_t lookObj : lookups) {
            if (lookObj->type() != QLatin1String("statement")) {
                QIcon icon = getIconForDefinition(lookObj, false, ++recursionGuard); // recursive
                if (!icon.isNull())
                    return icon;
            }
        }

    } else if (type == QLatin1String("function")) {
        // class or member?
        // TODO implement loockup backwards in document
        // paint backlayer
        imgLayers <<  QLatin1String("ClassBrowser/function.svg");

    } else if (type == QLatin1String("class")) {
        imgLayers << QLatin1String("ClassBrowser/type_class.svg");

    } else if (type == QLatin1String("instance")) {
        // find out what type it is can be quite tricky
        if (name == QLatin1String("int"))
            imgLayers << QLatin1String("ClassBrowser/type_int.svg");
        else if (name == QLatin1String("float"))
            imgLayers << QLatin1String("ClassBrowser/type_float.svg");
        else if (name == QLatin1String("list"))
            imgLayers << QLatin1String("ClassBrowser/type_list.svg");
        else if (name == QLatin1String("False") ||
                 name == QLatin1String("True"))
            imgLayers << QLatin1String("ClassBrowser/type_bool.svg");
        else if (name == QLatin1String("tuple"))
            imgLayers << QLatin1String("ClassBrowser/type_tuple.svg");
        else if (name == QLatin1String("dict"))
            imgLayers << QLatin1String("ClassBrowser/type_dict.svg");
        else if (name == QLatin1String("NoneType") ||
                 name == QLatin1String("None"))
            imgLayers << QLatin1String("ClassBrowser/type_none.svg");
        else if (name == QLatin1String("string"))
            imgLayers << QLatin1String("ClassBrowser/type_string.svg");
        else
            // default to variable
            imgLayers << QLatin1String("ClassBrowser/variable.svg");

    } else if (type == QLatin1String("module")) {
        imgLayers << QLatin1String("ClassBrowser/type_module.svg");

    } else if (type == QLatin1String("keyword")) {
        imgLayers << QLatin1String("ClassBrowser/type_keyword.svg");
        allowMarkers = false;
    }

    // if not set its unknown
    if (imgLayers.isEmpty()) {
        imgLayers << QLatin1String("ClassBrowser/type_unknown.svg");
    }

    // add extra markers
    if (allowMarkers) {
        QFileInfo fi_scr(d->editor->fileName());
        if (module_name != fi_scr.fileName())
            imgLayers << QLatin1String("ClassBrowser/imported_marker.svg");
        if (obj->in_builtin_module())
            imgLayers << QLatin1String("ClassBrowser/builtin_marker.svg");
    }

    // already in cache?
    const QStringList pixmapCache = BitmapFactory().pixmapNames();
    if (pixmapCache.contains(imgLayers.join(QLatin1String("|"))))
        return QIcon(BitmapFactory().pixmap(imgLayers.join(QLatin1String("|")).toLatin1()));

    // create the new icon and add it to cache
    QPixmap img;
    for (int i = 0; i < imgLayers.size(); ++i) {
        if (i == 0)
            img = BitmapFactory().pixmap(imgLayers[i].toLatin1());
        else {
            img = BitmapFactory().merge(img, BitmapFactory().pixmap(imgLayers[i].toLatin1()));
        }
    }
    //img = img.scaledToWidth(32);

    BitmapFactory().addPixmapToCache(imgLayers.join(QLatin1String("|")).toLatin1(), img);

    return QIcon(img);
}

PythonEditor *PythonEditorCodeAnalyzer::editor() const
{
    return d->editor;
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
    if (d->taskAfterReparse == d->Completer || d->taskAfterReparse == d->Both)
        complete();
    if (d->taskAfterReparse == d->CallSignature || d->taskAfterReparse == d->Both)
        d->callSignatureWgt->afterKeyPressed(d->keyEvent.get());

    d->taskAfterReparse = d->Nothing;
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
        // Qt doesn't let me snatch keypress originating from Completer
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
        if (event->type() == QEvent::Hide)
            hideCompleter(); // and stop timer

        // other popup events here
    }

    return false; // default is to allow events through
}

bool PythonEditorCodeAnalyzer::keyPressed(QKeyEvent *e)
{
    if (!d->isActive || !d->completerModel)
        return false; // allow event to reach editor

    int key = e->key();
    bool forcePopup = e->modifiers() & Qt::ShiftModifier && key == Qt::Key_Space;

    bool bailout = false;

    // do nothing if ctrl or shift on their own
    if (!forcePopup && (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) &&
        e->text().isEmpty())
        bailout = true;

    if (!forcePopup && e->text().isEmpty())
        bailout = true;

    // store for later use in a smartpointer
    d->keyEvent.reset(new QKeyEvent(e->type(), e->key(), e->modifiers(),
                                e->text(), e->isAutoRepeat(), e->count()));

    if (bailout) {
        // reparse for call signature
        d->taskAfterReparse = d->CallSignature;
        d->parseTimer.start(d->parseTimeoutMs);
        return false;
        //parseDocument();
        //return d->callSignatureWgt->afterKeyPressed(e);
    }


    // find out what we have beneath cursor,
    // if its a word (2 wordchars or more) and popup not shown -> parse and popup
    // if its not and we are popup-ed, hide popup
    QTextCursor cursor = d->editor->textCursor();
    int startPos = cursor.position();
    int posInLine = startPos - cursor.block().position();

    // check if this is a token we care about
    if (!cursor.block().isValid())
        return false;
    PythonTextBlockData *textData = reinterpret_cast<PythonTextBlockData*>(
                                                    cursor.block().userData());
    if (!textData)
        return false;
    const PythonToken *tok = textData->tokenAt(posInLine - 1);
    if (!tok || tok->token == PythonSyntaxHighlighter::T_Comment ||
         tok->token == PythonSyntaxHighlighter::T_LiteralBlockDblQuote ||
         tok->token == PythonSyntaxHighlighter::T_LiteralBlockSglQuote ||
         tok->token == PythonSyntaxHighlighter::T_LiteralDblQuote ||
         tok->token == PythonSyntaxHighlighter::T_LiteralSglQuote)
    {
        return false;  // not a token we care about
    }

    // find out how many ch it is in current word from cursor position
    if (!e->text().isEmpty())
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor); // select just typed chr
    cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
    QString prefix = cursor.selectedText();
    int chrTyped = prefix.size();
    QChar startChar;
    if (chrTyped > 0)
        startChar = prefix[chrTyped -1];

    // get completions when we type '.'
    if (!forcePopup)
        forcePopup = e->text() == QLatin1String(".");

    if ((startChar.isLetterOrNumber() && prefix.length() > 1) || forcePopup) {

        // set prefix
        d->editor->completer()->setCompletionPrefix(prefix);

        d->taskAfterReparse = d->Both;

        d->parseTimer.start(d->parseTimeoutMs); // user won't notice 50msec delay, but makes a lot of
                                 // difference on machine strain

    } else {
        hideCompleter();

        // handle callsignatures
        d->taskAfterReparse = d->CallSignature;
        d->parseTimer.start(d->parseTimeoutMs);
    }
    return false;
}

void PythonEditorCodeAnalyzer::popupChoiceSelected(const QModelIndex &idx)
{
    QTextCursor cursor(d->editor->textCursor());
    JediBaseDefinitionObj *def = d->completerModel->getCompletion(idx.row()).get();
    QString txt = def->name();
    QString prefix = d->editor->completer()->completionPrefix();
    if (prefix == QLatin1String("."))
        cursor.setPosition(cursor.position() - prefix.size() +1);
    else
        cursor.setPosition(cursor.position() - prefix.size());
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    cursor.insertText(txt);

    afterChoiceInserted(def);
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

    // for debug only, popup lock out all events so we can't debug
    //d->editor->completer()->popup()->hide();

    QToolTip::showText(pos, buildToolTipText(def), d->editor->completer()->popup());
}

bool PythonEditorCodeAnalyzer::afterChoiceInserted(JediBaseDefinitionObj *obj, int recursionGuard)
{
    if (recursionGuard > 10)
        return false;

    QString type = obj->type();
    bool parenthesis = false;
    if (type == QLatin1String("function") || type == QLatin1String("class")) {
        parenthesis = true;

    } else if(type == QLatin1String("statement")) {
        JediDefinition_list_t lookups;

        if (obj->isCompletionObj())
            lookups = d->currentScript->goto_definitions();
        else
            lookups = obj->goto_definitions();

        // take last, hopefully last assignment
        for (JediBaseDefinition_ptr_t lookObj : lookups)
        {
            if (lookObj->type() != QLatin1String("statement")) {
                return afterChoiceInserted(lookObj.get(), ++recursionGuard);
            }
        }
    }

    if (parenthesis) {
        QTextCursor cursor(d->editor->textCursor());
        cursor.insertText(QLatin1String("()"));
        cursor.movePosition(QTextCursor::Left);
        d->editor->setTextCursor(cursor); // must to move backward

        // suggestions for call signatures
        d->taskAfterReparse = d->CallSignature;
        parseDocument();
        d->callSignatureWgt->update();
        return true;
    }

    return false; // as in no extra things was needed
}

void PythonEditorCodeAnalyzer::createWidgets()
{
    if (d->editor->completer() == nullptr) {
        // create a new completer
        QCompleter *completer = new QCompleter(d->editor);
        d->completerModel = new PythonCompleterModel(this);
        completer->setModel(d->completerModel);
        d->editor->setCompleter(completer);
        completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        completer->setModelSorting(QCompleter::UnsortedModel); // Jedi sorts it?

        completer->popup()->installEventFilter(this);
        d->editor->removeEventFilter(this); // assure no more than 1 eventfileter is installed
        d->editor->installEventFilter(this);

        connect(completer, SIGNAL(activated(const QModelIndex&)),
                this, SLOT(popupChoiceSelected(const QModelIndex&)));
        connect(completer, SIGNAL(highlighted(const QModelIndex&)),
                this, SLOT(popupChoiceHighlighted(const QModelIndex)));
        connect(&d->parseTimer, SIGNAL(timeout()), this, SLOT(parseDocument()));
        d->parseTimer.setSingleShot(true);
    }

    if (d->callSignatureWgt == nullptr) {
        d->callSignatureWgt = new PythonCallSignatureWidget(this);
    }
}

void PythonEditorCodeAnalyzer::complete()
{
    // runs in python, potentially slow?
    d->completerModel->setCompletions(d->currentScript->completions());

    // set location in widget
    QRect completerRect = d->editor->cursorRect();
    completerRect.setWidth(d->editor->completer()->popup()->sizeHintForColumn(0) +
                           d->editor->completer()->popup()->verticalScrollBar()->sizeHint().width());

    // popup and search within internal data for prefix
    d->editor->completer()->complete(completerRect);

    if(!d->editor->completer()->popup()->isVisible()) {
        // install this object to filter timer events for the tooltip label
        //qApp->installEventFilter(this); // locks my system when debugging
    }
}

void PythonEditorCodeAnalyzer::hideCompleter()
{
    d->popupPos = 0;
    d->reparsedAt = -1;

    if (d->editor->completer()->popup()->isVisible()) {
        d->editor->completer()->popup()->hide();
        //qApp->removeEventFilter(this);
    }
    d->parseTimer.stop();
}

void PythonEditorCodeAnalyzer::afterFileNameChanged()
{
    d->taskAfterReparse = d->Nothing;
    d->parseTimer.start(d->parseTimeoutMs);
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


PythonCompleterModel::PythonCompleterModel(PythonEditorCodeAnalyzer *parent) :
    QAbstractListModel(parent), d(new PythonCompleterModelP)
{
    d->analyzer = parent;
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
        return d->analyzer->getIconForDefinition(d->currentList[index.row()]);
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

PythonCallSignatureWidget::PythonCallSignatureWidget(PythonEditorCodeAnalyzer *analyzer) :
    QLabel(analyzer->editor()->viewport()), d(new PythonCallSignatureWidgetP(analyzer))
{
    setStyleSheet(QLatin1String("QLabel { background: #f2de85; padding: 3px; border: 1px solid brown; }"));
    hide();
    setTextFormat(Qt::RichText);
}

PythonCallSignatureWidget::~PythonCallSignatureWidget()
{
    delete d;
}

void PythonCallSignatureWidget::reEvaluate()
{
    // compare this current callsignature with a new one if we should replace,
    // switch candidate or
    if (!d->signatures.size())
        resetList();

    // at which param are we?
    int paramIdxBefore = d->currentParamIdx;
    bool updateNeeded = setParamIdx(); // set where we at in argument chain and tells if we need update
    if (updateNeeded) {
        resetList();
        setParamIdx();
    }

    bool shouldShow = isVisible();

    // find the most likely signature (if more than one)
    bool forceRepaint = setCurrentIdx();
    if (forceRepaint)
        rebuildContent();

    if (paramIdxBefore != d->currentParamIdx || d->currentParamIdx < 0)
        shouldShow = rebuildContent();

    if (shouldShow) {
        if (isVisible())
            hide(); // to re-calculate showEvent
        show();
    } else
        hide();
}

void PythonCallSignatureWidget::update()
{
    d->signatures.clear();
    d->lineNr = 0;
    d->functionName.clear();
    reEvaluate();
}

void PythonCallSignatureWidget::showEvent(QShowEvent *event)
{
    // map to correct position
    int moveLeft = 0;
    QTextCursor cursor = d->analyzer->editor()->textCursor();
    QTextDocument *doc = d->analyzer->editor()->document();
    QChar currentChr = doc->characterAt(cursor.position());
    int signatureLine = d->signatures[d->candidateIdx]->line();
    while (cursor.block().blockNumber() >= signatureLine &&
           currentChr != QLatin1Char('('))
    {
        cursor.movePosition(QTextCursor::Left);
        currentChr = doc->characterAt(cursor.position());
        ++moveLeft;
    }

    QRect pos = d->analyzer->editor()->cursorRect(cursor);
    QRect newPos = pos;
    QRect viewport = d->analyzer->editor()->viewport()->rect();
    QSize thisSize = sizeHint();

    // move slightly above current line and size as this widget
    newPos.setSize(thisSize);
    newPos.moveTo(newPos.x(),  newPos.y() -5 - newPos.height());


    // move to within viewport
    if (newPos.x() + newPos.width() > viewport.width())
        newPos.moveLeft(viewport.width() - newPos.width());

    if (newPos.y() < 0) {
        // move below line
        newPos.moveTo(newPos.x(), pos.y() + pos.height() + 5);
    }

    // move this widget
    move(newPos.topLeft());

    QLabel::showEvent(event);
}

void PythonCallSignatureWidget::afterKeyPressed(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }

    if (d->candidateIdx == -1)
        return; // we should be in hidden state here

    if (event->text() == QLatin1String("(") || event->text() == QLatin1String(")")) {
        update();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left: case Qt::Key_Right: {
        // to bold the current selected parameter
        // should not switch candidate here
        QTextCursor cursor = d->analyzer->editor()->textCursor();
        int lineNr = cursor.block().blockNumber();
        if (lineNr != d->signatures[d->candidateIdx]->line()) {
            hide();
            return;
        }

        setParamIdx();
        bool shouldShow = rebuildContent();
        if (shouldShow)
            show();
        else
            hide();
        break;
    }
    case Qt::Key_Up: // select next signature (up)
        if (d->signatures.size() > d->candidateIdx +1) {
            d->userSelectedSignature = d->candidateIdx +1;
            QTextCursor cursor = d->analyzer->editor()->textCursor();
            cursor.movePosition(QTextCursor::Down); // editor have already moved up here at this point
            d->analyzer->editor()->setTextCursor(cursor);
            reEvaluate();
        }
        break;
    case Qt::Key_Down: // select previous signature (down)
        if (d->candidateIdx > 0 && d->signatures.size() > 1) {
            d->userSelectedSignature = d->candidateIdx -1;
            QTextCursor cursor = d->analyzer->editor()->textCursor();
            cursor.movePosition(QTextCursor::Up); // editor have already moved down here at this point
            d->analyzer->editor()->setTextCursor(cursor);
            reEvaluate();
        }
        break;
    case Qt::Key_Enter:     case Qt::Key_Return:  case Qt::Key_Delete:
    case Qt::Key_Backspace: case Qt::Key_Backtab: case Qt::Key_Tab:
    case Qt::Key_Insert:    case Qt:: Key_Pause:
        break;
    default:
        if (!event->text().isEmpty()) {
            reEvaluate(); // may switch candidate here
        }
    }
}

bool PythonCallSignatureWidget::setParamIdx()
{
    if (d->signatures.isEmpty()) {
        d->currentParamIdx = -1;
        d->lineNr = 0;
        d->functionName.clear();
        return false;
    }

    // find current paramNr
    d->currentParamIdx = -1;
    QTextCursor cursor = d->analyzer->editor()->textCursor();
    QTextDocument *doc = d->analyzer->editor()->document();
    QChar currentChar = doc->characterAt(cursor.position());
    int initialBlockNr  = cursor.block().blockNumber();

    // if our current char is ) move so our param finder loop works
    if (currentChar == QLatin1Char(')')) {
        cursor.movePosition(QTextCursor::Left);
        currentChar = doc->characterAt(cursor.position());
    }

    // finds all parameters
    bool hasValidChar = false;
    while (cursor.position() > 0 &&
          (currentChar != QLatin1Char('(') && currentChar != QLatin1Char(')')))
    {
         if (currentChar == QLatin1Char(','))
            d->currentParamIdx++;
         if (!hasValidChar && currentChar.isLetterOrNumber()) {
            d->currentParamIdx++;
            hasValidChar = true;
         }

         cursor.movePosition(QTextCursor::Left);
         currentChar = doc->characterAt(cursor.position());
    }

    // check if we are still on the same function, else we should update
    if (!d->functionName.isEmpty() && d->lineNr > 0) {

        bool forceUpdate = (currentChar == QLatin1Char(')') &&
                            initialBlockNr != cursor.block().blockNumber());

        // first move back until we find functionname
        // ie      func [(] <- cursor here
        // move to fun[c] (
        while (!cursor.atBlockStart()) {
            currentChar = doc->characterAt(cursor.position());
            if (currentChar.isLetterOrNumber()) {
                cursor.select(QTextCursor::WordUnderCursor);
                break;
            }
            cursor.movePosition(QTextCursor::Left);
        }

        if (forceUpdate == true ||
            (cursor.block().blockNumber() != d->lineNr ||
             cursor.selectedText() != d->functionName))
        {
            // another line or another function name
            return true; // as in should update
        }
    }
    return false;
}

bool PythonCallSignatureWidget::setCurrentIdx() const
{
    if (d->signatures.isEmpty()) {
        d->candidateIdx = -1;
        d->functionName.clear();
        d->lineNr = 0;
        return false;
    }

    //maybe user has selected a signature?
    if (d->userSelectedSignature > -1 && d->userSelectedSignature <= d->signatures.size() -1) {
        d->candidateIdx = d->userSelectedSignature;
        return true;
    }

    d->userSelectedSignature = -1;
    d->candidateIdx = 0;

    // walk the document to list all params written until now
    QStringList paramsWritten;

    auto walkDoc = [this, &paramsWritten] (int moveBy, const QList<QChar> endChrs) {
        QTextCursor cursor = d->analyzer->editor()->textCursor();
        QTextDocument *doc = d->analyzer->editor()->document();
        int pos = cursor.position();
        QChar currentChar = doc->characterAt(pos);
        int currentPos = cursor.position();
        bool haveValidChar = false;

        while (cursor.position() > 0 && !endChrs.contains(currentChar)) {
             currentChar = doc->characterAt(currentPos);
             // signify that we have atleast one valid char typed as argument
             if (!haveValidChar && currentChar.isLetterOrNumber())
                 haveValidChar = true;

             // a new parameter
             if (currentChar == QLatin1Char(',') ||
                 (endChrs.contains(currentChar) && haveValidChar))
             {
                while (!currentChar.isLetterOrNumber() && cursor.position() != pos) {
                    // find the text for this parameter, we might be at a whitespace
                    cursor.setPosition(cursor.position() - moveBy);
                }

                // get the param name
                cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
                if (doc->characterAt(cursor.position() +1) == QLatin1Char('='))
                    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                QString name = cursor.selectedText();

                // store result
                if (moveBy < 0)
                    paramsWritten.prepend(name);
                else
                    paramsWritten.append(name);
             }

             // next char in next loop
             currentPos += moveBy;
             cursor.setPosition(currentPos);
        }
    };

    // fill the params written list
    static QList<QChar> leftStopChars = { QLatin1Char('(') };
    static QList<QChar> rightStopChars = { QLatin1Char(')'), QLatin1Char(':') };
    walkDoc(-1, leftStopChars);
    walkDoc(+1, rightStopChars);

    if (paramsWritten.size() < 1) {
        d->candidateIdx = 0; // just as likely to be correct
        return false; // bail out, no params written yet
    }

    struct MatchSignature {
        // find the most likely candidate
        QMap<int, int> paramCnt;
    };

    QMap<int, MatchSignature> matchCnt;
    int signatureCnt = 0;

    for (JediCallSignature_ptr_t obj : d->signatures) {
        JediCallSignatureObj *sig = obj.get();
        int paramNr = 0;
        //matchCnt[signatureCnt] = MatchSignature;

        for (JediDefinition_ptr_t paramObj : sig->params()) {
            if (paramNr > paramsWritten.size() -1)
                goto selectFromScore; // break out of both for loops
            JediDefinitionObj *param = paramObj.get();
            QString name = param->name();
            QString code = param->description();
            if (name.size() > 1 && code.indexOf(QLatin1String("=")) > -1 &&
                paramsWritten[paramNr].indexOf(QLatin1String("=")) > -1)
            {
                matchCnt[paramNr].paramCnt[paramNr] +=2; // twice the score
            } else {
                matchCnt[paramNr].paramCnt[paramNr] +=1;
            }

            ++paramNr;
        }

        ++signatureCnt;
    }

selectFromScore:

    int highestScore = 0;
    for (int i = 0;  i < matchCnt.keys().size(); ++i) {
        MatchSignature &signature = matchCnt[i];
        for (int paramIdx : signature.paramCnt.keys())
            if (signature.paramCnt[paramIdx] > highestScore){
                d->candidateIdx = i;
                highestScore = signature.paramCnt[paramIdx];
            }
    }
    return false;
}

void PythonCallSignatureWidget::resetList()
{
    d->signatures = d->analyzer->currentScriptObj()->call_signatures();

    if (d->signatures.size()) {
        // store these for later comparison, so we don't open on another function call
        d->functionName = d->signatures[0]->name();
        d->lineNr = d->analyzer->editor()->textCursor().block().blockNumber();
    }

}

bool PythonCallSignatureWidget::rebuildContent()
{
    if (d->candidateIdx < 0)
        return false;

    JediDefinition_list_t params = d->signatures[d->candidateIdx]->params();
    if (params.size() < 1)
        return false;

    // bold the current one
    QStringList paramsList;
    int paramsCnt = 0;
    QString currentParam;
    for (JediDefinition_ptr_t obj : params) {
        QString name = obj->description().replace(QLatin1String("param "), QLatin1String(""));
        if (paramsCnt == d->currentParamIdx) {
            currentParam = name;
            name = QString(QLatin1String("<b>%1</b>")).arg(name);
        }

        paramsList.append(name);
        ++paramsCnt;
    }

    // set docstring
    QString docStr = d->signatures[d->candidateIdx]->docstring(true, false);
    if (!currentParam.isEmpty()) {
        // maybe docstr is formatted as :param name: is a ....
        // n:param\s+name\s*:\s*([^:]*)
        QRegExp re(QString(QLatin1String("\\n:param\\s+%1:\\s*([^:]*)")).arg(currentParam));
        if (re.indexIn(docStr) != -1)
            docStr = re.cap(1).trimmed();
    }

    QString txt;
    if (!docStr.isEmpty())
        txt = QString(QLatin1String("<pre>%1<pre>")).arg(docStr);

    // if we have multiple signatures
    if (d->signatures.size() > 1) // insert arrow chars
        txt += QString(QLatin1String("&#9650;&#9660;&nbsp; %1 of %2: "))
                .arg(d->candidateIdx + 1)
                .arg(d->signatures.size());

    // set text
    txt += QString(QLatin1String("%1(%2)"))
                        .arg(d->signatures[d->candidateIdx]->name())
                        .arg(paramsList.join(QLatin1String(", ")));

    setText(txt);

    return true;
}

// -------------------------------------------------------------------------

#include "moc_PythonEditor.cpp"
