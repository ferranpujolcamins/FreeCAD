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

#include <CXX/Extensions.hxx>
#include <frameobject.h>
#include <string>
#include <vector>

namespace Base {
class PyException;
class PyExceptionInfo;
}

namespace App {

class BreakpointFile;

/**
 * @brief The BreakpointLine class is the actual breakpoint
 */
class AppExport BreakpointLine {
public:
    //BreakPointLine(); // we should not create without lineNr
    BreakpointLine(int lineNr, const BreakpointFile *parent);
    BreakpointLine(const BreakpointLine &other);
    ~BreakpointLine();

    BreakpointLine& operator=(const BreakpointLine &other);
    bool operator==(int lineNr) const;
    bool operator!=(int lineNr) const;
    inline bool operator<(const BreakpointLine &other) const;
    bool operator<(const int &line) const;

    int lineNr() const; 
    void setLineNr(int lineNr);

    inline bool hit();
    void reset();
    int hitCount() const { return m_hitCount; }

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
    /**
     * @brief setIgnoreTo ignores hits on this line up to ignore hits
     * @param ignore
     */
    void setIgnoreTo(int ignore);
    int ignoreTo() const;
    /**
     * @brief setIgnoreFrom ignores hits on this line from ignore hits
     * @param ignore
     */
    void setIgnoreFrom(int ignore);
    int ignoreFrom() const;

    void setDisabled(bool disable);
    bool disabled() const;

    int uniqueNr() const;

    const BreakpointFile *parent() const;

private:
    QString m_condition;
    const BreakpointFile *m_parent;
    int m_lineNr;
    int m_hitCount;
    int m_ignoreTo;
    int m_ignoreFrom;
    int m_uniqueNumber; // a global unique number
    bool m_disabled;
};

/**
 * @brief The BreakpointFile class
 *  contains all breakpoints for a given source file
 */
class AppExport BreakpointFile
{
public:
    BreakpointFile();
    BreakpointFile(const BreakpointFile&);
    BreakpointFile& operator=(const BreakpointFile&);

    ~BreakpointFile();

    const QString& fileName() const;
    void setFilename(const QString& fn);

    bool operator ==(const BreakpointFile& bp);
    bool operator ==(const QString& fn);

    // make for : range work on this class
    std::vector<BreakpointLine>::iterator begin() { return _lines.begin(); }
    std::vector<BreakpointLine>::iterator end()   { return _lines.end(); }

    void addLine(int line);
    void addLine(BreakpointLine bpl);
    void removeLine(int line);
    bool containsLine(int line);
    void setDisable(int line, bool disable);
    bool disabled(int line);

    int size() const;
    void clear();
    /**
     * @brief moveLines moves all breakpoints
     * @param startLine, the line to start from, all lines above this is moved
     * @param moveSteps, positive = move forward, negative = backwards
     * @return moved lines count
     */
    int moveLines(int startLine, int moveSteps);

    bool checkBreakpoint(const QString& fn, int line);
    BreakpointLine *getBreakpointLine(int line);
    BreakpointLine *getBreakpointLineFromIdx(int idx);

private:
    QString _filename;
    std::vector<BreakpointLine> _lines;
};

inline const QString& BreakpointFile::fileName()const
{
    return _filename;
}

inline int BreakpointFile::size() const
{
    return static_cast<int>(_lines.size());
}

inline bool BreakpointFile::checkBreakpoint(const QString& fn, int line)
{
    assert(!_filename.isEmpty());
    for (BreakpointLine &bp : _lines) {
        if (bp == line)
            return fn == _filename;
    }

    return false;
}

inline bool BreakpointFile::operator ==(const BreakpointFile& bp)
{
    return _filename == bp._filename;
}

inline bool BreakpointFile::operator ==(const QString& fn)
{
    return _filename == fn;
}

class PythonDebugModuleP;
/**
 * @author Werner Mayer
 */
class AppExport PythonDebugModule : public Py::ExtensionModule<PythonDebugModule>
{
public:
    static void init_module(void);

    PythonDebugModule();
    virtual ~PythonDebugModule();

private:
    Py::Object getFunctionCallCount(const Py::Tuple &a);
    Py::Object getExceptionCount(const Py::Tuple &a);
    Py::Object getLineCount(const Py::Tuple &a);
    Py::Object getFunctionReturnCount(const Py::Tuple &a);
    PythonDebugModuleP *d;
};

/**
 * @author Werner Mayer
 */
class AppExport PythonDebugStdout : public Py::PythonExtension<PythonDebugStdout>
{
public:
    static void init_type(void);    // announce properties and methods

