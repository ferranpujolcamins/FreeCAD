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


// convinience macros, sets a global variable TOKEN_TEXT and TOKEN_NAME
// usefull when debugging, you cant inspect in your variable window
#ifdef BUILD_PYTHON_DEBUGTOOLS
#ifndef DEBUG_TOKENS
#define DEBUG_TOKENS 1
#endif
#endif

#if DEBUG_TOKENS == 1
#include "PythonCodeDebugTools.h"
// include at top of cpp file
#define DBG_TOKEN_FILE \
char TOKEN_TEXT_BUF[350]; \
char TOKEN_NAME_BUF[50]; \
char TOKEN_INFO_BUF[40];  \
char TOKEN_SRC_LINE_BUF[350];

// insert in start of each method that you want to dbg in
#define DEFINE_DBG_VARS \
/* we use c str here as it doesnt require dustructor calls*/ \
    /* squelsh compiler warnings*/ \
const char *TOKEN_TEXT = TOKEN_TEXT_BUF, \
           *TOKEN_NAME = TOKEN_NAME_BUF, \
           *TOKEN_INFO = TOKEN_INFO_BUF, \
           *TOKEN_SRC_LINE = TOKEN_SRC_LINE_BUF; \
    (void)TOKEN_TEXT; \
    (void)TOKEN_NAME; \
    (void)TOKEN_INFO; \
    (void)TOKEN_SRC_LINE;
#define DBG_TOKEN(TOKEN) if (TOKEN){\
    strcpy(TOKEN_NAME_BUF, Syntax::tokenToCStr(TOKEN->token)); \
    strcpy(TOKEN_INFO_BUF, (QString::fromLatin1("line:%1,start:%2,end:%3") \
                    .arg(TOKEN->txtBlock()->block().blockNumber()) \
                    .arg(TOKEN->startPos).arg(TOKEN->endPos)).toLatin1()); \
    strcpy(TOKEN_SRC_LINE_BUF, TOKEN->txtBlock()->block().text().toLatin1()); \
    strcpy(TOKEN_TEXT_BUF, TOKEN->text().toLatin1()); \
}
#define NEXT_TOKEN(TOKEN) {\
    if (TOKEN) TOKEN = TOKEN->next(); \
    DBG_TOKEN(TOKEN);}
#define PREV_TOKEN(TOKEN) {\
    if (TOKEN) TOKEN = TOKEN->previous(); \
    DBG_TOKEN(TOKEN);}

