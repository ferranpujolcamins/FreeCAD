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
#include "CXX/Objects.hxx"
#include <memory>


namespace Gui {

class PythonSyntaxHighlighter;
class PythonSyntaxHighlighterP;
class PythonEditorBreakpointDlg;
class PythonCodeP;
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

    // get thee root of the parent identifier ie os.path.join
    //                                                    ^
    // must traverse from os, then os.path before os.path.join
    Py::Object getDeepObject(PyObject *obj, QString key, QString &foundKey);

private:
    PythonCodeP *d;
};

// --------------------------------------------------------------------

/**
 * Syntax highlighter for Python.
 * \author Werner Mayer
 */
class GuiExport PythonSyntaxHighlighter : public SyntaxHighlighter
{
public:
    PythonSyntaxHighlighter(QObject* parent);
    virtual ~PythonSyntaxHighlighter();

    void highlightBlock (const QString & text);

    enum Tokens {
        T_Undetermined     = 0,     // Parser looks tries to figure out next char also Standard text
        // python
        T_Indent           = 1,
        T_Comment          = 2,     // Comment begins with #
        T_SyntaxError      = 3,

        // numbers
        T_NumberDecimal    = 10,     // Normal number
        T_NumberHex        = 11,
        T_NumberBinary     = 12,
        T_NumberOctal      = 13,    // starting with 0 ie 011 = 9, different color to let it stand out
        T_NumberFloat      = 14,

        // strings
        T_LiteralDblQuote      = 20,     // String literal beginning with "
        T_LiteralSglQuote      = 21,     // Other string literal beginning with '
        T_LiteralBlockDblQuote = 22,     // Block comments beginning and ending with """
        T_LiteralBlockSglQuote = 23,     // Other block comments beginning and ending with '''

        // Keywords
        T_Keyword          = 30,
        T_KeywordClass     = 31,
        T_KeywordDef       = 32,
        T_KeywordImport    = 33,
        T_KeywordFrom      = 34,
        T_KeywordAs        = 35, // not sure if needed? we arent exactly building a VM...
        T_KeywordIn        = 36, // likwise
        // leave some room for future keywords

        // operators
        T_Operator         = 60,   // **, |, &, ^, ~, +, -, *, /, %, ~, @
        T_OperatorCompare  = 61,   // ==, !=, <=, >=, <, >,
        T_OperatorAssign   = 70,   // =, +=, *=, -=, /=, @= introduced in py 3.5

        // delimiters
        T_Delimiter             = 80,   // all other non specified
        T_DelimiterOpenParen    = 81,   // (
        T_DelimiterCloseParen   = 82,   // )
        T_DelimiterOpenBracket  = 83,   // [
        T_DelimiterCloseBracket = 84,   // ]
        T_DelimiterOpenBrace    = 85,   // {
        T_DelimiterCloseBrace   = 86,   // }
        T_DelimiterPeriod       = 87,   // .
        T_DelimiterComma        = 88,   // ,
        T_DelimiterColon        = 89,   // :
        T_DelimiterSemiColon    = 90,   // ;
        // metadata such def funcname(arg: "documentation") ->
        //                            "returntype documentation":
        T_DelimiterMetaData     = 91,   // -> might also be ':' inside arguments
        T_DelimiterLineContinue = 92,   // when end of line is escaped like so '\'

        // identifiers
        T_IdentifierUnknown       = 110, // name is not identified at this point
        T_IdentifierDefined       = 111, // variable is in current context
        T_IdentifierModule        = 112, // its a module definition
        T_IdentifierFunction      = 113, // its a function definition
        T_IdentifierMethod        = 114, // its a method definition
        T_IdentifierClass         = 115, // its a class definition
        T_IdentifierSuperMethod   = 116, // its a method with name: __**__
        T_IdentifierBuiltin       = 117, // its builtin
        T_IdentifierDecorator     = 118, // member decorator like: @property
        T_IdentifierDefUnknown    = 119, // before system has determined if its a
                                         // method or function yet

        // metadata such def funcname(arg: "documentaion") -> "returntype documentation":
        T_MetaData                = 140,


        T_Invalid          = 512,
        T_PythonConsoleOutput = 1000,
        T_PythonConsoleError  = 1001
    };

private:
    PythonSyntaxHighlighterP* d;

