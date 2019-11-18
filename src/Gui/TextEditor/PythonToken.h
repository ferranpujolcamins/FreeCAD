#ifndef PYTHONTOKEN_H
#define PYTHONTOKEN_H

#include <vector>
#include <string>
#include <list>

namespace Gui {
namespace Python {

class SourceListNodeBase; // in code analyzer
class TextBlockData;
class TokenList;
class TokenLine;
class Tokenizer;
class TokenScanInfo;

class Token
{
public:
    enum Type {
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

    explicit Token(Type tokType, uint startPos, uint endPos, Python::TokenLine *line);
    Token(const Token &other);
    ~Token();
    bool operator==(const Token &rhs) const
    {
        return line() == rhs.line() && m_type == rhs.m_type &&
               m_startPos == rhs.m_startPos && m_endPos == rhs.m_endPos;
    }
    bool operator > (const Token &rhs) const
    {
        int myLine = line(), rhsLine = rhs.line();
        if (myLine > rhsLine)
            return true;
        if (myLine < rhsLine)
            return false;
        return m_startPos > rhs.m_startPos;
    }
    bool operator < (const Token &rhs) const { return (rhs > *this); }
    bool operator <= (const Token &rhs) const { return !(rhs < *this); }
    bool operator >= (const Token &rhs) const { return !(*this < rhs); }

    /// for traversing tokens in document
    /// returns token or nullptr if at end/begin
    Token *next() const { return m_next; }
    Token *previous() const { return m_previous; }

    // public properties
    Type type() const { return m_type; }
    void changeType(Type tokType);
    uint startPos() const { return m_startPos; }
    int startPosInt() const { return static_cast<int>(m_startPos); }
    uint endPos() const { return m_endPos; }
    int endPosInt() const { return  static_cast<int>(m_endPos); }

    uint textLength() const { return m_endPos - m_startPos; }

    /// pointer to our father textBlockData
    Python::TokenList *ownerList() const;
    Python::TokenLine *ownerLine() const;
    std::string text() const;
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
    Type m_type;
    uint m_startPos, m_endPos;
    std::list<Python::SourceListNodeBase*> m_srcLstNodes;

    Token *m_next,
          *m_previous;
    TokenLine *m_ownerLine;

#ifdef BUILD_PYTHON_DEBUGTOOLS
    std::string m_nameDbg;
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
    bool empty() const { return m_first == nullptr && m_last == nullptr; }
    uint32_t count() const;
    uint32_t max_size() const { return 20000000u; }

    // modifiers for tokens
    void clear();

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
    void removeLine(int32_t lineNr, bool deleteLine = true);
    /// remove line from this list and deletes the line
    void removeLine(Python::TokenLine *lineToRemove, bool deleteLine = true);

    // should modify through TokenLine
private:
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

private:
    friend class Python::TokenLine;
};

// -------------------------------------------------------------------------------------------

class TokenLine {
    Python::TokenList *m_ownerList;
    Python::Token *m_startTok;
    Python::TokenLine *m_nextLine, *m_previousLine;
    Python::TokenScanInfo *m_tokenScanInfo;
    std::string m_text;

    std::vector<int> m_undeterminedIndexes; // index to m_tokens where a undetermined is at
                                            //  (so context parser can determine it later)
    int m_indentCharCount; // as spaces NOTE according to python documentation a tab is 8 spaces
    int m_parenCnt, m_bracketCnt, m_braceCnt, m_blockStateCnt;
    bool m_isParamLine;

public:
    explicit TokenLine(Python::TokenList *ownerList,
                       Python::Token *startTok,
                       const std::string &text);
    TokenLine(const Python::TokenLine &other);
    virtual ~TokenLine();

    // line info methods
    /// returns the container that stores this line
    Python::TokenList *ownerList() const { return m_ownerList; }

    /// returns this lines linenr, 0 based
    uint32_t lineNr() const;
    /// returns this lines text content (without line ending chars)
    const std::string &text() const { return m_text; }

