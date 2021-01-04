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
#include "LangPluginBase.h"
#include "EditorView.h"
#include "TextEditor.h"
#include <QMimeDatabase>
#include <QCoreApplication>

#include <Gui/MainWindow.h>


// for Python plugin only
#include <memory>
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
#include "TextEditor/PythonSyntaxHighlighter.h"
#include "BitmapFactory.h"
#include "PythonCode.h"
#include "Macro.h"
#include "Application.h"

using namespace Gui;


namespace Gui {
class AbstractLangPluginP {
public:
    explicit AbstractLangPluginP(const char* pluginName)
        : pluginName(pluginName)
    {}
    ~AbstractLangPluginP() {}
    const char *pluginName;
};

class AbstractLangPluginDbgP {
public:
    QPixmap breakpoint, breakpointDisabled, debugMarker;
    const QColor breakPointScrollBarMarkerColor = QColor(242, 58, 82); // red
    const QColor exceptionScrollBarMarkerColor = QColor(252, 172, 0); // red-orange
    explicit AbstractLangPluginDbgP()
    {
        QTimer::singleShot(0, [&](){

            auto rowHeight = App::GetApplication().GetUserParameter()
                    .GetGroup("BaseApp")->GetGroup("Preferences")
                    ->GetGroup("Editor")->GetInt("FontSize", 10);
            loadIcons(static_cast<int>(rowHeight)); // must ensure this is done when event loop is running
        });
    }
    ~AbstractLangPluginDbgP() {}

    void loadIcons(int rowHeight)
    {
        breakpoint = BitmapFactory().iconFromTheme("breakpoint").pixmap(rowHeight, rowHeight);
        breakpointDisabled = BitmapFactory().iconFromTheme("breakpoint-disabled").pixmap(rowHeight, rowHeight);
        debugMarker = BitmapFactory().iconFromTheme("debug-marker").pixmap(rowHeight +2, rowHeight +2);
    }
};

class PythonLangPluginDbgP {
public:
    QList<Base::PyExceptionInfo*> exceptions;
    explicit PythonLangPluginDbgP() {}
    ~PythonLangPluginDbgP() {}
};

// ------------------------------------------------------------------
class AbstractLangPluginCodeP {
public:
    explicit AbstractLangPluginCodeP() {}
    ~AbstractLangPluginCodeP() {}
};

// ------------------------------------------------------------------

class CommonCodeLangPluginP {
public:
    explicit CommonCodeLangPluginP() {}
    ~CommonCodeLangPluginP() {}
};

// ------------------------------------------------------------------

} // namespace Gui

// -----------------------------------------------------------------

AbstractLangPlugin::AbstractLangPlugin(const char* langName)
    : d(new AbstractLangPluginP(langName))
{
}

AbstractLangPlugin::~AbstractLangPlugin()
{
    delete d;
}

const char *AbstractLangPlugin::name() const
{
    return d->pluginName;
}

bool AbstractLangPlugin::matchesMimeType(const QString& fn,
                                         const QString &mime) const
{
    static QMimeDatabase db;
    auto fnMime = db.mimeTypeForFile(fn).name();

    for (auto mime : mimetypes()) {
        if (fnMime == mime)
            return true;
    }

    for (auto m : mimetypes())
        if (m == mime)
            return true;

    QFileInfo fi(fn);
    for (auto suf : suffixes())
        if (fi.suffix().toLower() == suf)
            return true;

    return false;
}

bool AbstractLangPlugin::lineNrToolTipEvent(TextEditor *edit, const QPoint &pos, int line, QString &toolTipStr) const
{
    (void)edit;(void)pos;
    (void)line;(void)toolTipStr;
    return false;
}

bool AbstractLangPlugin::textAreaToolTipEvent(TextEditor *edit, const QPoint &pos, int line, QString &toolTipStr) const
{
    (void)edit;(void)pos;
    (void)line;(void)toolTipStr;
    return false;
}