    //const QString getWord(int pos, const QString &text) const;
    int lastWordCh(int startPos, const QString &text) const;
    int lastNumberCh(int startPos, const QString &text) const;
    int lastDblQuoteStringCh(int startAt, const QString &text) const;
    int lastSglQuoteStringCh(int startAt, const QString &text) const;
    Tokens numberType(const QString &text) const;

    void setRestOfLine(int &pos, const QString &text, Tokens token,
                       TColor color);

    void setWord(int &pos, int len, Tokens token, TColor color);

    void setBoldWord(int &pos, int len, Tokens token, TColor color);

    void setIdentifier(int &pos, int len, Tokens token, TColor color);
    void setUndeterminedIdentifier(int &pos, int len, Tokens token, TColor color);

    void setIdentifierBold(int &pos, int len, Tokens token, TColor color);
    void setUndeterminedIdentifierBold(int &pos, int len, Tokens token, TColor color);

    void setNumber(int &pos, int len, Tokens token);

    void setOperator(int &pos, int len, Tokens token);

    void setDelimiter(int &pos, int len, Tokens token);

    void setSyntaxError(int &pos, int len);

    void setLiteral(int &pos, int len, Tokens token, TColor color);

    // this is special can only be one per line
    // T_Indentation(s) token is generated related when looking at lines above as described in
    // https://docs.python.org/3/reference/lexical_analysis.html#indentation
    void setIndentation(int &pos, int len, int count);

};

// ------------------------------------------------------------------------

struct MatchingCharInfo
{
    char character;
    int position;
    MatchingCharInfo();
    MatchingCharInfo(const MatchingCharInfo &other);
    MatchingCharInfo(char chr, int pos);
    ~MatchingCharInfo();
    char matchingChar() const;
    static char matchChar(char match);
};

// ------------------------------------------------------------------------

struct PythonToken
{
    explicit PythonToken(PythonSyntaxHighlighter::Tokens token, int startPos, int endPos);
    ~PythonToken();
    bool operator==(const PythonToken &other) const;
    bool operator > (const PythonToken &other) const;
    PythonSyntaxHighlighter::Tokens token;
    int startPos;
    int endPos;
};

// -----------------------------------------------------------------------

class PythonTextBlockData : public QTextBlockUserData
{
public:
    PythonTextBlockData(QTextBlock block);
    ~PythonTextBlockData();

    const QTextBlock &block() const;
    const QVector<PythonToken *> &tokens() const;
    /**
     * @brief findToken searches for token in this block
     * @param token needle to search for
     * @param searchFrom start position in line to start the search
     *                   if negative start from the back ie -10 searches from pos 10 to 0
     * @return pointer to token if found, else nullptr
     */
    const PythonToken* findToken(PythonSyntaxHighlighter::Tokens token,
                                 int searchFrom = 0) const;

    /**
     * @brief tokensBetweenOfType counts tokens ot type match between these positions
     *         NOTE! this methos ignore T_Indent tokens
     * @param startPos position to start at
     * @param endPos position where it should stop looking
     * @param match against his token
     * @return number of times token was found
     */
    int tokensBetweenOfType(int startPos, int endPos, PythonSyntaxHighlighter::Tokens match) const;

    /**
     * @brief lastInserted gives the last inserted token
     *         NOTE! this method ignore T_Indent tokens
     * @param lookInPreviousRows if false limits to this textblock only
     * @return last inserted token or nullptr if empty
     */
    PythonToken *lastInserted(bool lookInPreviousRows = true) const;


    /**
     * @brief lastInsertedIsA checks if last token matches
     * @param match token to test against
     * @return true if matched
     */
    bool lastInsertedIsA(PythonSyntaxHighlighter::Tokens match, bool lookInPreviousRows = true) const;

    /**
     * @brief tokenAt returns the token defined under pos (in block)
     * @param pos from start of line
     * @return pointer to token or nullptr
     */
    const PythonToken* tokenAt(int pos) const;

    /**
     * @brief tokenAtAsString returns the token under pos (in block)
     * @param pos from start of line
     * @return token as string
     */
    QString tokenAtAsString(int pos) const;


    /**
     * @brief isMatchAt compares token at given pos
     * @param pos position i document
     * @param token match against this token
     * @return true if token are the same
     */
    bool isMatchAt(int pos, PythonSyntaxHighlighter::Tokens token) const;


    /**
     * @brief isMatchAt compares multiple tokens at given pos
     * @param pos position i document
     * @param token a list of tokens to match against this token
     * @return true if any of the token are the same
     */
    bool isMatchAt(int pos, const QList<PythonSyntaxHighlighter::Tokens> tokens) const;