#else
// No debug, squelsh warnings
#define DEFINE_DBG_VARS ;
#define DBG_TOKEN(TOKEN) ;
#define NEXT_TOKEN(TOKEN) if (TOKEN) TOKEN = TOKEN->next();
#define PREV_TOKEN(TOKEN) if (TOKEN) TOKEN = TOKEN->previous();
#endif



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
    enum MsgType { Message, LookupError, SyntaxError, IndentError, AllMsgTypes };
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
    /// get the ParseMsg for tok, filter by type
    /// nullptr if not found
    const ParseMsg *getParseMessage(const PythonToken *tok, MsgType type = AllMsgTypes) const;
    /// get the ParseMsg that is contained within startPos, endPos,, filter by type
    /// nullptr if not found
    const ParseMsg *getParseMessage(int startPos, int endPos, MsgType type = AllMsgTypes) const;
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
class PythonSourceIdentifierAssignment;
class PythonSourceIdentifier;
class PythonSourceIdentifierList;
class PythonSourceTypeHint;
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
        // and in is* functions
        InValidType,
        UnknownType,
        VoidType,

        // reference types
        ReferenceType, // references another variable
        ReferenceArgumentType,  // is a function/method argument
        ReferenceBuiltInType, // references a builtin identifier
        // import statements
        ReferenceImportUndeterminedType, // identifier is imported into current frame,
                                          // not sure yet if it's a py module or C
        ReferenceImportErrorType,   // module could not be found
        ReferenceImportPythonType, // identifier is imported into current frame, is a Py module
        ReferenceImportBuiltInType, // identifier is imported into current frame, is C module

        // standard builtin types
        // https://docs.python.org/3/library/types.html
        FunctionType,
        LambdaType,
        GeneratorType,
        CoroutineType, // no support for this yet
        CodeType,
        MethodType,
        BuiltinFunctionType,
        BuiltinMethodType,
        WrapperDescriptorType,
        MethodWrapperType,
        MethodDescriptorType,
        ClassMethodDescriptorType,
        ModuleType, // is root frame, for imports use: ReferenceImport*
        TracebackType,
        FrameType,
        GetSetDescriptorType,
        MemberDescriptorType,
        MappingProxyType,

        TypeObjectType,
        ObjectType,
        NoneType,
        BoolType,
        IntType,
        FloatType,
        StringType,
        BytesType,
        ListType,
        TupleType,
        SetType,
        FrozenSetType,
        RangeType,
        DictType,
        ClassType,
        ComplexType,
        EnumerateType,
        IterableType,
        FileType,

        // a special Custom Type
        CustomType
    };

    /// for a identifier type
    struct TypeInfo {
        explicit TypeInfo();
        explicit TypeInfo(PythonSourceRoot::DataTypes type);
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
        /// refernces another variable
        bool isReference() const {
            return type >= ReferenceType && type <= ReferenceImportBuiltInType;
        }
        /// imported as into current frame
        bool isReferenceImported() const {
            return type >= ReferenceImportUndeterminedType &&
                   type <= ReferenceImportBuiltInType;
        }
        /// if you can call this type
        bool isCallable() const {
            switch (type) {
            case FunctionType: case LambdaType: case CoroutineType:
            case MethodType: case BuiltinFunctionType: case BuiltinMethodType:
            case MethodDescriptorType: case ClassMethodDescriptorType:
                return true;
            default:
                return false;
            }
        }
        bool isValid() const { return type != InValidType; }
    };
    /// return type from functions/methods/properties
    struct TypeInfoPair {
        TypeInfo returnType;
        TypeInfo thisType;
        bool isReturnType() const { return !returnType.isValid(); }
        bool isValid() const { return thisType.isValid(); }
    };

    // used as a message parsing between functions
    struct Indent {
        explicit Indent() : frameIndent(-1), currentBlockIndent(-1), previousBlockIndent(-1){}
        ~Indent() {}
        Indent operator =(const Indent &other) {
            frameIndent = other.frameIndent;
            currentBlockIndent = other.currentBlockIndent;
            previousBlockIndent = other.previousBlockIndent;
            return *this;
        }
        bool operator == (const Indent &other) {
            return frameIndent == other.frameIndent &&
                   previousBlockIndent == other.previousBlockIndent &&
                   currentBlockIndent == other.previousBlockIndent;
        }
        bool isValid() const {
            return frameIndent > -1 || currentBlockIndent > -1 || previousBlockIndent > -1;
        }
        void setAsInValid() { frameIndent = currentBlockIndent = previousBlockIndent = -1; }

        int frameIndent,
            currentBlockIndent,
            previousBlockIndent;
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
    DataTypes mapMetaDataType(const QString typeAnnotation) const;

    /// get the type for this token, token must be a Identifier
    /// else it returns a inValid TypeInfo
    TypeInfoPair identifierType(const PythonToken *tok,
                                const PythonSourceFrame *frame) const;
    TypeInfoPair builtinType(const PythonToken *tok,
                             const PythonSourceFrame *frame) const;


    /// true if tok is a newline and previous token was a escape char
    bool isLineEscaped(const PythonToken *tok) const;

    // typename database for custom types
    QString customTypeNameFor(CustomNameIdx_t customIdx);
    CustomNameIdx_t addCustomTypeName(const QString name);
    CustomNameIdx_t indexOfCustomTypeName(const QString name);

    /// scans complete filePath, clears all old and re-doe it
    PythonSourceModule *scanCompleteModule(const QString filePath,
                                           PythonSyntaxHighlighter *highlighter);
    /// re-scan a single QTextBlock, as in we are typing
    PythonSourceModule *scanSingleRowModule(const QString filePath,
                                            PythonTextBlockData *row,
                                            PythonSyntaxHighlighter *highlighter);

    /// computes the return type of statement pointed to by startToken
    /// NOTE it has limitations, it isn' a fullblown interpreter
    TypeInfo statementResultType(const PythonToken *startToken, const PythonSourceFrame *frame) const;


private:
    QHash<CustomNameIdx_t, QString> m_customTypeNames;
    CustomNameIdx_t m_uniqueCustomTypeNames;
    PythonSourceModuleList *m_modules;
    static PythonSourceRoot *m_instance;

    const PythonToken *computeStatementResultType(const PythonSourceFrame *frame,
                                                  const PythonToken *startTok,
                                                  TypeInfo &typeInfo) const;

    //PythonToken *splitStmtParts(PythonToken, );

};

