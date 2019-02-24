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

DBG_TOKEN_FILE

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
         itm = itm->next())
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
    if (typeAnnotation == QLatin1String("Object"))
        return ObjectType;
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
    DEFINE_DBG_VARS

    // delete old data
    PythonSourceModule *mod = moduleFromPath(filePath);
    if (mod) {
        m_modules->remove(mod, true);
        mod = nullptr;
    }

    // create a new module
    mod = new PythonSourceModule(this, highlighter);
    mod->setFilePath(filePath);
    QFileInfo fi(filePath);
    mod->setModuleName(fi.baseName());
    m_modules->insert(mod);

    // find the first token of any kind
    // firstline might be empty
    const PythonTextBlockData *txtData = dynamic_cast<PythonTextBlockData*>(
                                            highlighter->document()->begin().userData());
    if (txtData) {
        PythonToken *tok = const_cast<PythonToken*>(txtData->tokenAt(0));
        DBG_TOKEN(tok)
        if (tok)
            mod->scanFrame(tok);
    }

    return mod;
}

PythonSourceModule *PythonSourceRoot::scanSingleRowModule(const QString filePath,
                                                          PythonTextBlockData *row,
                                                          PythonSyntaxHighlighter *highlighter)
{
    PythonSourceModule *mod = moduleFromPath(filePath);
    if (!mod) {
        // create a new module
        mod = new PythonSourceModule(this, highlighter);
        mod->setFilePath(filePath);
        QFileInfo fi(filePath);
        mod->setModuleName(fi.baseName());
        m_modules->insert(mod);
    }

    if (!row->isEmpty()) {
        mod->scanLine(const_cast<PythonToken*>(row->tokenAt(0)));
    }

    return mod;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// this struct is contained by PythonSourceRoot
PythonSourceRoot::TypeInfo::TypeInfo() :
    type(PythonSourceRoot::InValidType),
    customNameIdx(-1)
{
}

PythonSourceRoot::TypeInfo::TypeInfo(PythonSourceRoot::DataTypes type) :
    type(type),
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
    static const char *errStr = "<Typename lookup error>";

    switch (type) {
    case PythonSourceRoot::InValidType:         typeAsStr = "InValidType"; break;
    case PythonSourceRoot::UnknownType:         typeAsStr = "UnknownType"; break;

    case PythonSourceRoot::ReferenceType:       typeAsStr = "ReferenceType"; break;
    case PythonSourceRoot::ReferenceArgumentType: typeAsStr = "ReferenceArgumentType"; break;
    case PythonSourceRoot::ReferenceBuiltInType: typeAsStr = "ReferenceBuiltInType"; break;

    case PythonSourceRoot::ReferenceImportUndeterminedType:
                                                typeAsStr = "ReferenceImportUndeterminedType"; break;
    case PythonSourceRoot::ReferenceImportErrorType:
                                                typeAsStr = "ReferenceImportErrorType"; break;
    case PythonSourceRoot::ReferenceImportPythonType:
                                                typeAsStr = "ReferenceImportPythonType"; break;
    case PythonSourceRoot::ReferenceImportBuiltInType:
                                                typeAsStr = "ReferenceImportBuiltInType"; break;

    case PythonSourceRoot::ObjectType:          typeAsStr = "ObjectType";   break;
    case PythonSourceRoot::NoneType:            typeAsStr = "NoneType";     break;
    case PythonSourceRoot::BoolType:            typeAsStr = "BoolType";     break;
    case PythonSourceRoot::IntType:             typeAsStr = "IntType";      break;
    case PythonSourceRoot::FloatType:           typeAsStr = "FloatType";    break;
    case PythonSourceRoot::StringType:          typeAsStr = "StringType";   break;
    case PythonSourceRoot::BytesType:           typeAsStr = "BytesType";    break;
    case PythonSourceRoot::ListType:            typeAsStr = "ListType";     break;
    case PythonSourceRoot::TupleType:           typeAsStr = "TupleType";    break;
    case PythonSourceRoot::SetType:             typeAsStr = "SetType";      break;
    case PythonSourceRoot::RangeType:           typeAsStr = "RangeType";    break;
    case PythonSourceRoot::DictType:            typeAsStr = "DictType";     break;
    case PythonSourceRoot::FunctionType:        typeAsStr = "FunctionType"; break;
    case PythonSourceRoot::ClassType:           typeAsStr = "ClassType";    break;
    case PythonSourceRoot::MethodType:          typeAsStr = "MethodType";   break;
    case PythonSourceRoot::GeneratorType:       typeAsStr = "GeneratorType";   break;
    case PythonSourceRoot::CustomType:
        if (customNameIdx > -1) {
            return customName();
        }
        return QLatin1String(errStr);
        break;
    default:
        return QLatin1String(errStr);
    }

    return QLatin1String(typeAsStr);
}

