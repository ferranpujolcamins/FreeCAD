#include "PythonJedi.h"
#include "PythonSyntaxHighlighter.h"

#include <App/PythonDebugger.h>
#include <Base/Interpreter.h>
#include <QEventLoop>
#include <QApplication>
#include <QDebug>

#include <Base/PyObjectBase.h>
#include <Base/Interpreter.h>
#include <Base/Exception.h>
#include <Base/Parameter.h>
#include <App/PythonDebugger.h>

#include "Application.h"
#include "BitmapFactory.h"

using namespace Qt;
using namespace Gui;

namespace Gui {
class JediCommonP
{
public:
    static void printErr();

    // helper functions that GIL locks and swaps interpreter

    // these must be specialized
    static QString fetchStr(Py::Object self, const char *name,
                            Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);

    static bool fetchBool(Py::Object self, const char *name,
                          Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);

    static int fetchInt(Py::Object self, const char *name,
                        Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);


    // templated functions, more generalized, reduce risk for bugs
    template <typename retT, typename objT>
    static retT fetchObj(Py::Object self, const char *name,
                         Py::Tuple *args = nullptr, Py::Dict *kw = nullptr)
    {
        JediInterpreter::SwapIn jedi;
        try {
            Py::Object o;
            o = getProperty(self, name);
            if (o.isCallable())
                o = callProperty(o, args, kw);

            if (!o.isNull())
                return retT(new objT(o));

        } catch (Py::Exception) {
            printErr();
        }
        return nullptr;
    }

    template <typename retT, typename objT, typename ptrT>
    static retT fetchList(Py::Object self, const char *name,
                          Py::Tuple *args = nullptr, Py::Dict *kw = nullptr)
    {
        JediInterpreter::SwapIn jedi;
        try {
            Py::Object o;
            o = getProperty(self, name);
            if (o.isCallable())
                o = callProperty(o, args, kw);

            if (o.isList())
                return createList<retT, objT, ptrT>(o);

        } catch (Py::Exception) {
            printErr();
        }
        return retT();
    }


    // state should be swapped and GIL lock held before entering
    template<typename retT, typename objT, typename ptrT>
    static retT createList(const Py::List lst)
    {
        retT ret;

        for (const Py::Object &o : lst) {
            objT *defObj = new objT(o);
            if (defObj->isValid())
                ret.append(ptrT(defObj));
            else
                delete defObj;
        }

        return ret;
    }

    // these methods should be called while jedi is swaped in
    static Py::Object getProperty(Py::Object self, const char *method);
    static Py::Object callProperty(Py::Object prop, Py::Tuple *args = nullptr,
                                   Py::Dict *kw = nullptr);

    static bool squelshError;
};

} // namespace Gui
bool JediCommonP::squelshError = false;
void JediCommonP::printErr()
{
    JediInterpreter::SwapIn jedi;
    if (!squelshError) {
        //PyErr_Print(); // don't use python io
        PyObject *tp, *vl, *tr;
        PyErr_Fetch(&tp, &vl, &tr);
        if (!tp)
            return;
        PyErr_NormalizeException(&tp, &vl, &tr);
        PyObject *msgo = PyObject_Str(vl);
        if (!msgo)
            return;
        const char *traceback = nullptr;
#if PY_MAJOR_VERSION >=3
        const char *msg = PyUnicode_AsUTF8(msgo);
        if (tr)
            traceback = PyUnicode_AsUTF8(tr);
#else
        const char *msg = PyBytes_AsString(msgo);
        if (tr)
            traceback = PyBytes_AsString(tr);
#endif
        if (tr)
            Base::Console().Error("Code analyser: %s\n%s", msg, traceback);
        else
            Base::Console().Error("Code analyser: %s", msg);

    }

    PyErr_Clear();
}

QString JediCommonP::fetchStr(Py::Object self, const char *name,
                              Py::Tuple *args, Py::Dict *kw)
{
    JediInterpreter::SwapIn jedi;
    try {
        Py::Object o;
        o = getProperty(self, name);
        if (o.isCallable())
            o = callProperty(o, args, kw);

        if (o.isString())
            return QString::fromStdString(Py::String(o.as_string()).as_std_string());

    } catch (Py::Exception) {
        printErr();
    }
    return QString();
}