    /**
     * @brief isCodeLine checks if line has any code
     *        lines with only indent or comments return false
     * @return true if line has code
     */
    bool isCodeLine() const;

    bool isEmpty() const;
    /**
     * @brief indentString returns the indentation of line
     * @return QString of the raw string in document
     */
    QString indentString() const;

    /**
     * @brief indent the number of indent spaces as interpreted by a python interpreter,
     *        a comment only line returns 0 regardless of indent
     *
     * @return nr of whitespace chars (tab=8) beginning of line
     */
    int indent() const;


protected:

    /**
     * @brief insert should only be used by PythonSyntaxHighlighter
     */
    void setDeterminedToken(PythonSyntaxHighlighter::Tokens token, int startPos, int len);
    /**
     * @brief insert should only be used by PythonSyntaxHighlighter
     *  signifies that parse tree lookup is needed
     */
    void setUndeterminedToken(PythonSyntaxHighlighter::Tokens token, int startPos, int len);

    /**
     * @brief setIndentCount should be considered private, is
     * @param count
     */
    void setIndentCount(int count);


private:
    QVector<PythonToken *> m_tokens;
    QTextBlock m_block;
    int m_indentCharCount; // as spaces NOTE according  to python documentation a tab is 8 spaces

    friend class PythonSyntaxHighlighter; // in order to hide some api
};

// -----------------------------------------------------------------------

/**
 * @brief The PythonMatchingChars highlights the oposite (), [] or {}
 */
class PythonMatchingChars : public QObject
{
    Q_OBJECT

public:
    PythonMatchingChars(TextEdit *parent);
    ~PythonMatchingChars();

private Q_SLOTS:
    void cursorPositionChange();

private:
    TextEdit *m_editor;
};

// --------------------------------------------------------------------

class JediDefinitionObj;
class JediCompletionObj;
class JediCallSignatureObj;
class JediBaseDefinitionObj;
class JediScriptObj;

//
typedef std::shared_ptr<JediCompletionObj> JediCompletion_ptr_t;
typedef QList<JediCompletion_ptr_t> JediCompletion_list_t;
typedef std::shared_ptr<JediDefinitionObj> JediDefinition_ptr_t;
typedef QList<JediDefinition_ptr_t> JediDefinition_list_t;
typedef std::shared_ptr<JediCallSignatureObj> JediCallSignature_ptr_t;
typedef QList<JediCallSignature_ptr_t> JediCallSignature_list_t;
typedef std::shared_ptr<JediScriptObj> JediScript_ptr_t;
typedef std::shared_ptr<JediBaseDefinitionObj> JediBaseDefinition_ptr_t;
typedef QList<JediBaseDefinition_ptr_t> JediBaseDefinition_list_t;


/**
 * @brief The JediBaseDefinition class
 * wrapper for python class BaseDefinition
 */

class JediBaseDefinitionObj
{
public:
    JediBaseDefinitionObj(Py::Object obj);
    virtual ~JediBaseDefinitionObj();

    enum Types {
        Base, Definition, Completion, CallSignature
    };

    /*
        Name of variable/function/class/module.

        For example, for ``x = None`` it returns ``'x'``.

        :rtype: str or None
        @property
     */
    QString name() const;

    // The type of the definition.
    //     @property
    QString type() const;

    // The module name.
    //     @property
    QString module_name() const;

    QString module_path() const;

    // Whether this is a builtin module.
    bool in_builtin_module() const;

    // The line where the definition occurs (starting with 1).
    //     @property
    int line() const;

    // The column where the definition occurs (starting with 0).
    //     @property
    int column() const;

    // Return a document string for this completion object.
    //
    // Notice that useful extra information is added to the actual
    // docstring.  For function, it is call signature.  If you need
    // actual docstring, use ``raw=True`` instead.

    // :param fast: Don't follow imports that are only one level deep like
    //    ``import foo``, but follow ``from foo import bar``. This makes
    //    sense for speed reasons. Completing `import a` is slow if you use
    //    the ``foo.docstring(fast=False)`` on every object, because it
    //    parses all libraries starting with ``a``.
    QString docstring(bool raw = false, bool fast = true) const;

    // A textual description of the object.
    //     @property
    QString description() const;

    // Dot-separated path of this object.
    //     @property
    QString full_name() const;

    // return a list with assignments
    JediDefinition_list_t goto_assignments() const;

    // return a list with definitions
    JediDefinition_list_t goto_definitions() const;