QString PythonSourceRoot::TypeInfo::customName() const
{
    if (customNameIdx > -1) {
        return PythonSourceRoot::instance()->customTypeNameFor(customNameIdx);
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

PythonSourceListNodeBase::PythonSourceListNodeBase(const PythonSourceListNodeBase &other) :
    m_previous(nullptr), m_next(nullptr),
    m_owner(other.m_owner), m_token(other.m_token)
{
    assert(other.m_owner != nullptr && "Trying to copy PythonSourceListNodeBase with null as owner");
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

PythonSourceListParentBase::PythonSourceListParentBase(const PythonSourceListParentBase &other) :
    PythonSourceListNodeBase(this),
    m_first(other.m_first), m_last(other.m_last),
    m_preventAutoRemoveMe(other.m_preventAutoRemoveMe)
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

int PythonSourceListParentBase::compare(const PythonSourceListNodeBase *left,
                                        const PythonSourceListNodeBase *right) const
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
                                                                   const PythonToken *startToken,
                                                                   PythonSourceRoot::TypeInfo typeInfo) :
    PythonSourceListNodeBase(owner)
{
    m_type = typeInfo;
    assert(startToken != nullptr && "Must have valid token");
    m_token = startToken;
    const_cast<PythonToken*>(m_token)->attachReference(this);
}

PythonSourceIdentifierAssignment::PythonSourceIdentifierAssignment(PythonSourceTypeHint *owner,
                                                                   const PythonToken *startToken,
                                                                   PythonSourceRoot::TypeInfo typeInfo) :
        PythonSourceListNodeBase(owner)
{
    m_type = typeInfo;
    assert(startToken != nullptr && "Must have valid token");
    m_token = startToken;
    const_cast<PythonToken*>(m_token)->attachReference(this);
}

PythonSourceIdentifierAssignment::~PythonSourceIdentifierAssignment()
{
}

int PythonSourceIdentifierAssignment::linenr() const
{
    return m_token->line();
}

int PythonSourceIdentifierAssignment::position() const
{
    return m_token->startPos;
}

// ----------------------------------------------------------------------------

PythonSourceTypeHint::PythonSourceTypeHint(PythonSourceListParentBase *owner,
                                           const PythonSourceIdentifier *identifer,
                                           const PythonSourceModule *module) :
    PythonSourceListParentBase(owner),
    m_module(module),
    m_identifer(identifer)
{
}

PythonSourceTypeHint::~PythonSourceTypeHint()
{
}

const PythonSourceFrame *PythonSourceTypeHint::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
}

PythonSourceTypeHintAssignment *PythonSourceTypeHint::getFromPos(int line, int pos) const
{
    for (PythonSourceListNodeBase *lstElem = last();
         lstElem != end();
         lstElem = lstElem->previous())
    {
        if ((lstElem->token()->line() == line && lstElem->token()->endPos < pos) ||
            (lstElem->token()->line() < line))
        {
            return dynamic_cast<PythonSourceTypeHintAssignment*>(lstElem);
        }
    }

    // not found
    return nullptr;
}

PythonSourceTypeHintAssignment *PythonSourceTypeHint::getFromPos(const PythonToken *tok) const
{
    return getFromPos(tok->line(), tok->startPos);
}

QString PythonSourceTypeHint::name() const
{
    return m_type.typeAsStr();
}

int PythonSourceTypeHint::compare(const PythonSourceListNodeBase *left, const PythonSourceListNodeBase *right) const
{
    // should sort by linenr and (if same line also token startpos)
    const PythonSourceTypeHintAssignment *l = dynamic_cast<const PythonSourceTypeHintAssignment*>(left),
                                         *r = dynamic_cast<const PythonSourceTypeHintAssignment*>(right);
    assert(l != nullptr && r != nullptr && "PythonSourceTypeHint stored non compatible node");

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

// ------------------------------------------------------------------------

PythonSourceIdentifier::PythonSourceIdentifier(PythonSourceListParentBase *owner,
                                               const PythonSourceModule *module) :
    PythonSourceListParentBase(owner),
    m_module(module),
    m_typeHint(this, this, module)
{
    assert(m_module != nullptr && "Must have a valid module");
}

PythonSourceIdentifier::~PythonSourceIdentifier()
{
}

const PythonSourceFrame *PythonSourceIdentifier::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
}

PythonSourceIdentifierAssignment *PythonSourceIdentifier::getFromPos(int line, int pos) const
{
    for (PythonSourceListNodeBase *lstElem = last();
         lstElem != end();
         lstElem = lstElem->previous())
    {
        if ((lstElem->token()->line() == line && lstElem->token()->endPos < pos) ||
            (lstElem->token()->line() < line))
        {
            return dynamic_cast<PythonSourceIdentifierAssignment*>(lstElem);
        }
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

PythonSourceTypeHintAssignment *PythonSourceIdentifier::getTypeHintAssignment(PythonToken *tok) const
{
    return getTypeHintAssignment(tok->line());
}

PythonSourceTypeHintAssignment *PythonSourceIdentifier::getTypeHintAssignment(int line) const
{
    for (PythonSourceListNodeBase *lstElem = m_typeHint.last();
         lstElem != end();
         lstElem = lstElem->previous())
    {
        if (lstElem->token()->line() <= line)
            return dynamic_cast<PythonSourceTypeHintAssignment*>(lstElem);
    }

    return nullptr;
}

bool PythonSourceIdentifier::hasTypeHint(int line) const {
    return getTypeHintAssignment(line) != nullptr;
}


PythonSourceTypeHintAssignment *PythonSourceIdentifier::setTypeHint(const PythonToken *tok,
                                                                    PythonSourceRoot::TypeInfo typeInfo)
{
    PythonSourceTypeHintAssignment *typeHint = getTypeHintAssignment(tok->line());
    if (typeHint) {
        // we found the last declared type hint, update type
        typeHint->setType(typeInfo);
        return typeHint;
    }

    // create new assigment
    typeHint = new PythonSourceTypeHintAssignment(&m_typeHint, tok, typeInfo);
    m_typeHint.insert(typeHint);
    return typeHint;
}

int PythonSourceIdentifier::compare(const PythonSourceListNodeBase *left,
                                    const PythonSourceListNodeBase *right) const
{
    // should sort by linenr and (if same line also token startpos)
    const PythonSourceIdentifierAssignment *l = dynamic_cast<const PythonSourceIdentifierAssignment*>(left),
                                           *r = dynamic_cast<const PythonSourceIdentifierAssignment*>(right);
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


// -------------------------------------------------------------------------------


PythonSourceIdentifierList::PythonSourceIdentifierList(const PythonSourceModule *module):
    PythonSourceListParentBase(this),
    m_module(module)
{
    m_preventAutoRemoveMe = true;
    assert(module != nullptr && "Must have a valid module");
}

PythonSourceIdentifierList::~PythonSourceIdentifierList()
{
}

const PythonSourceFrame *PythonSourceIdentifierList::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
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

PythonSourceIdentifier
*PythonSourceIdentifierList::setIdentifier(const PythonToken *tok,
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
        identifier = new PythonSourceIdentifier(this, m_module);
        insert(identifier);
    } else {
        // check so we don't double insert
        for (PythonSourceListNodeBase *itm = identifier->begin();
             itm != nullptr;
             itm = itm->next())
        {
            if (itm->token() == tok) {
                assign = dynamic_cast<PythonSourceIdentifierAssignment*>(itm);
                if (assign->typeInfo() != typeInfo)
                    assign->setType(typeInfo); // type differ
                else
                    return identifier; // already have this one and it is equal
            }
        }
    }

    // create new assigment
    assign = new PythonSourceIdentifierAssignment(identifier, tok, typeInfo);
    identifier->insert(assign);
    return identifier;
}

const PythonSourceIdentifier
*PythonSourceIdentifierList::getIdentifierReferencedBy(const PythonToken *tok,
                                                       int limitChain) const
{
    if (tok->isIdentifier()) {
        // TODO builtin identifiers and modules lookup
        const PythonSourceIdentifier *ident = getIdentifier(tok->text());
        return getIdentifierReferencedBy(ident, tok->line(), tok->startPos, limitChain);
    }

    return nullptr;
}

const PythonSourceIdentifier
*PythonSourceIdentifierList::getIdentifierReferencedBy(const PythonSourceIdentifier *ident,
                                                       int line, int pos, int limitChain) const
{
    if (!ident)
        return nullptr;

    const PythonSourceIdentifier *startIdent = ident;

    // lookup nearest assignment
    const PythonSourceIdentifierAssignment *assign = ident->getFromPos(line, pos);
    if (!assign)
        return startIdent;

    // is this assignment also referenced?
    switch (assign->typeInfo().type) {
    case PythonSourceRoot::ReferenceType:
        // lookup recursive
        ident = getIdentifierReferencedBy(ident, line, pos, limitChain -1);
        if (!ident)
            return startIdent;
        return ident;
    // case PythonSourceRoot::ReferenceBuiltinType: // not sure if we need this specialcase?
    default:
        if (assign->typeInfo().isReferenceImported())
            return ident;
        break;
    }

    return ident;
}

int PythonSourceIdentifierList::compare(const PythonSourceListNodeBase *left,
                                        const PythonSourceListNodeBase *right) const
{
    const PythonSourceIdentifier *l = dynamic_cast<const PythonSourceIdentifier*>(left),
                                 *r = dynamic_cast<const PythonSourceIdentifier*>(right);
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

int PythonSourceParameterList::compare(const PythonSourceListNodeBase *left,
                                       const PythonSourceListNodeBase *right) const
{
    const PythonSourceParameter *l = dynamic_cast<const PythonSourceParameter*>(left),
                                *r = dynamic_cast<const PythonSourceParameter*>(right);
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
    m_frame(frame), m_aliasName(alias), m_type()
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
                                                     const PythonSourceModule *ownerModule) :
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

int PythonSourceImportPackage::compare(const PythonSourceListNodeBase *left,
                                       const PythonSourceListNodeBase *right) const
{
    QString lName, rName;
    const PythonSourceImportModule *lm = dynamic_cast<const PythonSourceImportModule*>(left),
                                   *rm = dynamic_cast<const PythonSourceImportModule*>(right);

    const PythonSourceImportPackage *lp = dynamic_cast<const PythonSourceImportPackage*>(left),
                                    *rp = dynamic_cast<const PythonSourceImportPackage*>(right);
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
                                               const PythonSourceModule *ownerModule) :
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

    return rootPkg;
}

PythonSourceModule *PythonSourceImportList::setModuleGlob(QStringList rootPackage)
{
    // FIXME implement
    Q_UNUSED(rootPackage);
    return nullptr;
}

// -----------------------------------------------------------------------------

PythonSourceFrame::PythonSourceFrame(PythonSourceFrame *owner,
                                     const PythonSourceModule *module,
                                     PythonSourceFrame *parentFrame,
                                     bool isClass):
    PythonSourceListParentBase(owner),
    m_identifiers(module),
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
                                     const PythonSourceModule *module,
                                     PythonSourceFrame *parentFrame,
                                     bool isClass):
    PythonSourceListParentBase(owner),
    m_identifiers(module),
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
    if (m_returns)
        delete m_returns;
}

QString PythonSourceFrame::docstring()
{
    DEFINE_DBG_VARS

    // retrive docstring for this function
    QStringList docStrs;
    if (m_token) {
        PythonToken *token = scanAllParameters(const_cast<PythonToken*>(m_token));
        DBG_TOKEN(token)
        int guard = 20;
        while(token && token->token != PythonSyntaxHighlighter::T_DelimiterSemiColon) {
            if ((--guard) <= 0)
                return QString();
            NEXT_TOKEN(token)
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
                NEXT_TOKEN(token)
                break;
            default:
                // some other token, probably some code
                token = nullptr; // bust out of while
                DBG_TOKEN(token)
            }
        }

        // return what we have collected
        return docStrs.join(QLatin1String("\n"));
    }

    return QString(); // no token
}

