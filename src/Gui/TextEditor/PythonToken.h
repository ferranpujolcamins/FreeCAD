#ifndef PYTHONTOKEN_H
#define PYTHONTOKEN_H

#include <QObject>

namespace Gui {
namespace Python {

class SourceListNodeBase; // in code analyzer
class TextBlockData;
class TokenList;
class TokenLine;

class Token
{
public:
    enum Tokens {
        //**
        //* Any token added or removed here must also be added or removed in PythonCodeDebugTools
        // all these tokens must be in grouped order ie: All operators grouped, numbers grouped etc
        T_Undetermined     = 0,     // Parser looks tries to figure out next char also Standard text
        // python
        T_Indent,
        T_Comment,     // Comment begins with #
        T_SyntaxError,
        T_IndentError,     // to signify that we have a indent error, set by PythonSourceRoot

        // numbers
        T__NumbersStart,
        T__NumbersIntStart = T__NumbersStart,
        T_NumberHex,
        T_NumberBinary,
        T_NumberOctal,     // starting with 0 ie 011 = 9, different color to let it stand out
        T_NumberDecimal,   // Normal number
        T__NumbersIntEnd,
        T_NumberFloat,
        T__NumbersEnd,

        // strings
        T__LiteralsStart = T__NumbersEnd,
        T_LiteralDblQuote,           // String literal beginning with "
        T_LiteralSglQuote,           // Other string literal beginning with '
        T__LiteralsMultilineStart,
        T_LiteralBlockDblQuote,      // Block comments beginning and ending with """
        T_LiteralBlockSglQuote,      // Other block comments beginning and ending with '''
        T__LiteralsMultilineEnd,
        T__LiteralsEnd,

        // Keywords
        T__KeywordsStart = T__LiteralsEnd,
        T_Keyword,
        T_KeywordClass,
        T_KeywordDef,
        T_KeywordImport,
        T_KeywordFrom,
        T_KeywordAs,
        T_KeywordYield,
        T_KeywordReturn,
        T__KeywordIfBlockStart,
        T_KeywordIf,
        T_KeywordElIf,
        T_KeywordElse,
        T__KeywordIfBlockEnd,
        T__KeywordLoopStart = T__KeywordIfBlockEnd,
        T_KeywordFor,
        T_KeywordWhile,
        T_KeywordBreak,
        T_KeywordContinue,
        T__KeywordsLoopEnd,
        T__KeywordsEnd = T__KeywordsLoopEnd,
        // leave some room for future keywords

        // operators
        // https://www.w3schools.com/python/python_operators.asp
        // https://docs.python.org/3.7/library/operator.html
        // artihmetic operators
        T__OperatorStart            = T__KeywordsEnd,   // operators start
        T__OperatorArithmeticStart = T__OperatorStart,
        T_OperatorPlus,              // +,
        T_OperatorMinus,             // -,
        T_OperatorMul,               // *,
        T_OperatorExponential,       // **,
        T_OperatorDiv,               // /,
        T_OperatorFloorDiv,          // //,
        T_OperatorModulo,            // %,
        T_OperatorMatrixMul,         // @
        T__OperatorArithmeticEnd,

        // bitwise operators
        T__OperatorBitwiseStart = T__OperatorArithmeticEnd,
        T_OperatorBitShiftLeft,       // <<,
        T_OperatorBitShiftRight,      // >>,
        T_OperatorBitAnd,             // &,
        T_OperatorBitOr,              // |,
        T_OperatorBitXor,             // ^,
        T_OperatorBitNot,             // ~,
        T__OperatorBitwiseEnd,

        // assignment operators
        T__OperatorAssignmentStart   = T__OperatorBitwiseEnd,
        T_OperatorEqual,              // =,
        T_OperatorPlusEqual,          // +=,
        T_OperatorMinusEqual,         // -=,
        T_OperatorMulEqual,           // *=,
        T_OperatorDivEqual,           // /=,
        T_OperatorModuloEqual,        // %=
        T_OperatorFloorDivEqual,      // //=
        T_OperatorExpoEqual,          // **=
        T_OperatorMatrixMulEqual,     // @= introduced in py 3.5

