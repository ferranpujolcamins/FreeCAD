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
#ifndef LANGSPECIFICS_H
#define LANGSPECIFICS_H

#include <FCConfig.h>
#include <memory>
#include <QObject>
#include <App/DebuggerBase.h>
#include <App/PythonDebugger.h>

#include "EditorView.h"

/**
 * Purpose of this file is to move all things that
 * can be common between langages to a separate class
 * such as breakpoint, debugger hide/show markers
 *
 * TextEdit can then reach these through a smart pointer
 * ie load a python file, then a javascript file,
 *   same editor different behaviour regarding indent new line
 */

namespace Gui {

class AbstractLangViewP;

/**
 * @brief Abstract baseclass for all related to view behaviour
 */
class AbstractLangView : public std::enable_shared_from_this<AbstractLangView>
{
    AbstractLangViewP *d;
    const char *m_lang;
public:
    explicit AbstractLangView(std::weak_ptr<EditorView> editView, const char *langName);
    virtual ~AbstractLangView();

    const char* name() const;
    std::shared_ptr<EditorView> view() const;
    virtual bool onMsg(const char* pMsg, const char** ppReturn) = 0;
    virtual bool onHasMsg(const char* pMsg, const char** ppReturn) = 0;
};

// ---------------------------------------------------------------------

/**
 * @brief The AbstractLangViewDbg class handles bridge between editor and debugger
 */
class AbstractLangViewDbg : public AbstractLangView
{
public:
    explicit AbstractLangViewDbg(std::weak_ptr<EditorView> editView,
                                 const char* langName);
    ~AbstractLangViewDbg();

    virtual App::Debugging::DebuggerBase* debugger() = 0;

public: // Q_SLOTS:
    /// render line marker area when these changes
    virtual void onBreakpointAdded(size_t uniqueId) = 0;
    virtual void onBreakPointClear(size_t uniqueId) = 0;
    virtual void onBreakPointChanged(size_t uniqueId) = 0;

    // haltAt and releaseAt slots
    /// shows the debug marker arrow in filename editor at line
    virtual bool editorShowDbgMrk(const QString &fn, int line) = 0;

    /// hides the debug marker arrow in filename  editor
    virtual bool editorHideDbgMrk(const QString &fn, int line) = 0;


    virtual void exceptionOccured(Base::Exception *exeption)  = 0;
    virtual void exceptionCleared(const QString &fn, int line) = 0;
    virtual void allExceptionsCleared() = 0;

    /// build a contextmenu regarding line
    virtual void contextMenuBrkPnt(QMenu *menu, const QString &fn, int line) = 0;
};

// ---------------------------------------------------------------------

class PythonLangViewDbgP;
class PythonLangViewDbg : public QObject,
                          public AbstractLangViewDbg
{
    Q_OBJECT
    PythonLangViewDbgP *d;
public:
    explicit PythonLangViewDbg(std::weak_ptr<EditorView> editView);
    ~PythonLangViewDbg() override;

    App::Debugging::Python::Debugger* debugger() override;

public Q_SLOTS:
    /// render line marker area when these changes
    void onBreakpointAdded(size_t uniqueId) override;
    void onBreakPointClear(size_t uniqueId) override;
    void onBreakPointChanged(size_t uniqueId) override;

    // haltAt and releaseAt slots
    /// shows the debug marker arrow in filename editor at line
    bool editorShowDbgMrk(const QString &fn, int line) override;

    /// hides the debug marker arrow in filename  editor
    bool editorHideDbgMrk(const QString &fn, int line) override;


    void exceptionOccured(Base::Exception *exeption) override;
    void exceptionCleared(const QString &fn, int line) override;
    void allExceptionsCleared() override;


    void contextMenuBrkPnt(QMenu *menu, const QString &fn, int line) override;
};


}; // namespace Gui

#endif // LANGSPECIFICS_H
