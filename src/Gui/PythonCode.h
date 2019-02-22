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

#ifndef PYTHONCODE_H
#define PYTHONCODE_H

#include "Base/Interpreter.h"

#include "Window.h"
#include "SyntaxHighlighter.h"


class QEventLoop;

namespace Gui {

class PythonSyntaxHighlighter;
class PythonSyntaxHighlighterP;
class PythonEditorBreakpointDlg;
class PythonCodeP;
class PythonTextBlockData;
class PythonToken;
class TextEdit;


/**
 * @brief Handles code inspection from the python engine internals
 */
class PythonCode : QObject
{
    Q_OBJECT
public:
    PythonCode(QObject *parent = 0);
    virtual ~PythonCode();


    // copies object and all subobjects
    PyObject* deepCopy(PyObject *obj);

    QString findFromCurrentFrame(QString varName);

    // get the root of the parent identifier ie os.path.join
    //                                                    ^
    // must traverse from os, then os.path before os.path.join
    Py::Object getDeepObject(PyObject *obj, QString key, QString &foundKey);

private:
    PythonCodeP *d;
};



// ----------------------------------------------------------------------

/**
 * @brief The PythonTextBlockScanInfo class stores scaninfo for this row
 *          Such as SyntaxError annotations
 */
class PythonTextBlockScanInfo
{
public:
    enum MsgType { Message, SyntaxError, AllMsgTypes };
    struct ParseMsg {
        explicit ParseMsg(QString message, int start, int end, MsgType type) :
                    message(message), startPos(start),
                    endPos(end), type(type)
        {}
        ~ParseMsg() {}
        QString message;
        int startPos;
        int endPos;
        MsgType type;
    };
    typedef QList<ParseMsg> parsemsgs_t;
    explicit PythonTextBlockScanInfo();
    ~PythonTextBlockScanInfo();

    /// set message for token
    void setParseMessage(const PythonToken *tok, QString message, MsgType type = Message);
    /// set message at line with startPos - endPos boundaries
    void setParseMessage(int startPos, int endPos, QString message, MsgType type = Message);
    /// get parseMessage for token, filter by type
    QString parseMessage(const PythonToken *tok, MsgType type = AllMsgTypes) const;
    /// get parseMessage for line contained by col, filter by type
    QString parseMessage(int col, MsgType type = AllMsgTypes) const;
    /// clear message
    void clearParseMessage(const PythonToken *tok);
    /// clears message that is contained by col
    void clearParseMessage(int col);
    /// clears all messages on this line
    void clearParseMessages();

    /// get all parseMessages for this module
    const parsemsgs_t allMessages() const { return m_parseMessages; }

private:
    parsemsgs_t m_parseMessages;
};


// -----------------------------------------------------------------------



// Begin AST Python source code introspection
// -------------------------------------------------------------------

class PythonSourceListParentBase;
class PythonSourceIdentifier;
class PythonSourceFrame;
class PythonSourceModule;
class PythonSourceModuleList;
class PythonSourceParameter;
class PythonSourceParameterList;
class PythonSourceImportPackage;
class PythonSourceImportModule;
class PythonSourceImportList;

/**
 * @brief The PythonCodeTree class hold all frames, vars and other identifiers
 * used to get info about src
 * Is intended as a singleton!
 */
class PythonSourceRoot : public QObject
{
    Q_OBJECT
public:
    typedef int CustomNameIdx_t;
    enum DataTypes {
        // changes here must be accompanied by changing in typeAsStr and mapDataType
        InValidType,
        UnknownType,
        ReferenceType, // references another variable
        ReferenceBuiltinType, // references a builtin identifier
        NoneType,
        BoolType,
        IntType,
        FloatType,
        StringType,
        BytesType,
        ListType,
        TupleType,
        SetType,
        RangeType,
        DictType,
        FunctionType,
        ClassType,
        MethodType,
        CustomType
    };

    struct TypeInfo {
        explicit TypeInfo();
        TypeInfo(const TypeInfo &other);
        ~TypeInfo();
        PythonSourceRoot::DataTypes type;
        CustomNameIdx_t customNameIdx;
        QString referenceName;
        QString typeAsStr() const;
        QString customName() const;
        bool operator ==(const TypeInfo &other) const {
            return type == other.type &&
                   customNameIdx == other.customNameIdx &&
                   referenceName == other.referenceName;
        }
        bool operator !=(const TypeInfo &other) const {
            return false == (*this == other);
        }
        TypeInfo operator =(const TypeInfo &other) {
            type = other.type;
            customNameIdx = other.customNameIdx;
            referenceName = other.referenceName;
            return *this;
        }
    };

