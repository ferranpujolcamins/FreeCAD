/***************************************************************************
 *   Copyright (c) 2010 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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
# include <QEventLoop>
# include <QCoreApplication>
# include <QFileInfo>
# include <QTimer>
#endif

#include "PythonDebugger.h"
#include "BitmapFactory.h"
#include "EditorView.h"
#include <Base/Interpreter.h>
#include <Base/Console.h>


using namespace Gui;


Py::ExceptionInfo::SwapIn::SwapIn(PyThreadState *newState) :
    m_oldState(nullptr)
{
    // ensure we don't re-swap and acquire lock (leads to deadlock)
    if (!static_GILHeld) {
        static_GILHeld = true;
        // acquire for global thread
        m_GILState = PyGILState_Ensure();
        m_oldState = PyThreadState_Get();

        // swap to and re-aquire lock for jedi thread
        PyGILState_Release(m_GILState);
        Base::PyGILStateRelease release;
        PyThreadState_Swap(newState);
        m_GILState = PyGILState_Ensure();
    }
}
Py::ExceptionInfo::SwapIn::~SwapIn()
{
    // ensure we only swap back if this instance was the swapping one
    if (m_oldState) {
        // release from jedi thread and swap back to old thread
        PyGILState_Release(m_GILState);
        PyThreadState_Swap(m_oldState);

        if (static_GILHeld)
            static_GILHeld = false;
    }
}
bool Py::ExceptionInfo::SwapIn::static_GILHeld = false;

Py::ExceptionInfo::ExceptionInfo() :
    m_pyType(nullptr),
    m_pyValue(nullptr),
    m_pyTraceback(nullptr),
    m_pyState(PyThreadState_GET()),
    m_tracebackLevel(0)
{
    PyErr_Fetch(&m_pyType, &m_pyValue,&m_pyTraceback);
    if (!m_pyType)
        return;

    PyErr_NormalizeException(&m_pyType, &m_pyValue, &m_pyTraceback);

    if (PyErr_GivenExceptionMatches(m_pyType, PyExc_KeyboardInterrupt) ||
        PyErr_GivenExceptionMatches(m_pyType, PyExc_SystemExit))
    {
        m_pyType = nullptr; // invalidate
    }

    Py_XINCREF(m_pyType);
    Py_XINCREF(m_pyTraceback);
    Py_XINCREF(m_pyValue);

    //need to restore error unset by PyErr_Fetch
    PyErr_Restore(m_pyType, m_pyValue, m_pyTraceback);

    m_tracebackLevel = tracebackSize() -1;
}

Py::ExceptionInfo::ExceptionInfo(PyObject *tracebackArg) :
    m_pyType(nullptr),
    m_pyValue(nullptr),
    m_pyTraceback(nullptr),
    m_pyState(PyThreadState_GET()),
    m_tracebackLevel(0)
{
    if (!PyTuple_Check(tracebackArg))
        return;

    SwapIn myState(m_pyState);

    m_pyType = PyTuple_GET_ITEM(tracebackArg, 0);
    m_pyValue = PyTuple_GET_ITEM(tracebackArg, 1);
    m_pyTraceback = PyTuple_GET_ITEM(tracebackArg, 2);

    if (PyErr_GivenExceptionMatches(m_pyType, PyExc_KeyboardInterrupt) ||
        PyErr_GivenExceptionMatches(m_pyType, PyExc_SystemExit))
    {
        m_pyType = nullptr; // invalidate
    }

    Py_XINCREF(m_pyType);
    Py_XINCREF(m_pyTraceback);
    Py_XINCREF(m_pyValue);

    m_tracebackLevel = tracebackSize() -1;
}

Py::ExceptionInfo::ExceptionInfo(const Py::ExceptionInfo &other)
{

    SwapIn myState(m_pyState);

    m_tracebackLevel = other.m_tracebackLevel;
    m_pyType = other.m_pyType;
    m_pyTraceback = other.m_pyTraceback;
    m_pyValue = other.m_pyValue;
    m_pyState =  other.m_pyState;
    Py_XINCREF(m_pyType);
    Py_XINCREF(m_pyTraceback);
    Py_XINCREF(m_pyValue);
}

Py::ExceptionInfo::~ExceptionInfo()
{
    // we might get deleted after file has ended
    // and some other python code is running
    SwapIn myState(m_pyState);

    Py_XDECREF(m_pyType);
    Py_XDECREF(m_pyValue);
    Py_XDECREF(m_pyTraceback);
}

int Py::ExceptionInfo::lineNr() const
{
    if (!isValid())
        return -1;

    SwapIn myState(m_pyState);

    if (m_pyTraceback) { // no traceback when in global space
        PyTracebackObject* tb = getTracebackFrame();
        return PyCode_Addr2Line(tb->tb_frame->f_code, tb->tb_frame->f_lasti);
    }

    PyObject *vlu = getAttr("lineno"); // new ref
    if (!vlu)
        return -1;

    long lVlu = PyInt_AS_LONG(vlu);
    Py_XDECREF(vlu);
    return static_cast<int>(lVlu);
}

int Py::ExceptionInfo::offset() const
{
    if (!isValid() || m_tracebackLevel > 0)
        return -1;

    if (!isSyntaxError() && !isIndentationError())
        return -1;

    SwapIn myState(m_pyState);

    PyObject *vlu = getAttr("offset"); // new ref
    if (!vlu)
        return -1;

    long lVlu = PyInt_AS_LONG(vlu);
    Py_XDECREF(vlu);
    return static_cast<int>(lVlu);
}

QString Py::ExceptionInfo::message() const
{
    if (!isValid())
        return QString();

    SwapIn myState(m_pyState);

    PyObject *vlu = getAttr("msg"); // new ref
    if (!vlu)
        vlu = getItem("msg");
    if (!vlu)
        vlu = getAttr("message");
        if (!vlu)
            vlu = getItem("message");
    if (!vlu)
        vlu = getAttr("what");
        if (!vlu)
            vlu = getItem("what");
    if (!vlu)
        vlu = getAttr("reason");
        if (!vlu)
            vlu = getItem("reason");
    if (!vlu && PyErr_GivenExceptionMatches(m_pyType, Base::BaseExceptionFreeCADError))
    {
        vlu = getItem("swhat");
        if (!vlu)
            vlu = getItem("sErrMsg");
    }

    if (!vlu) { // error just as a string
        if (PyBytes_CheckExact(m_pyValue) && PyBytes_Size(m_pyValue) > 0) {
            vlu = m_pyValue;
            Py_INCREF(vlu);
        } else if (PyBytes_CheckExact(m_pyType) && PyBytes_Size(m_pyType) > 0) {
            vlu = m_pyType;
            Py_INCREF(vlu);
        }
    }
    if (!vlu)
        return QString(); // unknown type

    const char *msg = PyBytes_AS_STRING(vlu);
    Py_XDECREF(vlu);
    return QLatin1String(msg);
}

QString Py::ExceptionInfo::fileName() const
{
    if (!isValid())
        return QString();

    SwapIn myState(m_pyState);

    if (m_pyTraceback) {
        PyTracebackObject* tb = getTracebackFrame();
        const char *filename = PyBytes_AsString(tb->tb_frame->f_code->co_filename);
        return QLatin1String(filename);
    }

    PyObject *vlu = getAttr("filename"); // new ref
    if (!vlu)
        return QString();

    const char *msg = PyBytes_AS_STRING(vlu);
    Py_XDECREF(vlu);
    return QLatin1String(msg);
}

QString Py::ExceptionInfo::functionName() const
{
    if (!isValid() || !m_pyTraceback)
        return QString();

    PyTracebackObject* tb = getTracebackFrame();
    const char *funcname = PyBytes_AsString(tb->tb_frame->f_code->co_name);
    return QLatin1String(funcname);
}

QString Py::ExceptionInfo::text() const
{
    if (!isValid())
        return QString();

    SwapIn myState(m_pyState);

    PyObject *vlu = getAttr("text"); // new ref
    if (!vlu)
        return QString();

    const char *txt = PyBytes_AS_STRING(vlu);
    Py_XDECREF(vlu);
    return QLatin1String(txt);
}

QString Py::ExceptionInfo::typeString() const
{
    if (!isValid())
        return QString();


    SwapIn myState(m_pyState);

    PyObject *vlu = PyObject_Str(m_pyType); // new ref
    if (!vlu)
        return QString();

    Py_INCREF(vlu);
    const char *txt = PyBytes_AS_STRING(vlu);
    Py_XDECREF(vlu);
    QRegExp re(QLatin1String("<.*\\.(\\w+).*>")); // extract typename from repr string
    if (re.indexIn(QLatin1String(txt)) > -1)
        return re.cap(1);

    return QString();
}

const PyObject *Py::ExceptionInfo::type() const
{
    if (!isValid())
        return nullptr;
    return m_pyType;
}

const PyThreadState *Py::ExceptionInfo::threadState() const
{
    return m_pyState;
}

bool Py::ExceptionInfo::isValid() const
{
    if (m_pyType == nullptr)
        return false;

    PyThreadState *currentState = PyThreadState_GET();
    if (currentState && m_pyState) {
        if (currentState->interp == m_pyState->interp)
            return true;
        return false; // another interpreter
    }

    // no currentState, rely on ThreadState swapping
    return true;
}

int Py::ExceptionInfo::tracebackSize() const
{
    if (!isValid())
        return -1;

    int count = 0;
    PyTracebackObject* tb = (PyTracebackObject*)m_pyTraceback;
    while (tb != nullptr) {
        tb = tb->tb_next;
        ++count;
    }
    return count;
}

void Py::ExceptionInfo::setTracebackLevel(int level)
{
    if (!isValid())
        return;

    if (level < 0 && level >= tracebackSize())
        return;

    m_tracebackLevel = level;
}

bool Py::ExceptionInfo::isWarning() const
{
    if (!isValid())
        return false;

    SwapIn myState(m_pyState);
    return PyErr_GivenExceptionMatches(m_pyType, PyExc_Warning);
}

bool Py::ExceptionInfo::isSyntaxError() const
{
    if (!isValid())
        return false;

    SwapIn myState(m_pyState);
    return PyErr_GivenExceptionMatches(m_pyType, PyExc_SyntaxError);
}

bool Py::ExceptionInfo::isIndentationError() const
{
    if (!isValid())
        return false;

    SwapIn myState(m_pyState);
    return PyErr_GivenExceptionMatches(m_pyType, PyExc_IndentationError) ||
           PyErr_GivenExceptionMatches(m_pyType, PyExc_TabError);
}

const char *Py::ExceptionInfo::iconName() const
{
    const char *iconStr = "exception";

    if (isIndentationError())
        iconStr = "indentation-error";
    else if (isSyntaxError())
        iconStr = "syntax-error";
    else if (isWarning())
        iconStr = "warning";
    return iconStr;
}

PyTracebackObject *Py::ExceptionInfo::getTracebackFrame() const
{
    SwapIn myState(m_pyState);

    int lvl = m_tracebackLevel;
    PyTracebackObject* tb = (PyTracebackObject*)m_pyTraceback;
    while (lvl > 0) {
        tb = tb->tb_next;
        --lvl;
    }
    return tb;
}

PyObject *Py::ExceptionInfo::getAttr(const char *attr) const
{
    SwapIn myState(m_pyState);

    PyObject *vlu = nullptr;
    if (PyObject_HasAttrString(m_pyValue, attr))
        vlu = PyObject_GetAttrString(m_pyValue, attr); // new ref
    if (!vlu)
        return nullptr;
    return vlu;
}

PyObject *Py::ExceptionInfo::getItem(const char *attr) const
{

    SwapIn myState(m_pyState);

    PyObject *vlu = nullptr;
    if (PyDict_Check(m_pyValue)) {
        vlu = PyDict_GetItemString(m_pyValue, attr); // borrowed ref

    } else if (PyList_Check(m_pyValue)) {
        int idx = atoi(attr);
        if (PyList_Size(m_pyValue) >= idx)
            vlu = PyList_GET_ITEM(m_pyValue, idx); // borrowed ref
    }

    if (!vlu)
        return nullptr;
    Py_XINCREF(vlu); // we decref in caller functions so we need to inref here
    return vlu;
}


// -----------------------------------------------------------------------------


BreakpointLine::BreakpointLine(int lineNr, const BreakpointFile *parent):
    m_parent(parent), m_lineNr(lineNr), m_hitCount(0),
    m_ignoreTo(0), m_ignoreFrom(0), m_disabled(false)
{
    static int globalNumber = 0;
    m_uniqueNumber = ++globalNumber;
}

BreakpointLine::BreakpointLine(const BreakpointLine &other)
{
    *this = other;
}

BreakpointLine::~BreakpointLine()
{
}

BreakpointLine& BreakpointLine::operator=(const BreakpointLine& other)
{
    m_condition = other.m_condition;
    m_lineNr = other.m_lineNr;
    m_hitCount = 0; // don't copy hit counts
    m_ignoreTo = other.m_ignoreTo;
    m_ignoreFrom = other.m_ignoreFrom;
    m_disabled = other.m_disabled;
    m_uniqueNumber = other.m_uniqueNumber;
    m_parent = other.m_parent;
    return *this;
}

//inline
bool BreakpointLine::operator ==(int lineNr) const
{
    return m_lineNr == lineNr;
}

//inline
bool BreakpointLine::operator !=(int lineNr) const
{
    return m_lineNr != lineNr;
}

bool BreakpointLine::operator<(const BreakpointLine &other) const
{
    return m_lineNr < other.m_lineNr;
}

bool BreakpointLine::operator<(const int &line) const
{
    return m_lineNr < line;
}

int BreakpointLine::lineNr() const
{
    return m_lineNr;
}

void BreakpointLine::setLineNr(int lineNr)
{
    m_lineNr = lineNr;
    PythonDebugger::instance()->breakpointChanged(this);
}

// inline
bool BreakpointLine::hit()
{
    ++m_hitCount;
    if (m_disabled)
        return false;
    if (m_ignoreFrom > 0 && m_ignoreFrom < m_hitCount)
        return true;
    if (m_ignoreTo < m_hitCount)
        return true;

    return false;
}

void BreakpointLine::reset()
{
    m_hitCount = 0;
}

void BreakpointLine::setCondition(const QString condition)
{
    m_condition = condition;
    PythonDebugger::instance()->breakpointChanged(this);
}

const QString BreakpointLine::condition() const
{
    return m_condition;
}

void BreakpointLine::setIgnoreTo(int ignore)
{
    m_ignoreTo = ignore;
    PythonDebugger::instance()->breakpointChanged(this);
}

int BreakpointLine::ignoreTo() const
{
    return m_ignoreTo;
}

void BreakpointLine::setIgnoreFrom(int ignore)
{
    m_ignoreFrom = ignore;
    PythonDebugger::instance()->breakpointChanged(this);
}

int BreakpointLine::ignoreFrom() const
{
    return m_ignoreFrom;
}

void BreakpointLine::setDisabled(bool disable)
{
    m_disabled = disable;
    PythonDebugger::instance()->breakpointChanged(this);
}

bool BreakpointLine::disabled() const
{
    return m_disabled;
}

int BreakpointLine::uniqueNr() const
{
    return m_uniqueNumber;
}

const BreakpointFile *BreakpointLine::parent() const
{
    return m_parent;
}

// -------------------------------------------------------------------------------------

BreakpointFile::BreakpointFile()
{
}

BreakpointFile::BreakpointFile(const BreakpointFile& rBp)
{
    setFilename(rBp.fileName());
    for (BreakpointLine bp : rBp._lines) {
        _lines.push_back(bp);
    }
}

BreakpointFile& BreakpointFile::operator= (const BreakpointFile& rBp)
{
    if (this == &rBp)
        return *this;
    setFilename(rBp.fileName());
    _lines.clear();
    for (const BreakpointLine &bp : rBp._lines) {
        _lines.push_back(bp);
    }
    return *this;
}

BreakpointFile::~BreakpointFile()
{

}

void BreakpointFile::setFilename(const QString& fn)
{
    _filename = fn;
}

void BreakpointFile::addLine(int line)
{
    for (BreakpointLine &bpl : _lines)
        if (bpl.lineNr() == line)
            return;

    BreakpointLine bLine(line, this);
    _lines.push_back(bLine);
    PythonDebugger::instance()->breakpointAdded(&_lines.back());
}

void BreakpointFile::addLine(BreakpointLine bpl)
{
    for (BreakpointLine &_bpl : _lines)
        if (_bpl.lineNr() == bpl.lineNr())
            return;

    _lines.push_back(bpl);
    PythonDebugger::instance()->breakpointAdded(&_lines.back());
}

void BreakpointFile::removeLine(int line)
{
    for (std::vector<BreakpointLine>::iterator it = _lines.begin();
         it != _lines.end(); ++it)
    {
        if (line == it->lineNr()) {
            _lines.erase(it);
            BreakpointLine *bpl = &(*it);
            int idx = PythonDebugger::instance()->getIdxFromBreakpointLine(*bpl);
            PythonDebugger::instance()->breakpointRemoved(idx, bpl);
            return;
        }
    }
}

bool BreakpointFile::containsLine(int line)
{
    for (BreakpointLine &bp : _lines) {
        if (bp.lineNr() == line)
            return true;
    }

    return false;
}

void BreakpointFile::setDisable(int line, bool disable)
{
    for (BreakpointLine &bp : _lines) {
        if (bp.lineNr() == line)
            bp.setDisabled(disable);
    }
}

bool BreakpointFile::disabled(int line)
{
    for (BreakpointLine &bp : _lines) {
        if (bp.lineNr() == line)
            return bp.disabled();
    }
    return false;
}

void BreakpointFile::clear()
{
    _lines.clear();
}

int BreakpointFile::moveLines(int startLine, int moveSteps)
{
    int count = 0;
    for (BreakpointLine &bp : _lines) {
        if (bp.lineNr() >= startLine) {
            bp.setLineNr(bp.lineNr() + moveSteps);
            ++count;
        }
    }
    return count;
}

BreakpointLine *BreakpointFile::getBreakpointLine(int line)
{
    for (BreakpointLine &bp : _lines) {
        if (bp.lineNr() == line)
            return &bp;
    }
    return nullptr;
}

BreakpointLine *BreakpointFile::getBreakpointLineFromIdx(int idx)
{
    if (idx >= static_cast<int>(_lines.size()))
        return nullptr;
    return &_lines[idx];
}

// -----------------------------------------------------

void PythonDebugModule::init_module(void)
{
    PythonDebugStdout::init_type();
    PythonDebugStderr::init_type();
    PythonDebugExcept::init_type();
    static PythonDebugModule* mod = new PythonDebugModule();
    Q_UNUSED(mod);
}

PythonDebugModule::PythonDebugModule()
  : Py::ExtensionModule<PythonDebugModule>("FreeCADDbg")
{
    add_varargs_method("getFunctionCallCount", &PythonDebugModule::getFunctionCallCount,
        "Get the total number of function calls executed and the number executed since the last call to this function.");
    add_varargs_method("getExceptionCount", &PythonDebugModule::getExceptionCount,
        "Get the total number of exceptions and the number executed since the last call to this function.");
    add_varargs_method("getLineCount", &PythonDebugModule::getLineCount,
        "Get the total number of lines executed and the number executed since the last call to this function.");
    add_varargs_method("getFunctionReturnCount", &PythonDebugModule::getFunctionReturnCount,
        "Get the total number of function returns executed and the number executed since the last call to this function.");

    initialize( "The FreeCAD Python debug module" );

    Py::Dict d(moduleDictionary());
    Py::Object out(Py::asObject(new PythonDebugStdout()));
    d["StdOut"] = out;
    Py::Object err(Py::asObject(new PythonDebugStderr()));
    d["StdErr"] = err;
}

PythonDebugModule::~PythonDebugModule()
{
}

Py::Object PythonDebugModule::getFunctionCallCount(const Py::Tuple &)
{
    return Py::None();
}

Py::Object PythonDebugModule::getExceptionCount(const Py::Tuple &)
{
    return Py::None();
}

Py::Object PythonDebugModule::getLineCount(const Py::Tuple &)
{
    return Py::None();
}

Py::Object PythonDebugModule::getFunctionReturnCount(const Py::Tuple &)
{
    return Py::None();
}

// -----------------------------------------------------

void PythonDebugStdout::init_type()
{
    behaviors().name("PythonDebugStdout");
    behaviors().doc("Redirection of stdout to FreeCAD's Python debugger window");
    // you must have overwritten the virtual functions
    behaviors().supportRepr();
    add_varargs_method("write",&PythonDebugStdout::write,"write to stdout");
    add_varargs_method("flush",&PythonDebugStdout::flush,"flush the output");
}

PythonDebugStdout::PythonDebugStdout()
{
}

PythonDebugStdout::~PythonDebugStdout()
{
}

Py::Object PythonDebugStdout::repr()
{
    std::string s;
    std::ostringstream s_out;
    s_out << "PythonDebugStdout";
    return Py::String(s_out.str());
}

Py::Object PythonDebugStdout::write(const Py::Tuple& args)
{
    char *msg;
    //PyObject* pObj;
    ////args contains a single parameter which is the string to write.
    //if (!PyArg_ParseTuple(args.ptr(), "Os:OutputString", &pObj, &msg))
    if (!PyArg_ParseTuple(args.ptr(), "s:OutputString", &msg))
        throw Py::Exception();

    if (strlen(msg) > 0)
    {
        //send it to our stdout
        printf("%s\n",msg);

        //send it to the debugger as well
        //g_DebugSocket.SendMessage(eMSG_OUTPUT, msg);
    }
    return Py::None();
}

Py::Object PythonDebugStdout::flush(const Py::Tuple&)
{
    return Py::None();
}

// -----------------------------------------------------

void PythonDebugStderr::init_type()
{
    behaviors().name("PythonDebugStderr");
    behaviors().doc("Redirection of stderr to FreeCAD's Python debugger window");
    // you must have overwritten the virtual functions
    behaviors().supportRepr();
    add_varargs_method("write",&PythonDebugStderr::write,"write to stderr");
}

PythonDebugStderr::PythonDebugStderr()
{
}

PythonDebugStderr::~PythonDebugStderr()
{
}

Py::Object PythonDebugStderr::repr()
{
    std::string s;
    std::ostringstream s_out;
    s_out << "PythonDebugStderr";
    return Py::String(s_out.str());
}

Py::Object PythonDebugStderr::write(const Py::Tuple& args)
{
    char *msg;
    //PyObject* pObj;
    //args contains a single parameter which is the string to write.
    //if (!PyArg_ParseTuple(args.ptr(), "Os:OutputDebugString", &pObj, &msg))
    if (!PyArg_ParseTuple(args.ptr(), "s:OutputDebugString", &msg))
        throw Py::Exception();

    if (strlen(msg) > 0)
    {
        //send the message to our own stderr
        //dprintf(msg);

        //send it to the debugger as well
        //g_DebugSocket.SendMessage(eMSG_TRACE, msg);
        Base::Console().Error("%s", msg);
    }

    return Py::None();
}

// -----------------------------------------------------

void PythonDebugExcept::init_type()
{
    behaviors().name("PythonDebugExcept");
    behaviors().doc("Custom exception handler");
    // you must have overwritten the virtual functions
    behaviors().supportRepr();
    add_varargs_method("fc_excepthook",&PythonDebugExcept::excepthook,"Custom exception handler");
}

PythonDebugExcept::PythonDebugExcept()
{
}

PythonDebugExcept::~PythonDebugExcept()
{
}

Py::Object PythonDebugExcept::repr()
{
    std::string s;
    std::ostringstream s_out;
    s_out << "PythonDebugExcept";
    return Py::String(s_out.str());
}

Py::Object PythonDebugExcept::excepthook(const Py::Tuple& args)
{
    PyObject *exc, *value, *tb;
    if (!PyArg_UnpackTuple(args.ptr(), "excepthook", 3, 3, &exc, &value, &tb))
        throw Py::Exception();

    PyErr_NormalizeException(&exc, &value, &tb);

    PyErr_Display(exc, value, tb);
/*
    if (eEXCEPTMODE_IGNORE != g_eExceptionMode)
    {
        assert(tb);

        if (tb && (tb != Py_None))
        {
            //get the pointer to the frame held by the bottom traceback object - this
            //should be where the exception occurred.
            tracebackobject* pTb = (tracebackobject*)tb;
            while (pTb->tb_next != NULL) 
            {
                pTb = pTb->tb_next;
            }
            PyFrameObject* frame = (PyFrameObject*)PyObject_GetAttr((PyObject*)pTb, PyBytes_FromString("tb_frame"));
            EnterBreakState(frame, (PyObject*)pTb);
        }
    }*/

    return Py::None();
}

