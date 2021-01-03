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
#define PY_FLOAT_FROM_STRING(str) PyFloat_FromDouble(strtod((str), nullptr))
#define PY_BOOL_FROM_STRING(str) PyBool_FromLong(strcmp((str), "True") == 0)




namespace Python {


static const uint BufSize = 1024;

/**
 * @brief The Haystack struct, compile time known strings to search for
 */
struct Haystack {
    Haystack(const char** items, size_t sz) : items(items), sz(sz) {}
    const char** items;
    const size_t sz;
};

static const char* _operators[] {
    // must be in most length first 2 chrs then 1
    // 2 chars long
    "==", "!=", ">=", "<=", "<<", ">>", "**", "//", ":="
    // 1 char long
    "^",  "<",  ">",  "=",  "+",  "-",  "*",  "/",  "%", "&", "|", "^", "~", "@"
};
static Haystack operStrs(_operators,  sizeof(_operators) / sizeof(*_operators));


static const char* _delimiters[] {
    // must be in most length first 3 char, the 2 then 1
    "//=", "**=","//=",">>=", "<<=", "//=",
    // 2 chars long
    "->",  "+=", "-=", "*=",  "/=",  "%=", "@=", "&=", "|=", "^=",
    // 1 char long
    ",", ":", ".", ";","@", "="
};
static Haystack delimStrs(_delimiters,  sizeof(_delimiters) / sizeof(*_delimiters));

static const char* _bool[] { "True", "False" };
static Haystack boolStrs(_bool, sizeof(_bool) / sizeof(*_bool));

static const char* _none[] { "None" };
static Haystack noneStrs(_none, sizeof(_none) / sizeof(*_none));

static const char* _keywords[] {
    "False",    "await",     "else",    "import",    "pass"
    "None",     "break",     "except",  "in",        "raise"
    "True",     "class",     "finally", "is",        "return"
    "and",      "continue",  "for",     "lambda",    "try"
    "as",       "def",       "from",    "nonlocal",  "while"
    "assert",   "del"        "global"   "not"        "with"
    "async",    "elif"       "if"       "or"         "yield"
};
static Haystack keywordStrs(_keywords, sizeof(_keywords) / sizeof(*_keywords));


/**
 * @brief The ParseTreeNode class base for our parse tree
 */
class ParseTreeNode {
    PyObject* vlu;
public:
    enum Type { Undefined, Root, Identifier, Attribute, Int,
                Float, Bool, String, None, Keyword,
                // delimiters and other stuff from down
                DelimiterStart, BracketOpen,  BraceOpen,  ParenOpen,
                BracketClose, BraceClose, ParenClose,
                Dot, Delimiter,
                // operators
                OperatorStart, Operator
              };
    ParseTreeNode *parent,
            *next, // chain in object retrieval
            *oper; // is for operator
    const char* startPos, *endPos, *c_txt;
    Type type;
    explicit ParseTreeNode(const char* startPos, const char* endPos, Type type)
        : vlu(nullptr)
        , parent(nullptr)
        , next(nullptr)
        , oper(nullptr)
        , startPos(startPos)
        , endPos(endPos)
        , type(type)
    {
        size_t len = static_cast<size_t>(endPos - startPos);
        c_txt = static_cast<const char*>(calloc(len+1, sizeof(const char)));
        strncpy(const_cast<char*>(c_txt), startPos, len);
    }
    ~ParseTreeNode()
    {
        delete next;
        delete oper;
        free(const_cast<char*>(c_txt));
        Py_XDECREF(vlu);
    }

    void nodeAsChild(ParseTreeNode* node) {
        node->parent = this;
        next = node;
    }
    void nodeAsOperator(ParseTreeNode* node) {
        node->parent = this;
        oper = node;
    }

    int position(const char* lineStart) { return static_cast<int>(startPos - lineStart); }

    QString txt() const {
        return QString::fromLatin1(startPos, static_cast<int>(endPos - startPos));
    }

