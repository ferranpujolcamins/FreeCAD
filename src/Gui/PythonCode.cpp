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

namespace Gui {
struct PythonCodeP
{
    PythonCodeP()
    {  }
    ~PythonCodeP()
    {  }
};

} // namespace Gui


using namespace Gui;


PythonCode::PythonCode(QObject *parent) :
    QObject(parent), d(new PythonCodeP)
{
}

PythonCode::~PythonCode()
{
    delete d;
}

/**
 * @brief deepcopys a object, caller takes ownership
 * @param obj to deep copy
 * @return the new obj
 */
PyObject *PythonCode::deepCopy(PyObject *obj)
{
    Base::PyGILStateLocker lock;

    // load copy module
    PyObject *deepcopyPtr = PP_Load_Attribute("copy", "deepcopy");
    if (!deepcopyPtr)
        return nullptr;

    Py_INCREF(deepcopyPtr);

    // create argument tuple
    PyObject *args = PyTuple_New(sizeof obj);
    if (!args) {
        Py_DECREF(deepcopyPtr);
        return nullptr;
    }

    Py_INCREF(args);

    if (PyTuple_SetItem(args, 0, (PyObject*)obj) != 0) {
        Py_DECREF(args);
        Py_DECREF(deepcopyPtr);
    }

    // call pythons copy.deepcopy
    PyObject *result = PyObject_CallObject(deepcopyPtr, args);
    if (!result) {
        PyErr_Clear();
    }

    return result;
}

