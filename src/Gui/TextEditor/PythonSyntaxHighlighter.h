#ifndef PYTHONSYNTAXHIGHLIGHTER_H
#define PYTHONSYNTAXHIGHLIGHTER_H

#include "PreCompiled.h"
#include "SyntaxHighlighter.h"
#include "TextEditor.h"
#include <Base/Parameter.h>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Base {
class PyException;
class PyExceptionInfo;
}

namespace Gui {
class PythonToken;
class PythonEditor;
class PythonConsoleTextEdit;
class PythonTextBlockData;
class TextEditBlockScanInfo;
class PythonSyntaxHighlighterP;

/**
 * Syntax highlighter for Python.
 */
class GuiExport PythonSyntaxHighlighter : public SyntaxHighlighter
{
    Q_OBJECT
public:
    PythonSyntaxHighlighter(QObject* parent);
    virtual ~PythonSyntaxHighlighter();

    void highlightBlock (const QString & text);

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

    /// masks for decoding userState()
    enum {
        ParamLineShiftPos = 17,
        ParenCntShiftPos  = 28,
        TokensMASK      = 0x0000FFFF,
        ParamLineMASK   = 0x00010000, // when we are whithin a function parameter line
        ParenCntMASK    = 0xF0000000, // how many parens we have open from previous block
        PreventTokenize = 0x00020000, // prevents this call from running tokenize, used to repaint this block
        Rehighlighted   = 0x00040000, // used to determine if we already have rehighlighted
    };

    /// returns the format to color this token
    QTextCharFormat getFormatToken(const PythonToken *token) const;

    // used by code analyzer, set by editor
    void setFilePath(QString filePath);
    QString filePath() const;

protected:
    // tokenizes block, called by highlightBlock when text have changed
    int tokenize(const QString &text, int &parenCnt, bool &isParamLine);

    /// sets (re-colors) txt contained from token
    void setFormatToken(const PythonToken *tok);

private Q_SLOTS:
    void sourceScanTmrCallback();

private:
    PythonSyntaxHighlighterP* d;

    //const QString getWord(int pos, const QString &text) const;
    int lastWordCh(int startPos, const QString &text) const;
    int lastNumberCh(int startPos, const QString &text) const;
    int lastDblQuoteStringCh(int startAt, const QString &text) const;
    int lastSglQuoteStringCh(int startAt, const QString &text) const;
    Tokens numberType(const QString &text) const;

    void setRestOfLine(int &pos, const QString &text, Tokens token);
    void scanIndentation(int &pos, const QString &text);

    void setWord(int &pos, int len, Tokens token);

    void setIdentifier(int &pos, int len, Tokens token);
    void setUndeterminedIdentifier(int &pos, int len, Tokens token);

    void setNumber(int &pos, int len, Tokens token);

    void setOperator(int &pos, int len, Tokens token);

    void setDelimiter(int &pos, int len, Tokens token);

    void setSyntaxError(int &pos, int len);

    void setLiteral(int &pos, int len, Tokens token);

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

// --------------------------------------------------------------------



/**
 * @brief The PythonMatchingChars highlights the opposite (), [] or {}
 */
class PythonMatchingChars : public QObject
{
    Q_OBJECT

public:
    PythonMatchingChars(PythonEditor *parent);
    PythonMatchingChars(PythonConsoleTextEdit *parent);
    ~PythonMatchingChars();

private Q_SLOTS:
    void cursorPositionChange();

private:
    QPlainTextEdit *m_editor;
};

// ------------------------------------------------------------------------
class PythonSourceListNodeBase; // in code analyzer

struct PythonToken
{
    explicit PythonToken(PythonSyntaxHighlighter::Tokens token, int startPos, int endPos,
                         PythonTextBlockData *block);
    ~PythonToken();
    bool operator==(const PythonToken &rhs) const
    {
        return line() == rhs.line() && token == rhs.token &&
               startPos == rhs.startPos && endPos == rhs.endPos;
    }
    bool operator > (const PythonToken &rhs) const
    {
        if (line() > rhs.line())
            return true;
        if (line() < rhs.line())
            return false;
        return startPos > rhs.startPos;
    }
    bool operator < (const PythonToken &rhs) const { return (rhs > *this); }
    bool operator <= (const PythonToken &rhs) const { return !(rhs > *this); }
    bool operator >= (const PythonToken &rhs) const { return !(*this > rhs); }

    /// for traversing tokens in document
    /// returns token or nullptr if at end/begin
    PythonToken *next() const;
    PythonToken *previous() const;

    // public properties
    PythonSyntaxHighlighter::Tokens token;
    int startPos;
    int endPos;

    /// pointer to our father textBlockData
    PythonTextBlockData *txtBlock() const { return m_txtBlock; }
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
    void attachReference(PythonSourceListNodeBase *srcListNode);
    void detachReference(PythonSourceListNodeBase *srcListNode);


private:
    QList<PythonSourceListNodeBase*> m_srcLstNodes;
    PythonTextBlockData *m_txtBlock;
};

// -----------------------------------------------------------------------
class PythonTextBlockScanInfo;

class PythonTextBlockData : public TextEditBlockData
{
public:
    typedef QVector<PythonToken*> tokens_t;
    explicit PythonTextBlockData(QTextBlock block);
    PythonTextBlockData(const PythonTextBlockData &other);
    ~PythonTextBlockData();