    // please not that it must be a singleton!
    explicit PythonSourceRoot();
    virtual ~PythonSourceRoot();

    /// get a global reference to our singleton instance
    static PythonSourceRoot *instance();

    /// Gets a stringlist of all loaded modules names
    QStringList modulesNames() const;
    /// Gets a stringlist of all loaded modules paths
    QStringList modulesPaths() const;
    /// gets modules count, -1 if none yet
    int modulesCount() const;
    /// get Module at idx, nullptr if not found
    PythonSourceModule *moduleAt(int idx) const;
    /// get Module from path
    PythonSourceModule *moduleFromPath(QString filePath) const;
    /// get reference to our modules collection
    const PythonSourceModuleList *modules() const { return m_modules; }

    /// map typestr from metadata Type annotation, ie x: int
    DataTypes mapDataType(const QString typeAnnotation) const;

    // typename database for custom types
    QString customTypeNameFor(CustomNameIdx_t customIdx);
    CustomNameIdx_t addCustomTypeName(const QString name);
    CustomNameIdx_t indexOfCustomTypeName(const QString name);

    /// scans complete filePath, clears all old and re-doe it
    PythonSourceModule *scanCompleteModule(const QString filePath,
                                           PythonSyntaxHighlighter *highlighter);
    /// re-scan a single QTextBlock, as in we are typing
    PythonSourceModule *scanSingleRowModule(const QString filePath, PythonTextBlockData *row);

private:
    QHash<CustomNameIdx_t, QString> m_customTypeNames;
    CustomNameIdx_t m_uniqueCustomTypeNames;
    PythonSourceModuleList *m_modules;
    static PythonSourceRoot *m_instance;
};

// -------------------------------------------------------------------------

/// base class, produces a double linked list
class PythonSourceListNodeBase {
public:
    explicit PythonSourceListNodeBase(PythonSourceListParentBase *owner);
    virtual ~PythonSourceListNodeBase();
    PythonSourceListNodeBase *next() const { return m_next; }
    PythonSourceListNodeBase *previous() const { return m_previous; }

    void setNext(PythonSourceListNodeBase *next) { m_next = next; }
    void setPrevious(PythonSourceListNodeBase *previous) { m_previous = previous; }
    /// python token
    const PythonToken *token() const { return m_token; }
    void setToken(const PythonToken *token) { m_token = token; }

    /// gets text for token (gets from document)
    QString text() const;

    /// owner node, setOwner is essential during cleanup, or ownership swaps
    PythonSourceListParentBase *owner() const { return m_owner; }
    void setOwner(PythonSourceListParentBase *owner) { m_owner = owner; }

    /// called by PythonToken when it gets destroyed
    virtual void tokenDeleted();

protected:
    PythonSourceListNodeBase *m_previous;
    PythonSourceListNodeBase *m_next;
    PythonSourceListParentBase *m_owner;
    const PythonToken *m_token;
};

// -------------------------------------------------------------------------

/// base class for collection, manages a linked list, might itself be a node i a linked list
class PythonSourceListParentBase : public PythonSourceListNodeBase
{
public:
    typedef PythonSourceListNodeBase iterator_t;
    explicit PythonSourceListParentBase(PythonSourceListParentBase *owner);
    ~PythonSourceListParentBase();

    /// adds node to current collection
    virtual void insert(PythonSourceListNodeBase *node);
    /// remove node from current collection
    /// returns true if node was removed (was in collection)
    /// NOTE! if deleteNode is false: caller is responsible for deleting node
    virtual bool remove(PythonSourceListNodeBase *node, bool deleteNode = false);
    /// true if node is contained
    bool contains(PythonSourceListNodeBase *node) const;
    bool empty() const;
    /// returns size of list
    std::size_t size() const;
    /// returns index of node or -1 if not found
    std::size_t indexOf(PythonSourceListNodeBase *node) const;
    /// returns node at idx
    PythonSourceListNodeBase *operator [](std::size_t idx) const;