// get thee root of the parent identifier ie os.path.join
//                                                    ^
// must traverse from os, then os.path before os.path.join
QString PythonCode::findFromCurrentFrame(QString varName)
{
    Base::PyGILStateLocker locker;
    PyFrameObject *current_frame = App::PythonDebugger::instance()->currentFrame();
    if (current_frame == 0)
        return QString();

    QString foundKey;

    PyFrame_FastToLocals(current_frame);
    Py::Object obj = getDeepObject(current_frame->f_locals, varName, foundKey);
    if (obj.isNull())
        obj = getDeepObject(current_frame->f_globals, varName, foundKey);

    if (obj.isNull())
        obj = getDeepObject(current_frame->f_builtins, varName, foundKey);

    if (obj.isNull()) {
        return QString();
    }

    return QString(QLatin1String("%1:%3\nType:%2\n%4"))
            .arg(foundKey)
            .arg(QString::fromStdString(obj.type().as_string()))
            .arg(QString::fromStdString(obj.str().as_string()))
            .arg(QString::fromStdString(obj.repr().as_string()));

//    // found correct object
//    const char *valueStr, *reprStr, *typeStr;
//    PyObject *repr_obj;
//    repr_obj = PyObject_Repr(obj);
//    typeStr = Py_TYPE(obj)->tp_name;
//    obj = PyObject_Str(obj);
//    valueStr = PyBytes_AS_STRING(obj);
//    reprStr = PyBytes_AS_STRING(repr_obj);
//    return QString(QLatin1String("%1:%3\nType:%2\n%4"))
//                .arg(foundKey)
//                .arg(QLatin1String(typeStr))
//                .arg(QLatin1String(valueStr))
//                .arg(QLatin1String(reprStr));
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
Py::Object PythonCode::getDeepObject(PyObject *obj, QString key, QString &foundKey)
{
    Base::PyGILStateLocker locker;
    PyObject *keyObj = nullptr;
    PyObject *tmp = nullptr;

    QStringList parts = key.split(QLatin1Char('.'));
    if (!parts.size())
        return Py::Object();

    for (int i = 0; i < parts.size(); ++i) {
        keyObj = PyBytes_FromString(parts[i].toLatin1());
        if (keyObj != nullptr) {
            do {
                if (PyObject_HasAttr(obj, keyObj) > 0) {
                    obj = PyObject_GetAttr(obj, keyObj);
                    tmp = obj;
                    Py_XINCREF(tmp);
                } else if (PyDict_Check(obj) &&
                           PyDict_Contains(obj, keyObj))
                {
                    obj = PyDict_GetItem(obj, keyObj);
                    Py_XDECREF(tmp);
                    tmp = nullptr;
                } else
                    break; // bust out as we havent found it

                // if we get here we have found what we want
                if (i == parts.size() -1) {
                    // found the last part
                    foundKey = parts[i];
                    return Py::Object(obj);
                } else {
                    continue;
                }

            } while(false); // bust of 1 time loop
        } else {
            return Py::Object();
        }
    }
    return Py::Object();
}


// -------------------------------------------------------------------------------





// Begin Python Code AST methods (allthough no tree as we constantly keep changing src in editor)
// ------------------------------------------------------------------------

PythonSourceRoot::PythonSourceRoot() :
    m_uniqueCustomTypeNames(-1),
    m_modules(nullptr)
{
    m_modules = new PythonSourceModuleList();
}

PythonSourceRoot::~PythonSourceRoot()
{
    delete m_modules;
}

// static
PythonSourceRoot *PythonSourceRoot::instance()
{
    if (m_instance == nullptr)
        m_instance = new PythonSourceRoot;
    return m_instance;

}

QStringList PythonSourceRoot::modulesNames() const
{
    QStringList ret;
    if (!m_modules)
        return ret;

    for(PythonSourceListNodeBase *itm = m_modules->begin();
        itm != m_modules->end();
        itm->next())
    {
        PythonSourceModule *module = dynamic_cast<PythonSourceModule*>(itm);
        if (module)
            ret << module->moduleName();
    }

    return ret;
}

QStringList PythonSourceRoot::modulesPaths() const
{
    QStringList ret;

    for(PythonSourceListNodeBase *itm = m_modules->begin();
        itm != m_modules->end();
        itm->next())
    {
        PythonSourceModule *module = dynamic_cast<PythonSourceModule*>(itm);
        if (module)
            ret << module->filePath();
    }

    return ret;
}

int PythonSourceRoot::modulesCount() const
{
    return m_modules->size();
}

PythonSourceModule *PythonSourceRoot::moduleAt(int idx) const
{
    if (idx < 0)
        idx = m_modules->size() - idx; // reverse so -1 becomes last
    int i = 0;
    PythonSourceListNodeBase *mod = m_modules->begin();
    while(mod) {
        if (++i == idx)
            return dynamic_cast<PythonSourceModule*>(mod);
        mod = mod->next();
    }

    return nullptr;

}

PythonSourceModule *PythonSourceRoot::moduleFromPath(QString filePath) const
{
    for (PythonSourceListNodeBase *itm = m_modules->begin();
         itm != m_modules->end();
         itm->next())
    {
        PythonSourceModule *module = dynamic_cast<PythonSourceModule*>(itm);
        if (module && module->filePath() == filePath)
            return module;
    }

    return nullptr;
}

PythonSourceRoot *PythonSourceRoot::m_instance = nullptr;

PythonSourceRoot::DataTypes PythonSourceRoot::mapDataType(const QString typeAnnotation) const
{
    // try to determine type form type annotaion
    // more info at:
    // https://www.python.org/dev/peps/pep-0484/
    // https://www.python.org/dev/peps/pep-0526/
    // https://mypy.readthedocs.io/en/latest/cheat_sheet_py3.html
    if (typeAnnotation == QLatin1String("none"))
        return NoneType;
    else if (typeAnnotation == QLatin1String("bool"))
        return BoolType;
    else if (typeAnnotation == QLatin1String("int"))
        return IntType;
    else if (typeAnnotation == QLatin1String("float"))
        return FloatType;
    else if (typeAnnotation == QLatin1String("str"))
        return StringType;
    else if (typeAnnotation == QLatin1String("bytes"))
        return BytesType;
    else if (typeAnnotation == QLatin1String("List"))
        return ListType;
    else if (typeAnnotation == QLatin1String("Tuple"))
        return TupleType;
    else if (typeAnnotation == QLatin1String("Dict"))
        return DictType;
    else if (typeAnnotation == QLatin1String("Set"))
        return SetType;
    return CustomType;
}

QString PythonSourceRoot::customTypeNameFor(PythonSourceRoot::CustomNameIdx_t customIdx)
{
    return m_customTypeNames[customIdx];
}

PythonSourceRoot::CustomNameIdx_t PythonSourceRoot::addCustomTypeName(const QString name)
{
    m_customTypeNames[++m_uniqueCustomTypeNames] = name;
    return m_uniqueCustomTypeNames;
}

PythonSourceRoot::CustomNameIdx_t PythonSourceRoot::indexOfCustomTypeName(const QString name)
{
    if (m_customTypeNames.values().contains(QString(name)))
        return m_customTypeNames.key(QString(name));
    return -1;
}

PythonSourceModule *PythonSourceRoot::scanCompleteModule(const QString filePath,
                                                         PythonSyntaxHighlighter *highlighter)
{
    // delete old data
    PythonSourceModule *mod = moduleFromPath(filePath);
    if (mod) {
        m_modules->remove(mod, true);
        mod = nullptr;
    }

    mod = new PythonSourceModule(this, highlighter);
    mod->setFilePath(filePath);
    QFileInfo fi(filePath);
    mod->setModuleName(fi.baseName());

    // find the first token of any kind
    // firstline might be empty
    const PythonToken *tok = dynamic_cast<PythonTextBlockData*>(
                             highlighter->document()->begin().userData())
                                    ->tokenAt(0);
    if (tok)
        tok = mod->rootFrame()->scanFrame(const_cast<PythonToken*>(tok));

    return mod;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// this struct is contained by PythonSourceRoot
PythonSourceRoot::TypeInfo::TypeInfo() :
    type(PythonSourceRoot::InValidType),
    customNameIdx(-1)
{
}

PythonSourceRoot::TypeInfo::TypeInfo(const PythonSourceRoot::TypeInfo &other) :
    type(other.type),
    customNameIdx(other.customNameIdx),
    referenceName(other.referenceName)
{
}


PythonSourceRoot::TypeInfo::~TypeInfo()
{
}

QString PythonSourceRoot::TypeInfo::typeAsStr() const
{
    const char *typeAsStr;
    switch (type) {
    case PythonSourceRoot::InValidType:  typeAsStr = "InValidType";  break;
    case PythonSourceRoot::NoneType:     typeAsStr = "NoneType";     break;
    case PythonSourceRoot::BoolType:     typeAsStr = "BoolType";     break;
    case PythonSourceRoot::IntType:      typeAsStr = "IntType";      break;
    case PythonSourceRoot::FloatType:    typeAsStr = "FloatType";    break;
    case PythonSourceRoot::StringType:   typeAsStr = "StringType";   break;
    case PythonSourceRoot::BytesType:    typeAsStr = "BytesType";    break;
    case PythonSourceRoot::ListType:     typeAsStr = "ListType";     break;
    case PythonSourceRoot::TupleType:    typeAsStr = "TupleType";    break;
    case PythonSourceRoot::SetType:      typeAsStr = "SetType";      break;
    case PythonSourceRoot::RangeType:    typeAsStr = "RangeType";    break;
    case PythonSourceRoot::DictType:     typeAsStr = "DictType";     break;
    case PythonSourceRoot::FunctionType: typeAsStr = "FunctionType"; break;
    case PythonSourceRoot::ClassType:    typeAsStr = "ClassType";    break;
    case PythonSourceRoot::MethodType:   typeAsStr = "MethodType";   break;
    case PythonSourceRoot::CustomType:   typeAsStr = "CustomType";   break;
    default:
        typeAsStr = "InValidType";
    }

    return QLatin1String(typeAsStr);
}

QString PythonSourceRoot::TypeInfo::customName() const
{
    if (customNameIdx > -1) {

    }
    return QString();
}

// ------------------------------------------------------------------------


PythonSourceListNodeBase::PythonSourceListNodeBase(PythonSourceListParentBase *owner) :
    m_previous(nullptr), m_next(nullptr),
    m_owner(owner), m_token(nullptr)
{
    assert(owner != nullptr && "Must have valid owner");
}

PythonSourceListNodeBase::~PythonSourceListNodeBase()
{
    if (m_token)
        const_cast<PythonToken*>(m_token)->detachReference(this);
    if (m_owner)
        m_owner->remove(this);
}

QString PythonSourceListNodeBase::text() const
{
    if (m_token)
        return m_token->text();
    return QString();
}

// this should only be called from PythonToken destructor when it gets destroyed
void PythonSourceListNodeBase::tokenDeleted()
{
    m_token = nullptr;
    if (m_owner)
        m_owner->remove(this, true); // remove me from owner and let him delete me
}

// -------------------------------------------------------------------------


PythonSourceListParentBase::PythonSourceListParentBase(PythonSourceListParentBase *owner) :
    PythonSourceListNodeBase(owner),
    m_first(nullptr), m_last(nullptr),
    m_preventAutoRemoveMe(false)
{
}

PythonSourceListParentBase::~PythonSourceListParentBase()
{
    // delete all children
    PythonSourceListNodeBase *n = m_first,
                             *tmp = nullptr;
    while(n) {
        n->setOwner(nullptr);
        tmp = n;
        n = n->next();
        delete tmp;
    }
}

void PythonSourceListParentBase::insert(PythonSourceListNodeBase *node)
{
    assert(node->next() == nullptr && "Node already owned");
    assert(node->previous() == nullptr && "Node already owned");
    if (m_last) {
        // look up the place for this
        PythonSourceListNodeBase *n = m_last,
                                 *tmp = nullptr;
        while (n != nullptr) {
            int res = compare(node, n);
            if (res < 0) {
                // n is less than node, inert after n
                tmp = n->next();
                n->setNext(node);
                node->setPrevious(n);
                if (tmp) {
                    tmp->setPrevious(node);
                    node->setNext(tmp);
                } else {
                    node->setNext(nullptr);
                    m_last = node;
                    break;
                }
            } else if (n->previous() == nullptr) {
                // n is more than node and we are at the beginning (won't iterate further)
                node->setNext(n);
                n->setPrevious(node);
                m_first = node;
                node->setPrevious(nullptr);
                break;
            }
            n = n->previous();
        }
    } else {
        m_first = node;
        m_last = node;
    }
}

bool PythonSourceListParentBase::remove(PythonSourceListNodeBase *node, bool deleteNode)
{
    if (contains(node)) {
        // remove from list
        if (node->previous())
            node->previous()->setNext(node->next());
        if (node->next())
            node->next()->setPrevious(node->previous());
        if (m_first == node)
            m_first = node->next();
        if (m_last == node)
            m_last = node->previous();

        node->setNext(nullptr);
        node->setPrevious(nullptr);
        if (deleteNode)
            delete node;

        // if we are empty we should remove ourselfs
        if (m_first == nullptr && m_last == nullptr &&
            m_owner != this && !m_preventAutoRemoveMe)
        {
            m_owner->remove(this, true);
        }
        return true;

    }

    return false;
}

bool PythonSourceListParentBase::contains(PythonSourceListNodeBase *node) const
{
    PythonSourceListNodeBase *f = m_first,
                             *l = m_last;
    // we search front and back to be 1/2 n at worst lookup time
    while(f && l) {
        if (f == node || l == node)
            return true;
        f = f->next();
        l = l->previous();
    }

    return false;
}

bool PythonSourceListParentBase::empty() const
{
    return m_first == nullptr && m_last == nullptr;
}

std::size_t PythonSourceListParentBase::size() const
{
    PythonSourceListNodeBase *f = m_first;
    std::size_t cnt = -1;
    while(f) {
        ++cnt;
        f = f->next();
    }

    return cnt;
}

std::size_t PythonSourceListParentBase::indexOf(PythonSourceListNodeBase *node) const
{
    std::size_t idx = -1;
    PythonSourceListNodeBase *f = m_first;
    while (f) {
        ++idx;
        if (f == node)
            break;
        f = f->next();
    }

    return idx;
}

PythonSourceListNodeBase *PythonSourceListParentBase::operator [](std::size_t idx) const
{
    PythonSourceListNodeBase *n = m_first;
    for (std::size_t i = 0; i < idx; ++i) {
        if (n == nullptr)
            return nullptr;
        n = n->next();
    }
    return n;
}

int PythonSourceListParentBase::compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const
{
    if (!left || ! right)
        return 0;
    if (left->token() > right->token())
        return -1;
    else if (left->token() < right->token())
        return +1;
    return 0;
}

// ------------------------------------------------------------------------


PythonSourceIdentifierAssignment::PythonSourceIdentifierAssignment(PythonSourceIdentifier *owner,
                                                                   const PythonToken *token,
                                                                   PythonSourceRoot::TypeInfo typeInfo) :
    PythonSourceListNodeBase(owner),
    m_linenr(-1)
{
    m_type = typeInfo;
    assert(token != nullptr && "Must have valid token");
    m_token = token;
    const_cast<PythonToken*>(m_token)->attachReference(this);
}

PythonSourceIdentifierAssignment::~PythonSourceIdentifierAssignment()
{
}

int PythonSourceIdentifierAssignment::linenr()
{
    if (m_linenr < 0) {
        m_linenr = m_token->line();
    }
    return m_linenr;
}

// ------------------------------------------------------------------------

PythonSourceIdentifier::PythonSourceIdentifier(PythonSourceListParentBase *owner, PythonSourceFrame *frame) :
    PythonSourceListParentBase(owner),
    m_frame(frame)
{
    assert(m_frame != nullptr && "Must have a valid frame");
}

PythonSourceIdentifier::~PythonSourceIdentifier()
{
}

int PythonSourceIdentifier::compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const
{
    // should sort by linenr and (if same line also token startpos)
    PythonSourceIdentifierAssignment *l = dynamic_cast<PythonSourceIdentifierAssignment*>(left),
                                     *r = dynamic_cast<PythonSourceIdentifierAssignment*>(right);
    assert(l != nullptr && r != nullptr && "PythonSourceIdentifier stored non compatible node");
    if (l->linenr() > r->linenr()) {
        return -1;
    } else if (l->linenr() < r->linenr()) {
        return +1;
    } else {
        // line nr equal
        if (l->token()->startPos > r->token()->startPos)
            return -1;
        else
            return +1; // can't be at same source position
    }
}

PythonSourceIdentifierAssignment *PythonSourceIdentifier::getFromPos(int line, int pos) const
{
    PythonSourceIdentifierAssignment *l = dynamic_cast<PythonSourceIdentifierAssignment*>(m_last);
    while(l) {
        if (l->linenr() == line && l->token()->endPos < pos) {
            return l;
        } else if (l->linenr() < line) {
            return l;
        }
        l = dynamic_cast<PythonSourceIdentifierAssignment*>(l->previous());
    }
    // not found
    return nullptr;
}

PythonSourceIdentifierAssignment *PythonSourceIdentifier::getFromPos(const PythonToken *tok) const
{
    return getFromPos(tok->line(), tok->startPos);
}

QString PythonSourceIdentifier::name() const
{
    if (m_first)
        return m_first->text();

    return QLatin1String("<lookup error>");
}

// -------------------------------------------------------------------------------


PythonSourceIdentifierList::PythonSourceIdentifierList(PythonSourceFrame *frame):
    PythonSourceListParentBase(frame),
    m_frame(frame)
{
    m_preventAutoRemoveMe = true;
    assert(frame != nullptr && "Must have a valid frame");
}

PythonSourceIdentifierList::~PythonSourceIdentifierList()
{
}

const PythonSourceIdentifier *PythonSourceIdentifierList::getIdentifier(const QString name) const
{
    PythonSourceIdentifier *i = dynamic_cast<PythonSourceIdentifier*>(m_first);
    while (i) {
        if (i->name() == name)
            return i;
        i = dynamic_cast<PythonSourceIdentifier*>(i->next());
    }
    return nullptr;
}

PythonSourceIdentifier *PythonSourceIdentifierList::setIdentifier(const PythonToken *tok,
                                                                  PythonSourceRoot::TypeInfo typeInfo)
{
    assert(tok && "Expected a valid pointer");
    QString name = tok->text();

    PythonSourceIdentifier *identifier = nullptr;
    PythonSourceIdentifierAssignment *assign = nullptr;

    // find in our list
    for(PythonSourceListNodeBase *itm = begin();
        itm != nullptr;
        itm = itm->next())
    {
        PythonSourceIdentifier *ident = dynamic_cast<PythonSourceIdentifier*>(itm);
        if (ident && ident->name() == name) {
            identifier = ident;
            break;
        }
    }

    // new indentifier
    if (!identifier) {
        identifier = new PythonSourceIdentifier(this, m_frame);
        insert(identifier);
    }

    // check so we don't double insert
    for (PythonSourceListNodeBase *itm = identifier->begin();
         itm != nullptr;
         itm = itm->next())
    {
        if (itm->token() == tok) {
            assign = dynamic_cast<PythonSourceIdentifierAssignment*>(itm);
            if (assign->type() != typeInfo)
                assign->setType(typeInfo); // type differ
            else
                return identifier; // already have this one and it is equal
        }
    }

    // create new assigment
    assign = new PythonSourceIdentifierAssignment(identifier, tok, typeInfo);
    identifier->insert(assign);
    return identifier;
}

int PythonSourceIdentifierList::compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const
{
    PythonSourceIdentifier *l = dynamic_cast<PythonSourceIdentifier*>(left),
                           *r = dynamic_cast<PythonSourceIdentifier*>(right);
    assert(l != nullptr && r != nullptr && "PythonSourceIdentifiers contained invalid nodes");
    if (l->name() > r->name())
        return -1;
    else if (r->name() > l->name())
        return +1;
    return 0;
}

// ----------------------------------------------------------------------------


PythonSourceParameter::PythonSourceParameter(PythonSourceParameterList *parent) :
    PythonSourceListNodeBase(parent),
    m_paramType(InValid)
{
}

PythonSourceParameter::~PythonSourceParameter()
{
}

PythonSourceIdentifierAssignment *PythonSourceParameter::identifierAssignment() const
{
    // lookup with same token
    PythonSourceFrame *frm = dynamic_cast<PythonSourceFrame*>(m_owner);
    assert(frm != nullptr && "Expected a PythonSourceFrame stored in PythonSourceParameter");
    for (PythonSourceListNodeBase *ident = frm->identifiers().begin();
         ident != nullptr;
         ident = ident->next())
    {
        if (ident->token() == m_token)
            return dynamic_cast<PythonSourceIdentifierAssignment*>(ident);
    }

    return nullptr;
}

// -----------------------------------------------------------------------------


PythonSourceParameterList::PythonSourceParameterList(PythonSourceFrame *frame) :
    PythonSourceListParentBase(frame),
    m_frame(frame)
{
    m_preventAutoRemoveMe = true;
}

PythonSourceParameterList::~PythonSourceParameterList()
{
}

const PythonSourceParameter *PythonSourceParameterList::getParameter(const QString name) const
{
    for (PythonSourceListNodeBase *itm = m_first;
         itm != nullptr;
         itm->next())
    {
        if (itm->text() == name)
            return dynamic_cast<PythonSourceParameter*>(itm);
    }
    return nullptr;
}

PythonSourceParameter *PythonSourceParameterList::setParameter(const PythonToken *tok,
                                                                PythonSourceRoot::TypeInfo typeInfo,
                                                                PythonSourceParameter::ParameterType paramType)
{
    assert(tok && "Expected a valid pointer");
    QString name = tok->text();

    PythonSourceParameter *parameter = nullptr;

    // find in our list
    for(PythonSourceListNodeBase *itm = begin();
        itm != nullptr;
        itm = itm->next())
    {
        PythonSourceParameter *param = dynamic_cast<PythonSourceParameter*>(itm);
        if (param->text() == name) {
            parameter = param;
            break;
        }
    }

    // create new parameter
    if (!parameter) {
        parameter = new PythonSourceParameter(this);
        insert(parameter);
    }

    if (parameter->type() != typeInfo)
        parameter->setType(typeInfo); // type differ
    if (parameter->parameterType() != paramType)
        parameter->setParameterType(paramType);

   return parameter; // already have this one and it is equal
}

int PythonSourceParameterList::compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const
{
    PythonSourceParameter *l = dynamic_cast<PythonSourceParameter*>(left),
                         *r = dynamic_cast<PythonSourceParameter*>(right);
    assert(l != nullptr && r != nullptr && "Non PythonSourceArgument items in agrmumentslist");
    if (l->token() < r->token())
        return +1;
    return -1; // r must be bigger
}

// ----------------------------------------------------------------------------


PythonSourceImportModule::PythonSourceImportModule(PythonSourceImportPackage *parent,
                                                   PythonSourceFrame *frame,
                                                   QString alias) :
    PythonSourceListNodeBase(parent),
    m_frame(frame), m_aliasName(alias)
{
}

PythonSourceImportModule::~PythonSourceImportModule()
{
}

QString PythonSourceImportModule::name() const
{
    if (!m_aliasName.isEmpty())
        return m_aliasName;

    QFileInfo fi(m_modulePath);
    return fi.baseName();
}

void PythonSourceImportModule::load()
{
    // FIXME implement this in PythonSourceRoot as many opened files might require the same module
}

bool PythonSourceImportModule::isBuiltIn() const
{
    // FIXME implement this in PythonSourceRoot as many opened files might require the same module
    return false;
}

// -----------------------------------------------------------------------------

PythonSourceImportPackage::PythonSourceImportPackage(PythonSourceImportPackage *parent,
                                                     QString name,
                                                     PythonSourceModule *ownerModule) :
    PythonSourceListParentBase(parent),
    m_name(name),
    m_ownerModule(ownerModule)
{
    // FIXME resolve path in PythonSourceRoot
    m_packagePath = name;
}

PythonSourceImportPackage::~PythonSourceImportPackage()
{
}

PythonSourceImportPackage *PythonSourceImportPackage::getImportPackagePath(QString filePath)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportPackage *pkg = dynamic_cast<PythonSourceImportPackage*>(itm);
        if (pkg && pkg->path() == filePath)
            return pkg;
    }

    return nullptr;
}

