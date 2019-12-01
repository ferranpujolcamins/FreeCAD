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

#include <QEventLoop>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>
#include <QRegExp>

#include "PythonDebugger.h"
#include "Application.h"
#include <Base/Parameter.h>
#include <Base/Interpreter.h>
#include <Base/Console.h>
#include <opcode.h> // python byte codes

#if PY_MAJOR_VERSION >= 3
# define PY_AS_C_STRING(pyObj) PyUnicode_AsUTF8(pyObj)
#else
# define PY_AS_C_STRING(pyObj) PyBytes_AsString(pyObj)
#endif

using namespace App;


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
    QRegExp re(QLatin1String("([^=><!])=([^=])"));
    m_condition =  QString(condition).replace(re, QLatin1String("\\1==\\2"));
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
namespace App {
class PythonDebugModuleP
{
public:
    PythonDebugModuleP() :
        stdout(new PythonDebugStdout()),
        stderr(new PythonDebugStderr())
    {}
    ~PythonDebugModuleP()
    {
        delete stdout;
        delete stderr;
    }
    PythonDebugStdout *stdout;
    PythonDebugStderr *stderr;
};
} // namespace App
// ----------------------------------------------------------------------------------

void PythonDebugModule::init_module(void)
{
    PythonDebugStdout::init_type();
    PythonDebugStderr::init_type();
    PythonDebugExcept::init_type();
    static PythonDebugModule* mod = new PythonDebugModule();
    Q_UNUSED(mod)
}

PythonDebugModule::PythonDebugModule()
  : Py::ExtensionModule<PythonDebugModule>("FreeCADDbg"),
    d(new PythonDebugModuleP())
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

    Py::Dict dict(moduleDictionary());
    Py::Object out(Py::asObject(d->stdout));
    dict["StdOut"] = out;
    Py::Object err(Py::asObject(d->stderr));
    dict["StdErr"] = err;
}

PythonDebugModule::~PythonDebugModule()
{
    delete d;
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
        fprintf(stdout, "%s\n", msg);

        //send it to the debugger as well
        //g_DebugSocket.SendMessage(eMSG_OUTPUT, msg);
        Base::Console().Message("%s", msg);
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
        fprintf(stderr, "%s\n", msg);

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

namespace App {
class PythonDebuggerPy : public Py::PythonExtension<PythonDebuggerPy> 
{
public:
    PythonDebuggerPy(PythonDebugger* d) :
        dbg(d), runtimeException(nullptr)
    { }
    ~PythonDebuggerPy() {}
    PythonDebugger* dbg;
    Base::PyExceptionInfo *runtimeException;
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
            PyObject *f = PyBytes_FromString((const char*)pxFileName);
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
            if (!d->trystop && d->state != RunningState::Stopped) {
                // script failed, syntax error, import error, etc
                Base::PyExceptionInfo exc;
                if (exc.isValid() && !exc.isSystemExit() && !exc.isKeyboardInterupt()) {
                    Q_EMIT exceptionFatal(&exc);

                    // user code exit() makes PyErr_Print segfault
                    Base::Console().Error("(debugger):%s\n%s\n%s",
                                            exc.getErrorType(true).c_str(),
                                            exc.getStackTrace().c_str(),
                                            exc.getMessage().c_str());
                    PyErr_Clear();
                }

            }
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
            const char *filename = PY_AS_C_STRING(frame->f_code->co_filename);
            QString file = QString::fromUtf8(filename);
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
    QString file = QString::fromUtf8(PY_AS_C_STRING(frame->f_code->co_filename));

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
        if (dbg->frameRelatedToOpenedFiles(frame)) {
            // is it within a try: block, might be in a parent frame
            QRegExp re(QLatin1String("importlib\\._bootstrap"));
            PyFrameObject *f = frame;

            while (f) {
                if (f->f_iblock > 0 && f->f_iblock <= CO_MAXBLOCKS) {
                    int b_type = f->f_blockstack[f->f_iblock -1].b_type; // blockstackindex is +1 based
                    if (b_type == SETUP_EXCEPT)
                        return 0; // should be caught by a try .. except block
                }
                const char *fn = PY_AS_C_STRING(f->f_code->co_filename);
                if (re.indexIn(QLatin1String(fn)) > -1)
                    return 0; // its C-code that have called this frame,
                              // can never be certain how C code handles exceptions
                f = f->f_back; // try with previous (calling) frame
            }

            Base::PyExceptionInfo *exc = new Base::PyExceptionInfo(arg);
            if (exc->isValid()) {
                Q_EMIT dbg->exceptionOccured(exc);

                // halt debugger so we can view stack?
                ParameterGrp::handle hPrefGrp = App::GetApplication().GetUserParameter().GetGroup("BaseApp")
                                                    ->GetGroup("Preferences")->GetGroup("Editor");
                if (hPrefGrp->GetBool("EnableHaltDebuggerOnExceptions", true)) {
                    dbg->d->state = RunningState::HaltOnNext;
                    return PythonDebugger::tracer_callback(obj, frame, PyTrace_LINE, arg);
                }
            } else
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
#if PY_MAJOR_VERSION >= 3
    PyThreadState* tstate = PyThreadState_GET();
    tstate->use_tracing = 0;
    result = PyEval_EvalCode(exprobj,
                             frame->f_globals,
                             frame->f_locals);
    tstate->use_tracing = 1;
#else
    frame->f_tstate->use_tracing = 0;
    result = PyEval_EvalCode((PyCodeObject*)exprobj,
                             frame->f_globals,
                             frame->f_locals);
    frame->f_tstate->use_tracing = 1;
#endif
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
    if (!frame || !frame->f_code)
        return false;
    do {
        const char *fileName = PY_AS_C_STRING(frame->f_code->co_filename);
        QString file = QString::fromUtf8(fileName);
        if (hasBreakpoint(file))
            return true;
        if (file == QLatin1String("<string>"))
            return false; // eval-ed code

    } while (frame != nullptr &&
             (frame = frame->f_back) != nullptr);

    return false;
}

#include "moc_PythonDebugger.cpp"