    // Return the original definitions. use with care see doumentation for jedi
    //     @property
    JediDefinition_list_t params() const;

    // return parent Definition or nullptr   -> in c++ = empty list
    JediDefinition_ptr_t parent() const;

    // Returns the line of code where this object was defined.
    // :param before: Add n lines before the current line to the output.
    // :param after: Add n lines after the current line to the output.
    // :return str: Returns the line(s) of code or an empty string if it's a
    //              builtin.
    int get_line_code(int before = 0, int after = 0) const;


    Types cppType() const { return m_type; }
    bool isValid() const { return m_obj.ptr() != nullptr; }
    const Py::Object pyObj() const { return m_obj; }
    JediDefinitionObj *toDefinitionObj();
    JediCompletionObj *toCompletionObj();
    JediCallSignatureObj *toCallSignatureObj();
    bool isDefinitionObj();
    bool isCompletionObj();
    bool isCallSignatureObj();

protected:

    Py::Object m_obj;
    Types m_type;
};

// ------------------------------------------------------------------------


class JediCompletionObj : public JediBaseDefinitionObj
{
    /*
    """
    `Completion` objects are returned from :meth:`api.Script.completions`. They
    provide additional information about a completion.
    """
     */
public:
    JediCompletionObj(Py::Object obj);
    ~JediCompletionObj();

    //  Return the rest of the word,
    //     @property
    QString complete() const;

    //  Similar to :attr:`name`, but like :attr:`name` returns also the
    //          symbols, for example assuming the following function definition::
    //              def foo(param=0):
    //                    pass
    //          completing ``foo(`` would give a ``Completion`` which
    //          ``name_with_symbols`` would be "param=".
    //     @property
    QString name_with_symbols() const;
};

// ---------------------------------------------------------------------------

class JediDefinitionObj : public JediBaseDefinitionObj
{
    /*
    """
    *Definition* objects are returned from :meth:`api.Script.goto_assignments`
    or :meth:`api.Script.goto_definitions`.
    """
     */
public:
    JediDefinitionObj(Py::Object obj);
    ~JediDefinitionObj();

    /*
        List sub-definitions (e.g., methods in class).
        :rtype: list of Definition
     */
    JediDefinition_list_t defined_names() const;

    /*
        Returns True, if defined as a name in a statement, function or class.
        Returns False, if it's a reference to such a definition.
     */
    bool is_definition() const;
};

// --------------------------------------------------------------------------

class JediCallSignatureObj : public JediDefinitionObj
{
    /*
    `CallSignature` objects is the return value of `Script.function_definition`.
    It knows what functions you are currently in. e.g. `isinstance(` would
    return the `isinstance` function. without `(` it would return nothing.
     */
public:
    JediCallSignatureObj(Py::Object obj);
    ~JediCallSignatureObj();

    /*
        The Param index of the current call.
        Returns nullptr if the index cannot be found in the curent call.
            @property
     */
    JediDefinition_ptr_t index() const;

    /*
        The indent of the bracket that is responsible for the last function
        call.
            @property
     */
    int bracket_start() const;
};

// --------------------------------------------------------------------------

/**
 * @brief The JediScriptObj class is used for code completion, and analysis
 * Like painting known variables and find usages
 * Uses the python library Jedi, intall via 'pip install jedi'
 */
class JediCommonP;
class JediScriptObj
{
    Py::Object m_obj;

public:
    /*
     *     :param source: The source code of the current file, separated by newlines.
    :type source: str
    :param line: The line to perform actions on (starting with 1).
    :type line: int
    :param column: The column of the cursor (starting with 0).
    :type column: int
    :param path: The path of the file in the file system, or ``''`` if
        it hasn't been saved yet.
    :type path: str or None
    :param encoding: The encoding of ``source``, if it is not a
        ``unicode`` object (default ``'utf-8'``).
    :type encoding: str
    :param sys_path: ``sys.path`` to use during analysis of the script
    :type sys_path: list
     */
    // source=None, line=None, column=None, path=None, encoding='utf-8'
    JediScriptObj(const QString source = QString(), int line = -1, int column = -1,
                  const QString path = QString(),
                  const QString encoding = QLatin1String("utf-8"));
    virtual ~JediScriptObj();

    /*
        Return :class:`classes.Completion` objects. Those objects contain
        information about the completions, more than just names.

        :return: Completion objects, sorted by name and __ comes last.
        :rtype: list of :class:`classes.Completion`
     */
    JediCompletion_list_t completions() const;