// -------------------------------------------------------------------------

/// We do a Collection implementation duobly inked list as it lets us cleanly handle
/// insert, deletions, changes in source document

///
/// Base class for items in collections
class PythonSourceListNodeBase {
public:
    explicit PythonSourceListNodeBase(PythonSourceListParentBase *owner);
    PythonSourceListNodeBase(const PythonSourceListNodeBase &other);
    virtual ~PythonSourceListNodeBase();
    PythonSourceListNodeBase *next() const { return m_next; }
    PythonSourceListNodeBase *previous() const { return m_previous; }

    void setNext(PythonSourceListNodeBase *next) { m_next = next; }
    void setPrevious(PythonSourceListNodeBase *previous) { m_previous = previous; }
    /// python token
    const PythonToken *token() const { return m_token; }
    void setToken(PythonToken *token) { m_token = token; }

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
    PythonToken *m_token;
};

// -------------------------------------------------------------------------

/// base class for collection, manages a linked list, might itself be a node i a linked list
class PythonSourceListParentBase : public PythonSourceListNodeBase
{
public:
    typedef PythonSourceListNodeBase iterator_t;
    explicit PythonSourceListParentBase(PythonSourceListParentBase *owner);

    // Note!! this doesn't copy the elements in the list
    PythonSourceListParentBase(const PythonSourceListParentBase &other);
    virtual ~PythonSourceListParentBase();

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

    /// finds the node that has token this exact token
    PythonSourceListNodeBase *findExact(const PythonToken *tok) const;
    /// finds the first token of this type
    PythonSourceListNodeBase *findFirst(PythonSyntaxHighlighter::Tokens token) const;
    /// finds the last token of this type (reverse lookup)
    PythonSourceListNodeBase *findLast(PythonSyntaxHighlighter::Tokens token) const;
    /// returns true if all elements are of token type false otherwise
    /// if list is empty it returns false
    bool hasOtherTokens(PythonSyntaxHighlighter::Tokens token) const;

    /// stl iterator stuff
    PythonSourceListNodeBase* begin() const { return m_first; }
    PythonSourceListNodeBase* rbegin() const { return m_last; }
    PythonSourceListNodeBase* last()  const { return m_last; }
    PythonSourceListNodeBase* end() const { return nullptr; }
    PythonSourceListNodeBase* rend() const { return nullptr; }

protected:
    /// subclass may implement, used to sort insert order
    ///     should return -1 if left is more than right
    ///                   +1 if right is more than left
    ///                    0 if both equal
    virtual int compare(const PythonSourceListNodeBase *left,
                        const PythonSourceListNodeBase *right) const;
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
public:
    explicit PythonSourceIdentifierAssignment(PythonSourceIdentifier *owner,
                                              PythonToken *startToken,
                                              PythonSourceRoot::TypeInfo typeInfo);
    explicit PythonSourceIdentifierAssignment(PythonSourceTypeHint *owner,
                                              PythonToken *startToken,
                                              PythonSourceRoot::TypeInfo typeInfo);
    ~PythonSourceIdentifierAssignment();
    PythonSourceRoot::TypeInfo typeInfo() const { return m_type; }
    void setType(PythonSourceRoot::TypeInfo &type) { m_type = type; } // type is implicitly copied
    int linenr() const;
    int position() const; // as in pos in document line
};
// --------------------------------------------------------------------

typedef PythonSourceIdentifierAssignment PythonSourceTypeHintAssignment;

// --------------------------------------------------------------------

class PythonSourceTypeHint : public PythonSourceListParentBase
{
    const PythonSourceModule *m_module;
    const PythonSourceIdentifier *m_identifer;
    PythonSourceRoot::TypeInfo m_type;
public:
    explicit PythonSourceTypeHint(PythonSourceListParentBase *owner,
                                  const PythonSourceIdentifier *identifer,
                                  const PythonSourceModule *module);
    ~PythonSourceTypeHint();