        // assignment bitwise
        T__OperatorAssignBitwiseStart,
        T_OperatorBitAndEqual,           // &=
        T_OperatorBitOrEqual,            // |=
        T_OperatorBitXorEqual,           // ^=
        T_OperatorBitNotEqual,           // ~=
        T_OperatorBitShiftRightEqual,    // >>=
        T_OperatorBitShiftLeftEqual,     // <<=
        T__OperatorAssignBitwiseEnd,
        T__OperatorAssignmentEnd = T__OperatorAssignBitwiseEnd,

        // compare operators
        T__OperatorCompareStart  = T__OperatorAssignmentEnd,
        T_OperatorCompareEqual,        // ==,
        T_OperatorNotEqual,            // !=,
        T_OperatorLessEqual,           // <=,
        T_OperatorMoreEqual,           // >=,
        T_OperatorLess,                // <,
        T_OperatorMore,                // >,
        T__OperatorCompareKeywordStart,
        T_OperatorAnd,                 // 'and'
        T_OperatorOr,                  // 'or'
        T_OperatorNot,                 // ! or 'not'
        T_OperatorIs,                  // 'is'
        T_OperatorIn,                  // 'in'
        T__OperatorCompareKeywordEnd,
        T__OperatorCompareEnd = T__OperatorCompareKeywordEnd,

        // special meaning
        T__OperatorParamStart = T__OperatorCompareEnd,
        T_OperatorVariableParam,       // * for function parameters ie (*arg1)
        T_OperatorKeyWordParam,        // ** for function parameters ir (**arg1)
        T__OperatorParamEnd,
        T__OperatorEnd = T__OperatorParamEnd,

        // delimiters
        T__DelimiterStart = T__OperatorEnd,
        T_Delimiter,                       // all other non specified
        T_DelimiterOpenParen,              // (
        T_DelimiterCloseParen,             // )
        T_DelimiterOpenBracket,            // [
        T_DelimiterCloseBracket,           // ]
        T_DelimiterOpenBrace,              // {
        T_DelimiterCloseBrace,             // }
        T_DelimiterPeriod,                 // .
        T_DelimiterComma,                  // ,
        T_DelimiterColon,                  // :
        T_DelimiterSemiColon,              // ;
        T_DelimiterEllipsis,               // ...
        // metadata such def funcname(arg: "documentation") ->
        //                            "returntype documentation":
        T_DelimiterMetaData,               // -> might also be ':' inside arguments

        // Text line specific
        T__DelimiterTextLineStart,
        T_DelimiterLineContinue,
                                           // when end of line is escaped like so '\'
        T_DelimiterNewLine,                // each new line
        T__DelimiterTextLineEnd,
        T__DelimiterEnd = T__DelimiterTextLineEnd,

        // identifiers
        T__IdentifierStart = T__DelimiterEnd,
        T__IdentifierVariableStart = T__IdentifierStart,
        T_IdentifierUnknown,                 // name is not identified at this point
        T_IdentifierDefined,                 // variable is in current context
        T_IdentifierSelf,                    // specialcase self
        T_IdentifierBuiltin,                 // its builtin in class methods
        T__IdentifierVariableEnd,

        T__IdentifierImportStart = T__IdentifierVariableEnd,
        T_IdentifierModule,                  // its a module definition
        T_IdentifierModulePackage,           // identifier is a package, ie: root for other modules
        T_IdentifierModuleAlias,             // alias for import. ie: import Something as Alias
        T_IdentifierModuleGlob,              // from mod import * <- glob
        T__IdentifierImportEnd,

        T__IdentifierDeclarationStart = T__IdentifierImportEnd,
        T_IdentifierFunction,                // its a function definition
        T_IdentifierMethod,                  // its a method definition
        T_IdentifierClass,                   // its a class definition
        T_IdentifierSuperMethod,             // its a method with name: __**__
        T_IdentifierDecorator,               // member decorator like: @property
        T_IdentifierDefUnknown,              // before system has determined if its a
                                             // method or function yet
        T_IdentifierNone,                    // The None keyword
        T_IdentifierTrue,                    // The bool True
        T_IdentifierFalse,                   // The bool False
        T_IdentifierInvalid,                 // The identifier could not be bound to any other variable
                                             // or it has some error, such as calling a non callable
        T__IdentifierDeclarationEnd,
        T__IdentifierEnd = T__IdentifierDeclarationEnd,

        // metadata such def funcname(arg: "documentaion") -> "returntype documentation":
        T_MetaData = T__IdentifierEnd,