    /// returns the number of tokens
    uint count() const;
    /// true if line is empty
    bool empty() const { return m_startTok == nullptr; }

    /// returns the number of chars that starts this line
    int indent() const;

    /// isCodeLine checks if line has any code
    /// lines with only indent or comments return false
    /// return true if line has code
    bool isCodeLine() const;

    /// number of parens in this line
    /// '(' = parenCnt++
    /// ')' = parenCnt--
    /// Can be negative if this line has more closing parens than opening ones
    int parenCnt() const { return  m_parenCnt; }
    /// number of brackets '[' int this line, see as parenCnt description
    int bracketCnt() const { return m_braceCnt; }
    /// number of braces '{' in this line, see parenCnt for description
    int braceCnt() const { return m_braceCnt; }

    /// same as above but reference
    int &parenCntRef() { return m_parenCnt; }
    int &bracketCntRef() { return m_bracketCnt; }
    int &braceCntRef() { return  m_braceCnt; }


    /// blockState tells if this block starts a new block,
    ///         such as '{' in C like languages or ':' in python
    ///         +1 = blockstart, -1 = blockend, -2 = 2 blockends ie '}}'
    virtual int blockState() const { return m_blockStateCnt; }
    virtual int incBlockState() { return ++m_blockStateCnt; }
    virtual int decBlockState() { return --m_blockStateCnt; }

    bool operator== (const Python::TokenLine &rhs) const;


    // accessor methods
    Python::Token *front() const { return m_startTok; }
    Python::Token *back() const;
    Python::Token *end() const;
    Python::Token *rend() const;
    /// gets the nextline sibling
    Python::TokenLine *nextLine() const { return m_nextLine; }
    Python::TokenLine *previousLine() const { return m_previousLine; }
    /// returns the token at idx position in line or nullptr
    Python::Token *operator[] (int idx) const;
    Python::Token *tokenAt(int idx) const;

    /// returns a list with all tokens in this line
    const std::list<Python::Token*> tokens() const;


    /// findToken searches for token in this block
    /// tokType = needle to search for
    /// searchFrom start position in line to start the search
    ///                   if negative start from the back ie -10 searches from pos 10 to 0
    /// returns pointer to token if found, else nullptr
    const Python::Token* findToken(Python::Token::Type tokType,
                                   int searchFrom = 0) const;

    /// firstTextToken lookup first token that is a text (comment or code) token or nullptr
    const Python::Token *firstTextToken() const;
    /// firstCodeToken lookup first token that is a code token or nullptr
    const Python::Token *firstCodeToken() const;



    // line/list mutator methods
    void push_back(Python::Token *tok);
    void push_front(Python::Token *tok);
    /// removes last token in this line
    /// caller taken ownership and must delete token
    Python::Token *pop_back();
    /// removes first token in this line
    /// caller taken ownership and must delete token
    Python::Token *pop_front();
    /// inserts token into list, returns position it is stored at
    int insert(Python::Token *tok);

    /// removes tok from list
    /// if deleteTok is true it also deletes these tokens (mem. free)
    bool remove(Python::Token *tok, bool deleteTok = true);

    /// removes tokens from list
    /// removes from from tok up til endTok, endTok is the first token not removed
    /// if deleteTok is true it also deletes these tokens (mem. free)
    bool remove(Python::Token *tok,
                Python::Token *endTok,
                bool deleteTok = true);

    // these are accessed from Python::Tokenizer
    /// create a token with tokType and insert at pos with length
    Python::Token *newDeterminedToken(Python::Token::Type tokType, uint startPos, uint len);
    /// this insert should only be used by PythonSyntaxHighlighter
    /// stores this token id as needing a parse tree lookup to determine the tokenType
    Python::Token *newUndeterminedToken(Python::Token::Type tokType, uint startPos, uint len);


    ///tokenScanInfo contains messages for a specific code line/col
    /// if initScanInfo is true it also creates a container if not existing
    TokenScanInfo *tokenScanInfo(bool initScanInfo = false);


private:
    /// set indentcount
    void setIndentCount(uint count);