PythonSourceRoot::TypeInfo PythonSourceFrame::returnTypeHint()
{
    DEFINE_DBG_VARS

    PythonSourceRoot::TypeInfo tpInfo;
    if (m_token) {
        const PythonToken *token = scanAllParameters(const_cast<PythonToken*>(m_token)),
                          *commentToken = nullptr;
        DBG_TOKEN(token)
        int guard = 10;
        while(token && token->token != PythonSyntaxHighlighter::T_DelimiterMetaData) {
            if ((--guard) <= 0)
                return tpInfo;

            if (token->token == PythonSyntaxHighlighter::T_DelimiterSemiColon) {
                // no metadata
                // we may still have type hint in a comment in row below
                while (token && token->token == PythonSyntaxHighlighter::T_Indent) {
                    NEXT_TOKEN(token)
                    if (token && token->token == PythonSyntaxHighlighter::T_Comment) {
                        commentToken = token;
                        token = nullptr; // bust out of loops
                        DBG_TOKEN(token)
                    }
                }

                if (!commentToken)
                    return tpInfo; // have no metadata, might have commentdata
            } else
                NEXT_TOKEN(token)
        }

        QString annotation;

        if (token) {
            // now we are at metadata token
            //  def func(arg1: int, arg2: bool) -> MyType:
            //                                  ^
            // step one more further
            NEXT_TOKEN(token)
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
    DEFINE_DBG_VARS
    DBG_TOKEN(startToken)

    if (!startToken)
        return startToken;

    PythonToken *tok = startToken;
    PythonSourceRoot::Indent indent = m_module->currentBlockIndent(tok);

    int startIndent = startToken->txtBlock()->indent();
    bool isRootFrame = m_module->rootFrame() == this;

    int guard = 15000; // max number of rows scanned
    while (tok && (guard--)) {
        tok = scanLine(tok, indent);
        DBG_TOKEN(tok)
        if (!tok || (!isRootFrame && tok->txtBlock()->indent() <= startIndent))
            break; // we have finished this frame
        NEXT_TOKEN(tok)
    }

    // set end token
    lastToken = tok;
    return lastToken ? lastToken->next() : nullptr;
}


PythonToken *PythonSourceFrame::scanLine(PythonToken *startToken,
                                         PythonSourceRoot::Indent &indent)
{
    DEFINE_DBG_VARS

    if (!startToken)
        return startToken;

    PythonToken *tok = startToken;
    DBG_TOKEN(tok)

    // move up to first code token
    int guard = 100;
    while(tok && (guard--)) {
        if (tok->isNewLine())
            return tok->next(); // empty row
        if (startToken->isText())
            break;
        NEXT_TOKEN(tok)
    }

    // do framestart if it is first row in document
    bool isFrameStarter = tok->txtBlock()->block().blockNumber() == 0;
    if (startToken->token == PythonSyntaxHighlighter::T_IdentifierFunction ||
        startToken->token == PythonSyntaxHighlighter::T_IdentifierMethod ||
        isFrameStarter)
    {
        isFrameStarter = true;
        // set initial start token
        setToken(startToken);

        // scan parameterLists
        if (m_parentFrame && !isClass()) {
            // not rootFrame, parse argumentslist
            tok = scanAllParameters(tok, true);
            DBG_TOKEN(tok)
        }
    }

    bool indentCached = indent.isValid();
    // we don't do this on each row when scanning all frame
    if (!indentCached) {
        indent = m_module->currentBlockIndent(tok);
    }

    PythonSourceModule *noConstModule = const_cast<PythonSourceModule*>(m_module);

    guard = 200; // max number of tokens in this line, unhang locked loop
    // scan document
    while (tok && (guard--)) {
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_DelimiterMetaData: {
            // make sure previous char was a ')'
            PythonToken *tmpTok = tok->previous();
            int guardTmp = 20;
            while (tmpTok && !tmpTok->isCode() && guardTmp--)
                tmpTok = tmpTok->previous();

            if (tmpTok && tmpTok->token != PythonSyntaxHighlighter::T_DelimiterCloseParen)
                m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected '%1' before '->'")
                                                    .arg(tmpTok->text()));
            else {
                PythonSourceRoot::TypeInfo typeInfo;
                tok = scanRValue(tok->next(), typeInfo, true);
                // TODO need to store this metadata in returns
                //ident = m_returns.setIdentifier(tok->previous(), typeInfo);
                DBG_TOKEN(tok)
            }

        } break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (!isLineEscaped(tok))
                return tok;
            break;
        case PythonSyntaxHighlighter::T_IdentifierDefUnknown: {
            // a function that can be function or method
            // determine what it is
            if (m_isClass && indent.frameIndent == tok->txtBlock()->indent()) {
                tok->token = PythonSyntaxHighlighter::T_IdentifierMethod;
                noConstModule->setFormatToken(tok);
                goto doFunction;
            }
            tok->token = PythonSyntaxHighlighter::T_IdentifierFunction;
            noConstModule->setFormatToken(tok);
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
            DBG_TOKEN(tok)
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
            DBG_TOKEN(tok)
            insert(funcFrm);

            if (indentCached)
                goto frame_indent_increased;
        } break;
        case PythonSyntaxHighlighter::T_IdentifierSuperMethod:
            if (isClass() &&
                tok->token == PythonSyntaxHighlighter::T_IdentifierSuperMethod &&
                tok->text() == QLatin1String("__init__"))
            {
                // parameters for this class
                tok = scanAllParameters(tok->next());
                DBG_TOKEN(tok)
            }
            // fallthrough
        case PythonSyntaxHighlighter::T_IdentifierMethod: {
doMethod:
            if (!isClass())
                m_module->setSyntaxError(tok, QLatin1String("Method without class"));

            // add method frame
            PythonSourceFrame *frm = new PythonSourceFrame(this, m_module, this, true);
            tok = frm->scanFrame(tok);
            DBG_TOKEN(tok)
            insert(frm);

            if (indentCached)
                goto frame_indent_increased;
        } break;
        case PythonSyntaxHighlighter::T_Indent: {
            int ind = tok->txtBlock()->indent();
            if (ind < indent.currentBlockIndent) {
                // dedent
                tok->txtBlock()->insertToken(PythonSyntaxHighlighter::T_BlockEnd, tok->startPos, 0);
                if (ind > indent.previousBlockIndent) {
                    m_module->setIndentError(tok);
                } else {
                    indent.currentBlockIndent = ind;
                    if (indent.currentBlockIndent < indent.frameIndent) {
                        // leave frame, we abort this row at this point
                        indent.setAsInValid();
                        return tok;
                    }
                }
            } else if (ind > indent.currentBlockIndent) {
                // indent
                // find previous ':'
                PythonToken *prev = tok->previous();
                DBG_TOKEN(prev)
                int guard = 20;
                while(prev && (guard--)) {
                    switch (prev->token) {
                    case PythonSyntaxHighlighter::T_DelimiterColon:
                        prev->txtBlock()->insertToken(PythonSyntaxHighlighter::T_BlockStart, tok->startPos, 0);
                        prev = nullptr; // break for loop
                        break;
                    default:
                        if (!prev->isText()) {
                            PREV_TOKEN(prev)
                            break;
                        }
                        // syntax error
                        m_module->setSyntaxError(tok, QLatin1String("Blockstart without ':'"));
                        prev = nullptr; // break for loop
                        break;
                    }
                }
                DBG_TOKEN(tok)
            }
        } break;
        default:
            if (tok->isImport()) {
                tok = scanImports(tok);
                DBG_TOKEN(tok)

            } else if (tok->isIdentifier()) {
                tok = scanIdentifier(tok);
                DBG_TOKEN(tok)

            } else if (tok->isNewLine()){
                return tok; // exit this line
            }

        } // end switch

        if (!tok)
            break;

        NEXT_TOKEN(tok)
        continue; // next loop

        // we should only get down here when we have goto-ed here


frame_indent_increased:
        // redo indent.frameIndent
        // for caching purposes

        if (tok) {
            // this finds next valid row
            int guard = 1000; // function with 1000 rows shoould be enough?
            const PythonTextBlockData *block = tok->txtBlock()->next();
            const PythonToken *tmpTok = nullptr;
            while (block && (guard--)) {
                if (block->isCodeLine()) {
                    tmpTok = block->tokenAt(-2);
                    if (tmpTok->token == PythonSyntaxHighlighter::T_DelimiterSemiColon)
                         break; // not yet a valid function block
                                // might be a freshly typed function above other block starter lines
                    else if (block->indent() > indent.currentBlockIndent &&
                             tmpTok->token != PythonSyntaxHighlighter::T_DelimiterLineContinue)
                    {
                        // refesh indent
                        indent = m_module->currentBlockIndent(block->tokenAt(0));
                    }
                    block = block->next();
                }
            }
        }

    } // end while

    return tok;
}