// -----------------------------------------------------

namespace Gui {
class PythonDebuggerPy : public Py::PythonExtension<PythonDebuggerPy> 
{
public:
    PythonDebuggerPy(PythonDebugger* d) :
        dbg(d), runtimeException(nullptr)
    { }
    ~PythonDebuggerPy() {}
    PythonDebugger* dbg;
    Py::ExceptionInfo *runtimeException;
};

// -----------------------------------------------------

class RunningState
{
public:
    enum States {
        Stopped,
        Running,
        SingleStep,
        StepOver,
        StepOut,
        HaltOnNext
    };

    RunningState(): state(Stopped) {  }
    ~RunningState() {  }
    States operator= (States s) { state = s; return state; }
    bool operator!= (States s) { return state != s; }
    bool operator== (States s) { return state == s; }

private:
    States state;
};

// -----------------------------------------------------

struct PythonDebuggerP {
    typedef void(PythonDebuggerP::*voidFunction)(void);
    PyObject* out_o;
    PyObject* err_o;
    PyObject* exc_o;
    PyObject* out_n;
    PyObject* err_n;
    PyObject* exc_n;
    PyFrameObject* currentFrame;
    PythonDebugExcept* pypde;
    RunningState state;
    bool init, trystop, halted;
    int maxHaltLevel;
    int showStackLevel;
    QEventLoop loop;
    PyObject* pydbg;
    std::vector<BreakpointFile*> bps;