PythonSourceImportPackage *PythonSourceImportPackage::getImportPackage(QString name)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportPackage *pkg = dynamic_cast<PythonSourceImportPackage*>(itm);
        if (pkg && pkg->name() == name)
            return pkg;
    }

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportPackage::getImportModulePath(QString filePath)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportModule *mod = dynamic_cast<PythonSourceImportModule*>(itm);
        if (mod && mod->path() == filePath)
            return mod;
    }

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportPackage::getImportModule(QString name)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportModule *mod = dynamic_cast<PythonSourceImportModule*>(itm);
        if (mod && mod->name() == name)
            return mod;
    }

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportPackage::setModule(QString name,
                                                               QString alias,
                                                               PythonSourceFrame *frame)
{
    QString importName = alias.isEmpty() ? name : alias;
    PythonSourceImportModule *mod = getImportModule(importName);

    if (mod)
        return mod;

    // not yet in list
    mod = new PythonSourceImportModule(this, frame, alias);
    insert(mod);
    return mod;
}

PythonSourceImportPackage *PythonSourceImportPackage::setPackage(QString name)
{
    PythonSourceImportPackage *pkg = getImportPackage(name);
    if (pkg)
        return pkg;

    pkg = new PythonSourceImportPackage(this, name, m_ownerModule);
    insert(pkg);
    return pkg;
}