bool JediCommonP::fetchBool(Py::Object self, const char *name,
               Py::Tuple *args, Py::Dict *kw)
{
    JediInterpreter::SwapIn jedi;
    try {
        Py::Object o;
        o = getProperty(self, name);
        if (o.isCallable())
            o = callProperty(o, args, kw);

        if (o.isBoolean())
            return Py::Boolean(o);

    } catch (Py::Exception) {
        printErr();
    }
    return false;
}

int JediCommonP::fetchInt(Py::Object self, const char *name,
             Py::Tuple *args, Py::Dict *kw)
{
    JediInterpreter::SwapIn jedi;
    try {
        Py::Object o;
        o = getProperty(self, name);
        if (o.isCallable())
            o = callProperty(o, args, kw);

        if (o.isNumeric())
            return static_cast<int>(Py::Long(o).as_long());

    } catch (Py::Exception) {
        printErr();
    }
    return -1;
}

Py::Object JediCommonP::getProperty(Py::Object self, const char *method)
{
    try {
        return self.getAttr(method);

    } catch(Py::Exception e) {
        printErr();
    }

    return Py::Object();
}

Py::Object JediCommonP::callProperty(Py::Object prop,
                                     Py::Tuple *args, Py::Dict *kw)
{
    Py::Object res;
    try {
        PyObject *a = args ? args->ptr() : Py::Tuple().ptr();
        PyObject *k = kw ? kw->ptr() : nullptr;
        res = PyObject_Call(prop.ptr(), a, k);
        if (res.isNull())
            throw Py::Exception(); // error msg is already set

    } catch(Py::Exception e) {
        printErr();
    }

    return res;
}




// -------------------------------------------------------------------------

JediBaseDefinitionObj::JediBaseDefinitionObj(Py::Object obj) :
    m_obj(obj), m_type(JediBaseDefinitionObj::Base)
{
}

JediBaseDefinitionObj::~JediBaseDefinitionObj()
{
}

QString JediBaseDefinitionObj::name() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "name");
}

QString JediBaseDefinitionObj::type() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "type");
}

QString JediBaseDefinitionObj::module_name() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "module_name");
}

QString JediBaseDefinitionObj::module_path() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "module_name");
}

bool JediBaseDefinitionObj::in_builtin_module() const
{
    if (!isValid())
        return false;
    return JediCommonP::fetchBool(m_obj, "in_builtin_module");
}

int JediBaseDefinitionObj::line() const
{
    if (!isValid())
        return -1;
    return JediCommonP::fetchInt(m_obj, "line");
}

int JediBaseDefinitionObj::column() const
{
    if (!isValid())
        return -1;
    return JediCommonP::fetchInt(m_obj, "column");
}

QString JediBaseDefinitionObj::docstring(bool raw, bool fast) const
{
    if (!isValid())
        return QString();

    JediInterpreter::SwapIn me;
    try {
        Py::Dict kw;
        Py::Tuple args;
        kw["raw"] = Py::Boolean(raw);
        kw["fast"] = Py::Boolean(fast);
        return JediCommonP::fetchStr(m_obj,"docstring", &args, &kw);

    } catch (Py::Exception) {
        JediCommonP::printErr();
    }

    return QString();
}

QString JediBaseDefinitionObj::description() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "description");
}

QString JediBaseDefinitionObj::full_name() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "full_name");
}

JediDefinition_list_t JediBaseDefinitionObj::goto_assignments() const
{
    if (!isValid())
        return JediDefinition_list_t();
    return JediCommonP::fetchList
                <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                (m_obj, "goto_assignments", nullptr, nullptr);
}

JediDefinition_list_t JediBaseDefinitionObj::goto_definitions() const
{
    if (!isValid())
        return JediDefinition_list_t();

    bool printErr = JediCommonP::squelshError;

    JediCommonP::squelshError = true;
    JediDefinition_list_t res = JediCommonP::fetchList
                                      <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                                (m_obj, "goto_definitions", nullptr, nullptr);

    // questionable workaround, this function isn't public yet according to comment in code it seems be soon
    if (!res.size())
        res = JediCommonP::fetchList
            <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                      (m_obj, "_goto_definitions", nullptr, nullptr);

    JediCommonP::squelshError = printErr;

    return res;
}

JediDefinition_list_t JediBaseDefinitionObj::params() const
{
    if (!isValid())
        return JediDefinition_list_t();
    return JediCommonP::fetchList
                <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                            (m_obj, "params", nullptr, nullptr);

}

