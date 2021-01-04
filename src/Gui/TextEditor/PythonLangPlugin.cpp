/***************************************************************************
 *   Copyright (c) 2020 Fredrik Johansson github.com/mumme74               *
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

#include "PythonLangPlugin.h"


#include <Gui/BitmapFactory.h>
#include <algorithm>
#include <QLabel>
#include <QGridLayout>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QPainter>
#include <QTimer>
#include <QFileInfo>
#include <QToolTip>
#include <QMimeDatabase>
#include <QCoreApplication>
#include <QTimer>
#include <QRegExp>
#include <QTextDocumentFragment>

#include "TextEditor/PythonSyntaxHighlighter.h"
#include "BitmapFactory.h"
#include "PythonCode.h"
#include "Macro.h"
#include "Application.h"
#include "TextEditor.h"
#include "EditorView.h"

using namespace Gui;



// private to this file only
namespace Gui {

// ------------------------------------------------------------------

class PythonLangPluginDbgP {
public:
    QList<Base::PyExceptionInfo*> exceptions;
    explicit PythonLangPluginDbgP() {}
    ~PythonLangPluginDbgP() {}
};

// --------------------------------------------------------------------

class PythonLangPluginCodeP {
public:
    explicit PythonLangPluginCodeP() {}
    ~PythonLangPluginCodeP() {}
};

} // namespace Gui


// ************************************************************************
//                 exposed API from here on down
// ------------------------------------------------------------------------


PythonLangPluginDbg::PythonLangPluginDbg()
    : QObject()
    , AbstractLangPluginDbg("PythonLangPluginDbg")
    , d(new PythonLangPluginDbgP())
{
    QTimer::singleShot(0, [&]{
        auto dbgr = debugger();
        using PyDbg = App::Debugging::Python::Debugger;
        connect(dbgr, &PyDbg::haltAt, this, &PythonLangPluginDbg::editorShowDbgMrk);
        connect(dbgr, &PyDbg::releaseAt, this, &PythonLangPluginDbg::editorHideDbgMrk);
        connect(dbgr, &PyDbg::breakpointAdded, this, &PythonLangPluginDbg::onBreakpointAdded);
        connect(dbgr, &PyDbg::breakpointChanged, this, &PythonLangPluginDbg::onBreakPointChanged);
        connect(dbgr, &PyDbg::breakpointRemoved, this, &PythonLangPluginDbg::onBreakPointClear);
        connect(dbgr, &PyDbg::exceptionOccured, this, &PythonLangPluginDbg::exceptionOccured);
        connect(dbgr, &PyDbg::exceptionFatal, this, &PythonLangPluginDbg::exceptionOccured);
        connect(dbgr, &PyDbg::exceptionCleared, this, &PythonLangPluginDbg::exceptionCleared);
        connect(dbgr, &PyDbg::allExceptionsCleared, this, &PythonLangPluginDbg::allExceptionsCleared);
        connect(dbgr, &PyDbg::setActiveLine, this, &PythonLangPluginDbg::editorShowDbgMrk);
    });
}

PythonLangPluginDbg::~PythonLangPluginDbg()
{
    delete d;
}

App::Debugging::Python::Debugger *PythonLangPluginDbg::debugger() const
{
    return App::Debugging::Python::Debugger::instance();
}

QStringList PythonLangPluginDbg::mimetypes() const
{
    static QMimeDatabase db;
    static QString pyMimeDb = db.mimeTypeForFile(QLatin1String("test.py")).name();
    static const QStringList mimes {
            QLatin1String("text/fcmacro"),
            QLatin1String("text/python"),
            QLatin1String("text/x-script.python"),
            pyMimeDb,
            QLatin1String("application/x-script.python")
    };
    return mimes;
}

QStringList PythonLangPluginDbg::suffixes() const
{
    static const QStringList suff {
        QLatin1String("py"),
        QLatin1String("fcmacro"),
    };
    return suff;
}

/// called by editor
void PythonLangPluginDbg::OnChange(TextEditor *editor,
                                   Base::Subject<const char *> &rCaller,
                                   const char *rcReason) const
{
    (void)editor;
    (void)rCaller;
    (void)rcReason;
}

/// called by view
bool PythonLangPluginDbg::onMsg(const EditorView *view, const char *pMsg, const char **ppReturn) const
{
    (void)ppReturn; (void)view;
    if (strcmp(pMsg, "Run")==0) {
        executeScript();
        return true;
    }
    else if (strcmp(pMsg,"StartDebug")==0) {
        QTimer::singleShot(300, this, SLOT(startDebug()));
        return true;
    }
    else if (strcmp(pMsg,"ToggleBreakpoint")==0) {
        toggleBreakpoint();
        return true;
    }
    return false;
}

/// called by editor, if true, it runs onMsg(...)
bool PythonLangPluginDbg::onHasMsg(const EditorView *view, const char *pMsg) const
{
    (void)view;
    if (strcmp(pMsg,"Run")==0)  return true;
    if (strcmp(pMsg,"StartDebug")==0)  return true;
    if (strcmp(pMsg,"ToggleBreakpoint")==0)  return true;
    return false;
}

const char *PythonLangPluginDbg::iconNameForException(const QString &fn, int line) const
{
    auto exc = exceptionFor(fn, line);

    const char *iconStr = "exception";
    if (!exc) return iconStr;

    if (exc->isIndentationError())
        iconStr = "indentation-error";
    else if (exc->isSyntaxError())
        iconStr = "syntax-error";
    else if (exc->isWarning())
        iconStr = "warning";
    return iconStr;
}

bool PythonLangPluginDbg::lineNrToolTipEvent(TextEditor *edit, const QPoint &pos,
                                             int line, QString &toolTipStr) const
{
    (void)pos;
    auto exc = exceptionFor(edit->filename(), line);
    if (exc) {
        QString srcText = QString::fromStdWString(exc->text());
        int offset = exc->getOffset();
        if (offset > 0) {
            // show which place in the line
            if (!srcText.endsWith(QLatin1String("\n")))
                srcText += QLatin1String("\n");

            for (int i = 0; i < offset -1; ++i)
                srcText += QLatin1Char('-');

            srcText += QLatin1Char('^');
        }

        toolTipStr += QString((tr("%1 on line %2\nreason: '%4'\n\n%5")))
                .arg(QString::fromStdString(exc->getErrorType(true)))
                .arg(QString::number(exc->getLine()))
                .arg(QString::fromStdString(exc->getMessage()))
                .arg(srcText);
        return true;
    }
    return false;
}

bool PythonLangPluginDbg::textAreaToolTipEvent(TextEditor *edit, const QPoint &pos,
                                               int line, QString &toolTipStr) const
{
    (void)line;
    auto debugger = App::Debugging::Python::Debugger::instance();
    if (debugger->isHalted() && debugger->currentFile() == edit->filename()) {
        // debugging state
        QTextCursor cursor = edit->cursorForPosition(pos);
        int chrInLine = cursor.position() - cursor.block().position();
        auto lineText = cursor.block().text();

        cursor.select(QTextCursor::WordUnderCursor);
        if (!cursor.hasSelection())
            return false;

        Python::Code codeInspector;

        toolTipStr += codeInspector.findFromCurrentFrame(
                            lineText, chrInLine, cursor.selectedText());

        return true;
    }
    return false;
}

void PythonLangPluginDbg::onBreakpointAdded(size_t uniqueId)
{
    onBreakPointChanged(uniqueId);
}

void PythonLangPluginDbg::onBreakPointClear(size_t uniqueId)
{
    onBreakPointChanged(uniqueId);
}

void PythonLangPluginDbg::onBreakPointChanged(size_t uniqueId)
{
    // trigger repaint of linemarkerarea
    auto bp = debugger()->getBreakpointFromUniqueId(uniqueId);
    if (!bp) return;

    for (auto edit : editors()) {
        if (bp->bpFile()->fileName() == edit->filename()) {
            edit->lineMarkerArea()->update();
            auto vBar = qobject_cast<AnnotatedScrollBar*>(edit->verticalScrollBar());
            if (vBar) {
                QList<int> lines;
                for (auto b : *bp->bpFile())
                    lines.push_back(b->lineNr());
                vBar->resetMarkers(lines, breakpointScrollbarMarkerColor());
            }
        }
    }
}

bool PythonLangPluginDbg::editorShowDbgMrk(const QString &fn, int line)
{
    if (fn.isEmpty() || line < 1)
        return false;

    bool ret = false;
    for (auto edit : editors()) {
        if (edit->view()
             == EditorViewSingleton::instance()->activeView())
        {
            // load this file into view
            edit->view()->open(fn);

            if (edit->filename() == fn) {
                scrollToPos(edit, line);

                edit->lineMarkerArea()->update();
                ret = true;
            }
        }
    }

    return ret;
}

bool PythonLangPluginDbg::editorHideDbgMrk(const QString &fn, int line)
{
    Q_UNUSED(line)
    bool ret = false;
    for (auto edit : editors()) {
        if (fn == edit->filename()) {
            edit->lineMarkerArea()->update();
            ret = true;
        }
    }

    return ret;
}

void PythonLangPluginDbg::exceptionOccured(Base::Exception* exception)
{
    auto excPyinfo = dynamic_cast<Base::PyExceptionInfo*>(exception);
    if (excPyinfo) {
        d->exceptions.push_back(excPyinfo);
        /// repaint
        for (auto editor : editors()) {
            if (editor->filename().toStdString() == excPyinfo->getFile()) {
                editor->lineMarkerArea()->update();
                auto vBar = qobject_cast<AnnotatedScrollBar*>(editor->verticalScrollBar());
                if (vBar)
                    vBar->setMarker(excPyinfo->getLine(), exceptionScrollbarMarkerColor());
            }
            else if (editor->view() &&
                     editor->view() == EditorViewSingleton::instance()->activeView())
            {
                editor->view()->open(QString::fromStdString(excPyinfo->getFile()));
                scrollToPos(editor, excPyinfo->getLine());
            }
        }
    }
}

void PythonLangPluginDbg::exceptionCleared(const QString &fn, int line)
{
    bool repaint = false;
    for (int i = 0; i < d->exceptions.size(); ++i) {
        auto exc = d->exceptions.at(i);
        if (exc->getLine() == line && exc->getFile() == fn.toStdString()) {
            d->exceptions.takeAt(i);
            repaint = true;
        }
    }

    if (repaint) {
        for (auto edit : editors(fn)){
            if (edit->filename() == fn) {
                edit->lineMarkerArea()->update();

                auto vBar = qobject_cast<AnnotatedScrollBar*>(edit->verticalScrollBar());
                if (vBar)
                    vBar->clearMarker(line, exceptionScrollbarMarkerColor());
            }
        }
    }
}

void PythonLangPluginDbg::allExceptionsCleared()
{
    for (auto edit : editors()) {
        auto vBar = qobject_cast<AnnotatedScrollBar*>(edit->verticalScrollBar());
        if (vBar) {
            for (auto exc : d->exceptions)
                vBar->clearMarker(exc->getLine(), exceptionScrollbarMarkerColor());
        }
    }

    d->exceptions.clear();
    for (auto edit : editors())
        edit->lineMarkerArea()->update();
}

void PythonLangPluginDbg::onFileOpen(const EditorView *view, const QString &fn) const
{
    (void)view;
    if (matchesMimeType(fn)) {
        QFileInfo fi(fn);
        debugger()->onFileOpened(fi.absoluteFilePath());
    }
}

void PythonLangPluginDbg::onFileClose(const EditorView *view, const QString &fn) const
{
    (void)view;
    if (matchesMimeType(fn)) {
        QFileInfo fi(fn);
        debugger()->onFileClosed(fi.absoluteFilePath());
    }
}

Base::PyExceptionInfo *PythonLangPluginDbg::exceptionFor(const QString &fn, int line) const
{
    for (auto e : d->exceptions) {
        if (e->getLine() == line && e->getFile() == fn.toStdString())
            return e;
    }

    return nullptr;
}

void PythonLangPluginDbg::contextMenuLineNr(TextEditor *edit, QMenu *menu, const QString &fn, int line) const
{
    if (edit->filename() != fn)
        return;

    // set default inherited context menu
    menu->addSeparator();

    if (exceptionFor(fn, line)) {
        QAction *clearException =
                new QAction(BitmapFactory().iconFromTheme(iconNameForException(fn, line)),
                              tr("Clear exception"), menu);
        connect(clearException, &QAction::triggered, [this, fn, line](bool){
            const_cast<PythonLangPluginDbg*>(this)->exceptionCleared(fn, line);
        });
        menu->addAction(clearException);
        menu->addSeparator();
    }

    auto debugger = App::Debugging::Python::Debugger::instance();
    auto bpl = debugger->getBreakpoint(edit->filename(), line);
    if (bpl != nullptr) {
        QAction *disableBrkPnt =
                    new QAction(BitmapFactory().iconFromTheme("breakpoint-disabled"),
                                          tr("Disable breakpoint"), menu);
        disableBrkPnt->setCheckable(true);
        disableBrkPnt->setChecked(bpl->disabled());
        connect(disableBrkPnt, &QAction::changed, [=](){
            bpl->setDisabled(disableBrkPnt->isChecked());
        });
        menu->addAction(disableBrkPnt);

        QAction *editBrkPnt = new QAction(BitmapFactory().iconFromTheme("preferences-general"),
                                   tr("Edit breakpoint"), menu);
        connect(editBrkPnt, &QAction::triggered, [=](bool) {
            auto dlg = new PythonBreakpointDlg(edit, bpl);
            dlg->exec();
        });
        menu->addAction(editBrkPnt);

        QAction *delBrkPnt = new QAction(BitmapFactory().iconFromTheme("delete"),
                                  tr("Delete breakpoint"), menu);
        connect(delBrkPnt, &QAction::triggered, [=](bool){
            debugger->removeBreakpoint(bpl);
        });
        menu->addAction(delBrkPnt);

    } else {
        QAction *createBrkPnt = new QAction(BitmapFactory().iconFromTheme("breakpoint"),
                                      tr("Add breakpoint"), menu);
        connect(createBrkPnt, &QAction::triggered, [=](bool) {
            debugger->setBreakpoint(edit->filename(), line);
        });
        menu->addAction(createBrkPnt);
    }
}

void PythonLangPluginDbg::contextMenuTextArea(TextEditor *edit, QMenu *menu, const QString &fn, int line) const
{
    (void)line;
    if (edit->filename() != fn)
        return;

    // set default inherited context menu
    menu->addSeparator();

    auto toggleBrkPnt = new QAction(BitmapFactory().iconFromTheme("breakpoint"),
                                    tr("Toggle breakpoint"), menu);
    connect(toggleBrkPnt, &QAction::triggered,
            this, &PythonLangPluginDbg::toggleBreakpoint);
    menu->addAction(toggleBrkPnt);

    auto debugPnt = new QAction(BitmapFactory().iconFromTheme("debug-start"),
                                 tr("Start debug"), menu);
    connect(debugPnt, &QAction::triggered, [=](){
        if (edit->document()->isModified() && edit->view())
            edit->view()->save();
        App::Debugging::Python::Debugger::instance()->runFile(edit->filename());
    });
    menu->addAction(debugPnt);
}

void PythonLangPluginDbg::paintEventTextArea(TextEditor *edit, QPainter *painter,
                                             const QTextBlock& block, QRect &coords) const
{
    // render exceptions
    auto exc = exceptionFor(edit->filename(), block.blockNumber() +1);
    if (exc && exc->getOffset() > 0 && !edit->document()->isModified()) {
        auto fm = edit->fontMetrics();
        auto cursor = edit->textCursor();
        cursor.setPosition(block.position() + exc->getOffset());
        cursor.select(QTextCursor::WordUnderCursor);
        auto leftPos = std::min(cursor.position(), cursor.anchor())
                             - block.position();
        auto upToText = block.text().left(leftPos);
        auto text = cursor.selectedText();
        auto upToWidth = fm.width(upToText);
        auto wordWidth = fm.width(text);
        int y = coords.bottom() - fm.underlinePos();
        QPen pen;
        pen.setBrush(QBrush(Qt::DotLine));
        pen.setWidth(2);
        pen.setColor(Qt::red);
        painter->setPen(pen);
        painter->drawLine(upToWidth + coords.left(), y,
                          upToWidth + wordWidth  + coords.left(), y);

        // debugging
        pen.setWidth(1);
        pen.setColor(Qt::blue);
        painter->setPen(pen);
        painter->drawRect(coords);
    }
}

void PythonLangPluginDbg::paintEventLineNumberArea(TextEditor *edit, QPainter *painter,
                                                   const QTextBlock& block, QRect &coords) const
{
    int line = block.blockNumber() +1;

    int baseX = coords.x() + 1,
        baseY = coords.top() + (coords.height() / 2) - (breakpointIcon().height() / 2);
    // breakpoints
    auto debugger = App::Debugging::Python::Debugger::instance();
    auto bpl = debugger->getBreakpoint(edit->filename(), line);
    bool hadBreakpoint = false;
    if (bpl != nullptr) {
        if (bpl->disabled())
            painter->drawPixmap(baseX, baseY, breakpointDisabledIcon());
        else
            painter->drawPixmap(baseX,baseY, breakpointIcon());
        hadBreakpoint = true;
    }

    // exceptions from Python interpreter
    auto exc = exceptionFor(edit->filename(), line);
    if (exc) {
        PyExceptionInfoGui excGui(exc);
        QIcon icon = BitmapFactory().iconFromTheme(excGui.iconName());
        int excX = baseX;
        if (edit->lineMarkerArea()->lineNumbersVisible() && hadBreakpoint)
            excX += breakpointIcon().width();
        painter->drawPixmap(excX, baseY, icon.pixmap(breakpointIcon().height()));
    }

    // debugger marker
    if (debugger->currentLine() == line) {
        painter->drawPixmap(baseX, baseY, debugMarkerIcon());
    }
}

void PythonLangPluginDbg::executeScript() const
{
    auto view = EditorViewSingleton::instance()->activeView();
    if (view && view->textEditor() &&
        matchesMimeType(view->filename(), view->textEditor()->mimetype()))
    {
        auto doc = view->editor()->document();
        if (doc->isModified())
            view->save();

        try {
            Gui::Application::Instance->macroManager()->run(
                        Gui::MacroManager::File, view->filename().toUtf8());
        }
        catch (const Base::SystemExitException&) {
            // handle SystemExit exceptions
            Base::PyGILStateLocker locker;
            Base::PyException e;
            e.ReportException();
        }
    }
}

void PythonLangPluginDbg::startDebug() const
{
    auto view = EditorViewSingleton::instance()->activeView();
    if (view && view->textEditor() &&
        matchesMimeType(view->filename(), view->textEditor()->mimetype()))
    {
        auto doc = view->editor()->document();
        if (doc->isModified())
            view->save();

        auto dbgr = App::Debugging::Python::Debugger::instance();
        dbgr->runFile(view->filename());
    }
}

void PythonLangPluginDbg::toggleBreakpoint() const
{
    auto view = EditorViewSingleton::instance()->activeView();
    if (view && view->textEditor() &&
        matchesMimeType(view->filename(), view->textEditor()->mimetype()))
    {
        auto textEdit = view->textEditor();
        if (textEdit) {
            auto cursor = textEdit->textCursor();
            int line = cursor.block().blockNumber() +1;

            auto dbgr = App::Debugging::Python::Debugger::instance();
            auto bp = dbgr->getBreakpoint(view->filename(), line);
            if (bp)
                dbgr->removeBreakpoint(bp);
            else
                dbgr->setBreakpoint(view->filename(), line);

            textEdit->lineMarkerArea()->repaint();
        }
    }
}

// ----------------------------------------------------------------------------

PythonLangPluginCode::PythonLangPluginCode()
    : CommonCodeLangPlugin("pythoncode")
    , d(new PythonLangPluginCodeP())
{
}

PythonLangPluginCode::~PythonLangPluginCode()
{
}

QStringList PythonLangPluginCode::mimetypes() const
{
    static QMimeDatabase db;
    static QString pyMimeDb = db.mimeTypeForFile(QLatin1String("test.py")).name();
    static const QStringList mimes {
            QLatin1String("text/fcmacro"),
            QLatin1String("text/python"),
            QLatin1String("text/x-script.python"),
            pyMimeDb,
            QLatin1String("application/x-script.python")
    };
    return mimes;
}

QStringList PythonLangPluginCode::suffixes() const
{
    static const QStringList suff {
        QLatin1String("py"),
        QLatin1String("fcmacro"),
    };
    return suff;
}

bool PythonLangPluginCode::onPaste(TextEditor *edit) const
{
    (void)edit; // TODO figure out how to autoindent pasted code
    return false;
}

bool PythonLangPluginCode::onEnterPressed(TextEditor *edit) const
{
    // auto indent
    auto cursor = edit->textCursor();
    ParameterGrp::handle hPrefGrp = edit->getWindowParameter();
    if (hPrefGrp->GetBool( "EnableAutoIndent", true)) {
        cursor.beginEditBlock();
        int indent = static_cast<int>(hPrefGrp->GetInt( "IndentSize", 4 ));
        bool space = hPrefGrp->GetBool( "Spaces", true );
        QString ch = space ? QString(indent, QLatin1Char(' '))
                           : QString::fromLatin1("\t");

        // get leading indent of row before newline
        QRegExp rx(QString::fromLatin1("^(\\s*)"));
        int pos = rx.indexIn(cursor.block().text());
        QString ind = pos > -1 ? rx.cap(1) : QString();

        QRegExp re(QLatin1String("[:{\\[]\\s*$"));
        if (re.indexIn(cursor.block().text()) > -1)
        {
            // begin new block, line ends with a ':'
            ind += ch;
        }
        // insert block and indent, move to top of new line
        cursor.insertBlock();
        //cursor.movePosition(QTextCursor::NextBlock);
        cursor.insertText(ind);
        //cursor.movePosition(QTextCursor::EndOfLine);
        edit->setTextCursor(cursor);
        cursor.endEditBlock();
        return true;
    }
    return false;
}

bool PythonLangPluginCode::onParenLeftPressed(TextEditor *edit) const
{
    return insertOpeningChar(edit, QLatin1String("()"));
}

bool PythonLangPluginCode::onParenRightPressed(TextEditor *edit) const
{
    return insertClosingChar(edit, QLatin1String("()"));
}

bool PythonLangPluginCode::onBracketLeftPressed(TextEditor *edit) const
{
    return insertOpeningChar(edit, QLatin1String("[]"));
}

bool PythonLangPluginCode::onBracketRightPressed(TextEditor *edit) const
{
    return insertClosingChar(edit, QLatin1String("[]"));
}

bool PythonLangPluginCode::onBraceLeftPressed(TextEditor *edit) const
{
    return insertOpeningChar(edit, QLatin1String("{}"));
}

bool PythonLangPluginCode::onBraceRightPressed(TextEditor *edit) const
{
    return insertClosingChar(edit, QLatin1String("{}"));
}

bool PythonLangPluginCode::onQuoteDblPressed(TextEditor *edit) const
{
    auto cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    if (cursor.selectedText() == QLatin1String("\""))
        return false;
    return insertOpeningChar(edit, QLatin1String("\"\""));
}

bool PythonLangPluginCode::onQuoteSglPressed(TextEditor *edit) const
{
    auto cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    if (cursor.selectedText() == QLatin1String("'"))
        return false;
    return insertOpeningChar(edit, QLatin1String("''"));
}

void PythonLangPluginCode::contextMenuTextArea(TextEditor *edit, QMenu *menu, const QString &fn, int line) const
{
    auto commentAct = new QAction(tr("Toggle comment"), menu);
    menu->addAction(commentAct);
    connect(commentAct, &QAction::triggered, [=]() { toggleComment(edit); });

    auto autoIndent = new QAction(tr("Auto indent"), menu);
    menu->addAction(autoIndent);
    connect(autoIndent, &QAction::triggered, [=](){ onAutoIndent(edit); });

    CommonCodeLangPlugin::contextMenuTextArea(edit, menu, fn, line);
}

void PythonLangPluginCode::toggleComment(TextEditor *edit) const
{
    QRegExp reComment(QLatin1String("^\\s*# ")),
            reEmptyLine(QLatin1String("^\\s*$"));

    auto cursor = edit->textCursor();
    int startPos = cursor.position(),
        endPos = cursor.position();

    cursor.beginEditBlock();
    if (cursor.hasSelection()) {
        startPos = cursor.selectionStart();
        endPos = cursor.selectionEnd();
    }

    // first find out if it has code on any line in selection
    auto startBlock = edit->document()->findBlock(startPos);
    bool code = false;
    for (auto block = startBlock;
         block.position() <= endPos && block.isValid();
         block = block.next())
    {
        if (reComment.indexIn(block.text()) == -1 &&
            reEmptyLine.indexIn(block.text()) == -1)
        {
            code = true;
        }
    }

    // then do the commenting
    for (auto block = startBlock;
         block.position() <= endPos && block.isValid();
         block = block.next())
    {
        cursor.setPosition(block.position());
        if (code) {
            if (reEmptyLine.indexIn(block.text()) == -1)
                cursor.insertText(QLatin1String("# "));
        } else {
            cursor.setPosition(cursor.block().position());
            cursor.setPosition(cursor.block().position() + 2,
                               QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        }
    }
    cursor.endEditBlock();
}

void PythonLangPluginCode::onAutoIndent(TextEditor *edit) const
{
    QTextCursor cursor = edit->textCursor();
    int selStart = cursor.selectionStart();
    int selEnd = cursor.selectionEnd();
    QList<int> indentLevel;// level of indentation at different indentaions

    QTextBlock block;
    QRegExp reNewBlock(QLatin1String("[:{]\\s*#*.*$"));
    QChar mCommentChr(0);
    int mCommentIndents = 0;

    ParameterGrp::handle hPrefGrp = edit->getWindowParameter();
    int indentSize = static_cast<int>(hPrefGrp->GetInt( "IndentSize", 4 ));
    bool useSpaces = hPrefGrp->GetBool( "Spaces", true );
    QString space = useSpaces ? QString(indentSize, QLatin1Char(' '))
                       : QString::fromLatin1("\t");

    cursor.beginEditBlock();
    for (block = edit->document()->begin(); block.isValid(); block = block.next()) {
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
                    if (mCommentChr.isNull()) {
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
            if (!mCommentChr.isNull())
                --mCommentIndents;
        }

        // start a new indent block?
        QString txt = block.text();
        if (mayIndent && reNewBlock.indexIn(txt) > -1) {
            indentLevel.append(chrCount);
            blockIndent = -1; // don't indent this row
            if (!mCommentChr.isNull())
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

bool PythonLangPluginCode::insertOpeningChar(TextEditor *edit, const QString &insertTxt) const
{
    auto cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    if (cursor.selectedText() == insertTxt.left(1))
        return false;
    cursor.beginEditBlock();
    cursor = edit->textCursor();
    cursor.insertText(insertTxt);
    cursor.movePosition(QTextCursor::Left);
    edit->setTextCursor(cursor);
    cursor.endEditBlock();
    return true;
}

bool PythonLangPluginCode::insertClosingChar(TextEditor *edit, const QString &insertTxt) const
{
    auto cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    if (cursor.selectedText() == insertTxt.right(1)) {
        cursor.movePosition(QTextCursor::Right);
        edit->setTextCursor(cursor);
        return true; // snatch this keypress from bubble through
    }
    return false;
}

/***************************************************************************
 * gui related stuff such as dialogs etc from here on
 **************************************************************************/

// ------------------------------------------------------------------------

PythonBreakpointDlg::PythonBreakpointDlg(
        QWidget *parent,
        std::shared_ptr<App::Debugging::Python::BrkPnt> bp):
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

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
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

PythonBreakpointDlg::~PythonBreakpointDlg()
{
}

void PythonBreakpointDlg::accept()
{
    m_bpl->setDisabled(!m_enabled->isChecked());
    m_bpl->setCondition(m_condition->text());
    m_bpl->setIgnoreTo(m_ignoreToHits->value());
    m_bpl->setIgnoreFrom(m_ignoreFromHits->value());

    QDialog::accept();
}

#include "moc_PythonLangPlugin.cpp"