    void setPyObject(PyObject* value) { vlu = value; Py_XINCREF(vlu); }
    PyObject *pyObject() { return vlu; }
};

bool parse(const char** iter, const char* end, ParseTreeNode* node);

void skipUtf8(const char** iter, const char* end) {
    while(*iter < end && **iter & 0x80)
        ++(*iter);
}

bool parse_tailcall(const char** iter,  const char* end, const char* start,
                    ParseTreeNode *parent, ParseTreeNode::Type type) {
    if (*iter > start) {
        if (type >= ParseTreeNode::BracketOpen) {
            parent->nodeAsOperator(new ParseTreeNode(start, *iter, type));
            return parse(iter, end, parent);
        }

        parent->nodeAsChild(new ParseTreeNode(start, *iter, type));
        return parse(iter, end, parent->next);
    }
    return (*iter > start);
}

// parses identifiers variables, properties and such
bool parse_ident(const char** iter, const char* end, ParseTreeNode* parent) {
    const char* start = *iter;
    for(; **iter !=0 && *iter < end; ++(*iter)) {
        if (!isalpha(**iter)) {
            if (isdigit(**iter) && *iter > start) // allow a980 as varname
                continue;
            else if (**iter == '_')
                continue;
            else if (**iter & 0x80)
                skipUtf8(iter, end);
            else
                break;
        }
    }

    ParseTreeNode::Type tp = parent->oper &&
                             parent->oper->type == ParseTreeNode::Dot ?
                                     ParseTreeNode::Attribute : ParseTreeNode::Identifier;
    return parse_tailcall(iter, end, start, parent, tp);
}

// parses numbers in source code
bool parse_number(const char** iter, const char* end, ParseTreeNode* parent) {
    const char* start = *iter;
    ParseTreeNode::Type type = ParseTreeNode::Int;
    for(;**iter !=0 && *iter < end; ++(*iter)) {
        if (!isdigit(**iter) && *iter > start) {
            if (**iter == '.')
            {   type = ParseTreeNode::Float; continue; }
            else if (isalnum(**iter) && type != ParseTreeNode::Float) // allow a980 as varname
                continue;
            else if (isdigit(**iter))
                continue;
            else if (**iter == '_') // complex numbers in python
                continue;
        }
        break;
    }
    return parse_tailcall(iter, end, start, parent, type);
}

// parses strings
bool parse_string(const char** iter, const char* end, ParseTreeNode* parent) {
    const char* start = *iter;
    char startingCh = 0;
    for(;**iter !=0 && *iter < end; ++(*iter)) {
        if ((**iter == '"' || **iter == '\'') && *iter == start)
            startingCh = **iter;
        else if (*iter > start) {
            if (**iter == startingCh && *((*iter)-1) != '\\')
                break;
            else if (**iter == '\n') {// eol before ending char
                *iter = start; // indicate failure
                break;
            }
        }
    }
    return parse_tailcall(iter, end, start, parent, ParseTreeNode::String);
}

static char oppositeStack[BufSize];
static size_t stackIdx = 0;

// parse matching char '[', '(', '{'
bool parse_opposite_open(const char** iter, const char openCh, const char closingCh,
                   const char* end, ParseTreeNode* parent, ParseTreeNode::Type type) {
    const char* start = *iter;
    if (**iter == openCh) {
        if (stackIdx == BufSize) return false;
        oppositeStack[stackIdx++] = closingCh;
        ++(*iter);
        if (!parse_tailcall(iter, end, start, parent, type))
            *iter = start;
    }
    return *iter > start;
}
// parse matching char ']', ')', '}'
bool parse_opposite_close(const char** iter, const char openCh, const char closingCh,
                   const char* end, ParseTreeNode* parent, ParseTreeNode::Type type) {
    const char* start = *iter;
    if (**iter == closingCh && stackIdx > 0) {
        while(oppositeStack[--stackIdx] != openCh)
            if (stackIdx == 0) {
                if (oppositeStack[0] != closingCh)
                    return false;
                break;
            }
        ++(*iter);
        if (!parse_tailcall(iter, end, start, parent, type))
            *iter = start;
        else
            return true;
    }
    return *iter > start;
}

// parse '.'
bool parse_dot(const char** iter, const char* end, ParseTreeNode* parent) {
    const char* start = *iter;
    if (**iter == '.') {
        ++(*iter);
        if (!parse_tailcall(iter, end, start, parent, ParseTreeNode::Dot))
            *iter = start;
    }
    return *iter > start;
}

// parse '=', '==' etc operartors, delimiters, keywords etc
bool parse_tokens(const char** iter, const char* end, Haystack& s,
                  ParseTreeNode* parent, ParseTreeNode::Type type) {
    const char* start = *iter;
    for (size_t i = 0; i < s.sz; ++i) {
        if (strncmp(*iter, s.items[i], strlen(s.items[i])) == 0) {
            (*iter) += strlen(s.items[i]);
            if (!parse_tailcall(iter, end, start, parent, type))
                *iter = start;
        }

    }
    return *iter > start;
}


// parse delegate function
bool parse(const char** iter, const char* end, ParseTreeNode* node) {
    const char* start = *iter;
    bool success = true;

    while (**iter != 0 && *iter < end && success) {
        success = false;
        if (isspace(**iter))
            { ++(*iter); success = true; continue; }
        if (!success && **iter == '.')
            success = parse_dot(iter, end, node);
        if (!success && **iter == '[')
            success = parse_opposite_open(iter, '[', ']', end, node, ParseTreeNode::BracketOpen);
        if (!success && **iter == ']')
            success = parse_opposite_close(iter, '[', ']', end, node, ParseTreeNode::BracketClose);
        if (!success && **iter == '(')
            success = parse_opposite_open(iter, '(', ')', end, node, ParseTreeNode::ParenOpen);
        if (!success && **iter == ')')
            success = parse_opposite_close(iter, '(', ')', end, node, ParseTreeNode::ParenClose);
        if (!success && **iter == '{')
            success = parse_opposite_open(iter, '{', '}', end, node, ParseTreeNode::BraceOpen);
        if (!success && **iter == '}')
            success = parse_opposite_close(iter, '{', '}', end, node, ParseTreeNode::BraceOpen);
        if (!success && (**iter == 'T' || **iter == 'F'))
            success = parse_tokens(iter, end, boolStrs, node, ParseTreeNode::Bool);
        if (!success)
            success = parse_tokens(iter, end, operStrs, node, ParseTreeNode::Operator);
        if (!success)
            success = parse_tokens(iter, end, delimStrs, node, ParseTreeNode::Delimiter);
        if (!success)
            success = parse_tokens(iter, end, noneStrs, node, ParseTreeNode::None);
        if (!success)
            success = parse_tokens(iter, end, keywordStrs, node, ParseTreeNode::Keyword);
        if (!success )
            success = parse_ident(iter, end, node);
        if (!success )
        if (!success && **iter == '#')
         {    success = true;  break; }// end of line
        success = *iter == end;
    }

    if (!success)
        *iter = start;

    return (*iter == end) || (*iter > start);
}

// get the owner this attr
PyObject* getOwnerForAttrObj(ParseTreeNode* attrNode) {
    ParseTreeNode* owner = attrNode->parent;

    if (owner) {
        if (!owner->pyObject())
            getOwnerForAttrObj(attrNode->parent); // look in parent

        if (owner->pyObject()) {
            auto key = PY_FROM_STRING(attrNode->c_txt);
            if (PyObject_HasAttr(attrNode->pyObject(), key))
                attrNode->setPyObject(PyObject_GetAttr(owner->pyObject(), key)); // ParseTreeNode handles refcnt
            Py_XDECREF(key);
            return attrNode->pyObject();
        }
    }
    return nullptr;
}

// returns new reference
PyObject* keyFromType(ParseTreeNode* node) {
    if (!node) return nullptr;
    switch (node->type) {
    case ParseTreeNode::Int:
        return PY_LONG_FROM_STRING(node->c_txt, nullptr, 0);
    case ParseTreeNode::Float:
        return PY_FLOAT_FROM_STRING(node->c_txt);
    case ParseTreeNode::Bool:
        return PY_BOOL_FROM_STRING(node->c_txt);
    case ParseTreeNode::String:
    case ParseTreeNode::Attribute:
    case ParseTreeNode::Identifier:
        return PY_FROM_STRING(node->c_txt);
    default:
        return nullptr;
    }
}

/// looks up identifier from currentframe
bool lookupIdent(ParseTreeNode *ident) {

    Base::PyGILStateLocker lock; (void)lock;
    auto dbgr = App::Debugging::Python::Debugger::instance();
    auto frame = dbgr->currentFrame();
    PyObject* obj = nullptr;

    auto key = keyFromType(ident); // returns new
    if (key) {
        // lookup through parent frames chain to top of stack
        do {
            PyFrame_FastToLocals(frame);
            if (PyDict_Contains(frame->f_locals, key))
                obj = PyDict_GetItem(frame->f_locals, key);
        } while (!obj && (frame = frame->f_back));

        // globals?
        if (!obj) {
            frame = dbgr->currentFrame();
            if (PyDict_Contains(frame->f_globals, key))
                obj = PyDict_GetItem(frame->f_globals, key);
        }

        // builtins?
        if (!obj) {
            if (PyDict_Contains(frame->f_builtins, key))
                obj = PyDict_GetItem(frame->f_builtins, key);
        }

        Py_DECREF(key);

        if (obj)
            ident->setPyObject(obj);
    }

    return obj != nullptr;
}

/// get the value for node
PyObject* getValue(ParseTreeNode* node) {
    Base::PyGILStateLocker lock;(void)lock;
    PyObject* ret = nullptr,
            * self = node->pyObject();
    // identifiers should already be looked up?
    if (!self) {
        if (node->type == ParseTreeNode::Identifier) {
            if (!lookupIdent(node))
                return nullptr;

        } else if (!self && node->type == ParseTreeNode::Attribute) {
            auto owner = getOwnerForAttrObj(node);
            auto key = keyFromType(node);
            if (owner && PyObject_HasAttr(owner, key))
                self = PyObject_GetAttr(owner, key);
            Py_XDECREF(key);

        } else if (node->type < ParseTreeNode::DelimiterStart) {
            // must be a string or number or bool
            auto vlu = keyFromType(node);
            node->setPyObject(vlu);
            Py_XDECREF(vlu); // now owned by node
            return node->pyObject();
        }
    }

    if (!self)
        return nullptr;

    if (node->oper) {
        PyObject* key = keyFromType(node->next);
        Py_XINCREF(key);

        if (node->oper->type == ParseTreeNode::BracketOpen) {
            if (key) {
                // look up value within brackets
                if (node->next)
                    ret = getValue(node->next);

                // get value from our collection type
                if (PyList_Check(self)) {
                    if (PyLong_Check(key))
                        ret = PyList_GET_ITEM(self, PyLong_AsLong(key));
                } else if (PyTuple_Check(node)) {
                    if (PyLong_Check(key))
                        ret = PyTuple_GET_ITEM(self, PyLong_AsLong(key));
                } else if (PyDict_Check(self)) {
                    if (PyDict_Contains(self, key))
                        ret = PyDict_GetItem(self, key);
                }
                // Set objects can't be accessed by []
            }
        } else if (node->oper->type == ParseTreeNode::Dot) {
            if (PyAnySet_Check(node)) // named sets set.key
                ret = PyObject_Repr(self);
            else if (PyObject_HasAttr(self, key))
                ret = PyObject_GetAttr(self, key);
        } else {
            // its not a container value
            ret = self;
        }

        Py_XDECREF(key);
    }

    return ret;
}



class CodeP
{
public:
    CodeP()
    {  }
    ~CodeP()
    {  }
};

} // namespace Python


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
PyObject *Python::Code::deepCopy(PyObject *obj) const
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



