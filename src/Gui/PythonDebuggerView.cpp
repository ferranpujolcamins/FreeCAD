/***************************************************************************
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
# include <QGridLayout>
# include <QApplication>
# include <QMenu>
# include <QContextMenuEvent>
# include <QTextCursor>
# include <QTextStream>
#endif

#include "PythonDebuggerView.h"
#include "PythonDebugger.h"
#include "BitmapFactory.h"
#include "EditorView.h"
#include "Window.h"
#include "MainWindow.h"
#include "PythonEditor.h"
#include "PythonCode.h"

#include <CXX/Extensions.hxx>
#include <frameobject.h>
#include <Base/Interpreter.h>
#include <Application.h>
#include <QVariant>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTableView>
#include <QTreeView>
#include <QTabWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QSplitter>
#include <QMenu>
#include <QDebug>



/* TRANSLATOR Gui::DockWnd::PythonDebuggerView */

namespace Gui {
namespace DockWnd {

class PythonDebuggerViewP
{
public:
    QPushButton *m_startDebugBtn;
    QPushButton *m_stopDebugBtn;
    QPushButton *m_stepIntoBtn;
    QPushButton *m_stepOverBtn;
    QPushButton *m_stepOutBtn;
    QPushButton *m_haltOnNextBtn;
    QPushButton *m_continueBtn;
    QLabel      *m_varLabel;
    QTreeView   *m_varView;
    QTableView  *m_stackView;
    QTableView  *m_breakpointView;
    QTableView  *m_issuesView;
    QTabWidget  *m_stackTabWgt;
    QTabWidget  *m_varTabWgt;
    QSplitter   *m_splitter;
    PythonDebuggerViewP() { }
    ~PythonDebuggerViewP() { }
};

} // namespace DockWnd
} //namespace Gui

using namespace Gui;
using namespace Gui::DockWnd;

/**
 *  Constructs a PythonDebuggerView which is a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'
 */
PythonDebuggerView::PythonDebuggerView(QWidget *parent)
   : QWidget(parent), d(new PythonDebuggerViewP())
{
    setObjectName(QLatin1String("DebuggerView"));

    QVBoxLayout *vLayout = new QVBoxLayout(this);

    initButtons(vLayout);

    d->m_splitter = new QSplitter(this);
    d->m_splitter->setOrientation(Qt::Vertical);
    vLayout->addWidget(d->m_splitter);

    // variables explorer view
    d->m_varTabWgt = new QTabWidget(d->m_splitter);
    d->m_splitter->addWidget(d->m_varTabWgt);

    d->m_varView = new QTreeView(this);
    d->m_varTabWgt->addTab(d->m_varView, tr("Variables"));
    VariableTreeModel *varModel = new VariableTreeModel(this);
    d->m_varView->setModel(varModel);
    d->m_varView->setIndentation(10);

    QWidget *dummy = new QWidget(this);
    d->m_varTabWgt->addTab(dummy, tr("dummy1"));

    connect(d->m_varView, SIGNAL(expanded(const QModelIndex)),
            varModel, SLOT(lazyLoad(const QModelIndex)));


    // stack and breakpoints tabwidget
    d->m_stackTabWgt = new QTabWidget(d->m_splitter);
    d->m_splitter->addWidget(d->m_stackTabWgt);

    // stack view
    d->m_stackView = new QTableView(this);
    StackFramesModel *stackModel = new StackFramesModel(this);
    d->m_stackView->setModel(stackModel);
    d->m_stackView->verticalHeader()->hide();
    d->m_stackView->setShowGrid(false);
    d->m_stackView->setTextElideMode(Qt::ElideLeft);
    d->m_stackView->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->m_stackTabWgt->addTab(d->m_stackView, tr("Stack"));

    connect(d->m_stackView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex&)),
            this, SLOT(stackViewCurrentChanged(const QModelIndex&, const QModelIndex&)));

    // breakpoints view
    d->m_breakpointView = new QTableView(this);
    PythonBreakpointModel *bpModel = new PythonBreakpointModel(this);
    d->m_breakpointView->setModel(bpModel);
    d->m_breakpointView->verticalHeader()->hide();
    d->m_breakpointView->setShowGrid(false);
    d->m_breakpointView->setTextElideMode(Qt::ElideLeft);
    d->m_breakpointView->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->m_breakpointView->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_stackTabWgt->addTab(d->m_breakpointView, tr("Breakpoints"));

    connect(d->m_breakpointView, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(customBreakpointContextMenu(const QPoint&)));
    connect(d->m_breakpointView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex&)),
            this, SLOT(breakpointViewCurrentChanged(const QModelIndex&, const QModelIndex&)));

    // issues view
    d->m_issuesView = new QTableView(this);
    IssuesModel *issueModel = new IssuesModel(this);
    d->m_issuesView->setModel(issueModel);
    d->m_issuesView->verticalHeader()->hide();
    d->m_issuesView->setShowGrid(false);
    d->m_issuesView->setTextElideMode(Qt::ElideLeft);
    d->m_issuesView->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->m_issuesView->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_issuesView->setStyleSheet(QLatin1String("QToolTip {font-size:12pt; font-family:'DejaVu Sans Mono', Courier; }"));
    d->m_stackTabWgt->addTab(d->m_issuesView, tr("Issues"));

    connect(d->m_issuesView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex&)),
            this, SLOT(issuesViewCurrentChanged(const QModelIndex&, const QModelIndex&)));
    connect(d->m_issuesView, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(customIssuesContextMenu(const QPoint&)));

    setLayout(vLayout);

    // raise the tab page set in the preferences
    ParameterGrp::handle hGrp = WindowParameter::getDefaultParameter()->GetGroup("DebugView");
    d->m_stackTabWgt->setCurrentIndex(hGrp->GetInt("AutoloadStackViewTab", 0));
    d->m_varTabWgt->setCurrentIndex(hGrp->GetInt("AutoloadVarViewTab", 0));


    // restore header data such column width etc
    std::string state = hGrp->GetASCII("BreakpointHeaderState", "");
    d->m_breakpointView->horizontalHeader()->restoreState(QByteArray::fromBase64(state.c_str()));

    // restore header data such column width etc
    state = hGrp->GetASCII("StackHeaderState", "");
    d->m_stackView->horizontalHeader()->restoreState(QByteArray::fromBase64(state.c_str()));

    // restore header data such column width etc
    state = hGrp->GetASCII("VarViewHeaderState", "");
    d->m_varView->header()->restoreState(QByteArray::fromBase64(state.c_str()));

    // restore header data such column width etc
    state = hGrp->GetASCII("IssuesHeaderState", "");
    d->m_issuesView->horizontalHeader()->restoreState(QByteArray::fromBase64(state.c_str()));

    // splitter setting
    state = hGrp->GetASCII("SplitterState", "");
    d->m_splitter->restoreState(QByteArray::fromBase64(state.c_str()));
}