    /// stl iterator stuff
    PythonSourceListNodeBase* begin() const { return m_first; }
    PythonSourceListNodeBase* rbegin() const { return m_last; }
    PythonSourceListNodeBase* end() const { return nullptr; }
    PythonSourceListNodeBase* rend() const { return nullptr; }

protected:
    /// subclass may implement, used to sort insert order
    ///     should return -1 if left is more than right
    ///                   +1 if right is more than left
    ///                    0 if both equal
    virtual int compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const;
    PythonSourceListNodeBase *m_first;
    PythonSourceListNodeBase *m_last;
    bool m_preventAutoRemoveMe; // set to true on root container,
                                // sub containers removes themselfs from here
                                // if container gets empty default behaviour is
                                // to let parent delete me
                                // true here sets prevents this
};

// ----------------------------------------------------------------------

/// a class for each assignment within frame (variable type might mutate)
class PythonSourceIdentifierAssignment : public PythonSourceListNodeBase {
    PythonSourceRoot::TypeInfo m_type;
    int m_linenr; // cached result to speed up things
public:
    explicit PythonSourceIdentifierAssignment(PythonSourceIdentifier *owner,
                                              const PythonToken *startToken,
                                              PythonSourceRoot::TypeInfo typeInfo);
    ~PythonSourceIdentifierAssignment();
    const PythonSourceRoot::TypeInfo type() const { return m_type; }
    void setType(PythonSourceRoot::TypeInfo &type) { m_type = type; } // type is implicitly copied
    int linenr();
};

// ----------------------------------------------------------------------

/// identifierClass, one for each variable in each frame
class PythonSourceIdentifier : public PythonSourceListParentBase
{
    PythonSourceFrame *m_frame;
public:
    explicit PythonSourceIdentifier(PythonSourceListParentBase *owner, PythonSourceFrame *frame);
    ~PythonSourceIdentifier();

    /// get the last assigment up til line and pos in document
    /// or nullptr if not found
    PythonSourceIdentifierAssignment *getFromPos(int line, int pos) const;
    PythonSourceIdentifierAssignment *getFromPos(const PythonToken *tok) const;

    /// get the name of identifier
    QString name() const;
protected:
    // should sort by linenr and (if same line also token startpos)
    int compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const;
};

// ----------------------------------------------------------------------------
/// a root container for all identifiers
class PythonSourceIdentifierList : public PythonSourceListParentBase
{
    PythonSourceFrame *m_frame;
public:
    explicit PythonSourceIdentifierList(PythonSourceFrame *frame);
    ~PythonSourceIdentifierList();

    /// get the frame contained for these collections
    const PythonSourceFrame *frame() const { return m_frame; }
    /// get the identifier with name or nullptr if not contained
    const PythonSourceIdentifier *getIdentifier(const QString name) const;
    bool hasIdentifier(const QString name) const { return getIdentifier(name) != nullptr; }
    /// sets a new assignment, creates identifier if not exists
    PythonSourceIdentifier *setIdentifier(const PythonToken *tok,
                                          PythonSourceRoot::TypeInfo typeInfo);

protected:
    int compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------
// class for function/method parameters
class PythonSourceParameter : public PythonSourceListNodeBase
{
public:
    /// type for this parameter, ie (arg1, arg2=0, *arg3, **arg4)
    ///  translates to      (Positional, PositionalDefault, Variable, Keyword)
    enum ParameterType {
        InValid,
        Positional,
        PositionalDefault,
        Variable,
        Keyword
    };
    explicit PythonSourceParameter(PythonSourceParameterList *parent);
    ~PythonSourceParameter();

    /// get the identifierAssignment of this argument
    PythonSourceIdentifierAssignment *identifierAssignment() const;

    /// type for this parameter, ie (arg1, arg2=0, *arg3, **arg4)
    ///  translates to      (Positional, PositionalDefault, Variable, Keyword)
    ParameterType parameterType() const { return m_paramType; }
    void setParameterType(ParameterType paramType) { m_paramType = paramType; }

    const PythonSourceRoot::TypeInfo type() const { return m_type; }
    void setType(PythonSourceRoot::TypeInfo &type) { m_type = type; } // type is implicitly copied


private:
    PythonSourceRoot::TypeInfo m_type;
    ParameterType m_paramType;
};

// ----------------------------------------------------------------------

class PythonSourceParameterList : public PythonSourceListParentBase
{
    PythonSourceFrame *m_frame;
public:
    explicit PythonSourceParameterList(PythonSourceFrame *frame);
    ~PythonSourceParameterList();

