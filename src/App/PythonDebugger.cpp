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

#include "DebuggerBase.h"
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



// for serializing deserializing breakpoint on app end/open
QDataStream& operator << (QDataStream& out,
                          const App::Debugging::Python::BrkPntFile& bpf)
{
    bpf.serialize(out);
    return out;
}

QDataStream& operator >> (QDataStream& in,
                          App::Debugging::Python::BrkPntFile& bpf)
{
    bpf.deserialize(in);
    return in;
}



// -----------------------------------------------------
namespace App {
namespace Debugging {
namespace Python {


class DebugModuleP
{
public:
    DebugModuleP() :
        stdout(new DebugStdout()),
        stderr(new DebugStderr())
    {}
    ~DebugModuleP()
    {
        delete stdout;
        delete stderr;
    }
    DebugStdout *stdout;
    DebugStderr *stderr;
};


// -----------------------------------------------------

class DebuggerPy : public Py::PythonExtension<DebuggerPy>
{
public:
    DebuggerPy(Debugger* d) :
        dbg(d), runtimeException(nullptr)
    { }
    ~DebuggerPy();
    Debugger* dbg;
    Base::PyExceptionInfo *runtimeException;
};
DebuggerPy::~DebuggerPy() {}

// -----------------------------------------------------

class DebuggerP {
public:
    typedef void(DebuggerP::*voidFunction)(void);
    PyObject* out_o;
    PyObject* err_o;
    PyObject* exc_o;
    PyObject* out_n;
    PyObject* err_n;
    PyObject* exc_n;
    PyObject* pydbg;
    PyFrameObject* currentFrame;
    DebugExcept* pypde;
    QEventLoop loop;
    QString tracerfile; // last filename to enter tracer_function
    int maxHaltLevel;
    int showStackLevel;
    int tracerline;     // last line to tracer_function
    bool init, trystop; //, halted;

    DebuggerP(Debugger* that) :
        maxHaltLevel(-1), showStackLevel(-1), tracerline(0),
        init(false), trystop(false) //,halted(false)
    {
        out_o = nullptr;
        err_o = nullptr;
        exc_o = nullptr;
        currentFrame = nullptr;
        Base::PyGILStateLocker lock;
        out_n = new DebugStdout();
        err_n = new DebugStderr();
        pypde = new DebugExcept();
        Py::Object func = pypde->getattr("fc_excepthook");
        exc_n = Py::new_reference_to(func);
        pydbg = new DebuggerPy(that);
    }
    ~DebuggerP()
    {
        Base::PyGILStateLocker lock;
        Py_DECREF(out_n);
        Py_DECREF(err_n);
        Py_DECREF(exc_n);
        Py_DECREF(pypde);
        Py_DECREF(pydbg);

        //for (BreakpointPyFile *bpf : bps)
        //    delete bpf;
    }
};

} // namespace Python
} // namespace Debugging
} // namespace App

// ----------------------------------------------------------------------------

using namespace App;
using namespace Debugging;
using namespace Python;

BrkPnt::BrkPnt(int lineNr, std::weak_ptr<BrkPntFile> parent)
    : BrkPntBase<BrkPntFile>(lineNr, parent)
{
}

BrkPnt::BrkPnt(const BrkPnt &other)
    : BrkPntBase<BrkPntFile>(other)
{
    m_condition = other.m_condition;
}

BrkPnt::~BrkPnt()
{
}

BrkPnt& BrkPnt::operator=(const BrkPnt& other)
{
    BrkPntBase<BrkPntFile>::operator=(other);
    m_condition = other.m_condition;
    return *this;
}

void BrkPnt::setCondition(const QString condition)
{
    QRegExp re(QLatin1String("([^=><!])=([^=])"));
    m_condition =  QString(condition).replace(re, QLatin1String("\\1==\\2"));
    bpFile()->debugger()->breakpointChanged(uniqueId());
}

const QString BrkPnt::condition() const
{
    return m_condition;
}

// -------------------------------------------------------------------------------------

// only here to make Qt Serialize work for QVariant
BrkPntFile::BrkPntFile()
    : BrkPntBaseFile<Debugger, BrkPnt, BrkPntFile>(nullptr)
{
}