void AbstractLangPlugin::OnChange(TextEditor *editor, Base::Subject<const char *> &rCaller, const char *rcReason) const
{
    (void)editor;(void)rCaller;
    (void)rcReason;
}

void AbstractLangPlugin::paintEventTextArea(TextEditor *edit, QPainter *painter, const QTextBlock &block, QRect &coords) const
{
    (void)edit;(void)painter;
    (void)block;(void)coords;
}

void AbstractLangPlugin::paintEventLineNumberArea(TextEditor *edit, QPainter *painter, const QTextBlock &block, QRect &coords) const
{
    (void)edit;(void)painter;
    (void)block;(void)coords;
}

bool AbstractLangPlugin::onCut(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPlugin::onPaste(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPlugin::onCopy(TextEditor *edit) const
{
    (void)edit;
    return false;
}

QList<TextEditor *> AbstractLangPlugin::editors(const QString &fn) const
{
    QList<TextEditor*> retLst;
    for (auto edit : EditorViewSingleton::instance()->editorViews()) {
        auto editor = edit->textEditor();
        if (editor && (fn.isEmpty() || fn == editor->filename()))
            retLst.push_back(editor);
    }
    return retLst;
}

void AbstractLangPlugin::onFileOpened(const QString &fn)
{
    (void)fn;
}

void AbstractLangPlugin::onFileClosed(const QString &fn)
{
    (void)fn;
}

// -------------------------------------------------------------------


AbstractLangPluginCode::AbstractLangPluginCode(const char *pluginName)
    : AbstractLangPlugin(pluginName)
    , d(new AbstractLangPluginCodeP())
{
}

AbstractLangPluginCode::~AbstractLangPluginCode()
{
    delete d;
}

bool AbstractLangPluginCode::onTabPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onDelPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onBacktabPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onSpacePressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onEnterPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onPeriodPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

// -------------------------------------------------------------------


AbstractLangPluginDbg::AbstractLangPluginDbg(const char *langName)
    : AbstractLangPlugin(langName)
    , d(new AbstractLangPluginDbgP())
{
}

AbstractLangPluginDbg::~AbstractLangPluginDbg()
{
    delete d;
}

void AbstractLangPluginDbg::OnChange(TextEditor *editor,
                                     Base::Subject<const char *> &rCaller,
                                     const char *rcReason) const
{
    (void)rCaller;
    if (strcmp(rcReason, "FontSize") == 0 || strcmp(rcReason, "Font") == 0) {
        int rowHeight = editor->fontMetrics().height();
        d->loadIcons(rowHeight);
    }
}

void AbstractLangPluginDbg::scrollToPos(TextEditor* edit, int line) const
{

    QTextCursor cursor = edit->textCursor();
    //auto origPos = cursor.position();
    cursor.setPosition(edit->document()->findBlockByNumber(line - 1).position());
    edit->setTextCursor(cursor);

    ParameterGrp::handle hPrefGrp = edit->getWindowParameter();
    if (hPrefGrp->GetBool("EnableCenterDebugmarker", true)) {
        // debugmarker stays centered text jumps around
        edit->centerCursor();
    } else {
        // scroll into view, line jumps around, text stays (if possible)
        edit->ensureCursorVisible();
    }

    // restore cursor position
    //cursor.setPosition(origPos);
    //edit->setTextCursor(cursor);
}

const QColor &AbstractLangPluginDbg::exceptionScrollbarMarkerColor() const
{
    return d->exceptionScrollBarMarkerColor;
}

const QColor &AbstractLangPluginDbg::breakpointScrollbarMarkerColor() const
{
    return d->breakPointScrollBarMarkerColor;
}

/* broken algorithm
void AbstractLangPluginDbg::breakpointPasteOrCut(TextEditor *edit, bool doCut) const
{
    // track breakpoint movements
    QTextCursor cursor = edit->textCursor();
    auto debugger = App::Debugging::Python::Debugger::instance();
    int beforeLineNr = cursor.block().blockNumber();
    if (cursor.hasSelection()) {
        int posEnd = qMax(cursor.selectionEnd(), cursor.selectionStart());
        beforeLineNr = edit->document()->findBlock(posEnd).blockNumber();
    }

    if (doCut)
        TextEditor::cut();
    else
        TextEditor::paste();

    int currentLineNr = edit->textCursor().block().blockNumber();
    if (beforeLineNr != currentLineNr) {
        int move = beforeLineNr - currentLineNr;
        auto bp = debugger->getBreakpointFile(edit->filename());
        if (bp)
            bp->moveLines(beforeLineNr, move);
    }
}
*/

const QPixmap &AbstractLangPluginDbg::breakpointIcon() const
{
    return d->breakpoint;
}

const QPixmap &AbstractLangPluginDbg::breakpointDisabledIcon() const
{
    return d->breakpointDisabled;
}

const QPixmap &AbstractLangPluginDbg::debugMarkerIcon() const
{
    return d->debugMarker;
}

/*
bool AbstractLangPluginDbg::onCut(TextEditor *edit) const
{
    // track breakpoint movements
    breakpointPasteOrCut(true);
}

bool AbstractLangPluginDbg::onPaste(TextEditor *edit) const
{
    // track breakpoint movements
    breakpointPasteOrCut(false);
}
*/

// -----------------------------------------------------------------------


CommonCodeLangPlugin::CommonCodeLangPlugin(const char *pluginName)
    : QObject()
    , AbstractLangPluginCode(pluginName)
    , d(new CommonCodeLangPluginP())
{
}

CommonCodeLangPlugin::~CommonCodeLangPlugin()
{
    delete d;
}

QStringList CommonCodeLangPlugin::mimetypes() const
{
    static QMimeDatabase db;
    static QString pyMimeDb = db.mimeTypeForFile(QLatin1String("test.py")).name();
    static const QStringList mimes {
            QLatin1String("text/fcmacro"),
            QLatin1String("text/python"),
            QLatin1String("text/x-script.python"),
            pyMimeDb,
            QLatin1String("application/x-script.python")
            ,QLatin1String("text/html") // only for debug linkin
    };
    return mimes;
}

QStringList CommonCodeLangPlugin::suffixes() const
{
    static const QStringList suff {
        QLatin1String("py"),
        QLatin1String("fcmacro"),
    };
    return suff;
}

void CommonCodeLangPlugin::OnChange(TextEditor *editor, Base::Subject<const char *> &rCaller, const char *rcReason) const
{
    (void)rCaller;
    (void)rcReason;

    if (strncmp(rcReason, "color_", 6) == 0) {
        // is a color change
        auto highlighter = dynamic_cast<PythonSyntaxHighlighter*>(editor->syntaxHighlighter());
        if (highlighter)
            highlighter->loadSettings();
    }
}

bool CommonCodeLangPlugin::onTabPressed(TextEditor *edit) const
{
    ParameterGrp::handle hPrefGrp = edit->getWindowParameter();
    int indent = static_cast<int>(hPrefGrp->GetInt("IndentSize", 4));
    bool space = hPrefGrp->GetBool( "Spaces", false );
    QString ch = space ? QString(indent, QLatin1Char(' '))
                       : QString::fromLatin1("\t");

    QTextCursor cursor = edit->textCursor();
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
        edit->setTextCursor(cursor);
    } else {
        // for each selected block insert a tab or spaces
        int selStart = cursor.selectionStart();
        int selEnd = cursor.selectionEnd();
        QTextBlock block;
        cursor.beginEditBlock();
        for (block = edit->document()->begin(); block.isValid(); block = block.next()) {
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

    return true;
}

bool CommonCodeLangPlugin::onBacktabPressed(TextEditor *edit) const
{
    QTextCursor cursor = edit->textCursor();
    if (!cursor.hasSelection())
        return false; // Shift+Tab should not do anything, let other plugins have their say
    // If some text is selected we remove a leading tab or
    // spaces from each selected block
    ParameterGrp::handle hPrefGrp = edit->getWindowParameter();
    int indent = static_cast<int>(hPrefGrp->GetInt("IndentSize", 4));

    int selStart = cursor.selectionStart();
    int selEnd = cursor.selectionEnd();
    QTextBlock block;
    cursor.beginEditBlock();
    for (block = edit->document()->begin(); block.isValid(); block = block.next()) {
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
    return true;
}

bool CommonCodeLangPlugin::onKeyPress(TextEditor *edit, QKeyEvent *evt) const
{
    (void)edit; (void)evt;
    return false;
}

void CommonCodeLangPlugin::contextMenuLineNr(TextEditor *edit, QMenu *menu, const QString &fn, int line) const
{
    (void)edit;(void)menu;
    (void)fn;(void)line;
}

void CommonCodeLangPlugin::contextMenuTextArea(TextEditor *edit, QMenu *menu, const QString &fn, int line) const
{
    (void)fn; (void)line;
    menu->addSeparator();
    auto cursor = edit->textCursor();
    bool disable = !cursor.hasSelection();

    auto indentAct = new QAction(tr("Indent"), menu);
    indentAct->setDisabled(disable);
    connect(indentAct, &QAction::triggered, [=](){ onTabPressed(edit); });
    menu->addAction(indentAct);

    auto dedentAct = new QAction(tr("Dedent"), menu);
    dedentAct->setDisabled(disable);
    connect(dedentAct, &QAction::triggered, [=] (){ onBacktabPressed(edit); });
    menu->addAction(dedentAct);
}

bool CommonCodeLangPlugin::onMsg(const EditorView *view, const char *pMsg, const char **ppReturn) const
{
    (void)ppReturn; (void)view;
    if (strcmp(pMsg, "Indent")==0) {
        onBacktabPressed(view->textEditor());
        return true;
    }
    else if (strcmp(pMsg,"Dedent")==0) {
        onTabPressed(view->textEditor());
        return true;
    }
    return false;
}

bool CommonCodeLangPlugin::onHasMsg(const EditorView *view, const char *pMsg) const
{
    (void)view;
    if (strcmp(pMsg,"Indent")==0)  return view->textEditor() != nullptr; // might be QPlainTextEdit
    if (strcmp(pMsg,"Dedent")==0)  return view->textEditor() != nullptr;
    return false;
}

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


        // let the our selfs know when a file closes or openes
        // and in turn relay that to PythonDebugger
        auto eds = EditorViewSingleton::instance();
        connect(eds, &EditorViewSingleton::fileOpened,
                this, &PythonLangPluginDbg::onFileOpened);
        connect(eds, &EditorViewSingleton::fileClosed,
                this, &PythonLangPluginDbg::onFileClosed);
    });
}

PythonLangPluginDbg::~PythonLangPluginDbg()
{
    delete d;
}

App::Debugging::Python::Debugger *PythonLangPluginDbg::debugger()
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
            ,QLatin1String("text/html") // only for debug linkin
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

void PythonLangPluginDbg::onFileOpened(const QString &fn)
{
    if (matchesMimeType(fn)) {
        QFileInfo fi(fn);
        debugger()->onFileOpened(fi.absoluteFilePath());
    }
}

void PythonLangPluginDbg::onFileClosed(const QString &fn)
{
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

    QAction *delBrkPnt = new QAction(BitmapFactory().iconFromTheme("delete"),
                              tr("test dbug plugin contextmenu"), menu);
    menu->addAction(delBrkPnt);

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
            int line = cursor.block().blockNumber();

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

// ------------------------------------------------------------------------

// ************************************************************************

///  this thing does nothing more than register our default plugins
//namespace Gui {
//static struct LangPluginRegistrator {
//    LangPluginRegistrator() {
//        auto ews = EditorViewSingleton::instance();
//        QTimer::singleShot(1000, [=](){
//            ews->registerLangPlugin(new CommonCodeLangPlugin());
//            ews->registerLangPlugin(new PythonLangPluginDbg());
//        });
//    }

//} _LangPluginRestratorLangPluginBase;
//} // namespace Gui


// ------------------------------------------------------------------------


#include "moc_LangPluginBase.cpp"
