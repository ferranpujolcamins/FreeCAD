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


}

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

void AbstractLangPluginDbg::OnChange(EditorView *view, Base::Subject<const char *> &rCaller, const char *rcReason) const
{
    (void)rCaller;
    (void)rcReason;

    if (strcmp(rcReason, "FontSize") == 0 || strcmp(rcReason, "Font") == 0) {
        int rowHeight = view->editor()->fontMetrics().height();
        d->loadIcons(rowHeight);
    } else if (strncmp(rcReason, "color_", 6) == 0) {
        // is a color change
        auto textEdit = dynamic_cast<TextEditor*>(view->editor());
        if (!textEdit) return;
        auto highlighter = dynamic_cast<PythonSyntaxHighlighter*>(textEdit->syntaxHighlighter());
        if (highlighter)
            highlighter->loadSettings();
    }
}


QList<TextEditor *> AbstractLangPluginDbg::editors(const QString &fn) const
{
    QList<TextEditor*> retLst;
    for (auto edit : EditorViewSingleton::instance()->editorViews()) {
        auto editor = edit->textEditor();
        if (editor && (fn.isEmpty() || fn == editor->filename()))
            retLst.push_back(editor);
    }
    return retLst;
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

void PythonLangPluginDbg::OnChange(EditorView *view, Base::Subject<const char *> &rCaller, const char *rcReason) const
{
    (void)rCaller;
    (void)rcReason;
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


//        if (!tok->isCode())
//            return false;


//        // TODO figure out how to extract attribute / item chain for objects, dicts and lists
//        // using tokens from syntaxhighlighter

//        QString str = d->pythonCode->findFromCurrentFrame(tok);
//        QToolTip::showText(tooltipPos, str, this);
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

    for (auto edit : editors())
        edit->lineMarkerArea()->update();
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
            if (editor->filename().toStdString() == excPyinfo->getFile())
                editor->lineMarkerArea()->update();
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
            if (edit->filename() == fn)
                edit->lineMarkerArea()->update();
        }
    }
}

void PythonLangPluginDbg::allExceptionsCleared()
{
    d->exceptions.clear();
    for (auto edit : editors()){
        edit->lineMarkerArea()->update();
    }
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

void PythonLangPluginDbg::contextMenuLineNr(EditorView *view, QMenu *menu, const QString &fn, int line) const
{
    auto editor = dynamic_cast<TextEditor*>(view->editor());
    if (editor && editor->filename() != fn)
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
    auto bpl = debugger->getBreakpoint(editor->filename(), line);
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
            auto dlg = new PythonBreakpointDlg(editor, bpl);
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
            debugger->setBreakpoint(editor->filename(), line);
        });
        menu->addAction(createBrkPnt);
    }
}

void PythonLangPluginDbg::contextMenuTextArea(EditorView *view, QMenu *menu, const QString &fn, int line) const
{
    (void)line;
    auto editor = dynamic_cast<TextEditor*>(view->editor());
    if (editor && editor->filename() != fn)
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


#include "moc_LangPluginBase.cpp"