BrkPntFile::BrkPntFile(std::shared_ptr<Debugger> dbgr)
    : BrkPntBaseFile<Debugger, BrkPnt, BrkPntFile>(dbgr)
{
}


BrkPntFile::BrkPntFile(const BrkPntFile &other)
    : BrkPntBaseFile<Debugger, BrkPnt, BrkPntFile>(other)
{
}

BrkPntFile::~BrkPntFile()
{
    if (weak_from_this().expired())
        return;
    auto self = std::dynamic_pointer_cast<BrkPntFile>(shared_from_this());
    debugger()->removeBreakpointFile(self);
}

void BrkPntFile::serialize(QDataStream &out) const
{
    BrkPntBaseFile<Debugger, BrkPnt, BrkPntFile>::serialize(out);
    // we dont have any properties inn this class to serialze, only in subclass
}

bool BrkPntFile::deserialize(QDataStream &in)
{
    in.startTransaction();
    QString filename,
            debuggerName;
    quint16 siz = 0;
    in >> debuggerName >> filename >> siz;
    if (debuggerName != QString::fromLatin1(Debugger::instance()->debuggerName())) {
        in.rollbackTransaction();
        return false;
    }

    auto dbgr = std::dynamic_pointer_cast<Debugger>(Debugger::instance()->shared_from_this());

    // smart pointers as such a bad thing, more in the way than ehlpfull
    // this object is created from new without a shared object and ref counter
    // we need to make a new one from within here
    auto self = std::make_shared<BrkPntFile>(dbgr);
    self->setFilename(filename);
    dbgr->setBreakpointFile(self);

    setFilename(filename);

    for (quint16 i = 0; i < siz; ++i) {
        auto bp = std::make_shared<BrkPnt>(0, self);
        bp->deserialize(in);
        self->setBreakpoint(bp);
    }

    return true;
}

// ----------------------------------------------------------------------------------

void DebugModule::init_module(void)
{
    DebugStdout::init_type();
    DebugStderr::init_type();
    DebugExcept::init_type();
    static DebugModule* mod = new DebugModule();
    Q_UNUSED(mod)
}

DebugModule::DebugModule()
  : Py::ExtensionModule<DebugModule>("FreeCADDbg"),
    d(new DebugModuleP())
{
    add_varargs_method("getFunctionCallCount", &DebugModule::getFunctionCallCount,
        "Get the total number of function calls executed and the number executed since the last call to this function.");
    add_varargs_method("getExceptionCount", &DebugModule::getExceptionCount,
        "Get the total number of exceptions and the number executed since the last call to this function.");
    add_varargs_method("getLineCount", &DebugModule::getLineCount,
        "Get the total number of lines executed and the number executed since the last call to this function.");
    add_varargs_method("getFunctionReturnCount", &DebugModule::getFunctionReturnCount,
        "Get the total number of function returns executed and the number executed since the last call to this function.");

    initialize( "The FreeCAD Python debug module" );

    Py::Dict dict(moduleDictionary());
    Py::Object out(Py::asObject(d->stdout));
    dict["StdOut"] = out;
    Py::Object err(Py::asObject(d->stderr));
    dict["StdErr"] = err;
}

DebugModule::~DebugModule()
{
    delete d;
}

Py::Object DebugModule::getFunctionCallCount(const Py::Tuple &)
{
    return Py::None();
}

Py::Object DebugModule::getExceptionCount(const Py::Tuple &)
{
    return Py::None();
}

Py::Object DebugModule::getLineCount(const Py::Tuple &)
{
    return Py::None();
}

Py::Object DebugModule::getFunctionReturnCount(const Py::Tuple &)
{
    return Py::None();
}

// -----------------------------------------------------

void DebugStdout::init_type()
{
    behaviors().name("PythonDebugStdout");
    behaviors().doc("Redirection of stdout to FreeCAD's Python debugger window");
    // you must have overwritten the virtual functions
    behaviors().supportRepr();
    add_varargs_method("write",&DebugStdout::write,"write to stdout");
    add_varargs_method("flush",&DebugStdout::flush,"flush the output");
}

DebugStdout::DebugStdout()
{
}

DebugStdout::~DebugStdout()
{
}