        // these are inserted by PythonSourceRoot
        T_BlockStart, // indicate a new block ie if (a is True):
                      //                                       ^
        T_BlockEnd,   // indicate block end ie:    dosomething
                      //                        dosomethingElse
                      //                       ^
        T_Invalid, // let compiler decide last +1

        // console specific
        T_PythonConsoleOutput     = 1000,
        T_PythonConsoleError      = 1001
    };

    explicit Token(Tokens token, int startPos, int endPos, Python::TokenLine *line = nullptr);
    Token(const Token &other);
    ~Token();
    bool operator==(const Token &rhs) const
    {
        return line() == rhs.line() && token == rhs.token &&
               startPos == rhs.startPos && endPos == rhs.endPos;
    }
    bool operator > (const Token &rhs) const
    {
        if (line() > rhs.line())
            return true;
        if (line() < rhs.line())
            return false;
        return startPos > rhs.startPos;
    }
    bool operator < (const Token &rhs) const { return (rhs > *this); }
    bool operator <= (const Token &rhs) const { return !(rhs < *this); }
    bool operator >= (const Token &rhs) const { return !(*this < rhs); }

    /// for traversing tokens in document
    /// returns token or nullptr if at end/begin
    Token *next() const { return m_next; }
    Token *previous() const { return m_previous; }

    // public properties
    Tokens token;
    int startPos;
    int endPos;

    int textLength() const { return endPos - startPos; }

    /// pointer to our father textBlockData
    Python::TextBlockData *txtBlock() const { return m_txtBlock; }
    Python::TokenList *ownerList() const;
    Python::TokenLine *ownerLine() const { return m_ownerLine; }
    QString text() const;
    int line() const;

    // tests
    bool isNumber() const;
    bool isInt() const;
    bool isFloat() const;
    bool isString() const;
    bool isMultilineString() const;
    bool isBoolean() const;
    bool isKeyword() const;
    bool isOperator() const;
    bool isOperatorArithmetic() const;
    bool isOperatorBitwise() const;
    bool isOperatorAssignment() const;
    bool isOperatorAssignmentBitwise() const;
    bool isOperatorCompare() const;
    bool isOperatorCompareKeyword() const;
    bool isOperatorParam() const;
    bool isDelimiter() const;
    bool isIdentifier() const;
    bool isIdentifierVariable() const;
    bool isIdentifierDeclaration() const;
    bool isNewLine() const; // might be escaped this checks for that
    bool isInValid() const;
    bool isUndetermined() const;
    bool isImport() const;

    // returns true if token represents code (not just T_Indent, T_NewLine etc)
    // comment is also ignored
    bool isCode() const;
    // like isCode but returns true if it is a comment
    bool isText() const;


    // all references get a call to PythonSourceListNode::tokenDeleted
    void attachReference(Python::SourceListNodeBase *srcListNode);
    void detachReference(Python::SourceListNodeBase *srcListNode);


private:
    QList<Python::SourceListNodeBase*> m_srcLstNodes;

    Token *m_next,
          *m_previous;
    TokenLine *m_ownerLine;

#ifdef BUILD_PYTHON_DEBUGTOOLS
    QString m_nameDbg;
    uint m_lineDbg;
#endif
    friend class Python::TokenList;
    friend class Python::TokenLine;
};

// ----------------------------------------------------------------------------------------

/// Handles all tokens for a single file
/// implements logic for 2 linked lists, Tokens and TokenLines
/// TokenList is container for each token in each row
class TokenList {
    Python::Token *m_first,
                  *m_last;
    Python::TokenLine *m_firstLine,
                      *m_lastLine;
    uint32_t m_size;
public:
    explicit TokenList();
    explicit TokenList(const TokenList &other);
    virtual ~TokenList();

    // accessor methods
    Python::Token *front() const { return m_first; }
    Python::Token *back() const { return m_last; }
    Python::Token *end() const { return nullptr; }
    Python::Token *rend() const { return nullptr; }
    Python::Token *operator[] (int32_t idx);

    // info
    bool empty() const { return m_first != nullptr && m_last != nullptr; }
    uint32_t count() const;
    uint32_t max_size() const { return 20000000u; }

    // modifiers
    void clear();
    void push_back(Python::Token *tok);
    void push_front(Python::Token *tok);
    Python::Token *pop_back();
    Python::Token *pop_front();
    /// inserts token into list, list takes ownership and may delete token
    void insert(Token *previousTok, Python::Token *insertTok);

