/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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

# include <QContextMenuEvent>
# include <QMenu>
# include <QPainter>
# include <QShortcut>
# include <QTextCursor>



#include "PythonCode.h"
#include "PythonSyntaxHighlighter.h"
#include "PythonEditor.h"
#include "Application.h"
#include "BitmapFactory.h"
#include "Macro.h"
#include <Base/PyTools.h>
#include <PythonSource/PythonSource.h>

#include <frameobject.h> // python
#include <CXX/Objects.hxx>
#include <Base/PyObjectBase.h>
#include <Base/Interpreter.h>
#include <Base/Exception.h>
#include <Base/Parameter.h>
#include <App/PythonDebugger.h>
#include <QApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFileInfo>
#include <QDir>
#include <QTextCharFormat>

DBG_TOKEN_FILE

#if PY_MAJOR_VERSION >= 3
# define PY_AS_STRING PyUnicode_AsUTF8
# define PY_FROM_STRING PyUnicode_FromString
# define PY_LONG_CHECK PyLong_Check
# define PY_LONG_FROM_STRING PyLong_FromString
# define PY_LONG_AS_LONG PyLong_AsLong
#else
# define PY_AS_STRING PyBytes_AsString
# define PY_FROM_STRING PyBytes_FromString
# define PY_LONG_CHECK PyInt_Check
# define PY_LONG_FROM_STRING PyInt_FromString
# define PY_LONG_AS_LONG PyInt_AsLong
#endif


namespace Gui {
namespace Python {
class CodeP
{
public:
    CodeP()
    {  }
    ~CodeP()
    {  }
};

} // namespace Python
} // namespace Gui


using namespace Gui;


Python::Code::Code(QObject *parent) :
    QObject(parent), d(new CodeP)
{
}

Python::Code::~Code()
{
    delete d;
}

/**
 * @brief deepcopys a object, caller takes ownership
 * @param obj to deep copy
 * @return the new obj, borrowed ref
 */
PyObject *Python::Code::deepCopy(PyObject *obj)
{
    Base::PyGILStateLocker lock;
    PyObject *deepCopyPtr = nullptr, *args = nullptr, *result = nullptr;

    // load copy module
    deepCopyPtr = PP_Load_Attribute("copy", "deepcopy");
    if (!deepCopyPtr)
        goto out;

    Py_INCREF(deepCopyPtr);

    // create argument tuple
    args = PyTuple_New(sizeof obj);
    if (!args)
        goto out;

    Py_INCREF(args);

    if (PyTuple_SetItem(args, 0, obj) != 0)
        goto out;

    // call pythons copy.deepcopy
    result = PyObject_CallObject(deepCopyPtr, args);
    if (!result)
        PyErr_Clear();

out:
    Py_XDECREF(deepCopyPtr);
    Py_XDECREF(args);

    return result;
}

// get thee root of the parent identifier ie os.path.join
//                                                    ^
// must traverse from os, then os.path before os.path.join
QString Python::Code::findFromCurrentFrame(const Python::Token *tok)
{
    Base::PyGILStateLocker locker;
    PyFrameObject *frame = App::PythonDebugger::instance()->currentFrame();
    if (frame == nullptr)
        return QString();

    QString foundKey;

    PyObject *obj = nullptr;

    // lookup through parant frames chain
    do {
        PyFrame_FastToLocals(frame);
        obj = getDeepObject(frame->f_locals, tok, foundKey);
    } while (!obj && (frame = frame->f_back));

    // if not found look in globals
    if (!obj)
        obj = getDeepObject(App::PythonDebugger::instance()->currentFrame()->f_globals,
                            tok, foundKey);
    // or in builtins
    if (!obj)
        obj = getDeepObject(App::PythonDebugger::instance()->currentFrame()->f_builtins,
                            tok, foundKey);
    if (!obj)
        return QString();

    // found correct object
    const char *valueStr = nullptr, *reprStr = nullptr, *typeStr = nullptr;
    PyObject *reprObj = nullptr, *valueObj = nullptr;
    // repr this object
    reprObj = PyObject_Repr(obj); // new reference
    reprStr = PY_AS_STRING(reprObj);
    Py_XDECREF(reprObj);

    // value of obj
    valueObj = PyObject_Str(obj); // new reference
    valueStr = PY_AS_STRING(valueObj);
    Py_XDECREF(valueObj);

    // type
    typeStr = Py_TYPE(obj)->tp_name;

    Py_DECREF(obj);

    return QString::fromLatin1("%1=%3\nType:%2\n%4")
                    .arg(foundKey)
                    .arg(QLatin1String(typeStr))
                    .arg(QLatin1String(valueStr))
                    .arg(QLatin1String(reprStr));
}

  /**
 * @brief PythonCode::findObjFromFrame
 * get thee root of the parent identifier ie os.path.join
 *                                                     ^
 *  must traverse from os, then os.path before os.path.join
 *
 * @param obj: Search in obj as a root
 * @param key: name of var to find
 * @return Obj if found or nullptr
 */