JediDefinition_ptr_t JediBaseDefinitionObj::parent() const
{
    if (!isValid())
        return nullptr;
    return JediCommonP::fetchObj<JediDefinition_ptr_t, JediDefinitionObj>
                                    (m_obj, "parent", nullptr, nullptr);
}

int JediBaseDefinitionObj::get_line_code(int before, int after) const
{
    if (!isValid())
        return -1;

    JediInterpreter::SwapIn me;
    try {
        Py::Dict kw;
        Py::Tuple args;
        kw["before"] = Py::Long(before);
        kw["after"] = Py::Long(after);

        return JediCommonP::fetchInt(m_obj, "get_line_code", &args, &kw);

    } catch (Py::Exception) {
        JediCommonP::printErr();
    }
    return -1;
}

JediDefinitionObj *JediBaseDefinitionObj::toDefinitionObj()
{
    if (!isValid())
        return nullptr;

    return dynamic_cast<JediDefinitionObj*>(this);
}

JediCompletionObj *JediBaseDefinitionObj::toCompletionObj()
{
    if (!isValid())
        return nullptr;

    return dynamic_cast<JediCompletionObj*>(this);
}

JediCallSignatureObj *JediBaseDefinitionObj::toCallSignatureObj()
{
    if (!isValid())
        return nullptr;

    return dynamic_cast<JediCallSignatureObj*>(this);
}

bool JediBaseDefinitionObj::isDefinitionObj()
{
    return m_type == Definition;
}

bool JediBaseDefinitionObj::isCompletionObj()
{
    return m_type == Completion;
}

bool JediBaseDefinitionObj::isCallSignatureObj()
{
    return m_type == CallSignature;
}


// ------------------------------------------------------------------------

JediCompletionObj::JediCompletionObj(Py::Object obj) :
    JediBaseDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::Completion;
}

JediCompletionObj::~JediCompletionObj()
{
}

QString JediCompletionObj::complete() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "complete");
}

QString JediCompletionObj::name_with_symbols() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "name_with_symbols");
}


// ------------------------------------------------------------------------

JediDefinitionObj::JediDefinitionObj(Py::Object obj) :
    JediBaseDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::Definition;
}

JediDefinitionObj::~JediDefinitionObj()
{
}

JediDefinition_list_t JediDefinitionObj::defined_names() const
{
    if (!isValid())
        return JediDefinition_list_t();

    return JediCommonP::fetchList
            <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                            (m_obj, "defined_names", nullptr, nullptr);
}

bool JediDefinitionObj::is_definition() const
{
    if (!isValid())
        return false;
    return JediCommonP::fetchBool(m_obj, "is_definition", nullptr, nullptr);
}

// -----------------------------------------------------------------------

JediCallSignatureObj::JediCallSignatureObj(Py::Object obj) :
    JediDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::CallSignature;
}

JediCallSignatureObj::~JediCallSignatureObj()
{
}

JediDefinition_ptr_t JediCallSignatureObj::index() const
{
    if (!isValid())
        return nullptr;
    return JediCommonP::fetchObj<JediDefinition_ptr_t, JediDefinitionObj>
                            (m_obj, "index", nullptr, nullptr);
}

int JediCallSignatureObj::bracket_start() const
{
    if (!isValid())
        return -1;
    return JediCommonP::fetchInt(m_obj, "bracket_start");
}



// -------------------------------------------------------------------------

JediScriptObj::JediScriptObj(const QString source, int line, int column,
                             const QString path, const QString encoding)
{
    if (!JediInterpreter::instance()->isValid())
        return;

    //Base::PyGILStateRelease release;
    JediInterpreter::SwapIn jedi;

    try {
        Py::Dict kw;
        Py::Tuple args;
        kw["source"] = Py::String(source.toUtf8().constData());//source.toStdString());
        kw["line"] = Py::Long(line);
        kw["column"] = Py::Long(column);
        kw["path"] = Py::String(path.toStdString());
        kw["encoding"] = Py::String(encoding.toStdString());

        // construct a new object
        PyObject *module = JediInterpreter::instance()->api().ptr();
        PyObject *dict = PyModule_GetDict(module);
        if(!dict)
            throw Py::AttributeError("Module error in jedi.api.*");

        PyObject *clsType = PyObject_GetItem(dict, Py::String("Script").ptr());
        if (!clsType)
            throw Py::AttributeError("jedi.api.String was not found");

        PyObject *newCls(PyObject_Call(clsType, args.ptr(), kw.ptr()));
        if (!newCls)
            throw Py::Exception(); // error message already set by PyObject_Call

        m_obj = newCls;

    } catch (Py::Exception e) {
        JediCommonP::printErr();
    }
}