    PythonDebuggerP(PythonDebugger* that) :
        init(false), trystop(false), halted(false),
        maxHaltLevel(-1), showStackLevel(-1)
    {
        out_o = 0;
        err_o = 0;
        exc_o = 0;
        currentFrame = 0;
        Base::PyGILStateLocker lock;
        out_n = new PythonDebugStdout();
        err_n = new PythonDebugStderr();
        pypde = new PythonDebugExcept();
        Py::Object func = pypde->getattr("fc_excepthook");
        exc_n = Py::new_reference_to(func);
        pydbg = new PythonDebuggerPy(that);
    }
    ~PythonDebuggerP()
    {
        Base::PyGILStateLocker lock;
        Py_DECREF(out_n);
        Py_DECREF(err_n);
        Py_DECREF(exc_n);
        Py_DECREF(pypde);
        Py_DECREF(pydbg);

        for (BreakpointFile *bpf : bps)
            delete bpf;
    }
};
}

// ---------------------------------------------------------------

PythonDebugger::PythonDebugger()
  : d(new PythonDebuggerP(this))
{
    globalInstance = this;

    connect(EditorViewSingleton::instance(), SIGNAL(fileOpened(QString)),
            this, SLOT(onFileOpened(QString)));
    connect(EditorViewSingleton::instance(), SIGNAL(fileClosed(QString)),
            this, SLOT(onFileClosed(QString)));
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(onAppQuit()));

    typedef void (*STATICFUNC)( );
    STATICFUNC fp = PythonDebugger::finalizeFunction;
    Py_AtExit(fp);
}

