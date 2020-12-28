/***************************************************************************
 *   Copyright (c) 2010 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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


#ifndef GUI_PYTHONDEBUG_H
#define GUI_PYTHONDEBUG_H

#include <FCConfig.h>
#include <CXX/Extensions.hxx>
#include <frameobject.h>
#include <string>
#include <vector>
#include <QObject>

#include "DebuggerBase.h"


namespace Base {
class PyException;
class PyExceptionInfo;
}

namespace App {
namespace Debugging {
namespace Python {

class BrkPnt;
class BrkPntFile;
class Debugger;
class DebuggerP;

/**
 * @brief The BreakpointPy class is the actual breakpoint
 */
class AppExport BrkPnt : public BrkPntBase<BrkPntFile>
{
public:
    explicit BrkPnt(int lineNr, std::weak_ptr<BrkPntFile> bpFile);
    explicit BrkPnt(const BrkPnt &other);
    virtual ~BrkPnt();

    BrkPnt& operator=(const BrkPnt &other);

    /**
     * @brief setCondition a python expression (True triggers breakpoint)
     * @param condition a python expression
     */
    void setCondition(const QString condition);
    /**
     * @brief condition: Gets the condition stored for this breakpoint
     * @param noSideEffects: Replaces a single = to ==
     *                       Prevents breakpoint from changing program state
     */
    const QString condition() const;

private:
    QString m_condition;
};

// --------------------------------------------------------------------

/**
 * @brief The BreakpointFile class
 *  contains all breakpoints for a given source file
 */
class AppExport BrkPntFile : public BrkPntBaseFile<Debugger, BrkPnt, BrkPntFile>
{
public:
    explicit BrkPntFile(std::shared_ptr<Debugger> dbgr);
    BrkPntFile(const BrkPntFile& other);
    ~BrkPntFile();
};

// ---------------------------------------------------------------------

class DebugModuleP;
/**
 * @author Werner Mayer
 */
class AppExport DebugModule : public Py::ExtensionModule<DebugModule>
{
public:
    static void init_module(void);

    DebugModule();
    virtual ~DebugModule();

private:
    Py::Object getFunctionCallCount(const Py::Tuple &a);
    Py::Object getExceptionCount(const Py::Tuple &a);
    Py::Object getLineCount(const Py::Tuple &a);
    Py::Object getFunctionReturnCount(const Py::Tuple &a);
    DebugModuleP *d;
};

// -----------------------------------------------------------------------------

/**
 * @author Werner Mayer
 */
class AppExport DebugStdout : public Py::PythonExtension<DebugStdout>
{
public:
    static void init_type(void);    // announce properties and methods

    DebugStdout();
    ~DebugStdout();

    Py::Object repr();
    Py::Object write(const Py::Tuple&);
    Py::Object flush(const Py::Tuple&);
};

// ------------------------------------------------------------------------------

/**
 * @author Werner Mayer
 */
class AppExport DebugStderr : public Py::PythonExtension<DebugStderr>
{
public:
    static void init_type(void);    // announce properties and methods

    DebugStderr();
    ~DebugStderr();

    Py::Object repr();
    Py::Object write(const Py::Tuple&);
};

// ---------------------------------------------------------------------------

/**
 * @author Werner Mayer
 */
class AppExport DebugExcept : public Py::PythonExtension<DebugExcept>
{
public:
    static void init_type(void);    // announce properties and methods

    DebugExcept();
    ~DebugExcept();

    Py::Object repr();
    Py::Object excepthook(const Py::Tuple&);
};

// ---------------------------------------------------------------------------

class AppExport Debugger : public AbstractDbgr<Debugger, BrkPnt, BrkPntFile>
{
    Q_OBJECT
public:
    explicit Debugger(QObject *parent = nullptr);
    //Debugger(const Debugger&) = delete;
    virtual ~Debugger();

    void runFile(const QString& fn);

    bool isHalted() const;

    PyFrameObject *currentFrame() const;

    /**
     * @brief callDepth: gets the call depth of frame
     * @param frame: if null use currentFrame
     * @return the call depth
     */
    int callDepth(const PyFrameObject *frame) const;
    int callDepth() const;

    /**
     * @brief changes currentFrame and emits signals
     * @param level 0 is to top/global
     * @return true on success
     */
    bool setStackLevel(int level);

    /**
     * @brief gets the global singleton instance
     * @return
     */
    static Debugger *instance();

public Q_SLOTS:
    bool start();
    bool stop();
    void tryStop();
    void haltOnNext();
    void stepOver();
    void stepInto();
    void stepOut();
    void stepContinue();
    void sendClearException(const QString &fn, int line);
    void sendClearAllExceptions();

private Q_SLOTS:
    void onFileOpened(const QString &fn);
    void onFileClosed(const QString &fn);
    void onAppQuit();

Q_SIGNALS:
    void _signalNextStep(); // used internally
    void started();
    void stopped();
    void nextInstruction();
    void functionCalled();
    void functionExited();
    void haltAt(const QString &filename, int line);
    void releaseAt(const QString &filename, int line);
    void exceptionOccured(std::shared_ptr<Base::PyExceptionInfo> exeption);
    void exceptionFatal(std::shared_ptr<Base::PyExceptionInfo> exception);
    void clearException(const QString &fn, int line);
    void clearAllExceptions();

private:
    static int tracer_callback(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg);
    static bool evalCondition(const char *condition, PyFrameObject *frame);
    static void finalizeFunction(); // called when interpreter finalizes
    bool frameRelatedToOpenedFiles(const PyFrameObject *frame) const;

    struct DebuggerP* d;
    static std::shared_ptr<Debugger> globalInstance;
};

} // namespace Python
} // namespace Debugging
} // namespace Gui

#endif // GUI_PYTHONDEBUG_H