Py::Object DebugStdout::repr()
{
    std::string s;
    std::ostringstream s_out;
    s_out << "PythonDebugStdout";
    return Py::String(s_out.str());
}

Py::Object DebugStdout::write(const Py::Tuple& args)
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

Py::Object DebugStdout::flush(const Py::Tuple&)
{
    return Py::None();
}

// -----------------------------------------------------

void DebugStderr::init_type()
{
    behaviors().name("PythonDebugStderr");
    behaviors().doc("Redirection of stderr to FreeCAD's Python debugger window");
    // you must have overwritten the virtual functions
    behaviors().supportRepr();
    add_varargs_method("write",&DebugStderr::write,"write to stderr");
}

DebugStderr::DebugStderr()
{
}

DebugStderr::~DebugStderr()
{
}

Py::Object DebugStderr::repr()
{
    std::string s;
    std::ostringstream s_out;
    s_out << "PythonDebugStderr";
    return Py::String(s_out.str());
}

Py::Object DebugStderr::write(const Py::Tuple& args)
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

void DebugExcept::init_type()
{
    behaviors().name("PythonDebugExcept");
    behaviors().doc("Custom exception handler");
    // you must have overwritten the virtual functions
    behaviors().supportRepr();
    add_varargs_method("fc_excepthook",&DebugExcept::excepthook,"Custom exception handler");
}

DebugExcept::DebugExcept()
{
}

DebugExcept::~DebugExcept()
{
}

Py::Object DebugExcept::repr()
{
    std::string s;
    std::ostringstream s_out;
    s_out << "PythonDebugExcept";
    return Py::String(s_out.str());
}

Py::Object DebugExcept::excepthook(const Py::Tuple& args)
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


// ---------------------------------------------------------------

Debugger::Debugger(QObject *parent)
  : AbstractDbgr<Debugger, BrkPnt, BrkPntFile>(parent)
  , d(new DebuggerP(this))
{
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Debugger::onAppQuit);

    typedef void (*STATICFUNC)( );
    STATICFUNC fp = Debugger::finalizeFunction;
    Py_AtExit(fp);
}

Debugger::~Debugger()
{
    stop();
    delete d;
    //globalInstance = nullptr; // smart pointers destruct again here
}