PythonDebugger::~PythonDebugger()
{
    stop();
    delete d;
    globalInstance = nullptr;
}

bool PythonDebugger::hasBreakpoint(const QString &fn) const
{
    for (BreakpointFile *bpf : d->bps) {
        if (fn == bpf->fileName())
            return true;
    }

    return false;
}

int PythonDebugger::breakpointCount() const
{
    int count = 0;
    for (BreakpointFile *bp : d->bps)
        count += bp->size();
    return count;
}

BreakpointLine *PythonDebugger::getBreakpointLineFromIdx(int idx) const
{
    int count = -1;
    for (BreakpointFile *bp : d->bps) {
        for (BreakpointLine &bpl : *bp) {
            ++count;
            if (count == idx)
                return &bpl;
        }
    }
    return nullptr;
}

BreakpointFile *PythonDebugger::getBreakpointFileFromIdx(int idx) const
{
    int count = -1;
    for (BreakpointFile *bp : d->bps) {
        for (int i = 0; i < bp->size(); ++i) {
            ++count;
            if (count == idx)
                return bp;
        }
    }
    return nullptr;
}

BreakpointFile *PythonDebugger::getBreakpointFile(const QString& fn) const
{
    for (BreakpointFile *bp : d->bps) {
        if (fn == bp->fileName())
            return bp;
    }

    return nullptr;
}