JediScriptObj::~JediScriptObj()
{
}

JediCompletion_list_t JediScriptObj::completions() const
{
    if (!isValid())
        return JediCompletion_list_t();

    return JediCommonP::fetchList
                <JediCompletion_list_t, JediCompletionObj, JediCompletion_ptr_t>
                            (m_obj, "completions", nullptr, nullptr);
}

JediDefinition_list_t JediScriptObj::goto_definitions() const
{
    if (!isValid())
        return JediDefinition_list_t();

    return JediCommonP::fetchList
                <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                            (m_obj, "goto_definitions", nullptr, nullptr);
}

JediDefinition_list_t JediScriptObj::goto_assignments(bool follow_imports) const
{
    if (!isValid())
        return JediDefinition_list_t();

    JediInterpreter::SwapIn jedi;

    try {
        Py::Tuple args(1);
        args[0] = Py::Boolean(follow_imports);

        return JediCommonP::fetchList
                    <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                (m_obj, "goto_assignments", &args, nullptr);

    } catch (Py::Exception e) {
        JediCommonP::printErr();
    }

    return JediDefinition_list_t();
}

JediDefinition_list_t JediScriptObj::usages(QStringList additional_module_paths) const
{
    if (!isValid())
        return JediDefinition_list_t();

    JediInterpreter::SwapIn jedi;

    try {
        Py::Tuple args(additional_module_paths.size());
        for (int i = 0; i < additional_module_paths.size(); ++i)
            args[i] = Py::String(additional_module_paths[i].toStdString());

        return JediCommonP::fetchList
                    <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                (m_obj, "usages", &args, nullptr);

    } catch (Py::Exception e) {
        JediCommonP::printErr();
    }

    return JediDefinition_list_t();
}

JediCallSignature_list_t JediScriptObj::call_signatures() const
{
    if (!isValid())
        return JediCallSignature_list_t();

    return JediCommonP::fetchList
                    <JediCallSignature_list_t, JediCallSignatureObj, JediCallSignature_ptr_t>
                            (m_obj, "call_signatures", nullptr, nullptr);
}

bool JediScriptObj::isValid() const
{
    return JediInterpreter::instance()->isValid() &&
           m_obj.ptr() != nullptr;
}

//--------------------------------------------------------------------------

JediDebugProxy::JediDebugProxy()
    : Py::ExtensionModule<JediDebugProxy>( "JediDebugProxy" )
{
    add_varargs_method("callback", &JediDebugProxy::proxy, "callback(color, str)");
    initialize( "Module for debug callback to C++" );
}

JediDebugProxy::~JediDebugProxy()
{
}

Py::Object JediDebugProxy::proxy(const Py::Tuple &args)
{
    try {
        Py::Tuple argsTuple(args);
        if (argsTuple.length() >= 2) {
            std::string color = Py::String(argsTuple[0]).as_std_string();
            std::string str = Py::String(argsTuple[1]).as_std_string();
            // need to lock here if our receivers uses anything python in receiving slots
            Base::PyGILStateLocker lock;
            Q_EMIT JediInterpreter::instance()->onDebugInfo(QString::fromStdString(color),
                                                            QString::fromStdString(str));
        }

    } catch (Py::Exception e) {
        e.clear();
    }
    return Py::None();
}
const char *JediDebugProxy::ModuleName = "_jedi_debug_proxy";

//-------------------------------------------------------------------------