    static PythonTextBlockData *pyBlockDataFromCursor(const QTextCursor &cursor);
    /**
     * @brief tokens all PythonTokens for this row
     */
    const tokens_t &tokens() const;

    PythonTextBlockData *next() const;
    PythonTextBlockData *previous() const;
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
     * @brief firstTextToken lookup first token that is non whitespace
     * @return first token that is a Text token (non whitespace)
     */
    const PythonToken *firstTextToken() const;

    /**
     * @brief firstCodeToken lookup first token that is a code token
     * @return first token that is a code token
     */
    const PythonToken *firstCodeToken() const;

    /**
     * @brief tokensBetweenOfType counts tokens to type match between these positions
     *         NOTE! this method ignores T_Indent tokens
     * @param startPos position to start at
     * @param endPos position where it should stop looking
     * @param match against this token
     * @return number of times token was found
     */
    int tokensBetweenOfType(int startPos, int endPos, PythonSyntaxHighlighter::Tokens match) const;

    /**
     * @brief lastInserted gives the last inserted token
     *         NOTE! this method ignores T_Indent tokens
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
    PythonToken *tokenAt(int pos) const;

    static PythonToken* tokenAt(const QTextCursor &cursor);

    /**
     * @brief tokenAtAsString returns the token under pos (in block)
     * @param pos from start of line
     * @return token as string
     */
    QString tokenAtAsString(int pos) const;

    QString tokenAtAsString(const PythonToken *tok) const;

    static QString tokenAtAsString(QTextCursor &cursor);


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

    /**
     * @brief insertToken inserts a token at position
     * @param token: Type of token
     * @param pos: where in textducument
     * @param len: token length in document
     */
    PythonToken *insertToken(PythonSyntaxHighlighter::Tokens token, int pos, int len);

    /**
     * @brief scanInfo contains parse messages set by code analyzer
     * @return nullptr if no parsemessages  or PythonTextBlockScanInfo*
     */
    PythonTextBlockScanInfo *scanInfo() const;

    /**
     * @brief setScanInfo set class with parsemessages
     * @param scanInfo instance of PythonTextBlockScanInfo
     */
    void setScanInfo(PythonTextBlockScanInfo *scanInfo);


    /**
     * @brief setReformat sets up token to be repainted by format
     *        We must later call PythonSyntaxHiglighter::rehighlight(this.block())
     * @param tok PythonToken to be reformated, must be stored in this instance
     * @param format QTextCharFormat to use
     * @return true if token was stored in here
     */
    bool setReformat(const PythonToken *tok, QTextCharFormat format);

    QHash<const PythonToken*, QTextCharFormat> &allReformats() { return m_reformats; }

protected:
    /**
     * @brief insert should only be used by PythonSyntaxHighlighter
     */
    const PythonToken *setDeterminedToken(PythonSyntaxHighlighter::Tokens token, int startPos, int len);
    /**
     * @brief insert should only be used by PythonSyntaxHighlighter
     *  signifies that parse tree lookup is needed
     */
    const PythonToken *setUndeterminedToken(PythonSyntaxHighlighter::Tokens token, int startPos, int len);


    /**
     * @brief setIndentCount should be considered private, is
     * @param count
     */
    void setIndentCount(int count);


private:
    tokens_t m_tokens;
    QVector<int> m_undeterminedIndexes; // index to m_tokens where a undetermined is at
                                        //  (so context parser can detemine it later)
    QHash<const PythonToken*, QTextCharFormat> m_reformats;
    int m_indentCharCount; // as spaces NOTE according to python documentation a tab is 8 spaces


    friend class PythonSyntaxHighlighter; // in order to hide some api
};

// -------------------------------------------------------------------

class PythonTextBlockScanInfo : public TextEditBlockScanInfo
{
public:
    explicit PythonTextBlockScanInfo();
    virtual ~PythonTextBlockScanInfo();


    /// set message for token
    void setParseMessage(const PythonToken *tok, QString message, MsgType type = Message);

    /// get the ParseMsg for tok, filter by type
    /// nullptr if not found
    const ParseMsg *getParseMessage(const PythonToken *tok, MsgType type = AllMsgTypes) const;

    /// get parseMessage for token, filter by type
    QString parseMessage(const PythonToken *tok, MsgType type = AllMsgTypes) const;

    /// clear message
    void clearParseMessage(const PythonToken *tok);
};

// -------------------------------------------------------------------

/**
 * @brief helper to determine what icon to use based on exception
 */
class PyExceptionInfoGui
{
public:
    explicit PyExceptionInfoGui(Base::PyExceptionInfo *exc);
    ~PyExceptionInfoGui();
    const char *iconName() const;
private:
    Base::PyExceptionInfo *m_exc;
};
 } // namespace Gui


#endif // PYTHONSYNTAXHIGHLIGHTER_H