    /// removes tok from list
    /// if deleteTok is true it also deletes these tokens (mem. free)
    /// else caller must delete tok
    bool remove(Python::Token *tok, bool deleteTok = true);

    /// removes tokens from list
    /// removes from from tok up til endTok, endTok is the first token not removed
    /// if deleteTok is true it also deletes these tokens (mem. free)
    /// else caller must delete tok
    bool remove(Python::Token *tok,
                Python::Token *endTok,
                bool deleteTok = true);

    //lines
    /// get the first line for this list/document
    Python::TokenLine *firstLine() const { return m_firstLine; }
    /// get the last line for this list/document
    Python::TokenLine *lastLine() const { return m_lastLine; }
    /// get the line at lineNr, might be negative for reverse. ie: -1 == lastLine
    /// lineNr is 0 based (ie. first row == 0), lastRow == -1
    Python::TokenLine *lineAt(int32_t lineNr) const;
    /// get total number of lines
    uint32_t lineCount() const;

    /// swap line, old line is deleted with all it containing tokens
    /// lineNr is 0 based (ie. first row == 0), lastRow == -1
    void swapLine(int32_t lineNr, Python::TokenLine *swapIn);

    /// swap line, old line is deleted with all it containing tokens
    /// list takes ownership of swapInLine and its tokens
    void swapLine(Python::TokenLine *swapOut, Python::TokenLine *swapIn);

    /// insert line at lineNr, list takes ownership of line and its tokens
    /// lineNr is 0 based (ie. first row == 0), last row == -1
    void insertLine(int32_t lineNr, Python::TokenLine *insertLine);
    /// insert line directly after previousLine, list takes ownership of line and its tokens
    void insertLine(Python::TokenLine *previousLine, Python::TokenLine *insertLine);

    /// inserts line at end of lines list
    /// list takes ownership of line and its tokens
    void appendLine(Python::TokenLine *lineToPush);

    /// remove line at lineNr and deletes the line
    /// lineNr is 0 based (ie. first row == 0), last row == -1
    void removeLine(int32_t lineNr);
    /// remove line from this list and deletes the line
    void removeLine(Python::TokenLine *lineToRemove);


private:
    friend class Python::TokenLine;
};

// -------------------------------------------------------------------------------------------

class TokenLine {
    Python::TokenList *m_ownerList;
    Python::Token *m_startTok;
    Python::TokenLine *m_nextLine, *m_previousLine;
    QString m_text;
    Python::TextBlockData *m_txtBlock;
public:
    explicit TokenLine(Python::TokenList *ownerList,
                       Python::Token *startTok,
                       const QString &text, TextBlockData *txtBlock = nullptr);
    ~TokenLine();

    Python::TokenList *ownerList() const { return m_ownerList; }


    /// returns this lines linenr, 0 based
    uint32_t lineNr() const;
    /// returns this lines text content (without line ending chars)
    QString text() const { return m_text; }

    Python::TextBlockData *txtBlock() const { return m_txtBlock; }

    /// returns the number of tokens
    uint count() const;
    bool empty() const { return m_startTok != nullptr; }

    Python::Token *front() const { return m_startTok; }
    Python::Token *back() const;
    Python::Token *end() const;
    Python::Token *rend() const;
    Python::TokenLine *nextLine() const { return m_nextLine; }
    Python::TokenLine *previousLine() const { return m_previousLine; }
    /// returns the token at idx position
    Python::Token *operator[] (int idx);


    void push_back(Python::Token *tok);
    void push_front(Python::Token *tok);
    /// removes last token in this line
    /// caller taken ownership and must delete token
    Python::Token *pop_back();
    /// removes first token in this line
    /// caller taken ownership and must delete token
    Python::Token *pop_front();
    /// inserts token into list
    void insert(Python::Token *tok);

    /// removes tok from list
    /// if deleteTok is true it also deletes these tokens (mem. free)
    bool remove(Python::Token *tok, bool deleteTok = true);

    /// removes tokens from list
    /// removes from from tok up til endTok, endTok is the first token not removed
    /// if deleteTok is true it also deletes these tokens (mem. free)
    bool remove(Python::Token *tok,
                Python::Token *endTok,
                bool deleteTok = true);

private:
    friend class Python::TokenList;
};


} // namespace Python
} // namespace Gui

#endif // PYTHONTOKEN_H