    /// get the frame contained for these collections
    const PythonSourceFrame *frame() const { return m_frame; }
    /// get the parameter with name or nullptr if not contained
    const PythonSourceParameter *getParameter(const QString name) const;
    bool hasParameter(const QString name) const { return getParameter(name) != nullptr; }
    /// updates param type and or creates parameter if not exists
    PythonSourceParameter *setParameter(const PythonToken *tok,
                                         PythonSourceRoot::TypeInfo typeInfo,
                                         PythonSourceParameter::ParameterType paramType);

protected:
    int compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const;
};

// --------------------------------------------------------------------

// a single import module, not a package
class PythonSourceImportModule : public PythonSourceListNodeBase
{
    PythonSourceFrame *m_frame;
    QString m_modulePath; // filepath to python file
    QString m_aliasName; // alias name in: import something as other <- other is alias

public:
    explicit PythonSourceImportModule(PythonSourceImportPackage *parent,
                                      PythonSourceFrame *frame,
                                      QString alias);
    ~PythonSourceImportModule();

    /// returns the name as used when lookup, ie baseName or alias if present
    QString name() const;
    QString alias() const { return m_aliasName; }
    QString path() const { return m_modulePath; }
    void setPath(QString path) { m_modulePath = path; }
    /// we use defered load, this function explicitly loads module
    void load();
    bool isLoaded() const;

    /// true when its a C module
    bool isBuiltIn() const;
    /// false when import couldn't find file
    bool isValid() const;

    const PythonSourceFrame *frame() const { return m_frame; }
};

// ---------------------------------------------------------------------

class PythonSourceImportPackage : public PythonSourceListParentBase
{
    QString m_packagePath; // filepath to folder for this package
    QString m_name;
    PythonSourceModule *m_ownerModule;
public:
    explicit PythonSourceImportPackage(PythonSourceImportPackage *parent,
                                       QString name, PythonSourceModule *ownerModule);
    ~PythonSourceImportPackage();

    /// get import package from filePath
    virtual PythonSourceImportPackage *getImportPackagePath(QString filePath);
    /// get import instance from name (basename of filepath or alias)
    virtual PythonSourceImportPackage *getImportPackage(QString name);

    /// get import module instance from filePath
    virtual PythonSourceImportModule *getImportModulePath(QString filePath);
    /// get import module instance from name (basename of filepath or alias)
    virtual PythonSourceImportModule *getImportModule(QString name);


    /// lookup module and return it
    /// if not stored in list it creates it
    virtual PythonSourceImportModule *setModule(QString name, QString alias,
                                        PythonSourceFrame *frame);

    virtual PythonSourceImportPackage *setPackage(QString name);

    QString name() const { return m_name; }
    QString path() const { return m_packagePath;}

protected:
    int compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------

class PythonSourceImportList : public PythonSourceImportPackage
{
    PythonSourceFrame *m_frame;
public:
    explicit PythonSourceImportList(PythonSourceFrame *owner,
                                    PythonSourceModule *ownerModule);
    ~PythonSourceImportList();

    /// returns package instance stored in 'filePath' from list
    PythonSourceImportModule *getImportModulePath(QString filePath);
    /// returns module instance from list with 'name' in 'rootPackage'
    /// rootPackage might be empty, ie: 'import file2' <- here rootPackage is empty
    PythonSourceImportModule *getImportModule(QString rootPackage, QString name);
    /// returns module instance form list with module paths as QStingList
    ///  ie: import sys.path.sub
    PythonSourceImportModule *getImportModule(QStringList modInheritance);

    /// returns package instance stored in 'filePath' from list
    PythonSourceImportPackage *getImportPackagePath(QString filePath);
    /// returns package instance from list with 'name' in rootPackage
    /// rootPackage might be empty, ie: import sys <- here rootPackage is empty
    PythonSourceImportPackage *getImportPackage(QString rootPackage, QString name);
    /// returns package instance form list with module paths as QStringList
    ///  ie: import sys.path.sub
    PythonSourceImportPackage *getImportPackage(QStringList modInheritance);

    /// set import inserts module at packages path
    /// If already inserted we just return the module
    /// returns Module instance
    PythonSourceImportModule *setModule(QStringList rootPackage,
                                        QString module,
                                        QString alias = QString());

    PythonSourceImportPackage *setPackage(QStringList rootPackage);

    /// set import inserts module at packages path
    /// If already inserted we just return the module
    /// returns Module instance
    PythonSourceModule *setModuleGlob(QStringList rootPackage);
};

// ---------------------------------------------------------------------

/// for function or method frames, contains all identifiers within this frame
class PythonSourceFrame : public PythonSourceListParentBase
{
    PythonSourceListParentBase *m_owner;
    /// stores all identifiers contained within each frame
    PythonSourceIdentifierList m_identifiers;
    PythonSourceParameterList m_parameters;
    PythonSourceImportList    m_imports;
    PythonSourceListParentBase *m_returns;
    PythonSourceFrame *m_parentFrame;
    PythonSourceModule *m_module;
    bool m_isClass;

public:
    explicit PythonSourceFrame(PythonSourceFrame *owner, PythonSourceModule *module,
                               PythonSourceFrame *parentFrame = nullptr, bool isClass = false);
    explicit PythonSourceFrame(PythonSourceModule *owner, PythonSourceModule *module,
                               PythonSourceFrame *parentFrame = nullptr, bool isClass = false);
    ~PythonSourceFrame();