// scans a for aditional TypeHint and scans rvalue
PythonToken *PythonSourceFrame::scanIdentifier(PythonToken *tok)
{
    DEFINE_DBG_VARS

    int guard = 20;
    PythonSourceIdentifier *ident = nullptr;
    PythonSourceRoot::TypeInfo typeInfo;

    while (tok && (guard--)) {
        // TODO figure out how to do tuple assignment
        switch (tok->token) {
        case PythonSyntaxHighlighter::T_IdentifierFalse:
            typeInfo.type = PythonSourceRoot::BoolType;
            m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case PythonSyntaxHighlighter::T_IdentifierTrue:
            typeInfo.type = PythonSourceRoot::BoolType;
            m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case PythonSyntaxHighlighter::T_IdentifierNone:
            typeInfo.type = PythonSourceRoot::NoneType;
            m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case PythonSyntaxHighlighter::T_OperatorEqual:
            // assigment
            tok = scanRValue(tok, typeInfo, false);
            ident = m_identifiers.setIdentifier(tok->previous(), typeInfo);
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            // type hint
            if (ident) {
                m_module->setSyntaxError(tok, QLatin1String("Unexpected ':'"));
                return tok;
            } else {
                ident = m_identifiers.setIdentifier(tok->previous(), typeInfo);
                tok = scanRValue(tok, typeInfo, true);
                DBG_TOKEN(tok)
                if (tok)
                    ident->setTypeHint(tok, typeInfo);
                if (!tok)
                    return nullptr;
                DBG_TOKEN(tok)
            }
            break;
// these should never get here
//        case PythonSyntaxHighlighter::T_IdentifierModule:
//            tok = scanImports(tok);
//            // TODO determine if module can be found and what type
//            break;
//        case PythonSyntaxHighlighter::T_IdentifierModuleGlob:
//            // TODO!
        default:
            assert(!tok->isImport() && "Should never get a import in scanIdentifier");
            break;
        }


        NEXT_TOKEN(tok)
    }

    return tok;
}