    const PythonSourceModule *module() const { return m_module; }
    const PythonSourceFrame *frame() const;
    const PythonSourceIdentifier *identifier() const { return m_identifer; }

    /// gets last typehint assignment up til line and pos in document
    /// or nullptr if not found
    PythonSourceTypeHintAssignment *getFromPos(int line, int pos) const;
    PythonSourceTypeHintAssignment *getFromPos(const PythonToken *tok) const;

    /// get name of typeHint
    QString name() const;

    /// get the type of typeHint
    PythonSourceRoot::TypeInfo type() const { return m_type; }

protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// ----------------------------------------------------------------------

/// identifierClass, one for each variable in each frame
class PythonSourceIdentifier : public PythonSourceListParentBase
{
    const PythonSourceModule *m_module;
    PythonSourceTypeHint m_typeHint;
public:
    explicit PythonSourceIdentifier(PythonSourceListParentBase *owner,
                                    const PythonSourceModule *module);
    ~PythonSourceIdentifier();

    /// module for this identifier
    const PythonSourceModule *module() const { return m_module; }
    /// frame for this identifier
    const PythonSourceFrame *frame() const;

    /// get the last assigment up til line and pos in document
    /// or nullptr if not found
    PythonSourceIdentifierAssignment *getFromPos(int line, int pos) const;
    PythonSourceIdentifierAssignment *getFromPos(const PythonToken *tok) const;

    /// get the name of identifier
    QString name() const;

    /// get last inserted TypeHint for identifier up to including line
    PythonSourceTypeHintAssignment *getTypeHintAssignment(const PythonToken *tok) const;
    PythonSourceTypeHintAssignment *getTypeHintAssignment(int line) const;
    /// returns true if identifier has a typehint up to including line
    bool hasTypeHint(int line) const;
    /// set typeHint
    PythonSourceTypeHintAssignment *setTypeHint(PythonToken *tok,
                                                PythonSourceRoot::TypeInfo typeInfo);

protected:
    // should sort by linenr and (if same line also token startpos)
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// ----------------------------------------------------------------------------
/// a root container for all identifiers
class PythonSourceIdentifierList : public PythonSourceListParentBase
{
    const PythonSourceModule *m_module;
public:
    explicit PythonSourceIdentifierList(const PythonSourceModule *module);
    ~PythonSourceIdentifierList();

    /// get the module for this identifierList
    const PythonSourceModule *module() const { return m_module; }
    /// get the frame contained for these collections
    const PythonSourceFrame *frame() const;
    /// get the identifier with name or nullptr if not contained
    const PythonSourceIdentifier *getIdentifier(const QString name) const;
    bool hasIdentifier(const QString name) const { return getIdentifier(name) != nullptr; }
    /// sets a new assignment, creates identifier if not exists
    PythonSourceIdentifier *setIdentifier(PythonToken *tok,
                                          PythonSourceRoot::TypeInfo typeInfo);

    /// get the identifier that is pointed to by token
    /// This traverses chain upwards until it finds identifier
    /// with non refernce type (discrete assignment)
    /// limitChain limits lookup chain
    ///     var1 = None <- none is discrete
    ///     var2 = var1 <- getIdentifierReferencedBy(var2token) returns var1
    ///     var3 = var2 <- getIdentifierReferencedBy(var3token) returns var1
    ///     var4 = var3 <- getIdentifierReferencedBy(var4token) also returns var1
    ///     var5 = var4 <- getIdentifierReferencedBy(var5token, limitChain = 1) returns var4
    const PythonSourceIdentifier *getIdentifierReferencedBy(const PythonToken *tok,
                                                            int limitChain = 10) const;
    const PythonSourceIdentifier *getIdentifierReferencedBy(const PythonSourceIdentifier *ident,
                                                            int line, int pos,
                                                            int limitChain = 10) const;

    /// returns a list containing all identifiers dependent on
    const PythonSourceIdentifierList getIdentifierDependants(const PythonSourceIdentifier *ident,
                                                             int line, int pos,
                                                             int limitChain = 10) const;