PyObject *Python::Code::getDeepObject(PyObject *obj, const Python::Token *needleTok,
                                    QString &foundKey)
{
    DEFINE_DBG_VARS

    Base::PyGILStateLocker locker;
    PyObject *keyObj = nullptr, *tmp = nullptr, *outObj = obj;

    QList<const Python::Token*> chain;
    const Python::Token *tok = needleTok;
    bool lookupSubItm = needleTok->type == Python::Token::T_DelimiterOpenBracket;
    if (lookupSubItm)
        PREV_TOKEN(tok)
    // search up to root ie cls.attr.dict[stringVariable]
    //                               ^
    //                          ^
    //                      ^
    while (tok){
        if (tok->isIdentifierVariable()) {
            chain.prepend(tok);
        } else if (tok->type != Python::Token::T_DelimiterPeriod)
            break;
        PREV_TOKEN(tok)
    }

    if (chain.size() == 0)
        return nullptr;

    for (int i = 0; i < chain.size(); ++i) {
        keyObj = PY_FROM_STRING(chain[i]->text().toLatin1().data());
        if (keyObj != nullptr) {
            Py_INCREF(keyObj);
            do {
                if (PyObject_HasAttr(outObj, keyObj) > 0) {
                    outObj = PyObject_GetAttr(outObj, keyObj);
                    tmp = outObj;
                    Py_XINCREF(tmp);
                } else if (PyDict_Check(outObj) &&
                           PyDict_Contains(outObj, keyObj))
                {
                    outObj = PyDict_GetItem(outObj, keyObj);
                    Py_XDECREF(tmp);
                    tmp = nullptr;
                } else
                    goto out; // bust out as we havent found it

                // if we get here we have found what we want
                if (i == chain.size() -1) {
                    // found the last part
                    foundKey = chain[i]->text();
                }
            } while(false); // bust of 1 time loop

            Py_DECREF(keyObj);
        } else
            break;
    }

    // find the value of this object with subobj as item
    if (lookupSubItm) {
        QString tmp;
        PyObject *needle = nullptr;
        tok = needleTok->next();
        DBG_TOKEN(tok)
        // move to last code before next ']' etc
        while(tok) {
            if (tok->type == Python::Token::T_DelimiterOpenBracket) {
                NEXT_TOKEN(tok)
                needle = getDeepObject(outObj, tok, tmp);
                break;
            } else if (tok->isIdentifierVariable()) {
                needle = getDeepObject(outObj, tok, tmp);
                break;
            } else if (tok->isNumber()) {
                needle = PY_LONG_FROM_STRING(tok->text().toLatin1().data(), nullptr, 0);
                if (!needle)
                    PyErr_Clear();
            } else if (tok->type != Python::Token::T_DelimiterPeriod) {
                break;
            }
            NEXT_TOKEN(tok)
        }

        if (needle) {
            Py_INCREF(needle);
            if (PyDict_Check(outObj)) {
                if (PyDict_Contains(outObj, needle)) {
                    outObj = PyDict_GetItem(outObj, needle);
                }
            } else if (PY_LONG_CHECK(needle)) {
                long idx = PY_LONG_AS_LONG(needle);
                if (idx > -1) {
                    if (PyList_Check(outObj) && PyList_Size(outObj) > idx) {
                        // lookup in list
                        outObj = PyList_GetItem(outObj, idx); // borrowed
                        foundKey += QString::fromLatin1("[%1]").arg(idx);
                    } else if (PyTuple_Check(outObj) && PyTuple_Size(outObj) > idx) {
                        // lookup in tuple
                        outObj = PyTuple_GetItem(outObj, idx); // borrowed
                        foundKey += QString::fromLatin1("[%1]").arg(idx);
                    }

                } else if (PyErr_Occurred())
                    PyErr_Clear();
            }
            Py_DECREF(needle);
        }
    }

out:
    Py_XINCREF(outObj);
    return outObj;
}


#include "moc_PythonCode.cpp"