BreakpointFile *PythonDebugger::getBreakpointFile(const BreakpointLine &bpl) const
{
    for (BreakpointFile *bp : d->bps) {
        for (BreakpointLine &bpl_l : *bp)
            if (bpl_l.uniqueNr() == bpl.uniqueNr())
                return bp;
    }
    return nullptr;
}

BreakpointLine *PythonDebugger::getBreakpointLine(const QString fn, int line) const
{
    for (BreakpointFile *bp : d->bps) {
        if (fn == bp->fileName())
            return bp->getBreakpointLine(line);
    }
    return nullptr;
}

BreakpointLine *PythonDebugger::getBreakpointFromUniqueNr(int uniqueNr) const
{
    for (BreakpointFile *bpf : d->bps) {
        for (BreakpointLine &bpl : *bpf)
            if (bpl.uniqueNr() == uniqueNr)
                return &bpl;
    }
    return nullptr;
}

int PythonDebugger::getIdxFromBreakpointLine(const BreakpointLine &bpl) const
{
    int count = 0;
    for (BreakpointFile *bpf : d->bps) {
        for (BreakpointLine &bp : *bpf) {
            if (bp.uniqueNr() == bpl.uniqueNr())
                return count;
            ++count;
        }
    }
    return -1;
}

void PythonDebugger::setBreakpointFile(const QString &fn)
{
    if (hasBreakpoint(fn))
        return;

    BreakpointFile *bp = new BreakpointFile;
    bp->setFilename(fn);
    d->bps.push_back(bp);
}