    const PythonSourceIdentifierList getIdentifierDependants(const PythonToken *tok,
                                                             int limitChain = 10) const;protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};


// ---------------------------------------------------------------------
// class for function/method parameters ie: def func(arg1, arg2=None, *arg3, **arg4)
//                                                   ^     ^           ^       ^
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
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// -------------------------------------------------------------------
/// a return can be built up by several Identifiers and operators
///  ie: return (var1 + var3) * 4 or 5
///              ^    ^ ^       ^    ^
/// we store pos of return, and compute type when we need it
class PythonSourceFrameReturnType : public PythonSourceListNodeBase
{
    const PythonSourceModule *m_module;
    PythonSourceRoot::TypeInfo m_typeInfo;
public:
    explicit PythonSourceFrameReturnType(PythonSourceListParentBase *owner,
                                         const PythonSourceModule *module,
                                         PythonToken *tok);
    virtual ~PythonSourceFrameReturnType();

    // getter/setters for typeInfo
    void setTypeInfo(PythonSourceRoot::TypeInfo typeInfo);
    PythonSourceRoot::TypeInfo typeInfo() const { return m_typeInfo; }

    /// return this type
    PythonSourceRoot::TypeInfo returnType() const;
    /// true if it is a yield
    bool isYield() const;
    /// yields this type
    PythonSourceRoot::TypeInfo yieldType() const;
};

// --------------------------------------------------------------------

class PythonSourceFrameReturnTypeList : public PythonSourceListParentBase
{
    const PythonSourceModule *m_module;
public:
    explicit PythonSourceFrameReturnTypeList(PythonSourceListParentBase *owner,
                                    const PythonSourceModule *module);
    PythonSourceFrameReturnTypeList(const PythonSourceFrameReturnTypeList &other);
    virtual ~PythonSourceFrameReturnTypeList();
protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// --------------------------------------------------------------------

// a single import module, not a package
class PythonSourceImportModule : public PythonSourceListNodeBase
{
    PythonSourceFrame *m_frame;
    QString m_modulePath; // filepath to python file
    QString m_aliasName; // alias name in: import something as other <- other is alias
    PythonSourceRoot::TypeInfo m_type;

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

    PythonSourceRoot::TypeInfo type() const { return m_type; }
    void setType(PythonSourceRoot::TypeInfo typeInfo) {
        if (typeInfo.isReferenceImported())
            m_type = typeInfo;
    }

    const PythonSourceFrame *frame() const { return m_frame; }
};

// ---------------------------------------------------------------------

class PythonSourceImportPackage : public PythonSourceListParentBase
{
    QString m_packagePath; // filepath to folder for this package
    QString m_name;
    const PythonSourceModule *m_ownerModule;
public:
    explicit PythonSourceImportPackage(PythonSourceImportPackage *parent,
                                       QString name, const PythonSourceModule *ownerModule);
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
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------

class PythonSourceImportList : public PythonSourceImportPackage
{
    PythonSourceFrame *m_frame;
public:
    explicit PythonSourceImportList(PythonSourceFrame *owner,
                                    const PythonSourceModule *ownerModule);
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
    PythonSourceFrameReturnTypeList m_returnTypes; // as in return types
    PythonSourceRoot::TypeInfo m_type;
    PythonSourceTypeHint *m_typeHint;
    PythonSourceFrame *m_parentFrame;
    const PythonSourceModule *m_module;
    bool m_isClass;

public:
    explicit PythonSourceFrame(PythonSourceFrame *owner, const PythonSourceModule *module,
                               PythonSourceFrame *parentFrame = nullptr, bool isClass = false);
    explicit PythonSourceFrame(PythonSourceModule *owner, const PythonSourceModule *module,
                               PythonSourceFrame *parentFrame = nullptr, bool isClass = false);
    ~PythonSourceFrame();


    /// get the parent function frame
    PythonSourceFrame *parentFrame() const { return m_parentFrame; }
    void setParentFrame(PythonSourceFrame *frame) { m_parentFrame = frame; }

    const PythonSourceModule *module() const { return m_module; }
    PythonSourceRoot::TypeInfo type() const { return m_type; }

    /// true if frame is itself a module, ie: rootframe
    bool isModule() const { return m_parentFrame == nullptr; }

    /// get the docstring for this function/method
    QString docstring();

    /// get the return type hint
    ///  def funt(arg1, arg2) -> Module.Class.Custom:
    ///    would return:         ^
    ///    CustomType with CustomTypeName == Module.Class.Custom
    /// returns Typeinfo::InValidType if no typeHint given
    PythonSourceRoot::TypeInfo returnTypeHint() const;