PythonDebuggerView::~PythonDebuggerView()
{
    // save currently viewed tab
    ParameterGrp::handle hGrp = WindowParameter::getDefaultParameter()->GetGroup("DebugView");
    hGrp->SetInt("AutoloadStackViewTab", d->m_stackTabWgt->currentIndex());
    hGrp->SetInt("AutoloadVarViewTab", d->m_varTabWgt->currentIndex());

    // save column width for breakpointView
    QByteArray state = d->m_breakpointView->horizontalHeader()->saveState();
    hGrp->SetASCII("BreakpointHeaderState", state.toBase64().data());

    // save column width for stackView
    state = d->m_stackView->horizontalHeader()->saveState();
    hGrp->SetASCII("StackHeaderState", state.toBase64().data());

    // save column width for varView
    state = d->m_varView->header()->saveState();
    hGrp->SetASCII("VarViewHeaderState", state.toBase64().data());

    // save column width for stackView
    state = d->m_issuesView->horizontalHeader()->saveState();
    hGrp->SetASCII("IssuesHeaderState", state.toBase64().data());

    // splitter setting
    state = d->m_splitter->saveState();
    hGrp->SetASCII("SplitterState", state.toBase64().data());

    delete d;
}

void PythonDebuggerView::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        d->m_stackTabWgt->setTabText(0, tr("Stack"));
        d->m_stackTabWgt->setTabText(1, tr("Breakpoints"));
        d->m_stackTabWgt->setTabText(2, tr("Issues"));
        d->m_varTabWgt->setTabText(0, tr("Variables"));
    }
}

void PythonDebuggerView::startDebug()
{
    PythonEditorView *editView = PythonEditorView::setAsActive();
    if (editView)
        editView->startDebug();
}

void PythonDebuggerView::enableButtons()
{
    PythonDebugger *debugger = PythonDebugger::instance();
    bool running = debugger->isRunning();
    bool halted = debugger->isHalted();
    if (d->m_startDebugBtn->isEnabled() != !running)
        d->m_startDebugBtn->setEnabled(!running);

    if (d->m_continueBtn->isEnabled() != running)
        d->m_continueBtn->setEnabled(running);

    if (d->m_stopDebugBtn->isEnabled() != running)
        d->m_stopDebugBtn->setEnabled(running);

    if (d->m_stepIntoBtn->isEnabled() != halted)
        d->m_stepIntoBtn->setEnabled(halted);

    if (d->m_stepOverBtn->isEnabled() != halted)
        d->m_stepOverBtn->setEnabled(halted);

    if (d->m_stepOutBtn->isEnabled() != halted)
        d->m_stepOutBtn->setEnabled(halted);

    if (d->m_haltOnNextBtn->isEnabled() != running && !debugger->isHaltOnNext() && !halted)
        d->m_haltOnNextBtn->setEnabled(running && !debugger->isHaltOnNext() && !halted);
}

void PythonDebuggerView::stackViewCurrentChanged(const QModelIndex &current,
                                                 const QModelIndex &previous)
{
    Q_UNUSED(previous);

    QAbstractItemModel *viewModel = d->m_stackView->model();
    QModelIndex idxLeft = viewModel->index(current.row(), 0, QModelIndex());
    QModelIndex idxRight = viewModel->index(current.row(),
                                            viewModel->columnCount(),
                                            QModelIndex());

    QItemSelection sel(idxLeft, idxRight);
    QItemSelectionModel *selModel = d->m_stackView->selectionModel();
    selModel->select(sel, QItemSelectionModel::Rows);

    QModelIndex stackIdx = current.sibling(current.row(), 0);
    StackFramesModel *model = reinterpret_cast<StackFramesModel*>(d->m_stackView->model());
    if (!model)
        return;
    QVariant idx = model->data(stackIdx, Qt::DisplayRole);

    PythonDebugger::instance()->setStackLevel(idx.toInt());
}

void PythonDebuggerView::breakpointViewCurrentChanged(const QModelIndex &current,
                                                      const QModelIndex &previous)
{
    Q_UNUSED(previous);
    BreakpointLine *bpl = PythonDebugger::instance()->
                                        getBreakpointLineFromIdx(current.row());
    if (!bpl)
        return;

    setFileAndScrollToLine(bpl->parent()->fileName(), bpl->lineNr());
}

void PythonDebuggerView::issuesViewCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    QAbstractItemModel *model = d->m_issuesView->model();
    QModelIndex idxLine = model->index(current.row(), 0);
    QModelIndex idxFile = model->index(current.row(), 1);
    QVariant vLine = model->data(idxLine);
    QVariant vFile = model->data(idxFile);
    if (vLine.isValid() && vFile.isValid())
        setFileAndScrollToLine(vFile.toString(), vLine.toInt());
}