PythonToken *PythonSourceFrame::scanRValue(PythonToken *tok,
                                           PythonSourceRoot::TypeInfo &typeInfo,
                                           bool isTypeHint)
{
    DEFINE_DBG_VARS

    typeInfo = PythonSourceRoot::TypeInfo(); // init with clean type

    int guard = 20;
    int parenCnt = 0,
        bracketCnt = 0,
        braceCnt = 0;

    const PythonSourceIdentifier *ident = nullptr;

    while (tok && (guard--)) {
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_DelimiterOpenParen:
            if (!isTypeHint)
                return tok;
            ++parenCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseParen:
            if (!isTypeHint)
                return tok;
            --parenCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBracket:
            if (!isTypeHint)
                return tok;
            ++bracketCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseBracket:
            if (!isTypeHint)
                return tok;
            --bracketCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBrace:
            if (!isTypeHint)
                return tok;
            ++braceCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseBrace:
            if (!isTypeHint)
                return tok;
            --braceCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterPeriod:
            // might have obj.sub1.sub2
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            if (!isTypeHint) {
                m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected '%1'").arg(tok->text()));
                return tok;
            } // else do next token
            break;
        case PythonSyntaxHighlighter::T_OperatorEqual:
            if (!isTypeHint) {
                // might have chained assignments ie: vlu1 = valu2 = vlu3 = 0
                tok = scanRValue(tok->next(), typeInfo, isTypeHint);
                DBG_TOKEN(tok)
            }

            if (ident && tok)
                typeInfo = ident->getFromPos(tok)->typeInfo();
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
                        typeInfo.type = PythonSourceRoot::ReferenceBuiltInType;
                    } else {
                        // TODO figure out how to lookup Identifier
                        ident = m_identifiers.getIdentifier(text);
                        if (ident) {
                            typeInfo.type = PythonSourceRoot::ReferenceType;
                            typeInfo.referenceName = text;
                        } else if (isTypeHint)
                            m_module->setSyntaxError(tok, QString::fromLatin1("Unknown type '%1'").arg(text));

                    }
                    return tok;

                } else if (tok->isCode()) {
                    if (isTypeHint)
                        typeInfo.type = PythonSourceRoot::instance()->mapDataType(tok->text());
                    else
                        m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected code (%1)").arg(text));
                    return tok;
                }
            }
        }
        NEXT_TOKEN(tok)
    }

    return tok;
}