int PythonSourceImportPackage::compare(PythonSourceListNodeBase *left,
                                       PythonSourceListNodeBase *right) const
{
    QString lName, rName;
    PythonSourceImportModule *lm = dynamic_cast<PythonSourceImportModule*>(left),
                             *rm = dynamic_cast<PythonSourceImportModule*>(right);

    PythonSourceImportPackage *lp = dynamic_cast<PythonSourceImportPackage*>(left),
                              *rp = dynamic_cast<PythonSourceImportPackage*>(right);
    if (lm)
        lName = lm->name();
    else if(lp)
        lName = lp->name();
    else
        assert("Invalid state, non module or package stored (left)" == nullptr);

    if (rm)
        rName = rm->name();
    else if (rp)
        rName = rp->name();
    else
        assert("Invalid state, non module or package stored (right)" == nullptr);

    if (lName < rName)
        return +1;
    else if (lName > rName)
        return -1;
    return 0;

}

// ----------------------------------------------------------------------------


PythonSourceImportList::PythonSourceImportList(PythonSourceFrame *owner,
                                               PythonSourceModule *ownerModule) :
    PythonSourceImportPackage(this, QLatin1String(""), ownerModule),
    m_frame(owner)
{
    m_preventAutoRemoveMe = true;
}

PythonSourceImportList::~PythonSourceImportList()
{
}

