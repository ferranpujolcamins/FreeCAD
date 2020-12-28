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
#include "LangSpecifics.h"
#include <memory>
#include "EditorView.h"
#include "TextEditor.h"

using namespace Gui;


namespace Gui {
class AbstractLangViewP {
public:
    explicit AbstractLangViewP(EditorView *editorView, const char* langName)
        : view(editorView)
        , lang(langName)
    {}
    ~AbstractLangViewP() {}
    EditorView* view;
    const char *lang;
};

class PythonLangViewDbgP {
public:
    explicit PythonLangViewDbgP() {}
    ~PythonLangViewDbgP() {}
};
}

// -----------------------------------------------------------------

AbstractLangView::AbstractLangView(EditorView* editView, const char* langName)
    : d(new AbstractLangViewP(editView, langName))
{
}

AbstractLangView::~AbstractLangView()
{
    delete d;
}

const char *AbstractLangView::name() const
{
    return d->lang;
}

// -------------------------------------------------------------------


AbstractLangViewDbg::AbstractLangViewDbg(EditorView *editView,
                                         const char *langName)
    : AbstractLangView(editView, langName)
{
}

AbstractLangViewDbg::~AbstractLangViewDbg()
{
}


// ------------------------------------------------------------------------



PythonLangViewDbg::PythonLangViewDbg(EditorView *editView)
    : QObject(editView),
      AbstractLangViewDbg(editView, "Python")
{
    auto dbgr = debugger();
    using PyDbg = App::Debugging::Python::Debugger;
    connect(dbgr, &PyDbg::haltAt, this, &PythonLangViewDbg::editorShowDbgMrk);
    connect(dbgr, &PyDbg::releaseAt, this, &PythonLangViewDbg::editorHideDbgMrk);
    connect(dbgr, &PyDbg::breakpointAdded, this, &PythonLangViewDbg::onBreakpointAdded);
    connect(dbgr, &PyDbg::breakpointChanged, this, &PythonLangViewDbg::onBreakPointChanged);
    connect(dbgr, &PyDbg::breakpointRemoved, this, &PythonLangViewDbg::onBreakPointClear);
    //connect(dbgr, &PyDbg::exceptionOccured, this, &PythonLangViewDbg::exceptionOccured);
    connect(dbgr, &PyDbg::exceptionCleared, this, &PythonLangViewDbg::exceptionCleared);
    connect(dbgr, &PyDbg::allExceptionsCleared, this, &PythonLangViewDbg::allExceptionsCleared);
}

PythonLangViewDbg::~PythonLangViewDbg()
{
}

App::Debugging::Python::Debugger *PythonLangViewDbg::debugger()
{
    return App::Debugging::Python::Debugger::instance();
}

void PythonLangViewDbg::onBreakpointAdded(size_t uniqueId)
{
    onBreakPointChanged(uniqueId);
}

void PythonLangViewDbg::onBreakPointClear(size_t uniqueId)
{
    onBreakPointChanged(uniqueId);
}

void PythonLangViewDbg::onBreakPointChanged(size_t uniqueId)
{
    // trigger repaint of linemarkerarea
    auto bp = debugger()->getBreakpointFromUniqueId(uniqueId);
    if (!bp || bp->bpFile()->fileName() != view()->fileName())
        return;

    auto textEditor = dynamic_cast<TextEditor*>(view()->editor());
    if (textEditor)
        textEditor->lineMarkerArea()->update();
}


#include "moc_LangSpecifics.cpp"