JediInterpreter::JediInterpreter() :
    m_jedi(nullptr), m_api(nullptr)
{
    assert(m_instance == nullptr && "JediInterpreter must be singleton");
    m_instance = const_cast<JediInterpreter*>(this);
    m_evtLoop = new QEventLoop(this);

    // reconstruct command line args passed to application on startup
    QStringList args = QApplication::arguments();
#if PY_MAJOR_VERSION >= 3
    wchar_t **argv = new wchar_t*[args.size()];
#else
    char **argv = new char*[args.size()];
#endif
    size_t argc = 0;

    for (const QString str : args) {
#if PY_MAJOR_VERSION >= 3
        wchar_t *dest = new wchar_t[sizeof(wchar_t) * (str.size())];
        str.toWCharArray(dest);
#else
        char *src = str.toLatin1().data();
        char *dest = new char[sizeof(char) * (strlen(src))];
        strcpy(dest, src);
#endif
        argv[argc++] = dest;
    }

    //Base::PyGILStateLocker lock;
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();


    // need to redirect in new interpreter
    PyObject *stdout = PySys_GetObject("stdout");
    PyObject *stderr = PySys_GetObject("stderr");
    auto *pyhome = Py_GetPythonHome();
    // halt possible current tracing/debugging
    PyEval_SetTrace(haltMainInterpreter, NULL);

    // create subinterpreter and set up its threadstate
    PyThreadState *mainInterp = PyThreadState_Swap(NULL); // take reference to main python


    if (!Py_IsInitialized())
        Py_InitializeEx(0);

    m_interpState = Py_NewInterpreter();// new sub interpreter
    m_threadState = m_interpState;// PyThreadState_New(m_interpState->interp);
    PyThreadState_Swap(m_threadState);
    PySys_SetArgvEx(argc, argv, 0);
    int sr = PySys_SetObject("stdout", stdout);
    int er = PySys_SetObject("stderr", stderr);
    Py_SetPythonHome(pyhome);

    // restore to main interpreter and release GIL lock
    PyThreadState_Swap(mainInterp);

    // reset halt for main tread
    PyEval_SetTrace(NULL, NULL);
    m_evtLoop->quit();
    PyGILState_Release(gstate);


    // delete argc again
    while(argc > 0)
        delete [] argv[--argc];
    delete [] argv;


    // is py interpreter is setup now?
    if (sr < 0 || er < 0 || !m_threadState) {
        // error handling
        destroy();

    } else {
        // we have a working interpreter

        SwapIn jedi; // scopeguarded to handle uncaught exceptions
                     // from here on its the new interpreter due to scope guard above

        Py_XINCREF(Py::Module("__main__").ptr()); // adds main module to new interpreter

        // init with default modules
        //Base::Interpreter().runString(Base::ScriptFactory().ProduceScript("CMakeVariables"));
        //Base::Interpreter().runString(Base::ScriptFactory().ProduceScript("FreeCADInit"));

        try {
            // init jedi
            // must use pointers here as Py::Module explicitly has no empty constructor
            PyObject *m = PyImport_ImportModule("jedi");
            if (m && PyModule_Check(m)) {
                m_jedi = new Py::Module(m);
            } else {
                PyErr_SetString(PyExc_ImportError, "jedi not found");
                throw Py::Exception();
            }

            PyObject *api_m = PyImport_ImportModule("jedi.api");
            if (api_m && PyModule_Check(api_m)) {
                m_api = new Py::Module(api_m);
            } else {
                PyErr_SetString(PyExc_ImportError, "jedi.api not found");
                throw Py::Exception();
            }

            QStringList preLoads = {
                QLatin1String("FreeCAD"), QLatin1String("FreeCAD.App"),
                QLatin1String("FreeCAD.Gui"), QLatin1String("Part")
            };

            preload_module(preLoads);


            // TODO jedi.settings

        } catch(Py::Exception e) {
            JediCommonP::printErr();
        }
    }
}

JediInterpreter::~JediInterpreter()
{
    destroy();
}

// used to halt execution on main interpreter while we setup our subinterpreter
QEventLoop *JediInterpreter::m_evtLoop = nullptr;
int JediInterpreter::haltMainInterpreter(PyObject *obj, PyFrameObject *frame,
                                         int what, PyObject *arg)
{
    Q_UNUSED(obj);
    Q_UNUSED(frame);
    Q_UNUSED(what);
    Q_UNUSED(arg);
    m_evtLoop->exec(); // halt main interpreter
    return 0;
}

JediInterpreter *JediInterpreter::m_instance = nullptr;
JediInterpreter *JediInterpreter::instance()
{
    if (!m_instance) {
        new JediInterpreter; // instance gets set in Constructor
    }
    return m_instance;
}