void PythonDebugger::setBreakpoint(const QString fn, int line)
{
    // if set, remove old to replace
    for (BreakpointFile *bp : d->bps) {
        if (fn == bp->fileName()) {
            bp->removeLine(line);
            bp->addLine(line);
            return;
        }
    }

    BreakpointFile *bp = new BreakpointFile;
    bp->setFilename(fn);
    bp->addLine(line);
    d->bps.push_back(bp);
}

void PythonDebugger::setBreakpoint(const QString fn, BreakpointLine bpl)
{
    // if set, remove old to replace
    for (BreakpointFile *bp : d->bps) {
        if (fn == bp->fileName()) {
            bp->removeLine(bpl.lineNr());
            bp->addLine(bpl.lineNr());
            return;
        }
    }

    BreakpointFile *bp = new BreakpointFile;
    bp->setFilename(fn);
    bp->addLine(bpl);
    d->bps.push_back(bp);
}

void PythonDebugger::deleteBreakpointFile(const QString &fn)
{
    for (BreakpointFile *bpf : d->bps) {
        if (bpf->fileName() == fn)
            bpf->clear();
    }
}

void PythonDebugger::deleteBreakpoint(const QString fn, int line)
{
    for (BreakpointFile *bp : d->bps) {
        if (fn == bp->fileName()) {
            bp->removeLine(line);
        }
    }
}

void PythonDebugger::deleteBreakpoint(BreakpointLine *bpl)
{
    for (BreakpointFile *bpf : d->bps) {
        for (BreakpointLine &bp : *bpf)
            if (bp.uniqueNr() == bpl->uniqueNr())
                bpf->removeLine(bp.lineNr());
    }
}

void PythonDebugger::setDisableBreakpoint(const QString fn, int line, bool disable)
{
    for (BreakpointFile *bp : d->bps) {
        if (fn == bp->fileName()) {
            bp->setDisable(line, disable);
        }
    }
}



bool PythonDebugger::toggleBreakpoint(int line, const QString& fn)
{
    for (BreakpointFile *bp : d->bps) {
        if (fn == bp->fileName()) {
            if (bp->containsLine(line)) {
                bp->removeLine(line);
                return false;
            }
            bp->addLine(line);
            return true;
        }
    }

    setBreakpoint(fn, line);
    return true;
}

void PythonDebugger::clearAllBreakPoints()
{
    while (!d->bps.empty()) {
        BreakpointFile *bpf = d->bps.back();
        d->bps.pop_back();
        for (BreakpointLine &bpl : *bpf)
            bpf->removeLine(bpl.lineNr());
        delete bpf;
    }
}

void PythonDebugger::runFile(const QString& fn)
{
    try {
        if (d->state != RunningState::HaltOnNext)
            d->state = RunningState::Running;
        QByteArray pxFileName = fn.toUtf8();
#ifdef FC_OS_WIN32
        Base::FileInfo fi((const char*)pxFileName);
        FILE *fp = _wfopen(fi.toStdWString().c_str(),L"r");
#else
        FILE *fp = fopen((const char*)pxFileName,"r");
#endif
        if (!fp) {
            d->state = RunningState::Stopped;
            return;
        }

        Base::PyGILStateLocker locker;
        PyObject *module, *dict;
        module = PyImport_AddModule("__main__");
        dict = PyModule_GetDict(module);
        dict = PyDict_Copy(dict);
        if (PyDict_GetItemString(dict, "__file__") == NULL) {
#if PY_MAJOR_VERSION >= 3
            PyObject *f = PyUnicode_FromString((const char*)pxFileName);
#else
            PyObject *f = PyString_FromString((const char*)pxFileName);
#endif
            if (f == NULL) {
                fclose(fp);
                d->state = RunningState::Stopped;
                return;
            }
            if (PyDict_SetItemString(dict, "__file__", f) < 0) {
                Py_DECREF(f);
                fclose(fp);
                d->state = RunningState::Stopped;
                return;
            }
            Py_DECREF(f);
        }

        PyObject *result = PyRun_File(fp, (const char*)pxFileName, Py_file_input, dict, dict);
        fclose(fp);
        Py_DECREF(dict);

        if (!result) {
            // script failed, syntax error, import error, etc
            Py::ExceptionInfo exp;
            if (exp.isValid())
                Q_EMIT exceptionFatal(&exp);

            // user code exit() makes PyErr_Print segfault
            if (!PyErr_ExceptionMatches(PyExc_SystemExit))
                PyErr_Print();

            d->state = RunningState::Stopped;
         } else
            Py_DECREF(result);
    }
    catch (const Base::PyException&/* e*/) {
        //PySys_WriteStderr("Exception: %s\n", e.what());
        PyErr_Clear();
    }
    catch (...) {
        Base::Console().Warning("Unknown exception thrown during macro debugging\n");
    }

    if (d->trystop) {
        stop(); // de init tracer_function and reset object
    }
    d->state = RunningState::Stopped;
}

bool PythonDebugger::isRunning() const
{
    return d->state != RunningState::Stopped;
}

bool PythonDebugger::isHalted() const
{
    Base::PyGILStateLocker locker;
    return d->halted;
}

bool PythonDebugger::isHaltOnNext() const
{
    return d->state == RunningState::HaltOnNext;
}