QString Python::Code::findFromCurrentFrame(const QString lineText, int pos, const QString word) const
{
    QString ret; // the text to return

    // walk the line to see if we can intrep it

    auto str = lineText.toStdString(); // need to take immidiery here
    const char* chPtr = str.c_str();
    const char* end = chPtr + strnlen(chPtr, BufSize);
    const char* start = chPtr;

    ParseTreeNode *root = new ParseTreeNode(start, start, ParseTreeNode::Root),
                  *current = nullptr;

    PyObject *vlu = nullptr;
    bool success = false;

    // Parse source code to tokens
    success = parse(&chPtr, end, root);

    //  we have parsed succesfully
    if (success) {
        Base::PyGILStateLocker lock; (void)lock;
        current = root;

        PyObject *initialType, *initialValue, *initialTraceback;
        PyErr_Fetch(&initialType, &initialValue, &initialTraceback);


        // goto end of tree, look up all litterals, insert into current
        do {
            if (current->type == ParseTreeNode::Identifier)
                lookupIdent(current);
        } while(current->next && (current = current->next));

        // goto the litteral we want
        current = root;
        while (current && static_cast<int>(current->startPos - start) < pos) {
            if (static_cast<int>(current->endPos - start) > pos)
                break;
            current = current->next;
        }

        if (current) {
            // we have found the litteral we want
            vlu = getValue(current);

            if (vlu) {
                auto vluStr = PyObject_Str(vlu);
                const char *msg = PY_AS_STRING(vluStr);
                ret = QString::fromUtf8(msg);
            }
        }

        PyErr_Clear();
        PyErr_Restore(initialType, initialValue, initialTraceback);
    }

    delete root;

    return ret;
}