bool JediInterpreter::isValid() const
{
    return m_threadState != nullptr &&
           Py_IsInitialized() &&
           (m_jedi && !m_jedi->isNull()) &&
           (m_api && !m_api->isNull());
}

Py::Object JediInterpreter::runString(const QString &src)
{
    SwapIn myself;
    return Base::Interpreter().runStringObject(src.toLatin1());
}

Py::Module &JediInterpreter::jedi() const
{
    return *m_jedi;
}

Py::Module &JediInterpreter::api() const
{
    return *m_api;
}

PyThreadState *JediInterpreter::threadState() const
{
    return m_threadState;
}

JediDefinition_list_t JediInterpreter::names(const QString source, const QString path,
                                             const QString encoding, bool all_scopes,
                                             bool definitions, bool references) const
{
    if (!isValid())
        return JediDefinition_list_t();

    JediDefinition_list_t ret;
    try {
        SwapIn jedi;

        Py::Dict kw;
        Py::Tuple args;
        kw["source"] = Py::String(source.toStdString());
        kw["path"] = Py::String(path.toStdString());
        kw["encoding"] = Py::String(encoding.toStdString());
        kw["all_scopes"] = Py::Boolean(all_scopes);
        kw["definitions"] = Py::Boolean(definitions);
        kw["references"] = Py::Boolean(references);

        return JediCommonP::fetchList
                    <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                (*m_api, "names", &args, &kw);

    } catch (Py::Exception e) {
        JediCommonP::printErr();
    }

    return ret;
}

// preload means that Jedi parsers them now instead of when detected in code
// it doesn't import them to local env. if they are not C modules
bool JediInterpreter::preload_module(const QStringList modules) const
{
    if (isValid()) {
        SwapIn jedi;

        try {
            Py::Tuple arg(modules.size());
            int i = 0;
            for (const QString &module : modules) {
                arg[i++] = Py::String(module.toStdString());
            }
            Py::Object prop = JediCommonP::getProperty(*m_jedi, "preload_module");
            JediCommonP::callProperty(prop, &arg);
            return true;

        } catch (Py::Exception e) {
            JediCommonP::printErr();
        }
    }

    return false;
}

bool JediInterpreter::setDebugging(bool on = true, bool warnings,
                                   bool notices, bool speed) const
{
    if (isValid()) {
        SwapIn jedi;
        try {
            // create and load the module
            static JediDebugProxy *proxy = nullptr;
            if (!proxy) {
                proxy = new JediDebugProxy;
                new Py::Module(JediDebugProxy::ModuleName);
            }

            Py::Dict kw;
            Py::Tuple arg;
            if (on)
                kw["debug_func"] = Py::Callable(proxy->module());
            else
                kw["debug_func"] = Py::None();
            kw["warnings"] = Py::Boolean(warnings);
            kw["notices"] = Py::Boolean(notices);
            kw["speed"] = Py::Boolean(speed);

            Py::Object prop = JediCommonP::getProperty(*m_jedi, "set_debug_func");
            JediCommonP::callProperty(prop, &arg, &kw);
            return true;

        } catch (Py::Exception e) {
            e.clear();
            return false;
        }
    }

    return false;
}

void JediInterpreter::destroy()
{
    if (m_interpState) {

        if (m_jedi) {
            SwapIn swap;
            delete m_jedi;
            m_jedi = nullptr;
        }

        if (m_api) {
            SwapIn swap;
            delete m_api;
            m_api = nullptr;
        }

        if (m_threadState) {
            PyThreadState_Clear(m_threadState);
            PyThreadState_Delete(m_threadState);
            m_threadState = nullptr;
        }

        Py_EndInterpreter(m_interpState);
        m_interpState = nullptr;
    }
}


// -----------------------------------------------------------------------


JediInterpreter::SwapIn::SwapIn() :
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
        PyThreadState_Swap(JediInterpreter::instance()->threadState());
        m_GILState = PyGILState_Ensure();
    }
}

JediInterpreter::SwapIn::~SwapIn()
{
    // ensure we only swap back if this isntance was the swapping one
    if (m_oldState) {
        // release from jedi thread and swap back to old thread
        PyGILState_Release(m_GILState);
        PyThreadState_Swap(m_oldState);

        if (static_GILHeld)
            static_GILHeld = false;
    }
}
bool JediInterpreter::SwapIn::static_GILHeld = false;


#include "moc_PythonJedi.cpp"