void Debugger::runFile(const QString& fn)
{
    try {
        if (state() != State::HaltOnNext)
            setState(State::Running);
        QByteArray pxFileName = fn.toUtf8();
#ifdef FC_OS_WIN32
        Base::FileInfo fi((const char*)pxFileName);
        FILE *fp = _wfopen(fi.toStdWString().c_str(),L"r");
#else
        FILE *fp = fopen(static_cast<const char*>(pxFileName), "r");
#endif
        if (!fp) {
            setState(State::Stopped);
            return;
        }

        Base::PyGILStateLocker locker;
        PyObject *module, *dict;
        module = PyImport_AddModule("__main__");
        dict = PyModule_GetDict(module);
        dict = PyDict_Copy(dict);
        if (PyDict_GetItemString(dict, "__file__") == nullptr) {
#if PY_MAJOR_VERSION >= 3
            PyObject *f = PyUnicode_FromString(static_cast<const char*>(pxFileName));
#else
            PyObject *f = PyBytes_FromString((const char*)pxFileName);
#endif
            if (f == nullptr) {
                fclose(fp);
                setState(State::Stopped);
                return;
            }
            if (PyDict_SetItemString(dict, "__file__", f) < 0) {
                Py_DECREF(f);
                fclose(fp);
                setState(State::Stopped);
                return;
            }
            Py_DECREF(f);
        }

        PyObject *result = PyRun_File(fp, static_cast<const char*>(pxFileName), Py_file_input, dict, dict);
        fclose(fp);
        Py_DECREF(dict);

        if (!result) {
            if (!d->trystop && state() != State::Stopped) {
                // script failed, syntax error, import error, etc
                auto exc = new Base::PyExceptionInfo();
                if (exc->isValid() && !exc->isSystemExit() && !exc->isKeyboardInterupt()) {
                    Q_EMIT exceptionFatal(exc);

                    // user code exit() makes PyErr_Print segfault
                    Base::Console().Error("(debugger):%s\n%s\n%s",
                                            exc->getErrorType(true).c_str(),
                                            exc->getStackTrace().c_str(),
                                            exc->getMessage().c_str());
                    PyErr_Clear();
                } else
                    delete exc;

            }
            setState(State::Stopped);
            // Q_EMIT stopped(); // handled in setState
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

    //if (d->trystop) {
        stop(); // de init tracer_function and reset object
    //}
    setState(State::Stopped);
}

bool Debugger::start()
{
    if (state() == State::Stopped)
        setState(State::Running);

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

bool Debugger::stop()
{
    if (!d->init)
        return false;
    if (isHalted()) {
        d->trystop = true;
        d->loop.quit();
        return false;
    }

    { // threadlock code block
        Base::PyGILStateLocker lock;
        PyErr_Clear();
        PyEval_SetTrace(nullptr, nullptr);
        PySys_SetObject("stdout", d->out_o);
        PySys_SetObject("stderr", d->err_o);
        PySys_SetObject("excepthook", d->exc_o);
        d->init = false;
    } // end thread lock code block
    d->currentFrame = nullptr;
    setState(State::Stopped); // also emit stop
    d->trystop = false;
    return true;
}

void Debugger::tryStop()
{
    d->trystop = true;
    d->loop.quit();
}

void Debugger::haltOnNext()
{
    start();
    setState(State::HaltOnNext);
}

void Debugger::stepOver()
{
    setState(State::StepOver);
    d->maxHaltLevel = callDepth();
    d->loop.quit();
}

void Debugger::stepInto()
{
    setState(State::SingleStep);
    d->loop.quit();
}

void Debugger::stepOut()
{
    setState(State::StepOut);
    d->maxHaltLevel = callDepth() -1;
    if (d->maxHaltLevel < 0)
        d->maxHaltLevel = 0;
    d->loop.quit();
}

void Debugger::stepContinue()
{
    setState(State::Running);
    d->loop.quit();
}

void Debugger::sendClearException(const QString &fn, int line)
{
    Q_EMIT clearException(fn, line);
}

void Debugger::sendClearAllExceptions()
{
    Q_EMIT clearAllExceptions();
}

void Debugger::onFileOpened(const QString &fn)
{
    QFileInfo fi(fn);
    if (fi.suffix().toLower() == QLatin1String("py") ||
        fi.suffix().toLower() == QLatin1String("fcmacro"))
    {
        createBreakpointFile(fn);
    }
}

void Debugger::onFileClosed(const QString &fn)
{
    QFileInfo fi(fn);
    if (fi.suffix().toLower() == QLatin1String("py") ||
        fi.suffix().toLower() == QLatin1String("fcmacro"))
    {
        removeBreakpointFile(fn);
    }
}

void Debugger::onAppQuit()
{
    stop();
}

PyFrameObject *Debugger::currentFrame() const
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

QString Debugger::currentFile() const
{
    auto frame = currentFrame();
    if (frame && isHalted()) {
        const char *filename = PY_AS_C_STRING(frame->f_code->co_filename);
        return QString::fromUtf8(filename);
    }
    return QString();
}

int Debugger::currentLine() const
{
    auto frame = currentFrame();
    if (frame && isHalted()) {
        return PyCode_Addr2Line(frame->f_code, frame->f_lasti);
    }
    return -1;
}

int Debugger::callDepth(const PyFrameObject *frame) const
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

int Debugger::callDepth() const
{
    Base::PyGILStateLocker locker;
    PyFrameObject const *fr = d->currentFrame;
    return callDepth(fr);
}

bool Debugger::setStackLevel(int level)
{
    if (!isHalted())
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
            Q_EMIT nextInstruction();
        }
    }

    return false;
}

// is owned by macro manager which handles delete
std::shared_ptr<Debugger> Debugger::globalInstance = nullptr;

Debugger *Debugger::instance()
{
    if (globalInstance == nullptr)
        // is owned by macro manager which handles delete
        globalInstance = std::make_shared<Debugger>();
    return globalInstance.get();
}

const char *Debugger::debuggerName() const {
    return "App::Debugging::Python::Debugger";
}


void Debugger::setState(State::States state)
{
    AbstractDbgr<Debugger, BrkPnt, BrkPntFile>::setState(state);
    Base::PyGILStateLocker locker; // event recievers might do strange things
                                   // TODO hide Python from usercode
    PyObject *e, *tp, *tr;
    PyErr_Fetch(&e, &tp, &tr);
    try {
        switch(state) {
        case State::Stopped:
            Q_EMIT stopped();
            break;
        case State::Halted:
            Q_EMIT haltAt(d->tracerfile, d->tracerline);
            Q_EMIT nextInstruction();
         break;
        case State::Running:
            Q_EMIT releaseAt(d->tracerfile, d->tracerline);
            break;
        default: ; // pass
        }
    } catch(...) { }

    PyErr_Restore(e, tp, tr);
}

// http://www.koders.com/cpp/fidBA6CD8A0FE5F41F1464D74733D9A711DA257D20B.aspx?s=PyEval_SetTrace
// http://code.google.com/p/idapython/source/browse/trunk/python.cpp
// http://www.koders.com/cpp/fid191F7B13CF73133935A7A2E18B7BF43ACC6D1784.aspx?s=PyEval_SetTrace
// http://stuff.mit.edu/afs/sipb/project/python/src/python2.2-2.2.2/Modules/_hotshot.c
// static
int Debugger::tracer_callback(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg)
{

    DebuggerPy* self = static_cast<DebuggerPy*>(obj);
    Debugger* dbg = self->dbg;
    QCoreApplication::processEvents(); // lets user quit a infinite loop (python can hang qt eventloop)

    dbg->d->tracerfile = QString::fromUtf8(PY_AS_C_STRING(frame->f_code->co_filename));
    dbg->d->tracerline = PyCode_Addr2Line(frame->f_code, frame->f_lasti);

    if (dbg->d->trystop) {
        PyErr_SetInterrupt();
        dbg->setState(State::Stopped);
        return 0;
    }

    switch (what) {
    case PyTrace_CALL:
        if (dbg->state() != State::Running &&
            dbg->frameRelatedToOpenedFiles(frame))
        {
            try {
                Q_EMIT dbg->functionCalled();
            } catch(...){
                PyErr_Clear();
            } // might throw
        }
        return 0;
    case PyTrace_RETURN:
        if (dbg->state() != State::Running &&
            dbg->frameRelatedToOpenedFiles(frame))
        {
            try {
                Q_EMIT dbg->functionExited();
            } catch (...) {
                PyErr_Clear();
            }
        }

        return 0;
    case PyTrace_LINE:
        {
            bool halt = false;
            if (dbg->state() == State::SingleStep ||
                dbg->state() == State::HaltOnNext)
            {
                halt = true;
            } else if((dbg->state() == State::StepOver ||
                       dbg->state() == State::StepOut) &&
                      dbg->d->maxHaltLevel >= dbg->callDepth(frame))
            {
                halt = true;
            } else { // RunningState
                auto bp = dbg->getBreakpoint(dbg->d->tracerfile,
                                             dbg->d->tracerline);
                if (bp != nullptr) {
                    auto condition = std::dynamic_pointer_cast<BrkPnt>(bp)->condition();
                    if (condition.size()) {
                        halt = Debugger::evalCondition(condition.toLatin1(), frame);
                    } else if (bp->hit()) {
                        halt = true;
                    }
                }
            }

            if (halt) {
                // prevent halting on non opened files
                if (!dbg->frameRelatedToOpenedFiles(frame))
                    return 0;

                while(dbg->isHalted()) {
                    // already halted, must be another thread here
                    // halt until current thread releases
                    QCoreApplication::processEvents();
                }

                {   // threadlock block
                    Base::PyGILStateLocker locker;
                    Py_XDECREF(dbg->d->currentFrame);
                    dbg->d->currentFrame = frame;
                    Py_XINCREF(dbg->d->currentFrame);

                    if (!dbg->isHalted()) {
                        try {
                            Q_EMIT dbg->functionCalled();
                        } catch (...) { }
                    }
                    dbg->setState(State::Halted); // also emits
                }   // end threadlock block
                dbg->d->loop.exec();
                { // threadlock block
                    Base::PyGILStateLocker locker;
                    try {
                        Q_EMIT dbg->releaseAt(dbg->d->tracerfile, dbg->d->tracerline);
                    } catch (...) {
                        PyErr_Clear();
                    }
                } // end threadlock block
            }

            return 0;
        }
    case PyTrace_EXCEPTION:
        if (dbg->frameRelatedToOpenedFiles(frame)) {
            // is it within a try: block, might be in a parent frame
            QRegExp re(QLatin1String("(^<.*frozen|^<.*built-in)"));
            if (re.indexIn(dbg->d->tracerfile) > -1)
                return 0;

            for (PyFrameObject *f = frame; f; f = f->f_back) {
                if (f->f_iblock > 0) { // an exeption handler at this frame
                    int b_type = f->f_blockstack[f->f_iblock -1].b_type; // blockstackindex is +1 based
#if PY_MAJOR_VERSION >= 3 and PY_MINOR_VERSION < 8
                    if (b_type == SETUP_EXCEPT
#else
                    if (b_type == EXCEPT_HANDLER ||
#endif
                        b_type == SETUP_FINALLY)
                    {
                        return 0; // should be caught by a try .. except block
                    }
                }

                if (re.indexIn(dbg->d->tracerfile) > -1)
                    return 0; // its C-code that have called this frame,
                              // can never be certain how C code handles exceptions
            }

            auto exc = new Base::PyExceptionInfo(arg);
            if (exc->isValid() && !exc->isKeyboardInterupt()) {
                Q_EMIT dbg->exceptionOccured(exc);

                // halt debugger so we can view stack?
                ParameterGrp::handle hPrefGrp = App::GetApplication().GetUserParameter().GetGroup("BaseApp")
                                                    ->GetGroup("Preferences")->GetGroup("Editor");
                if (hPrefGrp->GetBool("EnableHaltDebuggerOnExceptions", true)) {
                    dbg->setState(State::HaltOnNext);
                    return Debugger::tracer_callback(obj, frame, PyTrace_LINE, arg);
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
bool Debugger::evalCondition(const char *condition, PyFrameObject *frame)
{
    /* Eval breakpoint condition */
    PyObject *result, *e, *tp, *tr;
    bool ret = false;

    PyErr_Fetch(&e, &tp, &tr);

    do {
        /* Use empty filename to avoid obj added to object entry table */
        PyObject* exprobj = Py_CompileStringFlags(condition, "", Py_eval_input, NULL);
        if (!exprobj)
            break;

        /* Clear flag use_tracing in current PyThreadState to avoid
             tracing evaluation self */
    #if PY_MAJOR_VERSION >= 3
        PyThreadState* tstate = PyThreadState_GET();
        tstate->use_tracing = 0;
        result = PyEval_EvalCode(exprobj, frame->f_globals, frame->f_locals);
        tstate->use_tracing = 1;
    #else
        frame->f_tstate->use_tracing = 0;
        result = PyEval_EvalCode((PyCodeObject*)exprobj,
                                 frame->f_globals,
                                 frame->f_locals);
        frame->f_tstate->use_tracing = 1;
    #endif
        Py_DecRef(exprobj);

        if (result == nullptr)
            break;

        ret = PyObject_IsTrue(result) == 1;
        Py_DecRef(result);

    } while(false);

    PyErr_Restore(e, tp, tr);
    return ret;
}

// static
void Debugger::finalizeFunction()
{
    if (globalInstance != nullptr) {
        // user code with exit() finalizes interpreter so we start over
        if (!Py_IsInitialized()) {
            Debugger::instance()->sendClearAllExceptions();
            Base::InterpreterSingleton::Instance().init(App::Application::GetARGC(),
                                         App::Application::GetARGV());
        }
        globalInstance->stop(); // release a pending halt on app close
    }
}

bool Debugger::frameRelatedToOpenedFiles(const PyFrameObject *frame) const
{
    if (!frame || !frame->f_code)
        return false;
    do {
        const char *fileName = PY_AS_C_STRING(frame->f_code->co_filename);
        QString file = QString::fromUtf8(fileName);
        if (hasBreakpointFile(file))
            return true;
        if (file == QLatin1String("<string>"))
            return false; // eval-ed code

    } while (frame != nullptr &&
             (frame = frame->f_back) != nullptr);

    return false;
}

#include "moc_PythonDebugger.cpp"