PythonToken *PythonSourceFrame::scanImports(PythonToken *tok)
{
    DEFINE_DBG_VARS

    int guard = 20;
    QStringList fromPackages, importPackages, modules;
    QString alias;
    bool isAlias = false;
    bool isImports = tok->previous()->token == PythonSyntaxHighlighter::T_KeywordImport;
    PythonToken *moduleTok = nullptr,
                *aliasTok = nullptr;

    while (tok && (guard--)) {
        QString text = tok->text();
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_KeywordImport:
            isImports = true;
            break;
        case PythonSyntaxHighlighter::T_KeywordFrom:
            if (isImports)
                m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected token '%1'").arg(text));
            break;
        case PythonSyntaxHighlighter::T_KeywordAs:
            isAlias = true;
            break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (!isLineEscaped(tok)) {
                guard = 0; // we are finished, bail out
                PREV_TOKEN(tok) // needed becase it is advanced in parent loop in scanline before check
                if (modules.size() > 0) {
                    if (isAlias)
                        m_module->setSyntaxError(tok, QLatin1String("Expected aliasname before newline"));
                    goto store_module;
                }
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
                m_module->setSyntaxError(tok, QString::fromLatin1("Parsed as Alias but analyzer disagrees. '%1'").arg(text));
            goto set_identifier;
        default:
            if (tok->isCode())
                m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected token '%1'").arg(text));
            break;
        }

next_token:
        if (guard > 0)
            NEXT_TOKEN(tok)
        continue;

        // we should only get here by explicit goto statements
set_identifier:
            if (!isAlias) {
                // previous text was a package
                if (modules.size())
                    importPackages << modules.takeLast();
                modules << text;
                moduleTok = tok;
                goto next_token;
            } else {
                alias = text;
                aliasTok = tok;
            }

// same code used from multiple places, keeps loop running
store_module:
        assert(modules.size() > 0 && "Modules set called by no modules in container");
        if (alias.size() > 0) {
            // store import and set identifier as alias
            PythonSourceImportModule *imp = m_imports.setModule(fromPackages + importPackages, modules[0], alias);
            PythonSourceRoot::TypeInfo typeInfo(imp->type());
            m_identifiers.setIdentifier(aliasTok, typeInfo);
        } else {
            // store import and set identifier as modulename
            PythonSourceImportModule *imp = m_imports.setModule(fromPackages + importPackages, modules[0]);
            PythonSourceRoot::TypeInfo typeInfo(imp->type());
            m_identifiers.setIdentifier(moduleTok, typeInfo);
        }
        alias.clear();
        importPackages.clear();
        modules.clear();
        isAlias = false;
        moduleTok = aliasTok = nullptr;
        goto next_token;
    }
    return tok;
}