    /// needed because offset when multiple inheritance
    /// returns the real pointer to this instance, not to subclass
    Python::TokenLine *instance() const {
        return static_cast<Python::TokenLine*>(
                    const_cast<Python::TokenLine*>(this));
    }

    friend class Python::TokenList;
    friend class Python::Tokenizer;
};

// -------------------------------------------------------------------------------------

/// implements logic to tokenize a string input
class TokenizerP;
class Tokenizer {
public:
    explicit Tokenizer();
    virtual ~Tokenizer();

    Python::TokenList &list();

    virtual void tokenTypeChanged(const Python::Token *tok) const;


    void setFilePath(const std::string &filePath);
    const std::string &filePath() const;

    /// event callbacks, subclass may override these to get info
    virtual void setMessage(const Python::Token *tok) const { (void)tok; }
    virtual void setIndentError(const Python::Token *tok) const { (void)tok; }
    virtual void setSyntaxError(const Python::Token *tok) const { (void)tok; }

protected:
    Python::TokenizerP *d_tok;

    uint tokenize(Python::TokenLine *tokLine);

    /// this method is called when we cant tokenize a char
    /// subclasses should implement this function
    virtual Python::Token::Type unhandledState(uint &pos, int state, const std::string &text);

    /// call this method when tok type has changed
    virtual void tokenUpdated(const Python::Token *tok);

    Python::Token::Type &endStateOfLastParaRef() const;
    Python::TokenLine *activeLine() const;
    void setActiveLine(Python::TokenLine *line);


private:
    uint lastWordCh(uint startPos, const std::string &text) const;
    uint lastNumberCh(uint startPos, const std::string &text) const;
    uint lastDblQuoteStringCh(uint startAt, const std::string &text) const;
    uint lastSglQuoteStringCh(uint startAt, const std::string &text) const;
    Python::Token::Type numberType(const std::string &text) const;

    void setRestOfLine(uint &pos, const std::string &text, Python::Token::Type tokType);
    void scanIndentation(uint &pos, const std::string &text);
    void setWord(uint &pos, uint len, Python::Token::Type tokType);
    void setIdentifier(uint &pos, uint len, Python::Token::Type tokType);
    void setUndeterminedIdentifier(uint &pos, uint len, Python::Token::Type tokType);
    void setNumber(uint &pos, uint len, Python::Token::Type tokType);
    void setOperator(uint &pos, uint len, Python::Token::Type tokType);
    void setDelimiter(uint &pos, uint len, Python::Token::Type tokType);
    void setSyntaxError(uint &pos, uint len);
    void setLiteral(uint &pos, uint len, Python::Token::Type tokType);
    void setIndentation(uint &pos, uint len, uint count);

};

// ------------------------------------------------------------------------------------------

class TokenScanInfo
{
public:
    enum MsgType { Message, LookupError, SyntaxError, IndentError, Warning, Issue, AllMsgTypes };
    struct ParseMsg {
    public:
        explicit ParseMsg(const std::string &message, const Python::Token *tok, MsgType type);
        ~ParseMsg();
        const std::string msgTypeAsString() const;
        const std::string message() const;
        const Python::Token *token() const;
        MsgType type() const;

    private:
        std::string m_message;
        const Python::Token *m_token;
        MsgType m_type;
    };
    explicit TokenScanInfo();
    virtual ~TokenScanInfo();
    /// create a new parsemsg attached to token
    void setParseMessage(const Python::Token *tok, const std::string &msg, MsgType type);
    /// get all parseMessage attached to token
    /// filter by type
    const std::list<const ParseMsg*> getParseMessages(const Python::Token *tok,
                                                      MsgType type = AllMsgTypes) const;
    /// remove all messages attached to tok, returns number of deleted parseMessages
    int clearParseMessages(const Python::Token *tok);

    std::list<const ParseMsg*> allMessages() const;

private:
    std::list<const ParseMsg*> m_parseMsgs;
};


} // namespace Python
} // namespace Gui

#endif // PYTHONTOKEN_H