void PythonDebuggerView::customBreakpointContextMenu(const QPoint &pos)
{
    QModelIndex currentItem = d->m_breakpointView->indexAt(pos);
    if (!currentItem.isValid())
        return;

    BreakpointLine *bpl = PythonDebugger::instance()->
                                getBreakpointLineFromIdx(currentItem.row());
    if (!bpl)
        return;

    QMenu menu;
    QAction disable(BitmapFactory().iconFromTheme("breakpoint-disabled"),
                    tr("Disable breakpoint"), &menu);
    QAction enable(BitmapFactory().iconFromTheme("breakpoint"),
                   tr("Enable breakpoint"), &menu);
    QAction edit(BitmapFactory().iconFromTheme("preferences-general"),
                    tr("Edit breakpoint"), &menu);
    QAction del(BitmapFactory().iconFromTheme("delete"),
                   tr("Delete breakpoint"), &menu);
    QAction clear(BitmapFactory().iconFromTheme("process-stop"),
                  tr("Clear all breakpoints"), &menu);

    if (bpl->disabled())
        menu.addAction(&enable);
    else
        menu.addAction(&disable);
    menu.addAction(&edit);
    menu.addAction(&del);
    menu.addSeparator();
    menu.addAction(&clear);


    QAction *res = menu.exec(d->m_breakpointView->mapToGlobal(pos));
    if (res == &disable) {
        bpl->setDisabled(true);
    } else if(res == &enable) {
        bpl->setDisabled(false);
    } else if (res == &edit) {
        PythonEditorBreakpointDlg dlg(this, bpl);
        dlg.exec();
    } else if (res == &del) {
        PythonDebugger::instance()->deleteBreakpoint(bpl);
    } else if (res == &clear) {
        PythonDebugger::instance()->clearAllBreakPoints();
    }
}

void PythonDebuggerView::customIssuesContextMenu(const QPoint &pos)
{
    QAbstractItemModel *model = d->m_issuesView->model();
    if (model->rowCount() < 1)
        return;

    int line = 0;
    QMenu menu;
    QAction *clearCurrent = nullptr;
    QModelIndex currentItem = d->m_issuesView->indexAt(pos);
    QString fn;
    if (currentItem.isValid()) {
        fn = model->data(currentItem.sibling(currentItem.row(), 1),
                         Qt::DisplayRole).toString();
        line = model->data(currentItem.sibling(currentItem.row(), 0),
                         Qt::DisplayRole).toInt();
        QVariant vIcon = model->data(currentItem.sibling(currentItem.row(), 0),
                                     Qt::DecorationRole);

        if (vIcon.isValid())
            clearCurrent = new QAction(vIcon.value<QIcon>(), tr("Clear issue"), &menu);
        else
            clearCurrent = new QAction(tr("Clear issue"), &menu);
        menu.addAction(clearCurrent);
    }

    QAction clearAll(BitmapFactory().iconFromTheme("process-stop"),
                          tr("Clear all Issues"), &menu);
    menu.addAction(&clearAll);

    // run context menu
    QAction *res = menu.exec(d->m_issuesView->mapToGlobal(pos));
    // rely on signals between our model and PythonDebugger
    if (res == clearCurrent)
        PythonDebugger::instance()->sendClearException(fn, line);
    else
        PythonDebugger::instance()->sendClearAllExceptions();

    if (clearCurrent)
        delete clearCurrent;
}

void PythonDebuggerView::saveExpandedVarViewState()
{

}

void PythonDebuggerView::restoreExpandedVarViewState()
{

}

void PythonDebuggerView::initButtons(QVBoxLayout *vLayout)
{
    // debug buttons
    d->m_startDebugBtn = new QPushButton(this);
    d->m_continueBtn   = new QPushButton(this);
    d->m_stopDebugBtn  = new QPushButton(this);
    d->m_stepIntoBtn   = new QPushButton(this);
    d->m_stepOverBtn   = new QPushButton(this);
    d->m_stepOutBtn    = new QPushButton(this);
    d->m_haltOnNextBtn = new QPushButton(this);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(d->m_startDebugBtn);
    btnLayout->addWidget(d->m_stopDebugBtn);
    btnLayout->addWidget(d->m_haltOnNextBtn);
    btnLayout->addWidget(d->m_continueBtn);
    btnLayout->addWidget(d->m_stepIntoBtn);
    btnLayout->addWidget(d->m_stepOverBtn);
    btnLayout->addWidget(d->m_stepOutBtn);
    vLayout->addLayout(btnLayout);

    d->m_startDebugBtn->setIcon(BitmapFactory().iconFromTheme("debug-start").pixmap(16,16));
    d->m_continueBtn->setIcon(BitmapFactory().iconFromTheme("debug-continue").pixmap(16,16));
    d->m_stopDebugBtn->setIcon(BitmapFactory().iconFromTheme("debug-stop").pixmap(16,16));
    d->m_stepOverBtn->setIcon(BitmapFactory().iconFromTheme("debug-step-over").pixmap(16,16));
    d->m_stepOutBtn->setIcon(BitmapFactory().iconFromTheme("debug-step-out").pixmap(16,16));
    d->m_stepIntoBtn->setIcon(BitmapFactory().iconFromTheme("debug-step-into").pixmap(16,16));
    d->m_haltOnNextBtn->setIcon(BitmapFactory().iconFromTheme("debug-halt").pixmap(16,16));
    d->m_startDebugBtn->setToolTip(tr("Start debugging"));
    d->m_stopDebugBtn->setToolTip(tr("Stop debugger"));
    d->m_continueBtn->setToolTip(tr("Continue running"));
    d->m_stepIntoBtn->setToolTip(tr("Next instruction, steps into functions"));
    d->m_stepOverBtn->setToolTip(tr("Next instruction, don't step into functions"));
    d->m_stepOutBtn->setToolTip(tr("Continue until current function ends"));
    d->m_haltOnNextBtn->setToolTip(tr("Halt on any python code"));
    d->m_continueBtn->setAutoRepeat(true);
    d->m_stepIntoBtn->setAutoRepeat(true);
    d->m_stepOverBtn->setAutoRepeat(true);
    d->m_stepOutBtn->setAutoRepeat(true);
    enableButtons();

    PythonDebugger *debugger = PythonDebugger::instance();

    connect(d->m_startDebugBtn, SIGNAL(clicked()), this, SLOT(startDebug()));
    connect(d->m_stopDebugBtn, SIGNAL(clicked()), debugger, SLOT(stop()));
    connect(d->m_haltOnNextBtn, SIGNAL(clicked()), debugger, SLOT(haltOnNext()));
    connect(d->m_stepIntoBtn, SIGNAL(clicked()), debugger, SLOT(stepInto()));
    connect(d->m_stepOverBtn, SIGNAL(clicked()), debugger, SLOT(stepOver()));
    connect(d->m_stepOutBtn, SIGNAL(clicked()), debugger, SLOT(stepOut()));
    connect(d->m_continueBtn, SIGNAL(clicked()), debugger, SLOT(stepContinue()));
    connect(debugger, SIGNAL(haltAt(QString,int)), this, SLOT(enableButtons()));
    connect(debugger, SIGNAL(releaseAt(QString,int)), this, SLOT(enableButtons()));
    connect(debugger, SIGNAL(started()), this, SLOT(enableButtons()));
    connect(debugger, SIGNAL(stopped()), this, SLOT(enableButtons()));

}