bool PythonDebugger::start()
{
    if (d->state == RunningState::Stopped)
        d->state = RunningState::Running;

    if (d->init)
        return false;

    d->init = true;
    d->trystop = false;

    { // thread lock code block
        Base::PyGILStateLocker lock;
        d->out_o = PySys_GetObject("stdout");
        d->err_o = PySys_GetObject("stderr");
        d->exc_o = PySys_GetObject("excepthook");

        PySys_SetObject("stdout", d->out_n);
        PySys_SetObject("stderr", d->err_n);
        PySys_SetObject("excepthook", d->exc_o);

        PyEval_SetTrace(tracer_callback, d->pydbg);
    } // end threadlock codeblock

    Q_EMIT started();
    return true;
}

bool PythonDebugger::stop()
{
    if (!d->init)
        return false;
    if (d->halted) {
        d->trystop = true;
        _signalNextStep();
        return false;
    }

    { // threadlock code block
        Base::PyGILStateLocker lock;
        PyErr_Clear();
        PyEval_SetTrace(NULL, NULL);
        PySys_SetObject("stdout", d->out_o);
        PySys_SetObject("stderr", d->err_o);
        PySys_SetObject("excepthook", d->exc_o);
        d->init = false;
    } // end thread lock code block
    d->currentFrame = nullptr;
    d->state = RunningState::Stopped;
    d->halted = false;
    d->trystop = false;
    Q_EMIT stopped();
    return true;
}

void PythonDebugger::tryStop()
{
    d->trystop = true;
    _signalNextStep();
}

void PythonDebugger::haltOnNext()
{
    start();
    d->state = RunningState::HaltOnNext;
}

void PythonDebugger::stepOver()
{
    d->state = RunningState::StepOver;
    d->maxHaltLevel = callDepth();
    _signalNextStep();
}

void PythonDebugger::stepInto()
{
    d->state = RunningState::SingleStep;
    _signalNextStep();
}

void PythonDebugger::stepOut()
{
    d->state = RunningState::StepOut;
    d->maxHaltLevel = callDepth() -1;
    if (d->maxHaltLevel < 0)
        d->maxHaltLevel = 0;
    _signalNextStep();
}

void PythonDebugger::stepContinue()
{
    d->state = RunningState::Running;
    _signalNextStep();
}

void PythonDebugger::sendClearException(const QString &fn, int line)
{
    Q_EMIT clearException(fn, line);
}

void PythonDebugger::sendClearAllExceptions()
{
    Q_EMIT clearAllExceptions();
}

void PythonDebugger::onFileOpened(const QString &fn)
{
    QFileInfo fi(fn);
    if (fi.suffix().toLower() == QLatin1String("py") ||
        fi.suffix().toLower() == QLatin1String("fcmacro"))
    {
        setBreakpointFile(fn);
    }
}

void PythonDebugger::onFileClosed(const QString &fn)
{
    QFileInfo fi(fn);
    if (fi.suffix().toLower() == QLatin1String("py") ||
        fi.suffix().toLower() == QLatin1String("fcmacro"))
    {
        deleteBreakpointFile(fn);
    }
}

void PythonDebugger::onAppQuit()
{
    stop();
}

PyFrameObject *PythonDebugger::currentFrame() const
{
    Base::PyGILStateLocker locker;
    if (d->showStackLevel < 0)
        return d->currentFrame;

    // lets us show different stacks
    PyFrameObject *fr = d->currentFrame;
    int i = callDepth() - d->showStackLevel;
    while (nullptr != fr && i > 1) {
        fr = fr->f_back;
        --i;
    }

    return fr;

}

int PythonDebugger::callDepth(const PyFrameObject *frame) const
{
    PyFrameObject const *fr = frame;
    int i = 0;
    Base::PyGILStateLocker locker;
    while (nullptr != fr) {
        fr = fr->f_back;
        ++i;
    }

    return i;
}

int PythonDebugger::callDepth() const
{
    Base::PyGILStateLocker locker;
    PyFrameObject const *fr = d->currentFrame;
    return callDepth(fr);
}

bool PythonDebugger::setStackLevel(int level)
{
    if (!d->halted)
        return false;

    --level; // 0 based

    int calls = callDepth() - 1;
    if (calls >= level && level >= 0) {
        if (d->showStackLevel == level)
            return true;

        Base::PyGILStateLocker lock;

        if (calls == level)
            d->showStackLevel = -1;
        else
            d->showStackLevel = level;

        // notify observers
        PyFrameObject *frame = currentFrame();
        if (frame) {
            int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
            QString file = QString::fromUtf8(PyBytes_AsString(frame->f_code->co_filename));
            Q_EMIT haltAt(file, line);
            Q_EMIT nextInstruction(frame);
        }
    }

    return false;
}

// is owned by macro manager which handles delete
PythonDebugger* PythonDebugger::globalInstance = nullptr;

PythonDebugger *PythonDebugger::instance()
{
    if (globalInstance == nullptr)
        // is owned by macro manager which handles delete
        globalInstance = new PythonDebugger;
    return globalInstance;
}