PythonSourceImportModule *PythonSourceImportList::getImportModulePath(QString filePath)
{
    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    dir.cdUp();
    return getImportModule(dir.dirName(), fi.baseName());
}

PythonSourceImportModule *PythonSourceImportList::getImportModule(QString rootPackage, QString name)
{
    PythonSourceImportPackage *pkg = getImportPackage(QString(), rootPackage);
    if (pkg)
        return pkg->getImportModule(name);

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportList::getImportModule(QStringList modInheritance)
{
    if (empty() || modInheritance.size() < 1)
        return nullptr;

    QString modName = modInheritance[modInheritance.size() -1];

    if (modInheritance.size() < 2)
        return getImportModule(QString(), modName);

    // get root package by slicing QStringList
    PythonSourceImportPackage *pkg = getImportPackage(QString(), modInheritance.last());
    if (pkg)
        return pkg->getImportModule(modName);

    return nullptr;
}

PythonSourceImportPackage *PythonSourceImportList::getImportPackagePath(QString filePath)
{
    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    dir.cdUp();
    return getImportPackage(dir.dirName(), fi.baseName());
}

PythonSourceImportPackage *PythonSourceImportList::getImportPackage(QString rootPackage, QString name)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportPackage *pkg = dynamic_cast<PythonSourceImportPackage*>(itm);
        // lookup among packages, all modules are stored in packages
        if (pkg) {
            if (!rootPackage.isEmpty()) {
                if (rootPackage == pkg->name())
                    return pkg->getImportPackage(name);
            } else {
                return pkg;
            }

        }
    }

    return nullptr;
}

PythonSourceImportPackage *PythonSourceImportList::getImportPackage(QStringList modInheritance)
{
    if (empty() || modInheritance.size() < 1)
        return nullptr;

    // single name like import sys
    if (modInheritance.size() == 1)
        return getImportPackage(QString(), modInheritance[0]);

    // package.module.... like import sys.path.join
    PythonSourceImportPackage *pkg = nullptr;
    QString curName = modInheritance.takeLast();

    // first find our root package
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        pkg = dynamic_cast<PythonSourceImportPackage*>(itm);
        if (pkg && pkg->name() == curName) {
            // found our root package
            while(pkg &&
                  !(curName = modInheritance.takeLast()).isEmpty() && // note! action here
                  !curName.isEmpty())
            {
                if (modInheritance.size() == 0)
                    return pkg->getImportPackage(curName);
                pkg = pkg->getImportPackage(curName);
            }

        }
    }

    return nullptr;

}

PythonSourceImportModule *PythonSourceImportList::setModule(QStringList rootPackage,
                                                            QString module, QString alias)
{
    PythonSourceImportModule *mod = nullptr;
    PythonSourceImportPackage *rootPkg = getImportPackage(rootPackage);

    if (!rootPkg)
        rootPkg = setPackage(rootPackage);

    if (rootPkg) {
        mod = rootPkg->getImportModule(alias.isEmpty() ? module : alias);
        if (mod)
            return mod;

        // we have package but not module
        return rootPkg->setModule(module, alias, m_frame);
    }

    // an error occured
    return nullptr;
}

PythonSourceImportPackage *PythonSourceImportList::setPackage(QStringList rootPackage)
{
    PythonSourceImportPackage *rootPkg = nullptr;

    // no rootPackages
    if (rootPackage.size() < 1) {
       rootPkg = PythonSourceImportPackage::setPackage(QLatin1String(""));
       return rootPkg;
    }

    // lookup package
    rootPkg = getImportPackage(rootPackage);

    if (!rootPkg) {
        // we doesn't have rootPackage yet, recurse until we find it
        rootPkg = setPackage(rootPackage.mid(0, rootPackage.size() -1));
        return rootPkg;
    }

    return nullptr;
}

PythonSourceModule *PythonSourceImportList::setModuleGlob(QStringList rootPackage)
{
    // FIXME implement
    Q_UNUSED(rootPackage);
    return nullptr;
}

// -----------------------------------------------------------------------------

PythonSourceFrame::PythonSourceFrame(PythonSourceFrame *owner,
                                     PythonSourceModule *module,
                                     PythonSourceFrame *parentFrame,
                                     bool isClass):
    PythonSourceListParentBase(owner),
    m_identifiers(this),
    m_parameters(this),
    m_imports(this, module),
    m_returns(nullptr),
    m_parentFrame(parentFrame),
    m_module(module),
    m_isClass(isClass),
    lastToken(nullptr)
{
}

PythonSourceFrame::PythonSourceFrame(PythonSourceModule *owner,
                                     PythonSourceModule *module,
                                     PythonSourceFrame *parentFrame,
                                     bool isClass):
    PythonSourceListParentBase(owner),
    m_identifiers(this),
    m_parameters(this),
    m_imports(this, module),
    m_returns(nullptr),
    m_parentFrame(parentFrame),
    m_module(module),
    m_isClass(isClass),
    lastToken(nullptr)
{
}

PythonSourceFrame::~PythonSourceFrame()
{
    delete m_returns;
}

QString PythonSourceFrame::docstring()
{
    // retrive docstring for this function
    QStringList docStrs;
    if (m_token) {
        const PythonToken *token = endOfParametersList(m_token);
        int guard = 20;
        while(token && token->token != PythonSyntaxHighlighter::T_DelimiterSemiColon) {
            if ((--guard) <= 0)
                return QString();
            token = token->next();
        }

        if (!token)
            return QString();

        // now we are at semicolon (even with typehint)
        //  def func(arg1,arg2) -> str:
        //                            ^
        // find first string, before other stuff
        while (token) {
            if (!token->txtBlock())
                continue;
            switch (token->token) {
            case PythonSyntaxHighlighter::T_LiteralBlockDblQuote:// fallthrough
            case PythonSyntaxHighlighter::T_LiteralBlockSglQuote: {
                // multiline
                QString tmp = token->text();
                docStrs << tmp.mid(3, tmp.size() - 6); // clip """
            }   break;
            case PythonSyntaxHighlighter::T_LiteralDblQuote: // fallthrough
            case PythonSyntaxHighlighter::T_LiteralSglQuote: {
                // single line
                QString tmp = token->text();
                docStrs << tmp.mid(1, tmp.size() - 2); // clip "
            }   break;
            case PythonSyntaxHighlighter::T_Indent: // fallthrough
            case PythonSyntaxHighlighter::T_Comment:
                token = token->next();
                break;
            default:
                // some other token, probably some code
                token = nullptr; // bust out of while
            }
        }

        // return what we have collected
        return docStrs.join(QLatin1String("\n"));
    }

    return QString(); // no token
}