    /*
        Return the definitions of a the path under the cursor.  goto function!
        This follows complicated paths and returns the end, not the first
        definition. The big difference between :meth:`goto_assignments` and
        :meth:`goto_definitions` is that :meth:`goto_assignments` doesn't
        follow imports and statements. Multiple objects may be returned,
        because Python itself is a dynamic language, which means depending on
        an option you can have two different versions of a function.

        :rtype: list of :class:`classes.Definition`
     */
    JediDefinition_list_t goto_definitions() const;

    /*
        Return the first definition found, while optionally following imports.
        Multiple objects may be returned, because Python itself is a
        dynamic language, which means depending on an option you can have two
        different versions of a function.

        :rtype: list of :class:`classes.Definition`
     */
    JediDefinition_list_t goto_assignments(bool follow_imports = false) const;

    /*
        Return :class:`classes.Definition` objects, which contain all
        names that point to the definition of the name under the cursor. This
        is very useful for refactoring (renaming), or to show all usages of a
        variable.

        .. todo:: Implement additional_module_paths

        :rtype: list of :class:`classes.Definition`
     */
    JediDefinition_list_t usages(QStringList additional_module_paths = QStringList()) const;

    /*
        Return the function object of the call you're currently in.

        E.g. if the cursor is here::

            abs(# <-- cursor is here

        This would return the ``abs`` function. On the other hand::

            abs()# <-- cursor is here

        This would return an empty list..

        :rtype: list of :class:`classes.CallSignature`
     */
    JediCallSignature_list_t call_signatures() const;


    bool isValid() const;
};

// -----------------------------------------------------------------------

// a callback module that proxies jedi debug info to JediInterpreter
extern "C" {
class JediDebugProxy : public Py::ExtensionModule<JediDebugProxy>
{
public:
    JediDebugProxy();

    virtual ~JediDebugProxy();
    static const char *ModuleName;
private:
    Py::Object proxy(const Py::Tuple &args);
};
}

// -----------------------------------------------------------------------

/**
 * @brief The JediInterpreter class
 * we need a separate interpreter to isolate
 * code analysis from freecad built in interpreter
 */
class JediInterpreter : public QObject
{
    Q_OBJECT
public:
    JediInterpreter();
    ~JediInterpreter();

    // scope guard to do work with this interpreter
    class SwapIn {
        PyThreadState *m_oldState;
        PyGILState_STATE m_GILState;
        static bool static_GILHeld;
    public:
        SwapIn();
        ~SwapIn();
    };

    static JediInterpreter *instance();

    bool isValid() const;

    /**
     * @brief createScriptObj
     * @return a new
     */
    Py::Object runString(const QString &src);

    Py::Module &jedi() const;
    Py::Module &api() const;
    PyThreadState *threadState() const;

    /*

        Returns a list of `Definition` objects, containing name parts.
        This means you can call ``Definition.goto_assignments()`` and get the
        reference of a name.
        The parameters are the same as in :py:class:`Script`, except or the
        following ones:

        :param all_scopes: If True lists the names of all scopes instead of only
            the module namespace.
        :param definitions: If True lists the names that have been defined by a
            class, function or a statement (``a = b`` returns ``a``).
        :param references: If True lists all the names that are not listed by
            ``definitions=True``. E.g. ``a = b`` returns ``b``.
    */
    JediDefinition_list_t  names(const QString source = QString(), const QString path = QString(),
                                 const QString encoding = QLatin1String("utf-8"), bool all_scopes = false,
                                 bool definitions = true, bool references = true) const;

    /*
    Preloading modules tells Jedi to load a module now, instead of lazy parsing
    of modules. Usful for IDEs, to control which modules to load on startup.

    :param modules: different module names, list of string.
     */
    bool preload_module(const QStringList modules) const;

    /*
     * Turn on Jedi set_debug_function, although it transforms those functions calls
     * to a Qt signal
     */
    bool setDebugging(bool on, bool warnings = true,
                            bool notices = true, bool speed = true) const;


Q_SIGNALS:
    void onDebugInfo(const QString &color, const QString &str);


private:
    void destroy();
    PyThreadState *m_threadState;
    PyThreadState *m_interpState;
    Py::Module *m_jedi;
    Py::Module *m_api;
    static JediInterpreter *m_instance;

    // so the proxy can emit as if originating from this class
    friend class JediDebugProxy;
};

// -------------------------------------------------------------------------------

} // namespace Gui



#endif // PYTHONCODE_H