/* static */
void PythonDebuggerView::setFileAndScrollToLine(const QString &fn, int line)
{
    // switch file in editor so we can view line
    PythonEditorView* editView = qobject_cast<PythonEditorView*>(
                                        getMainWindow()->activeWindow());
    if (!editView)
        return;

    //getMainWindow()->setActiveWindow(editView);
    if (editView->fileName() != fn)
        editView->open(fn);


    // scroll to view
    QTextCursor cursor(editView->getEditor()->document()->
                       findBlockByLineNumber(line - 1)); // ln-1 because line number starts from 0
    editView->getEditor()->setTextCursor(cursor);
}

// ---------------------------------------------------------------------------

StackFramesModel::StackFramesModel(QObject *parent) :
    QAbstractTableModel(parent),
    m_currentFrame(nullptr)
{

    PythonDebugger *debugger = PythonDebugger::instance();

    connect(debugger, SIGNAL(functionCalled(PyFrameObject*)),
               this, SLOT(updateFrames(PyFrameObject*)));
    connect(debugger, SIGNAL(functionExited(PyFrameObject*)),
               this, SLOT(updateFrames(PyFrameObject*)));
    connect(debugger, SIGNAL(stopped()), this, SLOT(clear()));
}

StackFramesModel::~StackFramesModel()
{
}

int StackFramesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return PythonDebugger::instance()->callDepth(m_currentFrame);
}

int StackFramesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return colCount +1;
}

QVariant StackFramesModel::data(const QModelIndex &index, int role) const
{
    if (m_currentFrame == nullptr)
        return QVariant();

    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::ToolTipRole))
        return QVariant();

    if (index.column() > colCount)
        return QVariant();

    Base::PyGILStateLocker locker;
    PyFrameObject *frame = m_currentFrame;

    int i = 0,
        j = PythonDebugger::instance()->callDepth(m_currentFrame);

    while (NULL != frame) {
        if (i == index.row()) {
            switch(index.column()) {
            case 0:
                return QString::number(j);
            case 1: { // function
                const char *funcname = PyBytes_AsString(frame->f_code->co_name);
                return QString(QLatin1String(funcname));
            }
            case 2: {// file
                const char *filename = PyBytes_AsString(frame->f_code->co_filename);
                return QString(QLatin1String(filename));
            }
            case 3: {// line
                int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
                return QString::number(line);
            }
            default:
                return QVariant();
            }
        }

        frame = frame->f_back;
        ++i;
        --j; // need to reversed, display current on top
    }


    return QVariant();
}

QVariant StackFramesModel::headerData(int section,
                                      Qt::Orientation orientation,
                                      int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0:
        return QString(tr("Level"));
    case 1:
        return QString(tr("Function"));
    case 2:
        return QString(tr("File"));
    case 3:
        return QString(tr("Line"));
    default:
        return QVariant();
    }
}

void StackFramesModel::clear()
{
    int i = 0;
    Base::PyGILStateLocker locker;
    Py_XDECREF(m_currentFrame);
    while (nullptr != m_currentFrame) {
        m_currentFrame = m_currentFrame->f_back;
        ++i;
    }

    beginRemoveRows(QModelIndex(), 0, i);
    endRemoveRows();
}

void StackFramesModel::updateFrames(PyFrameObject *frame)
{
    Base::PyGILStateLocker locker;
    if (m_currentFrame != frame) {
        clear();
        m_currentFrame = frame;
        Py_XINCREF(frame);

        beginInsertRows(QModelIndex(), 0, rowCount()-1);
        endInsertRows();
    }
}

// --------------------------------------------------------------------


PythonBreakpointModel::PythonBreakpointModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    PythonDebugger *dbg = PythonDebugger::instance();
    connect(dbg, SIGNAL(breakpointAdded(const BreakpointLine*)),
            this, SLOT(added(const BreakpointLine*)));
    connect(dbg, SIGNAL(breakpointChanged(const BreakpointLine*)),
            this, SLOT(changed(const BreakpointLine*)));
    connect(dbg, SIGNAL(breakpointRemoved(int, const BreakpointLine*)),
            this, SLOT(removed(int, const BreakpointLine*)));
}

PythonBreakpointModel::~PythonBreakpointModel()
{
}

int PythonBreakpointModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return PythonDebugger::instance()->breakpointCount();
}

int PythonBreakpointModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return colCount +1;
}

