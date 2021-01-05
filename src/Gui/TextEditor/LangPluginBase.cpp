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

#include <QMimeDatabase>
#include <QCoreApplication>
#include <QTimer>
#include <QFileInfo>
#include <Gui/BitmapFactory.h>
#include "SyntaxHighlighter.h"
#include "EditorView.h"
#include "TextEditor.h"

#include "PythonLangPlugin.h"



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

/* static */
void AbstractLangPlugin::registerBuildInPlugins()
{
    //  this thing does nothing more than register our default plugins
    //QTimer::singleShot(0, [=](){
        auto ews = EditorViewSingleton::instance();
        ews->registerLangPlugin(new CommonCodeLangPlugin());
        ews->registerLangPlugin(new PythonLangPluginDbg());
        ews->registerLangPlugin(new PythonLangPluginCode());
    //});
}

bool AbstractLangPlugin::matchesMimeType(const QString& fn,
                                         const QString &mime) const
{
    static QMimeDatabase db;
    auto fnMime = db.mimeTypeForFile(fn).name();

    for (auto mime : mimetypes()) {
        if (fnMime == mime)
            return true;
        else if (mime.right(1) == QLatin1Char('*') && // allow for text/*, text/x-*
                 mime.left(mime.length()-2) == fnMime.left(mime.length()-1))
        {
            return true;
        }

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

void AbstractLangPlugin::contextMenuLineNr(TextEditor *edit, QMenu *menu,
                                           const QString &fn, int line) const
{
    (void)edit;(void)menu;
    (void)fn;(void)line;
}

void AbstractLangPlugin::contextMenuTextArea(TextEditor *edit, QMenu *menu,
                                             const QString &fn, int line) const
{
    (void)edit;(void)menu;
    (void)fn;(void)line;
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

void AbstractLangPlugin::onFileOpen(const EditorView *view, const QString &fn) const
{
    (void)view;(void)fn;
}

void AbstractLangPlugin::onFileSave(const EditorView *view, const QString &fn) const
{
    (void)view;(void)fn;
}

void AbstractLangPlugin::onFileClose(const EditorView *view, const QString &fn) const
{
    (void)view;(void)fn;
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

bool AbstractLangPluginCode::onBackspacePressed(TextEditor *edit) const
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

bool AbstractLangPluginCode::onEscPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onParenLeftPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onParenRightPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onBraceLeftPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onBraceRightPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onBracketLeftPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onBracketRightPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onAtPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onQuoteSglPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onQuoteDblPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onAmpersandPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onCommaPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onColonPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onSemiColonPressed(TextEditor *edit) const
{
    (void)edit;
    return false;
}

bool AbstractLangPluginCode::onKeyPress(TextEditor *edit, QKeyEvent *evt) const
{
    (void)edit;(void)evt;
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
    static const QStringList mimes {
            QLatin1String("text/*"),
            QLatin1String("application/x*")
    };
    return mimes;
}

QStringList CommonCodeLangPlugin::suffixes() const
{
    static const QStringList suff {
        QLatin1String("js"),QLatin1String("ts"),
        QLatin1String("htm"),QLatin1String("html"),
        QLatin1String("cpp"),QLatin1String("c"),
        QLatin1String("h"),QLatin1String("hpp"),
    };
    return suff;
}

void CommonCodeLangPlugin::OnChange(TextEditor *editor, Base::Subject<const char *> &rCaller, const char *rcReason) const
{
    (void)rCaller;
    (void)rcReason;

    if (strncmp(rcReason, "color_", 6) == 0) {
        // is a color change
        auto highlighter = dynamic_cast<SyntaxHighlighter*>(editor->syntaxHighlighter());
        if (highlighter)
            highlighter->loadSettings();
    }
}

bool CommonCodeLangPlugin::onTabPressed(TextEditor *edit) const
{
    ParameterGrp::handle hPrefGrp = edit->getWindowParameter();
    int indent = static_cast<int>(hPrefGrp->GetInt("IndentSize", 4));
    bool space = hPrefGrp->GetBool( "Spaces", true );
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

void CommonCodeLangPlugin::onFileSave(const EditorView *view, const QString &fn) const
{
    (void)fn;
    auto edit = view->textEditor();
    if (edit)
        return;

    QRegExp re(QLatin1String("(\\s*)$"));

    // trim trailing whitespace?
    ParameterGrp::handle hPrefGrp = edit->getWindowParameter();
    if (hPrefGrp->GetBool("EnableTrimTrailingWhitespaces", true )) {
        // to restore cursor and scroll position
        QTextCursor cursor = edit->textCursor();
        //int vScroll = edit->verticalScrollBar()->value(),
        //    hScroll = edit->horizontalScrollBar()->value();

        QTextBlock block = edit->document()->firstBlock();
        for (;block.isValid(); block = block.next()) {
            QString row = block.text();
            re.indexIn(row);
            if (re.captureCount() > 1) {
                cursor.setPosition(block.position() + row.length() - re.cap(1).length());
                cursor.setPosition(block.position() + row.length(), QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
        }

        // restore cursor and scroll position
        //edit->verticalScrollBar()->setValue(vScroll);
        //edit->horizontalScrollBar()->setValue(hScroll);
    }
}




// ------------------------------------------------------------------------


#include "moc_LangPluginBase.cpp"