// http://www.koders.com/cpp/fidBA6CD8A0FE5F41F1464D74733D9A711DA257D20B.aspx?s=PyEval_SetTrace
// http://code.google.com/p/idapython/source/browse/trunk/python.cpp
// http://www.koders.com/cpp/fid191F7B13CF73133935A7A2E18B7BF43ACC6D1784.aspx?s=PyEval_SetTrace
// http://stuff.mit.edu/afs/sipb/project/python/src/python2.2-2.2.2/Modules/_hotshot.c
// static
int PythonDebugger::tracer_callback(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg)
{
    PythonDebuggerPy* self = static_cast<PythonDebuggerPy*>(obj);
    PythonDebugger* dbg = self->dbg;
    if (dbg->d->trystop) {
        PyErr_SetInterrupt();
        dbg->d->state = RunningState::Stopped;
        return 0;
    }
    QCoreApplication::processEvents();
    //int no;

    //no = frame->f_tstate->recursion_depth;
    //std::string funcname = PyBytes_AsString(frame->f_code->co_name);
#if PY_MAJOR_VERSION >= 3
    QString file = QString::fromUtf8(PyUnicode_AsUTF8(frame->f_code->co_filename));
#else
    QString file = QString::fromUtf8(PyBytes_AsString(frame->f_code->co_filename));
#endif
    switch (what) {
    case PyTrace_CALL:
        if (dbg->d->state != RunningState::Running &&
            dbg->frameRelatedToOpenedFiles(frame))
        {
            try {
                Q_EMIT dbg->functionCalled(frame);
            } catch(...){
                PyErr_Clear();
            } // might throw
        }
        return 0;
    case PyTrace_RETURN:
        if (dbg->d->state != RunningState::Running &&
            dbg->frameRelatedToOpenedFiles(frame))
        {
            try {
                Q_EMIT dbg->functionExited(frame);
            } catch (...) {
                PyErr_Clear();
            }
        }

        // we need to check for exceptions and throw them here as
        // PyTrace_EXCEPTION throws on all exceptions including them that are handled in user code
        if (self->runtimeException &&
            self->runtimeException->threadState() == PyThreadState_GET())
        {
            // if arg is set, no exception occurred
            if (arg) {
                // error has been cleared
                delete self->runtimeException;
                self->runtimeException = nullptr;

            } else if (self->runtimeException->isValid()) {
                Q_EMIT dbg->exceptionOccured(self->runtimeException);

                // halt debugger so we can view stack?
                ParameterGrp::handle hPrefGrp = WindowParameter::getDefaultParameter()->GetGroup("Editor");;
                if (hPrefGrp->GetBool("EnableHaltDebuggerOnExceptions", true)) {
                    dbg->d->state = RunningState::HaltOnNext;
                    return PythonDebugger::tracer_callback(obj, frame, PyTrace_LINE, arg);
                }
            }
        }
        return 0;
    case PyTrace_LINE:
        {

            int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
            bool halt = false;
            if (dbg->d->state == RunningState::SingleStep ||
                dbg->d->state == RunningState::HaltOnNext)
            {
                halt = true;
            } else if((dbg->d->state == RunningState::StepOver ||
                       dbg->d->state == RunningState::StepOut) &&
                      dbg->d->maxHaltLevel >= dbg->callDepth(frame))
            {
                halt = true;
            } else { // RunningState
                BreakpointLine *bp = dbg->getBreakpointLine(file, line);
                if (bp != nullptr) {
                    if (bp->condition().size()) {
                        halt = PythonDebugger::evalCondition(bp->condition().toLatin1(),
                                                             frame);
                    } else if (bp->hit()) {
                        halt = true;
                    }
                }
            }

            if (halt) {
                // prevent halting on non opened files
                if (!dbg->frameRelatedToOpenedFiles(frame))
                    return 0;

                while(dbg->d->halted) {
                    // already halted, must be another thread here
                    // halt until current thread releases
                    QCoreApplication::processEvents();
                }

                QEventLoop loop;
                {   // threadlock block
                    Base::PyGILStateLocker locker;
                    dbg->d->currentFrame = frame;

                    if (!dbg->d->halted) {
                        try {
                            Q_EMIT dbg->functionCalled(frame);
                        } catch (...) { }
                    }
                    dbg->d->halted = true;
                    try {
                        Q_EMIT dbg->haltAt(file, line);
                        Q_EMIT dbg->nextInstruction(frame);
                    } catch(...) {
                        PyErr_Clear();
                    }
                }   // end threadlock block
                QObject::connect(dbg, SIGNAL(_signalNextStep()), &loop, SLOT(quit()));
                loop.exec();
                {   // threadlock block
                    Base::PyGILStateLocker locker;
                    dbg->d->halted = false;
                }   // end threadlock block
                try {
                    Q_EMIT dbg->releaseAt(file, line);
                } catch (...) {
                    PyErr_Clear();
                }
            }

            return 0;
        }
    case PyTrace_EXCEPTION:
        // we need to store this exception for later as it might be a recoverable exception
        if (dbg->frameRelatedToOpenedFiles(frame)) {
            Py::ExceptionInfo *exc = new Py::ExceptionInfo(arg);
            if (exc->isValid())
                self->runtimeException = exc;
            else
                delete exc;
        }
        return 0;
    case PyTrace_C_CALL:
        return 0;
    case PyTrace_C_EXCEPTION:
        return 0;
    case PyTrace_C_RETURN:
        return 0;
    default:
        /* ignore PyTrace_EXCEPTION */
        break;
    }
    return 0;
}

// credit to: https://github.com/jondy/pyddd/blob/master/ipa.c
// static
bool PythonDebugger::evalCondition(const char *condition, PyFrameObject *frame)
{
    /* Eval breakpoint condition */
    PyObject *result;

    /* Use empty filename to avoid obj added to object entry table */
    PyObject* exprobj = Py_CompileStringFlags(condition, "", Py_eval_input, NULL);
    if (!exprobj) {
        PyErr_Clear();
        return false;
    }

    /* Clear flag use_tracing in current PyThreadState to avoid
         tracing evaluation self
      */
    frame->f_tstate->use_tracing = 0;
    result = PyEval_EvalCode((PyCodeObject*)exprobj,
                             frame->f_globals,
                             frame->f_locals);
    frame->f_tstate->use_tracing = 1;
    Py_DecRef(exprobj);

    if (result == NULL) {
        PyErr_Clear();
        return false;
    }

    if (PyObject_IsTrue(result) != 1) {
        Py_DecRef(result);
        return false;
    }
    Py_DecRef(result);

    return true;
}

// static
void PythonDebugger::finalizeFunction()
{
    if (globalInstance != nullptr) {
        // user code with exit() finalizes interpreter so we start over
        if (!Py_IsInitialized()) {
            PythonDebugger::instance()->sendClearAllExceptions();
            Base::InterpreterSingleton::Instance().init(App::Application::GetARGC(),
                                         App::Application::GetARGV());
        }
        globalInstance->stop(); // release a pending halt on app close
    }
}

bool PythonDebugger::frameRelatedToOpenedFiles(const PyFrameObject *frame) const
{
    do {
        QString file = QString::fromUtf8(PyString_AsString(frame->f_code->co_filename));
        if (hasBreakpoint(file))
            return true;
        if (file == QLatin1String("<string>"))
            return false; // eval-ed code

        frame = frame->f_back;

    } while (frame != nullptr);

    return false;
}

#include "moc_PythonDebugger.cpp"