QVariant PythonBreakpointModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole &&
                             role != Qt::ToolTipRole &&
                             role != Qt::DecorationRole))
    {
        return QVariant();
    }

    if (index.column() > colCount)
        return QVariant();

    BreakpointLine *bpl = PythonDebugger::instance()->getBreakpointLineFromIdx(index.row());
    if (!bpl)
        return QVariant();

    if (role == Qt::DecorationRole) {
        if (index.column() != 0)
            return QVariant();
        const char *icon = bpl->disabled() ? "breakpoint-disabled" : "breakpoint" ;

        return QVariant(BitmapFactory().iconFromTheme(icon));

    } else if (role == Qt::ToolTipRole) {
        QString enabledStr(QLatin1String("enabled"));
        if (bpl->disabled())
            enabledStr = QLatin1String("disabled");

        QString txt = QString(QLatin1String("line %1 [%2] number: %3\n%4"))
                    .arg(bpl->lineNr()).arg(enabledStr)
                    .arg(bpl->uniqueNr()).arg(bpl->parent()->fileName());
        return QVariant(txt);
    }

    switch(index.column()) {
    case 0:
        return QString::number(bpl->uniqueNr());
    case 1: {// file
        return bpl->parent()->fileName();
    }
    case 2: {// line
        return QString::number(bpl->lineNr());
    }
    default:
        return QVariant();
    }

    return QVariant();
}

QVariant PythonBreakpointModel::headerData(int section,
                                           Qt::Orientation orientation,
                                           int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0:
        return QString(tr("Number"));
    case 1:
        return QString(tr("File"));
    case 2:
        return QString(tr("Line"));
    default:
        return QVariant();
    }
}

void PythonBreakpointModel::added(const BreakpointLine *bp)
{
    Q_UNUSED(bp);
    int count = rowCount();
    beginInsertRows(QModelIndex(), count, count);
    endInsertRows();
}

void PythonBreakpointModel::changed(const BreakpointLine *bp)
{
    int idx = PythonDebugger::instance()->getIdxFromBreakpointLine(*bp);
    if (idx > -1) {
        Q_EMIT dataChanged(index(idx, 0), index(idx, colCount));
    }
}

void PythonBreakpointModel::removed(int idx, const BreakpointLine *bpl)
{
    Q_UNUSED(bpl);
    beginRemoveRows(QModelIndex(), idx, idx);
    endRemoveRows();
}

// --------------------------------------------------------------------


IssuesModel::IssuesModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    connect(PythonDebugger::instance(), SIGNAL(exceptionOccured(const Py::ExceptionInfo*)),
            this, SLOT(exceptionOccured(const Py::ExceptionInfo*)));

    connect(PythonDebugger::instance(), SIGNAL(exceptionFatal(const Py::ExceptionInfo*)),
            this, SLOT(exception(const Py::ExceptionInfo*)));

    connect(PythonDebugger::instance(), SIGNAL(started()), this, SLOT(clear()));
    connect(PythonDebugger::instance(), SIGNAL(clearAllExceptions()), this, SLOT(clear()));
    connect(PythonDebugger::instance(), SIGNAL(clearException(QString,int)),
            this, SLOT(clearException(QString,int)));
}

IssuesModel::~IssuesModel()
{
    qDeleteAll(m_exceptions);
}

int IssuesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_exceptions.size();
}

int IssuesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return colCount;
}

QVariant IssuesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole &&
                             role != Qt::ToolTipRole &&
                             role != Qt::DecorationRole))
    {
        return QVariant();
    }

    if (index.column() > colCount)
        return QVariant();

    Py::ExceptionInfo *exc = m_exceptions.at(index.row());
    if (!exc || !exc->isValid())
        return QVariant();

    if (role == Qt::DecorationRole) {
        if (index.column() != 0)
            return QVariant();
        return QVariant(BitmapFactory().iconFromTheme(exc->iconName()));

    } else if (role == Qt::ToolTipRole) {

        QString srcText = exc->text();
        int offset = exc->offset();
        if (offset > 0) { // syntax error and such that give a column where it occurred
            if (!srcText.endsWith(QLatin1String("\n")))
                srcText += QLatin1String("\n");
            for (int i = 0; i < offset -1; ++i) {
                srcText += QLatin1Char('-');
            }
            srcText += QLatin1Char('^');
        }

        return  QString(tr("%1 on line %2 in file %3\nreason: '%4'\n\n%5"))
                                .arg(exc->typeString())
                                .arg(QString::number(exc->lineNr()))
                                .arg(exc->fileName())
                                .arg(exc->message())
                                .arg(srcText);
    }

    switch(index.column()) {
    case 0:
        return QString::number(exc->lineNr());
    case 1: // file
        return exc->fileName();
    case 2: // msg
        return exc->message();
    case 3: // function
        return exc->functionName();
    default:
        return QVariant();
    }

    return QVariant();
}

QVariant IssuesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0:
        return QString(tr("Line"));
    case 1:
        return QString(tr("File"));
    case 2:
        return QString(tr("Msg"));
    case 3:
        return QString(tr("Function"));
    default:
        return QVariant();
    }
}

bool IssuesModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    beginRemoveRows(QModelIndex(), row, count -1);
    for (int i = row; i < row + count; ++i) {
        m_exceptions.removeAt(i);
    }
    endRemoveRows();
    return true;
}

void IssuesModel::exceptionOccured(const Py::ExceptionInfo *exc)
{
    // on runtime, we don't want to catch runtime exceptions before they are fatal
    // but we do want to catch warnings
    //if (exc->isWarning())
        exception(exc);

}

void IssuesModel::exception(const Py::ExceptionInfo *exception)
{
    // already set?
    for (const Py::ExceptionInfo *exc : m_exceptions) {
        if (exc->fileName() == exception->fileName() &&
            exc->lineNr() == exception->lineNr())
            return;
    }

    // copy exception and store it
    Py::ExceptionInfo *exc = new Py::ExceptionInfo(*exception);
    beginInsertRows(QModelIndex(), m_exceptions.size(), m_exceptions.size());
    m_exceptions.append(exc);
    endInsertRows();
}

void IssuesModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, m_exceptions.size() -1);
    m_exceptions.clear();
    endRemoveRows();
}

void IssuesModel::clearException(const QString &fn, int line)
{
    for (int i = 0; i < m_exceptions.size(); ++i) {
        if (m_exceptions[i]->fileName() == fn &&
            m_exceptions[i]->lineNr() == line)
        {
            beginRemoveRows(QModelIndex(), i, 1);
            m_exceptions.removeAt(i);
            endRemoveRows();
            break;
        }
    }
}


// --------------------------------------------------------------------


VariableTreeItem::VariableTreeItem(const QList<QVariant> &data,
                                   VariableTreeItem *parent):
    parentItem(parent),
    lazyLoad(false)
{
    itemData = data;
}

VariableTreeItem::~VariableTreeItem()
{
    qDeleteAll(childItems);
}

void VariableTreeItem::addChild(VariableTreeItem *item)
{
    QRegExp re(QLatin1String("^__\\w[\\w\\n]*__$"));
    bool insertedIsSuper = re.indexIn(item->name()) != -1;

    // insert sorted
    for (int i = 0; i < childItems.size(); ++i) {
        bool itemIsSuper = re.indexIn(childItems[i]->name()) != -1;

        if (insertedIsSuper && itemIsSuper &&
            item->name() < childItems[i]->name()) {
            // both super sort alphabetically
            childItems.insert(i, item);
            return;
        } else if (!insertedIsSuper && itemIsSuper) {
            // new item not super but this child item is
            // insert before this one
            childItems.insert(i, item);
            return;
        } else if (item->name() < childItems[i]->name()) {
            // no super, just sort aplhabetically
            childItems.insert(i, item);
            return;
        }
    }

    // if we get here are definitely last
    childItems.append(item);
}

bool VariableTreeItem::removeChild(int row)
{
    if (row > -1 && row < childItems.size()) {
        VariableTreeItem *item = childItems.takeAt(row);
        item->parentItem = nullptr;
        delete item;
        return true;
    }
    return false;
}

bool VariableTreeItem::removeChildren(int row, int nrRows)
{
    if (row < 0 || row >= childItems.size())
        return false;
    if (row + nrRows > childItems.size())
        return false;

    for (int i = row; i < row + nrRows; ++i) {
        VariableTreeItem *item = childItems.takeAt(row);
        delete item;
    }
    return true;
}

VariableTreeItem *VariableTreeItem::child(int row)
{
    return childItems.value(row);
}

int VariableTreeItem::childRowByName(const QString &name)
{
    for (int i = 0; i < childItems.size(); ++i) {
        VariableTreeItem *item = childItems[i];
        if (item->name() == name)
            return i;
    }
    return -1;
}

VariableTreeItem *VariableTreeItem::childByName(const QString &name)
{
    for (int i = 0; i < childItems.size(); ++i) {
        VariableTreeItem *item = childItems.at(i);
        if (item->name() == name)
            return item;
    }

    return nullptr;
}

int VariableTreeItem::childCount() const
{
    return childItems.count();
}

int VariableTreeItem::columnCount() const
{
    return itemData.count();
}

QVariant VariableTreeItem::data(int column) const
{
    return itemData.value(column);
}

VariableTreeItem *VariableTreeItem::parent()
{
    return parentItem;
}

const QString VariableTreeItem::name() const
{
    return itemData.value(0).toString();
}

const QString VariableTreeItem::value() const
{
    return itemData.value(1).toString();
}

void VariableTreeItem::setValue(const QString value)
{
    itemData[1] = value;
}

const QString VariableTreeItem::type() const
{
    return itemData.value(2).toString();
}

void VariableTreeItem::setType(const QString type)
{
    itemData[2] = type;
}

void VariableTreeItem::setLazyLoadChildren(bool lazy)
{
    lazyLoad = lazy;
}

Py::Object VariableTreeItem::getAttr(const QString attrName) const
{
    Py::Object me;
    if (rootObj.isNull())
        me = parentItem->getAttr(itemData[0].toString());
    else
        me = rootObj;

    if (me.isNull())
        return me; // return nullobj

    Py::String attr(attrName.toStdString());

    if (PyModule_Check(me.ptr())) {
        if (me.hasAttr(attr))
            return me.getAttr(attr);

        me = PyModule_GetDict(me.ptr());
    }
    if (me.isDict() || me.isList())
        return me.getItem(attr);
    if (me.hasAttr(attr))
        return me.getAttr(attr);
    return Py::None();
}

bool VariableTreeItem::hasAttr(const QString attrName) const
{
    return getAttr(attrName).isNull();
}

void VariableTreeItem::setMeAsRoot(Py::Object root)
{
    rootObj = root;
}

bool VariableTreeItem::lazyLoadChildren() const
{
    return lazyLoad;
}

int VariableTreeItem::childNumber() const
{
    if (parentItem)
        return parentItem->childItems.indexOf(const_cast<VariableTreeItem*>(this));

    return 0;
}

// --------------------------------------------------------------------------



VariableTreeModel::VariableTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    QList<QVariant> rootData, locals, globals; //, builtins;
    rootData << tr("Name") << tr("Value") << tr("Type");
    m_rootItem = new VariableTreeItem(rootData, nullptr);

    locals << QLatin1String("locals") << QLatin1String("") << QLatin1String("");
    globals << QLatin1String("globals") << QLatin1String("") << QLatin1String("");
    //builtins << QLatin1String("builtins") << QLatin1String("") << QLatin1String("");
    m_localsItem = new VariableTreeItem(locals, m_rootItem);
    m_globalsItem = new VariableTreeItem(globals, m_rootItem);
    //m_builtinsItem = new VariableTreeItem(builtins, m_rootItem);
    m_rootItem->addChild(m_localsItem);
    m_rootItem->addChild(m_globalsItem);
    //m_rootItem->appendChild(m_builtinsItem);

    PythonDebugger *debugger = PythonDebugger::instance();

    connect(debugger, SIGNAL(nextInstruction(PyFrameObject*)),
               this, SLOT(updateVariables(PyFrameObject*)));

    connect(debugger, SIGNAL(functionExited(PyFrameObject*)),
               this, SLOT(clear()));

    connect(debugger, SIGNAL(stopped()), this, SLOT(clear()));
}