PythonSourceRoot::TypeInfo PythonSourceFrame::returnTypeHint()
{
    PythonSourceRoot::TypeInfo tpInfo;
    if (m_token) {
        const PythonToken *token = endOfParametersList(m_token),
                          *commentToken = nullptr;
        int guard = 10;
        while(token && token->token != PythonSyntaxHighlighter::T_DelimiterMetaData) {
            if ((--guard) <= 0)
                return tpInfo;

            if (token->token == PythonSyntaxHighlighter::T_DelimiterSemiColon) {
                // no metadata
                // we may still have type hint in a comment in row below
                while (token && token->token == PythonSyntaxHighlighter::T_Indent) {
                    token = token->next();
                    if (token && token->token == PythonSyntaxHighlighter::T_Comment) {
                        commentToken = token;
                        token = nullptr; // bust out of loops
                    }
                }

                if (!commentToken)
                    return tpInfo; // have no metadata, might have commentdata
            } else
                token = token->next();
        }

        QString annotation;

        if (token) {
            // now we are at metadata token
            //  def func(arg1: int, arg2: bool) -> MyType:
            //                                  ^
            // step one more further
            token = token->next();
            if (token)
                annotation = token->text();

        } else if (commentToken) {
            // extract from comment
            // type: def func(int, bool) -> MyType
            QRegExp re(QLatin1String("type\\s*:\\s*def\\s+[_a-zA-Z]+\\w*\\(.*\\)\\s*->\\s*(\\w+)"));
            if (re.indexIn(token->text()) > -1)
                annotation = re.cap(1);
        }

        if (!annotation.isEmpty()) {
            // set type and customname
            PythonSourceRoot *sr = PythonSourceRoot::instance();
            tpInfo.type = sr->mapDataType(annotation);
            if (tpInfo.type == PythonSourceRoot::CustomType) {
                tpInfo.customNameIdx = sr->indexOfCustomTypeName(annotation);
                if (tpInfo.customNameIdx < 0)
                    tpInfo.customNameIdx = sr->addCustomTypeName(annotation);
            }
        }
    }

    return tpInfo;
}

PythonToken *PythonSourceFrame::scanFrame(PythonToken *startToken)
{
    if (!startToken)
        return startToken;

    PythonToken *tok = startToken;

    // set initial start token
    setToken(startToken);

    //int rootIndentChrs = tok->txtBlock()->indent();

    int firstRowIndent = 0,
        currentBlockIndent = 0,
        previousBlockIndent = 0;

    // scan parameterLists
    if (m_parentFrame && !isClass()) {
        // not rootFrame, parse argumentslist
        tok = const_cast<PythonToken*>(scanParameter(tok));
        // get indent of first line after newline
        int newLines = 0;
        while(tok && newLines < 2) {
            switch (tok->token){
            case PythonSyntaxHighlighter::T_DelimiterLineContinue: break; // don't count newline
            case PythonSyntaxHighlighter::T_DelimiterColon:
                tok->txtBlock()->insertToken(PythonSyntaxHighlighter::T_BlockStart, tok->startPos, 0);
                break;
            case PythonSyntaxHighlighter::T_DelimiterNewLine:
                ++newLines;
                break;
            case PythonSyntaxHighlighter::T_Indent:
            case PythonSyntaxHighlighter::T_Comment:
                break; // bail these tokens
            default:
                if (newLines > 0) {
                    firstRowIndent = tok->txtBlock()->indent();
                    previousBlockIndent = currentBlockIndent = firstRowIndent;
                    ++newLines; // exit while
                }
            }
        }
    }

    int guard = 1000000; // max number or tokens, unhang locked loop
    // scan document
    while (tok && (guard--)) {
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_IdentifierDefUnknown: {
            // a function that can be function or method
            // determine what it is
            if (m_isClass && firstRowIndent == tok->txtBlock()->indent()) {
                tok->token = PythonSyntaxHighlighter::T_IdentifierMethod;
                m_module->highlighter()->setFormatToken(tok);
                goto doFunction;
            }
            tok->token = PythonSyntaxHighlighter::T_IdentifierFunction;
            m_module->highlighter()->setFormatToken(tok);
            goto doMethod;
        } break;
        case PythonSyntaxHighlighter::T_IdentifierClass: {
            // store this as a class identifier
            PythonSourceRoot::TypeInfo typeInfo;
            typeInfo.type = PythonSourceRoot::DataTypes::ClassType;
            m_identifiers.setIdentifier(tok, typeInfo);

            // add class frame
            PythonSourceFrame *clsFrm = new PythonSourceFrame(this, m_module, this, true);
            tok = clsFrm->scanFrame(tok);
            insert(clsFrm);
        } break;
        case PythonSyntaxHighlighter::T_IdentifierFunction: {
            // store this as a function identifier
doFunction:
            PythonSourceRoot::TypeInfo typeInfo;
            typeInfo.type = PythonSourceRoot::DataTypes::ClassType;
            m_identifiers.setIdentifier(tok, typeInfo);

            // add function frame
            PythonSourceFrame *funcFrm = new PythonSourceFrame(this, m_module, this, false);
            tok = funcFrm->scanFrame(tok);
            insert(funcFrm);
        } break;
        case PythonSyntaxHighlighter::T_IdentifierSuperMethod:
            if (isClass() &&
                tok->token == PythonSyntaxHighlighter::T_IdentifierSuperMethod &&
                tok->text() == QLatin1String("__init__"))
            {
                // parameters for this class
                scanParameter(tok->next());
            }
            // fallthrough
        case PythonSyntaxHighlighter::T_IdentifierMethod: {
doMethod:
            if (!isClass())
                setSyntaxError(tok, QLatin1String("Method without class"));

            // add method frame
            PythonSourceFrame *frm = new PythonSourceFrame(this, m_module, this, true);
            tok = frm->scanFrame(tok);
            insert(frm);

        } break;
        case PythonSyntaxHighlighter::T_Indent: {
            int indent = tok->txtBlock()->indent();
            if (indent < currentBlockIndent) {
                // dedent
                tok->txtBlock()->insertToken(PythonSyntaxHighlighter::T_BlockEnd, tok->startPos, 0);
                if (indent > previousBlockIndent) {
                    tok->token = PythonSyntaxHighlighter::T_IndentError;
                    m_module->highlighter()->setFormatToken(tok);
                } else {
                    currentBlockIndent = indent;
                    // leave frame ?
                    if (currentBlockIndent < firstRowIndent)
                        goto leaveFrame;
                }
            } else if (indent > currentBlockIndent) {
                // indent
                // find previous ':'
                PythonToken *prev = tok->previous();
                int guard = 20;
                while(prev && (guard--)) {
                    switch (prev->token) {
                    case PythonSyntaxHighlighter::T_DelimiterLineContinue:
                    case PythonSyntaxHighlighter::T_DelimiterNewLine:
                    case PythonSyntaxHighlighter::T_Indent:
                    case PythonSyntaxHighlighter::T_IndentError:
                    case PythonSyntaxHighlighter::T_Comment:
                        prev = prev->previous();
                        break;
                    case PythonSyntaxHighlighter::T_DelimiterColon:
                        prev->txtBlock()->insertToken(PythonSyntaxHighlighter::T_BlockStart, tok->startPos, 0);
                        prev = nullptr; // break for loop
                        break;
                    default:
                        // syntax error
                        setSyntaxError(tok, QLatin1String("Blockstart without ':'"));
                        prev = nullptr; // break for loop
                    }
                }
            }
        } break;
        default:
            if (tok->isIdentifier()) {
                tok = scanIdentifier(tok);
            }
        }

        if (!tok)
            break;

        tok = tok->next();
    }