    PythonDebugStdout();
    ~PythonDebugStdout();

    Py::Object repr();
    Py::Object write(const Py::Tuple&);
    Py::Object flush(const Py::Tuple&);
};

/**
 * @author Werner Mayer
 */
class AppExport PythonDebugStderr : public Py::PythonExtension<PythonDebugStderr>
{
public:
    static void init_type(void);    // announce properties and methods

    PythonDebugStderr();
    ~PythonDebugStderr();

    Py::Object repr();
    Py::Object write(const Py::Tuple&);
};

/**
 * @author Werner Mayer
 */
class AppExport PythonDebugExcept : public Py::PythonExtension<PythonDebugExcept>
{
public:
    static void init_type(void);    // announce properties and methods

    PythonDebugExcept();
    ~PythonDebugExcept();

    Py::Object repr();
    Py::Object excepthook(const Py::Tuple&);
};

class AppExport PythonDebugger : public QObject
{
    Q_OBJECT

public:
    PythonDebugger();
    ~PythonDebugger();


    bool hasBreakpoint(const QString &fn) const;
    /**
     * @brief the total number of breakpoints
     */
    int breakpointCount() const;
    /**
     * @brief gets the n-th breakpoint
     * @param idx = the n-th breakpoint
     * @return ptr to BreakpintLine or nullptr
     */
    BreakpointLine *getBreakpointLineFromIdx(int idx) const;
    BreakpointFile *getBreakpointFileFromIdx(int idx) const;
    BreakpointFile *getBreakpointFile(const QString&) const;
    BreakpointFile *getBreakpointFile(const BreakpointLine &bpl) const;
    BreakpointLine *getBreakpointLine(const QString fn, int line) const;
    BreakpointLine *getBreakpointFromUniqueNr(int uniqueNr) const;

    /**
     * @brief Returns which number in store bpl has
     * @param bpl = BreakpointLine instance
     * @return the n-th number
     */
    int getIdxFromBreakpointLine(const BreakpointLine &bpl) const;

    /**
     * @brief setBreakpointFile used to notify that we are tracing this file
     * @param fn = fileName
     */
    void setBreakpointFile(const QString &fn);
    void setBreakpoint(const QString fn, int line);
    void setBreakpoint(const QString fn, BreakpointLine bpl);
    /**
     * @brief deleteBreakpointFile stop tracing this file, used when editor closes file
     * @param fn = fileName
     */
    void deleteBreakpointFile(const QString &fn);
    void deleteBreakpoint(const QString fn, int line);
    void deleteBreakpoint(BreakpointLine *bpl);

    void setDisableBreakpoint(const QString fn, int line, bool disable);
    bool toggleBreakpoint(int line, const QString&);
    void clearAllBreakPoints();


    void runFile(const QString& fn);
    bool isRunning() const;
    bool isHalted() const;
    bool isHaltOnNext() const;
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
    static PythonDebugger *instance();

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
    void nextInstruction(PyFrameObject *frame);
    void functionCalled(PyFrameObject *frame);
    void functionExited(PyFrameObject *frame);
    void haltAt(const QString &filename, int line);
    void releaseAt(const QString &filename, int line);
    void breakpointAdded(const App::BreakpointLine *bpl);
    void breakpointChanged(const App::BreakpointLine *bpl);
    void breakpointRemoved(int idx, const App::BreakpointLine *bpl);
    void exceptionOccured(Base::PyExceptionInfo *exeption);
    void exceptionFatal(Base::PyExceptionInfo *exception);
    void clearException(const QString &fn, int line);
    void clearAllExceptions();

private:
    static int tracer_callback(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg);
    static bool evalCondition(const char *condition, PyFrameObject *frame);
    static void finalizeFunction(); // called when interpreter finalizes
    bool frameRelatedToOpenedFiles(const PyFrameObject *frame) const;

    struct PythonDebuggerP* d;
    static PythonDebugger *globalInstance;


    // to allow breakpoints emitting signals as if they emanated from this class
    friend void BreakpointFile::addLine(BreakpointLine bpl);
    friend void BreakpointFile::addLine(int line);
    friend void BreakpointFile::removeLine(int line);
    friend class BreakpointLine;
};

} // namespace Gui

#endif // GUI_PYTHONDEBUG_H