// may return nullptr on error
PythonToken *PythonSourceFrame::scanAllParameters(PythonToken *tok, bool storeParameters)
{
    DEFINE_DBG_VARS

    // safely goes to closingParen of arguments list
    int parenCount = 0,
        safeGuard = 50;
    while(tok && (safeGuard--) > 0 &&
          tok->token != PythonSyntaxHighlighter::T_DelimiterMetaData)
    {
        // first we must make sure we clear all parens
        if (tok->token == PythonSyntaxHighlighter::T_DelimiterOpenParen)
            ++parenCount;
        else if (tok->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            --parenCount;

        if (parenCount == 0 && tok->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            return tok;

        // advance one token
        NEXT_TOKEN(tok)

        // scan parameters
        if (storeParameters && parenCount > 0) {
            if (tok->isIdentifier() ||
                tok->token == PythonSyntaxHighlighter::T_OperatorVariableParam ||
                tok->token == PythonSyntaxHighlighter::T_OperatorKeyWordParam)
            {
                tok = scanParameter(tok, parenCount); // also should handle typehints
                DBG_TOKEN(tok)
            }
        }
    }

    return tok;
}

PythonToken *PythonSourceFrame::scanParameter(PythonToken *paramToken, int &parenCount)
{
    DEFINE_DBG_VARS

    // safely constructs our parameters list
    int safeGuard = 20;

    // init empty
    PythonSourceParameter *param = nullptr;
    PythonSourceParameter::ParameterType paramType = PythonSourceParameter::InValid;

    // scan up to ',' or closing ')'
    while(paramToken && (safeGuard--) &&
          paramToken->token != PythonSyntaxHighlighter::T_DelimiterComma)
    {
        // might have subparens
        switch (paramToken->token) {
        case PythonSyntaxHighlighter::T_DelimiterOpenParen:
            ++parenCount;
            break;
        case PythonSyntaxHighlighter::T_DelimiterCloseParen:
            --parenCount;
            if (parenCount == 0)
                return paramToken->previous(); // caller must know where closing paren is
            break;
        case PythonSyntaxHighlighter::T_OperatorVariableParam:
            if (parenCount > 0)
                paramType = PythonSourceParameter::Variable;
            break;
        case PythonSyntaxHighlighter::T_OperatorKeyWordParam:
            if (parenCount > 0)
                    paramType = PythonSourceParameter::Keyword;
            break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (!isLineEscaped(paramToken))
                return paramToken->previous(); // caller must know where newline is at
            break;
        default:
            if (parenCount > 0) {
                if (paramToken->isIdentifier()) {
                    // check if we have default value
                    if (paramType == PythonSourceParameter::InValid) {
                        const PythonToken *nextTok = paramToken->next();
                        if (nextTok && nextTok->token == PythonSyntaxHighlighter::T_OperatorEqual)
                            paramType = PythonSourceParameter::PositionalDefault; // have default value
                        else
                            paramType = PythonSourceParameter::Positional;
                    }

                    // set identifier, first try with type hints
                    PythonSourceRoot::TypeInfo typeInfo = guessIdentifierType(paramToken);
                    if (!typeInfo.isValid())
                        typeInfo.type = PythonSourceRoot::ReferenceArgumentType;

                    PythonSourceIdentifier *ident = m_identifiers.setIdentifier(paramToken, typeInfo);
                    // set parameter
                    param = m_parameters.setParameter(paramToken, typeInfo, paramType);

                    // Change tokenValue
                    if (paramToken->token == PythonSyntaxHighlighter::T_IdentifierUnknown) {

                        PythonToken *tok = const_cast<PythonToken*>(paramToken);
                        if (m_parentFrame->isClass() && m_parameters.indexOf(param) == 0)
                            tok->token = PythonSyntaxHighlighter::T_IdentifierSelf;
                        else
                            tok->token = PythonSyntaxHighlighter::T_IdentifierDefined;
                    }

                    // repaint in highligher
                    const_cast<PythonSourceModule*>(m_module)->setFormatToken(paramToken);

                } else if(paramType != PythonSourceParameter::InValid) {
                    m_module->setSyntaxError(paramToken, QString::fromLatin1("Expected identifier, got '%1'")
                                                                             .arg(paramToken->text()));
                }
            }
        }

        NEXT_TOKEN(paramToken)
    }

    return paramToken;

}

const PythonSourceRoot::TypeInfo PythonSourceFrame::guessIdentifierType(const PythonToken *token)
{
    DEFINE_DBG_VARS

    PythonSourceRoot::TypeInfo typeInfo;
    if (token && (token = token->next())) {
        DBG_TOKEN(token)
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

bool PythonSourceFrame::isLineEscaped(PythonToken *tok) const {
    DEFINE_DBG_VARS
    PREV_TOKEN(tok)
    if (tok)
        return tok->token != PythonSyntaxHighlighter::T_DelimiterLineContinue;
    return false;
}


// --------------------------------------------------------------------------

PythonSourceModule::PythonSourceModule(PythonSourceRoot *root,
                                       PythonSyntaxHighlighter *highlighter) :
    PythonSourceListParentBase(this),
    m_root(root),
    m_rootFrame(this, this),
    m_highlighter(highlighter),
    m_rehighlight(false)
{
}

PythonSourceModule::~PythonSourceModule()
{
}

void PythonSourceModule::scanFrame(PythonToken *tok)
{
    const PythonSourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const_cast<PythonSourceFrame*>(frm)->scanFrame(tok);

    if (m_rehighlight)
        m_highlighter->rehighlight();
}

void PythonSourceModule::scanLine(PythonToken *tok)
{
    PythonSourceRoot::Indent indent;
    const PythonSourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const_cast<PythonSourceFrame*>(frm)->scanLine(tok, indent);
}

PythonSourceRoot::Indent PythonSourceModule::currentBlockIndent(const PythonToken *tok) const
{
    DEFINE_DBG_VARS

    PythonSourceRoot::Indent ind;
    if (!tok)
        return ind;

    const PythonSourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const PythonToken *beginTok = frm->token();

    if (frm == &m_rootFrame)
        ind.frameIndent = 0;
    else
        ind.frameIndent = _currentBlockIndent(beginTok);

    beginTok = tok;
    DBG_TOKEN(beginTok)

    PythonTextBlockData *txtData = tok->txtBlock();
    int indent = txtData->indent();
    int guard = 1000;
    while (txtData && (guard--) > 0) {
        if (!txtData->isEmpty()){
            if (indent > txtData->indent()) {
                // look for ':' if not found its a syntax error unless its a comment
                beginTok = txtData->findToken(PythonSyntaxHighlighter::T_DelimiterColon, -1);
                while (beginTok && (guard--) > 0) {
                    PREV_TOKEN(beginTok)
                    // make sure isn't a comment line
                    if (!beginTok || beginTok->token == PythonSyntaxHighlighter::T_Comment)
                        break; //goto parent loop, do previous line
                    else if(beginTok->token == PythonSyntaxHighlighter::T_DelimiterNewLine) {
                        guard = -1; // exit parent loop
                        break;
                    }
                }

                if (guard < 0)
                    setSyntaxError(beginTok, QString::fromLatin1("Blockstart without ':'"));
            }

        }

        if (guard > 0) // guard is 0 when we exit, no need to look up previous row
            txtData = txtData->previous();
    }

    if (guard < 0) {
        // we found it
        ind.previousBlockIndent = beginTok->txtBlock()->indent();
        ind.currentBlockIndent = _currentBlockIndent(beginTok);
        return ind;
    }

    // we dind't find any ':', must be in root frame
    assert(ind.frameIndent == 0 && "Should be in root frame here!");
    ind.previousBlockIndent = ind.currentBlockIndent = 0;
    return ind;
}

int PythonSourceModule::_currentBlockIndent(const PythonToken *tok) const
{
    DEFINE_DBG_VARS

    const PythonToken *beginTok = tok;
    DBG_TOKEN(beginTok)

    // get indent of first line after newline at frame start
    int newLines = 0;
    while(beginTok && newLines < 2) {
        switch (beginTok->token){
        case PythonSyntaxHighlighter::T_DelimiterLineContinue:
            --newLines; // newline escaped
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            if (beginTok->next()->token != PythonSyntaxHighlighter::T_BlockStart) {
                beginTok->txtBlock()->insertToken(PythonSyntaxHighlighter::T_BlockStart,
                                                  beginTok->startPos, 0);
            }
            break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            ++newLines;
            break;
        case PythonSyntaxHighlighter::T_Comment:
            if (newLines)
                --newLines; // don't count a comment as a indent
            break;
        default:
            if (newLines > 0 && beginTok->isCode())
                return beginTok->txtBlock()->indent();
        }
        NEXT_TOKEN(beginTok)
    }
    // we didn't find any block
    return 0;
}

void PythonSourceModule::setFormatToken(const PythonToken *tok, QTextCharFormat format)
{
    if (tok->txtBlock()->setReformat(tok, format))
        m_rehighlight = true; // only re-highlight if we succeed
}

void PythonSourceModule::setFormatToken(const PythonToken *tok)
{
    setFormatToken(tok, m_highlighter->getFormatToken(tok));
}

int PythonSourceModule::compare(const PythonSourceListNodeBase *left,
                                const PythonSourceListNodeBase *right)
{
    const PythonSourceModule *l = dynamic_cast<const PythonSourceModule*>(left),
                             *r = dynamic_cast<const PythonSourceModule*>(right);
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

const PythonSourceFrame *PythonSourceModule::getFrameForToken(const PythonToken *tok,
                                                        const PythonSourceFrame *parentFrame) const
{
    DEFINE_DBG_VARS

    // find the frame associated with this token
    if (empty())
        return parentFrame; // we have no children, this is it!

    const PythonSourceFrame *childFrm = nullptr;
    PythonSourceListNodeBase *itm = nullptr;

    for (itm = parentFrame->begin();
         itm != parentFrame->end();
         itm = itm->next())
    {
        if (tok >= childFrm->token() && tok <=childFrm->lastToken) {
            childFrm = dynamic_cast<const PythonSourceFrame*>(itm);
            // find recursivly
            if (childFrm) {
                if (!childFrm->empty())
                    return getFrameForToken(tok, dynamic_cast<const PythonSourceFrame*>(childFrm));
                // error during dynamic_cast
                return parentFrame;
            }
        }
    }

    return parentFrame;
}


void PythonSourceModule::setSyntaxError(const PythonToken *tok, QString parseMessage) const
{
    DEFINE_DBG_VARS

    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, parseMessage, PythonTextBlockScanInfo::SyntaxError);

    // create format with default format for syntax error
    const_cast<PythonToken*>(tok)->token = PythonSyntaxHighlighter::T_SyntaxError;
    QColor lineColor = m_highlighter->colorByType(SyntaxHighlighter::SyntaxError);

    // set text underline of all text in here
    // move to beginning of text
    const PythonToken *startTok = tok;
    DBG_TOKEN(startTok)

    // first move up to end of code if its a whitespace
    if (!startTok->isCode()) {
        int guard = 100;
        bool hasCode = false;
        while (guard--) {
            NEXT_TOKEN(startTok)
            if (hasCode) {
                if (!startTok->isCode())
                    break;
            } else
                hasCode = startTok->isCode();
        }
    }
    // then move down the line til first whitespace
    while(startTok && startTok->isCode()) {
        QTextCharFormat format = m_highlighter->getFormatToken(tok);
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        format.setUnderlineColor(lineColor);
        const_cast<PythonSourceModule*>(this)->setFormatToken(tok, format);

        // previous token
        if (startTok->previous()) {
            PREV_TOKEN(startTok)
        } else
            break;
    }
}

void PythonSourceModule::setIndentError(const PythonToken *tok) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, QLatin1String("Unexpected indent"), PythonTextBlockScanInfo::IndentError);

    // create format with default format for syntax error
    const_cast<PythonToken*>(tok)->token = PythonSyntaxHighlighter::T_SyntaxError;
    QColor lineColor = m_highlighter->colorByType(SyntaxHighlighter::SyntaxError);

    QTextCharFormat format = m_highlighter->getFormatToken(tok);
    format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    format.setUnderlineColor(lineColor);
    const_cast<PythonSourceModule*>(this)->setFormatToken(tok, format);
}

void PythonSourceModule::setMessage(const PythonToken *tok, QString parseMessage) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, parseMessage, PythonTextBlockScanInfo::Message);
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