VariableTreeModel::~VariableTreeModel()
{
    delete m_rootItem; // root item handles delete of children
}

int VariableTreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        VariableTreeItem *item =  getItem(parent);
        if (item)
            return item->columnCount();

    }

    return m_rootItem->columnCount();
}

bool VariableTreeModel::addRows(QList<VariableTreeItem*> added, const QModelIndex &parent)
{
    VariableTreeItem *parentItem = getItem(parent);
    if (!parentItem)
        return false;

    int childCount =  parentItem->childCount();

    beginInsertRows(parent, childCount, childCount + added.size() -1);
    for (VariableTreeItem *item : added) {
        parentItem->addChild(item);
    }
    endInsertRows();
    Q_EMIT layoutChanged();

    return true;
}

bool VariableTreeModel::removeRows(int firstRow, int nrRows, const QModelIndex &parent)
{
    VariableTreeItem *item = getItem(parent);
    if (!item)
        return false;

    if (firstRow < 0 || firstRow >= item->childCount())
        return false;
    if (nrRows < 0 || firstRow + nrRows > item->childCount())
        nrRows = item->childCount() - firstRow;

    beginRemoveRows(parent, firstRow, firstRow + nrRows -1);
    for (int i = firstRow; i < firstRow + nrRows; ++i) {
        item->removeChild(firstRow);
    }
    endRemoveRows();

    // invalidate abstractmodels internal index, otherwise we crash
    for(QModelIndex &idx : persistentIndexList()) {
        changePersistentIndex(idx, QModelIndex());
    }

    return true;
}

bool VariableTreeModel::hasChildren(const QModelIndex &parent) const
{
    VariableTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = getItem(parent);

    if (!parentItem)
        return 0;

    return parentItem->lazyLoadChildren() || parentItem->childCount() > 0;
}

VariableTreeItem *VariableTreeModel::getItem(const QModelIndex &index) const
{
    if (index.isValid()) {
        VariableTreeItem *item = static_cast<VariableTreeItem*>(index.internalPointer());
        if (item)
            return item;
    }
    return m_rootItem;
}


QVariant VariableTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant();

    VariableTreeItem *item = getItem(index);

    if (!item)
        return QVariant();

    return item->data(index.column());
}

Qt::ItemFlags VariableTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return QAbstractItemModel::flags(index); // Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant VariableTreeModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_rootItem->data(section);

    return QVariant();
}

QModelIndex VariableTreeModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    VariableTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = getItem(parent);

    if (!parentItem)
        return QModelIndex();

    VariableTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);

    return QModelIndex();
}

QModelIndex VariableTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    VariableTreeItem *childItem = getItem(index);
    if (!childItem)
        return QModelIndex();

    VariableTreeItem *parentItem = childItem->parent();

    if (parentItem == m_rootItem || parentItem == nullptr)
        return QModelIndex();

    return createIndex(parentItem->childNumber(), 0, parentItem);
}

int VariableTreeModel::rowCount(const QModelIndex &parent) const
{
    VariableTreeItem *item = getItem(parent);
    if (!item)
        return 0;
    return item->childCount();
}


void VariableTreeModel::clear()
{
    // locals
    if (m_localsItem->childCount() > 0) {
        QModelIndex localsIdx = createIndex(0, 0, m_localsItem);
        removeRows(0, m_localsItem->childCount(), localsIdx);
    }

    // globals
    if (m_globalsItem->childCount()) {
        QModelIndex globalsIdx = createIndex(0, 0, m_globalsItem);
        removeRows(0, m_globalsItem->childCount(), globalsIdx);
    }
}

void VariableTreeModel::updateVariables(PyFrameObject *frame)
{
    Base::PyGILStateLocker lock;


    PyFrame_FastToLocals(frame);

    // first locals
    VariableTreeItem *parentItem = m_localsItem;
    PyObject *rootObject = (PyObject*)frame->f_locals;
    if (rootObject) {
        m_localsItem->setMeAsRoot(Py::Object(rootObject));
        scanObject(rootObject, parentItem, 0, true);
    }

    // then globals
    parentItem = m_globalsItem;
    PyObject *globalsDict = frame->f_globals;
    if (globalsDict) {
        m_globalsItem->setMeAsRoot(Py::Object(globalsDict));
        scanObject(globalsDict, parentItem, 0, true);
    }

    /*
    // and the builtins
    parentItem = m_builtinsItem;
    rootObject = (PyObject*)frame->f_builtins;
    if (rootObject) {
        m_builtinsItem->setMeAsRoot(Py::Object(rootObject));
        scanObject(rootObject, parentItem, 0, false);
    }*/
}

void VariableTreeModel::lazyLoad(const QModelIndex &parent)
{
    VariableTreeItem *parentItem = getItem(parent);
    if (parentItem && parentItem->lazyLoadChildren())
        lazyLoad(parentItem);
}

void VariableTreeModel::lazyLoad(VariableTreeItem *parentItem)
{
    Py::Dict dict;
    // workaround to not being able to store pointers to variables
    QString myName = parentItem->name();
    Py::Object me = parentItem->parent()->getAttr(myName);
    if (me.isNull())
        return;

    try {

        Py::List lst (PyObject_Dir(me.ptr()));

        for (uint i = 0; i < lst.length(); ++i) {
            Py::Object attr(PyObject_GetAttr(me.ptr(), lst[i].str().ptr()));
            dict.setItem(lst[i], attr);
        }

        scanObject(dict.ptr(), parentItem, 15, true);

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
    }
}