    /// true if we have identifier
    bool hasIdentifier(const QString name) const {
        return m_identifiers.getIdentifier(name);
    }

    const PythonSourceTypeHint *typeHint() const { return m_typeHint; }

    /// is root frame for class
    bool isClass() const { return m_isClass; }

    /// looks up name among identifiers (variables/constants)
    const PythonSourceIdentifier *getIdentifier(const QString name) const {
        return m_identifiers.getIdentifier(name);
    }
    /// get reference to all identifiers within this frame
    const PythonSourceIdentifierList &identifiers() const { return m_identifiers; }

    /// returns a list with all the types for this frame
    ///   returnType Might differ
    const PythonSourceFrameReturnTypeList returnTypes() const;

    /// the token (unindent) that ends this frame
    const PythonToken *lastToken;

    // scan* functions might mutate PythonToken, ie change from Undetermined -> determined etc.
    /// on complete rescan, returns lastToken->next()
    PythonToken *scanFrame(PythonToken *startToken);

    /// on single row rescan
    PythonToken *scanLine(PythonToken *startToken, PythonSourceRoot::Indent &indent);


private:
    // moves token til next line with tokens
    PythonToken *gotoNextLine(PythonToken *tok);

    // set identifier and sets up reference to RValue
    PythonToken *scanIdentifier(PythonToken *tok);
    // scans the RValue ie after '='
    // or scans type hint ie after :
    PythonToken *scanRValue(PythonToken *tok,
                            PythonSourceRoot::TypeInfo &typeInfo,
                            bool isTypeHint);
    PythonToken *scanImports(PythonToken *tok);

    // scan return statement
    PythonToken *scanReturnStmt(PythonToken *tok);
    // scan yield statement
    PythonToken *scanYieldStmt(PythonToken *tok);

    // used to traverse to semicolon after argumentslist for typehint
    // if storeParameters, we add found parameters to parametersList
    PythonToken *scanAllParameters(PythonToken *token, bool storeParameters = false);
    // sets a parameter
    PythonToken *scanParameter(PythonToken *paramToken, int &parenCount);
    // guess type for identifier
    PythonSourceRoot::TypeInfo guessIdentifierType(const PythonToken *token);

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
    bool m_rehighlight;
public:

    explicit PythonSourceModule(PythonSourceRoot *root,
                                PythonSyntaxHighlighter *highlighter);
    ~PythonSourceModule();

    PythonSourceRoot *root() const { return m_root; }
    const PythonSourceFrame *rootFrame() const { return &m_rootFrame; }

    /// filepath for this module
    QString filePath() const { return m_filePath; }
    void setFilePath(QString filePath) { m_filePath = filePath; }

    /// modulename, ie sys or PartDesign
    QString moduleName() const { return m_moduleName; }
    void setModuleName(QString moduleName) { m_moduleName = moduleName; }

    /// rescans all lines in frame that encapsulates tok
    void scanFrame(PythonToken *tok);
    /// rescans a single row
    void scanLine(PythonToken *tok);

    /// returns indent info for block where tok is
    PythonSourceRoot::Indent currentBlockIndent(const PythonToken *tok) const;

    // syntax highlighter stuff
    PythonSyntaxHighlighter *highlighter() const { return m_highlighter; }
    void setFormatToken(const PythonToken *tok, QTextCharFormat format);
    void setFormatToken(const PythonToken *tok);

    /// stores a syntax error at tok with message and sets up for repaint
    void setSyntaxError(const PythonToken *tok, QString parseMessage) const;

    /// sets a indenterror (sets token to indentError and sets up for style document)
    void setIndentError(const PythonToken *tok) const;

    /// sets a lookup error (variable not found) with message, or default message
    void setLookupError(const PythonToken *tok, QString parseMessage = QString()) const;

    /// stores a message at tok
    void setMessage(const PythonToken *tok, QString parseMessage) const;

    /// returns the frame for given token
    const PythonSourceFrame *getFrameForToken(const PythonToken *tok,
                                              const PythonSourceFrame *parentFrame) const;


protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right);


private:
    int _currentBlockIndent(const PythonToken *tok) const;
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