leaveFrame:
    // set end token
    lastToken = tok;
    return lastToken ? lastToken->next() : nullptr;
}

// scans a for aditional TypeHint and scans rvalue
PythonToken *PythonSourceFrame::scanIdentifier(PythonToken *tok)
{
    int guard = 20;
    PythonSourceIdentifier *ident = nullptr;
    PythonSourceRoot::TypeInfo typeInfo;

    while (tok && (guard--)) {
        // TODO figure out how to do tuple assignment
        switch (tok->token) {
        case PythonSyntaxHighlighter::T_OperatorEqual:
            tok = scanRValue(tok, typeInfo, false);
            ident = m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            // type hint
            tok = scanRValue(tok, typeInfo, true);
            break;
        case PythonSyntaxHighlighter::T_IdentifierModule:
            tok = scanImports(tok);
            break;
        case PythonSyntaxHighlighter::T_IdentifierModuleGlob:
        default:
            break;
        }
        tok = tok->next();
    }

    return tok;
}

PythonToken *PythonSourceFrame::scanRValue(PythonToken *tok,
                                           PythonSourceRoot::TypeInfo &typeInfo,
                                           bool isTypeHint)
{
    typeInfo = PythonSourceRoot::TypeInfo(); // init with clean type

    int guard = 20;
    int parenCnt = 0,
        bracketCnt = 0,
        braceCnt = 0;

    const PythonSourceIdentifier *ident = nullptr;

    while (tok && (guard--)) {
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_DelimiterOpenParen:
            ++parenCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseParen:
            --parenCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBracket:
            ++bracketCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseBracket:
            --bracketCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBrace:
            ++braceCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseBrace:
            --braceCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterPeriod:
            // might have obj.sub1.sub2
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            if (!isTypeHint) {
                setSyntaxError(tok, QString::fromLatin1("Unexpected '%1'").arg(tok->text()));
                return tok;
            } // else do next token
            break;
        case PythonSyntaxHighlighter::T_OperatorEqual:
            if (!isTypeHint) {
                // might have chained assignments ie: vlu1 = valu2 = vlu3 = 0
                tok = scanRValue(tok->next(), typeInfo, isTypeHint);
            }

            if (ident && tok)
                typeInfo = ident->getFromPos(tok)->type();
            return tok;
        default:
            if (parenCnt == 0 && braceCnt == 0 && braceCnt == 0) {
                QString text = tok->text();// one roundtrip to document
                if (!isTypeHint) {
                    // ordinary RValue
                    if (tok->isInt())
                        typeInfo.type = PythonSourceRoot::IntType;
                    else if (tok->isFloat())
                        typeInfo.type = PythonSourceRoot::FloatType;
                    else if (tok->isString())
                        typeInfo.type = PythonSourceRoot::StringType;
                    else if (tok->isKeyword()) {
                        if (text == QLatin1String("None"))
                            typeInfo.type = PythonSourceRoot::NoneType;
                        else if (text == QLatin1String("False") ||
                                 text == QLatin1String("True"))
                        {
                            typeInfo.type = PythonSourceRoot::BoolType;
                        }
                    }
                }

                if (tok->isIdentifierVarable()){
                    if (tok->token == PythonSyntaxHighlighter::T_IdentifierBuiltin) {
                        typeInfo.type = PythonSourceRoot::ReferenceBuiltinType;
                    } else {
                        // TODO figure out how to lookup Identifier
                        ident = m_identifiers.getIdentifier(text);
                        if (ident) {
                            typeInfo.type = PythonSourceRoot::ReferenceType;
                            typeInfo.referenceName = text;
                        } else if (isTypeHint)
                            setSyntaxError(tok, QString::fromLatin1("Unknown type '%1'").arg(text));

                    }
                    return tok;

                } else if (tok->isCode()) {
                    setSyntaxError(tok, QString::fromLatin1("Unexpected code (%1)").arg(text));
                    return tok;
                }
            }
        }
        tok = tok->next();
    }

    return tok;
}

PythonToken *PythonSourceFrame::scanImports(PythonToken *tok)
{
    int guard = 20;
    QStringList fromPackages, importPackages, modules;
    QString alias;
    bool isAlias = false;
    bool isImports = tok->previous()->token == PythonSyntaxHighlighter::T_KeywordImport;

    while (tok && (guard--)) {
        QString text = tok->text();
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_KeywordImport:
            isImports = true;
            break;
        case PythonSyntaxHighlighter::T_KeywordFrom:
            if (isImports)
                setSyntaxError(tok, QString::fromLatin1("Unexpected token '%1'").arg(text));
            break;
        case PythonSyntaxHighlighter::T_KeywordAs:
            isAlias = true;
            break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (tok->previous()->token != PythonSyntaxHighlighter::T_DelimiterLineContinue) {
                guard = 0; // we are finished, bail out
                if (modules.size() > 0)
                    goto store_module;
            }
            break;
        case PythonSyntaxHighlighter::T_DelimiterComma:
            if (!modules.isEmpty())
                goto store_module;
            break;
        case PythonSyntaxHighlighter::T_IdentifierModuleGlob:
            m_imports.setModuleGlob(fromPackages + importPackages);
            modules.clear();
            importPackages.clear();
            break;
        case PythonSyntaxHighlighter::T_IdentifierModulePackage:
            if (!isImports) {
                fromPackages << text;
                break;
            }
            goto set_identifier;
        case PythonSyntaxHighlighter::T_IdentifierModule:
            goto set_identifier;
        case PythonSyntaxHighlighter::T_IdentifierModuleAlias:
            if (!isAlias)
                setSyntaxError(tok, QString::fromLatin1("Parsed as Alias but analyzer disagrees. '%1'").arg(text));
            goto set_identifier;
        default:
            if (tok->isCode())
                setSyntaxError(tok, QString::fromLatin1("Unexpected token '%1'").arg(text));
            break;
        }

next_token:
        tok = tok->next();
        continue;

        // we should only get here by explicit goto statements
set_identifier:
            if (!isAlias) {
                // previous text was a package
                if (modules.size())
                    importPackages << modules.takeLast();
                modules << text;
                goto next_token;
            } else {
                alias = text;
            }

// same code used from multiple places, keeps loop running
store_module:
        assert(modules.size() > 0 && "Modules set called by no modules in container");
        if (alias.size() > 0)
            m_imports.setModule(fromPackages + importPackages, modules[0], alias);
        else
            m_imports.setModule(fromPackages + importPackages, modules[0]);
        alias.clear();
        importPackages.clear();
        modules.clear();
        isAlias = false;
        goto next_token;
    }
    return tok;
}