void VariableTreeModel::scanObject(PyObject *startObject, VariableTreeItem *parentItem,
                                   int depth, bool noBuiltins)
{
    // avoid cyclic recursion
    static const int maxDepth = 15;
    if (depth > maxDepth)
        return;

    QList<VariableTreeItem*> added;
    QList<QString> visited, updated, deleted;

    Py::List keys;
    Py::Dict object;

    // only scan object which has keys
    try {
        if (PyDict_Check(startObject))
            object = Py::Dict(startObject);
        else if (PyList_Check(startObject)) {
            Py::List lst(startObject);
            for (int i = 0; i < static_cast<int>(lst.size()); ++i) {
                Py::Int key(i);
                object[key.str()] = lst[i];
            }
        } else if (PyTuple_Check(startObject)) {
            Py::Tuple tpl(startObject);
            for (int i = 0; i < static_cast<int>(tpl.size()); ++i) {
                Py::Int key(i);
                object[key.str()] = tpl[i];
            }
        } else if (PyFrame_Check(startObject)){
            PyFrameObject *frame = (PyFrameObject*)startObject;
            object = Py::Dict(frame->f_locals);
        } else if (PyModule_Check(startObject)) {
            object = Py::Dict(PyModule_GetDict(startObject));
        } else {
            // all objects should be able to use dir(o) on?
            object = Py::Dict(PyObject_Dir(startObject));

        }
        // let other types fail and return here

        keys = object.keys();

    } catch(Py::Exception e) {
        //Get error message
        PyObject *ptype, *pvalue, *ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        Py::String err(pvalue);
        qDebug() << "Not supported type in debugger:" << QString::fromStdString(err) << endl;
        e.clear();
        return;
    }


    // first we traverse and compare before we do anything so
    // view doesn't get updated too many times

    // traverse and compare
    for (Py::List::iterator it = keys.begin(); it != keys.end(); ++it) {
        Py::String key(*it);
        QString name = QString::fromStdString(key);
        // PyCXX methods throws here on type objects from class self type?
        // use Python C instead
        QString newValue, newType;
        PyObject *vl, *itm;
        itm = PyDict_GetItem(object.ptr(), key.ptr());
        if (itm) {
            Py_XINCREF(itm);
            if (PyCallable_Check(itm))
                continue; // don't want to crowd explorer with functions
            vl = PyObject_Bytes(itm);
            if (vl) {
                char *vlu = PyBytes_AS_STRING(vl);
                newValue = QString(QLatin1String(vlu));

                if (!PyBytes_Check(itm)) {
                    // extract memory address if needed, but not on ordinary strings
                    QRegExp re(QLatin1String("^<\\w+[\\w\\s'\"\\.\\-]* at (?:0x)?([0-9a-fA-F]+)>$"));
                    if (re.indexIn(newValue) != -1) {
                        newValue = re.cap(1);
                    }
                } else {
                    // it's a string, wrap in quotes ('')
                    newValue = QString(QLatin1String("'%1'")).arg(newValue);
                }

                newType = QString::fromLatin1(Py_TYPE(itm)->tp_name);
            }
            Py_DECREF(itm);
            Py_XDECREF(vl);
            PyErr_Clear();
        }

        VariableTreeItem *currentChild = parentItem->childByName(name);
        if (currentChild == nullptr) {
            // not found, should add to model
            QRegExp reBuiltin(QLatin1String(".*built-*in.*"));
            if (noBuiltins) {
                if (reBuiltin.indexIn(newType) != -1)
                    continue;
            } else if (reBuiltin.indexIn(newType) == -1)
                continue;

            QList<QVariant> data;
            data << name << newValue << newType;
            VariableTreeItem *createdItem = new VariableTreeItem(data, parentItem);
            added.append(createdItem);

            if (PyDict_Check(itm) || PyList_Check(itm) || PyTuple_Check(itm)) {
                // check for subobject recursively
                scanObject(itm, createdItem, depth +1, true);

            } else if (Py_TYPE(itm)->tp_dict && PyDict_Size(Py_TYPE(itm)->tp_dict)) // && // members check
                      // !createdItem->parent()->hasAttr(name)) // avoid cyclic child inclusions
            {
                // set lazy load for these
                createdItem->setLazyLoadChildren(true);
            }
        } else {
            visited.append(name);

            if (newType != currentChild->type() ||
                newValue != currentChild->value())
            {
                currentChild->setType(newType);
                currentChild->setValue(newValue);
                updated.append(name);
            }

            // check its children
            if (currentChild->lazyLoadChildren() && currentChild->childCount() > 0)
                lazyLoad(currentChild);
        }
    }

    // maybe some values have gotten out of scope
    for (int i = 0; i < parentItem->childCount(); ++i) {
        VariableTreeItem *item = parentItem->child(i);
        if (!visited.contains(item->name()))
            deleted.append(item->name());
    }

    // finished compare, now do the changes


    // handle updates
    // this tries to find out how many consecutive rows that've changed
    int row = 0;
    for (const QString &item : updated) {
        row = parentItem->childRowByName(item);
        if (row < 0) continue;

        QModelIndex topLeftIdx = createIndex(row, 1, parentItem);
        QModelIndex bottomRightIdx = createIndex(row, 2, parentItem);
        Q_EMIT dataChanged(topLeftIdx, bottomRightIdx);
    }

    // handle deletes
    for (const QString &name : deleted) {
        row = parentItem->childRowByName(name);
        if (row < 0) continue;
        QModelIndex parentIdx = createIndex(row, 0, parentItem);
        removeRows(row, 1, parentIdx);
    }

    // handle inserts
    // these are already created during compare phase
    // we insert last so variables don't jump around in the treeview
    if (added.size()) {
        QModelIndex parentIdx = createIndex(row, 0, parentItem);
        addRows(added, parentIdx);
    }

    // notify view that parentItem might have children now
    if (updated.size() || added.size() || deleted.size()) {
        //Q_EMIT layoutChanged();
    }
}



#include "moc_PythonDebuggerView.cpp"