    /// get the parent function frame
    PythonSourceFrame *parentFrame() const { return m_parentFrame; }
    void setParentFrame(PythonSourceFrame *frame) { m_parentFrame = frame; }

    PythonSourceModule *module() const { return m_module; }

    /// get the docstring for this function/method
    QString docstring();

    /// get the return type hint
    ///  def funt(arg1, arg2) -> Module.Class.Custom:
    ///    would return:         ^
    ///    CustomType with CustomTypeName == Module.Class.Custom
    /// returns Typeinfo::InValidType if no typeHint given
    PythonSourceRoot::TypeInfo returnTypeHint();

    /// true if we have identifier
    bool hasIdentifier(const QString name) const {
        return m_identifiers.getIdentifier(name);
    }

    /// is root frame for class
    bool isClass() const { return m_isClass; }

    /// looks up name among identifiers (variables/constants)
    const PythonSourceIdentifier *getIdentifier(const QString name) const {
        return m_identifiers.getIdentifier(name);
    }
    /// get reference to all identifiers within this frame
    const PythonSourceIdentifierList &identifiers() const { return m_identifiers; }

    /// the token (unindent) that ends this frame
    PythonToken *lastToken;

    /// on complete rescan, returns lastToken->next()
    PythonToken *scanFrame(PythonToken *startToken);
    /// on single row rescan
    void scanRow(PythonTextBlockData *row, PythonSyntaxHighlighter *highlighter) const;

private:
    // set identifier and sets up reference to RValue
    PythonToken *scanIdentifier(PythonToken *tok);
    // scans the RValue ie after '='
    // or scans type hint ie after :
    PythonToken *scanRValue(PythonToken *tok,
                            PythonSourceRoot::TypeInfo &typeInfo,
                            bool isTypeHint);
    PythonToken *scanImports(PythonToken *tok);


    // used to traverse to semicolon after argumentslist for typehint
    // if storeParameters, we add found parameters to parametersList
    const PythonToken *endOfParametersList(const PythonToken *token, bool storeParameters = false);
    // sets a parameter
    const PythonToken *scanParameter(const PythonToken *paramToken);
    // guess type for identifier
    const PythonSourceRoot::TypeInfo guessIdentifierType(const PythonToken *token);

    void setSyntaxError(const PythonToken *tok, QString parseMessage);

    void setMessage(const PythonToken *tok, QString parseMessage);
};

// -------------------------------------------------------------------------

/// for modules contains module global identifiers and methods
/// Don't confuse with PythonSourceImport that stares each import in code
class PythonSourceModule : public PythonSourceListParentBase
{
    PythonSourceRoot *m_root;
    PythonSourceFrame m_rootFrame;
    QString m_filePath;
    QString m_moduleName;
    PythonSyntaxHighlighter *m_highlighter;
public:

    explicit PythonSourceModule(PythonSourceRoot *root,
                                PythonSyntaxHighlighter *highlighter);
    ~PythonSourceModule();

    PythonSourceFrame *rootFrame() { return &m_rootFrame; }

    /// filepath for this module
    QString filePath() const { return m_filePath; }
    void setFilePath(QString filePath) { m_filePath = filePath; }

    /// modulename, ie sys or PartDesign
    QString moduleName() const { return m_moduleName; }
    void setModuleName(QString moduleName) { m_moduleName = moduleName; }

    PythonSyntaxHighlighter *highlighter() const { return m_highlighter; }


protected:
    int compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const;

};

// --------------------------------------------------------------------------

/// collection class for all modules
class PythonSourceModuleList : public PythonSourceListParentBase
{
    PythonSourceModule *m_main;
public:
    explicit PythonSourceModuleList();
    ~PythonSourceModuleList();

    PythonSourceModule *main() const { return m_main; }
    void setMain(PythonSourceModule *main);
    virtual bool remove(PythonSourceListNodeBase *node, bool deleteNode = false);
};



// --------------------------------------------------------------------



// -------------------------------------------------------------------------------

} // namespace Gui



#endif // PYTHONCODE_H