// may return nullptr on error
const PythonToken *PythonSourceFrame::endOfParametersList(const PythonToken *tok, bool storeParameters)
{
    // safely goes to closingParen of arguments list
    int parenCount = 0,
        safeGuard = 50;
    while(tok && tok->token != PythonSyntaxHighlighter::T_DelimiterMetaData)
    {
        if ((--safeGuard) <= 0)
            return nullptr;
        // first we must make sure we clear all parens
        if (tok->token == PythonSyntaxHighlighter::T_DelimiterOpenParen)
            ++parenCount;
        else if (tok->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            --parenCount;

        if (parenCount == 0 && tok->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            return tok;

        // advance one token
        tok = tok->next();

        // scan parameters
        if (storeParameters && parenCount > 0) {
            if (tok->token == PythonSyntaxHighlighter::T_DelimiterColon)
                tok = scanParameter(tok);
            else if (tok->token == PythonSyntaxHighlighter::T_DelimiterOpenParen) {
                ++parenCount;
                tok = scanParameter(tok);
            }
        }
    }

    return nullptr;
}

const PythonToken *PythonSourceFrame::scanParameter(const PythonToken *paramToken)
{
    // safely constructs our parameters list
    int parenCount = 0,
        safeGuard = 20;

    // init empty
    PythonSourceParameter *param = nullptr;
    PythonSourceIdentifier *ident = nullptr;
    PythonSourceParameter::ParameterType paramType = PythonSourceParameter::InValid;

    // scan up to ',' or closing ')'
    while(paramToken && paramToken->token != PythonSyntaxHighlighter::T_DelimiterComma)
    {
        if ((--safeGuard) <= 0)
            return paramToken;
        // might have subparens
        if (paramToken->token == PythonSyntaxHighlighter::T_DelimiterOpenParen)
            ++parenCount;
        else if (paramToken->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            --parenCount;

        if (parenCount == 0 && paramToken->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            return paramToken->previous(); // caller must know where closing paren is
        else if (parenCount > 0) {
            if (paramToken->token == PythonSyntaxHighlighter::T_OperatorVariableParam)
                paramType = PythonSourceParameter::Variable;
            else if (paramToken->token == PythonSyntaxHighlighter::T_OperatorKeyWordParam)
                paramType = PythonSourceParameter::Keyword;
            else if (paramToken->isIdentifier()) {
                const PythonToken *nextTok = paramToken->next();
                if (nextTok && nextTok->token == PythonSyntaxHighlighter::T_OperatorEqual)
                    paramType = PythonSourceParameter::PositionalDefault; // have default value
                else
                    paramType = PythonSourceParameter::Positional;

                // set identifier
                const PythonSourceRoot::TypeInfo typeInfo = guessIdentifierType(paramToken);
                ident = m_identifiers.setIdentifier(paramToken, typeInfo);
                // set parameter
                param = m_parameters.setParameter(paramToken, typeInfo, paramType);

                // Change tokenValue
                if (paramToken->token == PythonSyntaxHighlighter::T_IdentifierUnknown) {

                    PythonToken *tok = const_cast<PythonToken*>(paramToken);
                    if (m_parentFrame->isClass() && m_parameters.indexOf(param) == 0)
                        tok->token = PythonSyntaxHighlighter::T_IdentifierSelf;
                    else if (typeInfo.type == PythonSourceRoot::ReferenceType)
                        tok->token = PythonSyntaxHighlighter::T_IdentifierDefUnknown;
                    else
                        tok->token = PythonSyntaxHighlighter::T_IdentifierDefined;
                }

                // repaint in highligher
                m_module->highlighter()->setFormatToken(paramToken);
            }
        }


        paramToken = paramToken->next();
    }

    return paramToken;

}

const PythonSourceRoot::TypeInfo PythonSourceFrame::guessIdentifierType(const PythonToken *token)
{
    PythonSourceRoot::TypeInfo typeInfo;
    if (token && (token = token->next())) {
        if (token->token == PythonSyntaxHighlighter::T_DelimiterColon) {
            // type hint like foo : None = Nothing
            QString explicitType = token->next()->text();
            PythonSourceRoot *root = PythonSourceRoot::instance();
            typeInfo.type = root->mapDataType(explicitType);
            if (typeInfo.type == PythonSourceRoot::CustomType) {
                typeInfo.customNameIdx = root->indexOfCustomTypeName(explicitType);
                if (typeInfo.customNameIdx < 0)
                    typeInfo.customNameIdx = root->addCustomTypeName(explicitType);
            }
        } else if (token->token == PythonSyntaxHighlighter::T_OperatorEqual) {
            // ordinare assignment foo = Something
            typeInfo.type = PythonSourceRoot::ReferenceType;
        }
    }
    return typeInfo;
}

void PythonSourceFrame::setSyntaxError(const PythonToken *tok, QString parseMessage)
{
    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, parseMessage, PythonTextBlockScanInfo::SyntaxError);

    // paint accordingly
    const_cast<PythonToken*>(tok)->token = PythonSyntaxHighlighter::T_SyntaxError;
    m_module->highlighter()->setFormatToken(tok);

    // set text underline
    const PythonToken *startTok = tok;
    while(startTok && startTok->isCode()) {
        if (startTok->previous())
            startTok = startTok->previous();
        else
            break;
    }

    QTextCharFormat format;
    format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    format.setUnderlineColor(m_module->highlighter()->
                                colorByType(SyntaxHighlighter::SyntaxError));
    m_module->highlighter()->setTextFormat(tok->txtBlock()->block(), startTok->startPos, tok->endPos, format);
}

void PythonSourceFrame::setMessage(const PythonToken *tok, QString parseMessage)
{
    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, parseMessage, PythonTextBlockScanInfo::Message);
}


// --------------------------------------------------------------------------

PythonSourceModule::PythonSourceModule(PythonSourceRoot *root,
                                       PythonSyntaxHighlighter *highlighter) :
    PythonSourceListParentBase(this),
    m_root(root),
    m_rootFrame(this, this),
    m_highlighter(highlighter)
{
}

PythonSourceModule::~PythonSourceModule()
{
}


int PythonSourceModule::compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const
{
    PythonSourceModule *l = dynamic_cast<PythonSourceModule*>(left),
                       *r = dynamic_cast<PythonSourceModule*>(right);
    assert(l != nullptr && r != nullptr && "Invalid class stored as PythonSourceModule");
    if (l->moduleName() < r->moduleName())
        return +1;
    else if (l->moduleName() > r->moduleName())
        return -1;
    else {
        if (l->filePath() < r->filePath())
            return -1;
        if (l->filePath() > r->filePath())
            return +1;
        assert(false && "Trying to store the same module twice");
    }
}


// -----------------------------------------------------------------------

PythonSourceModuleList::PythonSourceModuleList() :
    PythonSourceListParentBase(this),
    m_main(nullptr)
{
}

PythonSourceModuleList::~PythonSourceModuleList()
{
}

void PythonSourceModuleList::setMain(PythonSourceModule *main)
{
    if (!contains(main))
        insert(main);

    m_main = main;
}

bool PythonSourceModuleList::remove(PythonSourceListNodeBase *node, bool deleteNode)
{
    if (node == m_main)
        m_main = nullptr;

    return PythonSourceListParentBase::remove(node, deleteNode);
}


// -------------------------------------------------------------------------

#include "moc_PythonCode.cpp"