// get thee root of the parent identifier ie os.path.join
//                                                    ^
// must traverse from os, then os.path before os.path.join
QString Python::Code::findFromCurrentFrame(const Python::Token *tok) const
{
    auto debugger = App::Debugging::Python::Debugger::instance();
    Base::PyGILStateLocker locker;
    PyFrameObject *frame = debugger->currentFrame();
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
        obj = getDeepObject(debugger->currentFrame()->f_globals,
                            tok, foundKey);
    // or in builtins
    if (!obj)
        obj = getDeepObject(debugger->currentFrame()->f_builtins,
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
                                      QString &foundKey) const
{
    DEFINE_DBG_VARS

    Base::PyGILStateLocker locker;
    PyObject *keyObj = nullptr, *tmp = nullptr, *outObj = obj;

    QList<const Python::Token*> chain;
    const Python::Token *tok = needleTok;
    bool lookupSubItm = needleTok->type() == Python::Token::T_DelimiterOpenBracket;
    if (lookupSubItm)
        PREV_TOKEN(tok)
    // search up to root ie cls.attr.dict[stringVariable]
    //                               ^
    //                          ^
    //                      ^
    while (tok){
        if (tok->isIdentifierVariable()) {
            chain.prepend(tok);
        } else if (tok->type() != Python::Token::T_DelimiterPeriod)
            break;
        PREV_TOKEN(tok)
    }

    if (chain.size() == 0)
        return nullptr;

    for (int i = 0; i < chain.size(); ++i) {
        keyObj = PY_FROM_STRING(chain[i]->text().c_str());
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
                    foundKey = QString::fromStdString(chain[i]->text());
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
            if (tok->type() == Python::Token::T_DelimiterOpenBracket) {
                NEXT_TOKEN(tok)
                needle = getDeepObject(outObj, tok, tmp);
                break;
            } else if (tok->isIdentifierVariable()) {
                needle = getDeepObject(outObj, tok, tmp);
                break;
            } else if (tok->isNumber()) {
                needle = PY_LONG_FROM_STRING(tok->text().c_str(), nullptr, 0);
                if (!needle)
                    PyErr_Clear();
            } else if (tok->type() != Python::Token::T_DelimiterPeriod) {
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
