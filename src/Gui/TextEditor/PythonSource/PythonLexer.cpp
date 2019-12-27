
#include "Python.h"
#include "PythonLexer.h"

#include "PythonToken.h"
#include "PythonSource.h"


#include <algorithm>
#include <unordered_map>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>


bool isNumber(char ch)
{
    return (ch >= '0' && ch <= '9');
}

bool isSpace(char ch)
{
    return ch == ' ' || ch == '\t';
}

// need to differentiate between unicode in Py3 and ascii in Py2
typedef bool (*letterTypPtr)(char ch);
bool isLetterPy2(char ch)
{
    // is letter
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

bool isLetterPy3(char ch)
{
    // is letter or unicode
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch < 0);
}
static letterTypPtr isLetter = &isLetterPy3;


namespace Python {
class LexerP {
    static void insertKW(const char *cstr)
    {
        std::size_t hash = cstrToHash(cstr, strlen(cstr));
        keywords[hash] = cstr;
    }
public:
    LexerP(Python::Lexer *lexer) :
        tokenList(lexer),
        activeLine(nullptr),
        insertDedent(false), isCodeLine(false)
    {
        if (LexerP::activeVersion != Version::Invalid)
            reGenerate(LexerP::activeVersion);
        else
            reGenerate(Version::Latest);
    }
    ~LexerP()
    {
    }

    static void reGenerate(Version::versions ver)
    {
        if (version.version() == ver)
            return;

        LexerP::version.setVersion(ver);

        activeVersion = version.version();

        if (version.version() >= Version::v3_0)
            isLetter = &isLetterPy3;
        else
            isLetter = &isLetterPy2;

        keywords.clear();
        builtins.clear();
        // GIL lock code block
        PyGILState_STATE lock = PyGILState_Ensure();

        PyObject *pyObj = PyEval_GetBuiltins();
        PyObject *key, *value;
        Py_ssize_t pos = 0;

        while (PyDict_Next(pyObj, &pos, &key, &value)) {

#if PY_MAJOR_VERSION >= 3
            const char *name = PyUnicode_AsUTF8(key);
#else
            const char *name = PyString_AS_STRING(key);
#endif
            if (name != nullptr) {
                size_t hash = Python::cstrToHash(name, strlen(name));
                builtins[hash] = name;
            }
        }
        PyGILState_Release(lock);

        //
        // keywords
        // 2.6 and 2.7 https://docs.python.org/2.7/reference/lexical_analysis.html#keywords
        // and       del       from      not       while
        // as        elif      global    or        with
        // assert    else      if        pass      yield
        // break     except    import    print
        // class     exec      in        raise
        // continue  finally   is        return
        // def       for       lambda    try
        insertKW("and");      insertKW("del");
        insertKW("from");     insertKW("not");
        insertKW("while");    insertKW("as");
        insertKW("elif");     insertKW("global");
        insertKW("or");       insertKW("with");
        insertKW("assert");   insertKW("else");
        insertKW("if");       insertKW("pass");
        insertKW("yield");    insertKW("break");
        insertKW("except");   insertKW("import");
        insertKW("class");    insertKW("in");
        insertKW("raise");    insertKW("continue");
        insertKW("finally");  insertKW("is");
        insertKW("return");   insertKW("def");
        insertKW("for");      insertKW("lambda");
        insertKW("lambda");   insertKW("try");

        if (Lexer::version().majorVersion() == 2) {
            insertKW("print"); insertKW("exec");

        } else if (Lexer::version().majorVersion() >= 3) {
            insertKW("nonlocal");

            // 3.0 https://docs.python.org/3.0/reference/lexical_analysis.html#keywords
            // False      class      finally    is         return
            // None       continue   for        lambda     try
            // True       def        from       nonlocal   while
            // and        del        global     not        with
            // as         elif       if         or         yield
            // assert     else       import     pass
            // break      except     in         raise
            insertKW("False");    insertKW("None");
            insertKW("True");

            if (Lexer::version().version() >= Version::v3_7) {
                insertKW("async");    insertKW("await"); //2 new keywords from 3.7
            }
        }


        // keywords takes precedence over builtins
        for (auto &item : keywords) {
            auto it = builtins.find(item.first);
            if (it != builtins.end())
                builtins.erase(it);
        }

    }
    static Python::Version version;
    static Python::Version::versions activeVersion;
    static std::unordered_map<std::size_t, std::string> keywords;
    static std::unordered_map<std::size_t, std::string> builtins;
    Python::TokenList tokenList;
    std::string filePath;
    Python::TokenLine *activeLine;
    bool insertDedent, isCodeLine;
    uint8_t __pad[2];

    // these hashes are constant and wont change, use for quicker lookup
    static const std::size_t defHash, classHash, importHash, fromHash, andHash,
                             asHash, inHash, isHash, notHash, orHash,yieldHash,
                             returnHash, trueHash, falseHash, noneHash, ifHash,
                             elifHash, elseHash, forHash, whileHash, breakHash,
                             continueHash, tryHash, exceptHash, finallyHash,
                             raiseHash, withHash, globalHash, lambdaHash,
                             passHash, assertHash;
};

// static
Python::Version Python::LexerP::version(Version::Latest);
//static
Python::Version::versions Python::LexerP::activeVersion = Python::Version::Invalid;
//static
std::unordered_map<std::size_t, std::string> Python::LexerP::keywords;
//static
std::unordered_map<std::size_t, std::string> Python::LexerP::builtins;

// hashes to speed up lookups in lexer
const std::size_t Python::LexerP::defHash    = cstrToHash("def", 3);
const std::size_t Python::LexerP::classHash  = cstrToHash("class", 5);
const std::size_t Python::LexerP::importHash = cstrToHash("import", 6);
const std::size_t Python::LexerP::fromHash   = cstrToHash("from", 4);
const std::size_t Python::LexerP::andHash    = cstrToHash("and", 3);
const std::size_t Python::LexerP::asHash     = cstrToHash("as", 2);
const std::size_t Python::LexerP::inHash     = cstrToHash("in", 2);
const std::size_t Python::LexerP::isHash     = cstrToHash("is", 2);
const std::size_t Python::LexerP::notHash    = cstrToHash("not", 3);
const std::size_t Python::LexerP::orHash     = cstrToHash("or", 2);
const std::size_t Python::LexerP::yieldHash  = cstrToHash("yield", 5);
const std::size_t Python::LexerP::returnHash = cstrToHash("return", 6);
const std::size_t Python::LexerP::trueHash   = cstrToHash("True", 4);
const std::size_t Python::LexerP::falseHash  = cstrToHash("False", 5);
const std::size_t Python::LexerP::noneHash   = cstrToHash("None", 4);
const std::size_t Python::LexerP::ifHash     = cstrToHash("if", 2);
const std::size_t Python::LexerP::elifHash   = cstrToHash("elif", 4);
const std::size_t Python::LexerP::elseHash   = cstrToHash("else", 4);
const std::size_t Python::LexerP::forHash    = cstrToHash("for", 3);
const std::size_t Python::LexerP::whileHash  = cstrToHash("while", 5);
const std::size_t Python::LexerP::breakHash  = cstrToHash("break", 5);
const std::size_t Python::LexerP::continueHash = cstrToHash("continue", 8);
const std::size_t Python::LexerP::tryHash    = cstrToHash("try", 3);
const std::size_t Python::LexerP::exceptHash = cstrToHash("except", 6);
const std::size_t Python::LexerP::finallyHash = cstrToHash("finally", 7);
const std::size_t Python::LexerP::withHash   = cstrToHash("with", 4);
const std::size_t Python::LexerP::raiseHash  = cstrToHash("raise", 5);
const std::size_t Python::LexerP::globalHash = cstrToHash("global", 6);
const std::size_t Python::LexerP::lambdaHash = cstrToHash("lambda", 6);
const std::size_t Python::LexerP::passHash   = cstrToHash("pass", 4);
const std::size_t Python::LexerP::assertHash = cstrToHash("assert", 6);


// ---------------------------------------------------------------------------

/// specialcase this one to get restore from disk to work on protected members
class LexerPTokenLine : public TokenLine
{
public:
    LexerPTokenLine(const std::string &text) :
        TokenLine(nullptr, text)
    {}
    ~LexerPTokenLine() override;
    void setIsParamLine(bool isParam) { m_isParamLine = isParam; }
    void setIsContinuation(bool isCont) { m_isContinuation = isCont; }
    void setParenCnt(int16_t parenCnt) { m_parenCnt = parenCnt; }
    void setBraceCnt(int16_t braceCnt) { m_braceCnt = braceCnt; }
    void setBracketCnt(int16_t bracketCnt) { m_bracketCnt = bracketCnt; }
    void setBlockStateCnt(int16_t blockStateCnt) { m_blockStateCnt = blockStateCnt; }
    void setIndentCnt(uint16_t indentCount) { m_indentCharCount = indentCount; }
    void setUnfinishedTokenIdx(std::list<int> &idxs) {
        for(auto idx : idxs)
            m_unfinishedTokenIndexes.push_back(idx);
    }
};
LexerPTokenLine::~LexerPTokenLine()
{
}

} // namespace Python

// -------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------

Python::Lexer::Lexer() :
    d_lex(new LexerP(this))
{
}

Python::Lexer::~Lexer()
{
    delete d_lex;
}

Python::TokenList &Python::Lexer::list()
{
    return d_lex->tokenList;
}

void Python::Lexer::tokenTypeChanged(const Python::Token *tok) const
{
    (void)tok; // handle in subclass, squelsh compile warning
}

void Python::Lexer::setFilePath(const std::string &filePath)
{
    d_lex->filePath = filePath;
}

const std::string &Python::Lexer::filePath() const
{
    return d_lex->filePath;
}

// static
Python::Version Python::Lexer::version()
{
    return LexerP::version;
}

// static
void Python::Lexer::setVersion(Version::versions value)
{
    LexerP::reGenerate(value);
}

// static
Python::TokenLine *Python::Lexer::previousCodeLine(Python::TokenLine *line)
{
    // get the nearest sibling above that has code
    uint guard = TokenList::max_size();
    while (line && !line->isCodeLine() && (--guard))
        line = line->m_previousLine;
    return line;
}

uint Python::Lexer::tokenize(TokenLine *tokLine)
{
    d_lex->activeLine = tokLine;
    bool isModuleLine = false;

    d_lex->isCodeLine = false;

    if (tokLine->m_previousLine) {
        tokLine->m_isParamLine = tokLine->m_previousLine->m_isParamLine;
        tokLine->m_braceCnt = tokLine->m_previousLine->m_braceCnt;
        tokLine->m_bracketCnt = tokLine->m_previousLine->m_bracketCnt;
        tokLine->m_parenCnt = tokLine->m_previousLine->m_parenCnt;
        // determine if line is continued from prevoius line
        if (tokLine->m_previousLine->m_backTok)
            tokLine->m_isContinuation =
                    tokLine->m_previousLine->m_backTok->type() == Token::T_DelimiterBackSlash;
        if (!tokLine->m_isContinuation) {
            tokLine->m_isContinuation = tokLine->m_braceCnt> 0 ||
                                        tokLine->m_bracketCnt > 0 ||
                                        tokLine->m_parenCnt > 0;
        }
        d_lex->activeLine->m_endStateOfLastPara =
                tokLine->m_previousLine->m_endStateOfLastPara;
    }

    uint prefixLen = 0;

    const std::string text(tokLine->text());

    uint i = 0, lastI = 0;
    char ch; //, prev;

    while ( i < text.length() )
    {
        ch = text.at( i );

        switch ( d_lex->activeLine->m_endStateOfLastPara )
        {
        case Python::Token::T_Invalid:
            d_lex->activeLine->m_endStateOfLastPara = Token::T_Undetermined;
            continue;
        case Python::Token::T_Undetermined:
        {
            char thisCh = text[i],
                 nextCh = 0, thirdCh = 0;
            if (text.length() > i+1)
                nextCh = text.at(i+1);
            if (text.length() > i+2)
                thirdCh = text.at(i+2);

            switch (thisCh)
            {
            // all these chars are described at: https://docs.python.org/3/reference/lexical_analysis.html
            case '#': {
                // begin a comment
                // it goes on to the end of the row
                uint e = static_cast<uint>(text.length()) - i -1; // allow the \n line end to be handled correctly
                setToToken(i, e, Python::Token::T_Comment); // function advances i
            }   break;
            case '"': case '\'':
            {
                // Begin either string literal or block string
                Python::Token::Type tokType = Python::Token::T_Undetermined;
                uint32_t stringOptions = 0;
                i -= prefixLen;
                uint len = lastStringCh(i, text, tokType, stringOptions);
                Token *tok = setLiteral(i, len, tokType);
                prefixLen = 0;
                tok->m_customMask = stringOptions;

            } break;

            // handle indentation
            case ' ': case '\t':
            {
                if (i == 0)
                    scanIndentation(i, text);
            } break;

                // operators or delimiters
                // These are the allowed operators
                // arithmetic
                //  +       -       *       **      /       //      %      @

                // bitwise
                //  <<      >>      &       |       ^       ~

                // assign
                // =       +=      -=      *=      /=       %=
                // //=     **=     @=

                // compare
                //  <       >       <=      >=      ==      !=

                // assign bitwise
                // &=      |=      ^=      >>=     <<=      ~=

                // These are the allowed Delimiters
                // (       )       [       ]       {       }
                // ,       :       .       ;       ->     ...


            // handle all with possibly 3 chrs
            case '*':
                if (tokLine->m_isParamLine) {
                    // specialcase * as it may be a *arg or **arg
                    if (nextCh == '*') {
                        setToToken(i, 2, Python::Token::T_OperatorKeyWordParam);
                        break;
                    }
                    setToToken(i, 1, Python::Token::T_OperatorVariableParam);
                    break;
                } else if (isModuleLine) {
                    // specialcase * as it is also used as glob for modules
                    i -= 1;
                    d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_IdentifierModuleGlob;
                    break;
                } else if (nextCh == '*') {
                    if (thirdCh == '=')
                        setOperator(i, 3, Python::Token::T_OperatorExpoEqual);
                    else
                        setOperator(i, 2, Python::Token::T_OperatorExponential);
                } else if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorMulEqual);
                } else {
                    setOperator(i, 1, Python::Token::T_OperatorMul);
                }
                 break;
            case '/':
                if (nextCh == '/') {
                    if (thirdCh == '=') {
                        setOperator(i, 3, Python::Token::T_OperatorFloorDivEqual);
                    } else {
                        setOperator(i, 2, Python::Token::T_OperatorFloorDiv);
                    }
                } else if(nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorDivEqual);
                    break;
                } else {
                    setOperator(i, 1, Python::Token::T_OperatorDiv);
                }
                break;
            case '>':
            {
                if (nextCh == thisCh) {
                    if(thirdCh == '=') {
                        setOperator(i, 3, Python::Token::T_OperatorBitShiftRightEqual);
                        break;
                    }
                    setOperator(i, 2, Python::Token::T_OperatorBitShiftRight);
                    break;
                } else if(nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorMoreEqual);
                    break;
                }
                // no just single * or <
                setOperator(i, 1, Python::Token::T_OperatorMore);
            } break;
            case '<': // might be 3 chars ie <<=
            {
                if (nextCh == thisCh) {
                    if(thirdCh == '=') {
                        setOperator(i, 3, Python::Token::T_OperatorBitShiftLeftEqual);
                        break;
                    }
                    setOperator(i, 2, Python::Token::T_OperatorBitShiftLeft);
                    break;
                } else if(nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorLessEqual);
                    break;
                }
                // no just single <
                setOperator(i, 1, Python::Token::T_OperatorLess);
            } break;

            // handle all left with possibly 2 chars only
            case '-':
                if (nextCh == '>') {
                    // -> is allowed function return type documentation
                    setDelimiter(i, 2, Python::Token::T_DelimiterMetaData);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorMinus);
                break;
            case '+':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorPlusEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorPlus);
                break;
            case '%':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorModuloEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorModulo);
                break;
            case '&':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorBitAndEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorBitAnd);
                break;
            case '^':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorBitXorEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorBitXor);
                break;
            case '|':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorBitOrEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorOr);
                break;
            case '=':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorCompareEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorEqual);
                break;
            case '!':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorNotEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorNot);
                break;
            case '~':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorBitNotEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorBitNot);
                break;
            case '(':
                setDelimiter(i, 1, Python::Token::T_DelimiterOpenParen);
                ++tokLine->m_parenCnt;
                break;
            case '\r':
                // newline token should not be generated for empty and comment only lines
                if ((d_lex->activeLine->back() &&
                     d_lex->activeLine->back()->type() == Token::T_DelimiterBackSlash) ||
                    !d_lex->isCodeLine)
                    break;
                if (nextCh == '\n') {
                    setDelimiter(i, 2, Python::Token::T_DelimiterNewLine);
                    break;
                }
                setDelimiter(i, 1, Python::Token::T_DelimiterNewLine);
                checkLineEnd();
                break;
            case '\n':
                // newline token should not be generated for empty and comment only lines
                if ((d_lex->activeLine->back() &&
                     d_lex->activeLine->back()->type() == Token::T_DelimiterBackSlash) ||
                    !d_lex->isCodeLine)
                    break;
                setDelimiter(i, 1, Python::Token::T_DelimiterNewLine);
                checkLineEnd();
                break;
            case '[':
                setDelimiter(i, 1, Python::Token::T_DelimiterOpenBracket);
                ++tokLine->m_bracketCnt;
                break;
            case '{':
                setDelimiter(i, 1, Python::Token::T_DelimiterOpenBrace);
                ++tokLine->m_braceCnt;
                break;
            case '}':
                setDelimiter(i, 1, Python::Token::T_DelimiterCloseBrace);
                --tokLine->m_braceCnt;
                break;
            case ')':
                setDelimiter(i, 1, Python::Token::T_DelimiterCloseParen);
                --tokLine->m_parenCnt;
                if (tokLine->parenCnt() == 0)
                    tokLine->m_isParamLine = false;
                break;
            case ']':
                setDelimiter(i, 1, Python::Token::T_DelimiterCloseBracket);
                --tokLine->m_bracketCnt;
                break;
            case ',':
                setDelimiter(i, 1, Python::Token::T_DelimiterComma);
                break;
            case '.':
                if (isdigit(nextCh)) {
                    Token::Type tp;
                    uint len = lastNumberCh(i, text, tp);
                    if ( len > 0) {
                        setNumber(i, len, tp);
                        break;
                    }
                } else if (nextCh == '.' && thirdCh == '.') {
                    setDelimiter(i, 3, Python::Token::T_DelimiterEllipsis);
                    break;
                }
                setDelimiter(i, 1, Python::Token::T_DelimiterPeriod);
                break;
            case ';':
                isModuleLine = false;
                setDelimiter(i, 1, Python::Token::T_DelimiterSemiColon);
                break;
            case ':':
                if (nextCh == '=') {
                    if (d_lex->version.version() >= Version::v3_8)
                        setDelimiter(i, 2, Token::T_OperatorWalrus);
                    else
                        setSyntaxError(i, 2);
                } else
                    setDelimiter(i, 1, Python::Token::T_DelimiterColon);
                break;
            case '@':
            {   // decorator or matrix add
                if(nextCh &&
                   ((nextCh >= 'A' && nextCh <= 'Z') ||
                    (nextCh >= 'a' && nextCh <= 'z') ||
                    (nextCh == '_')))
                {
                    uint len = lastWordCh(i + 1, text);
                    setToToken(i, len +1, Python::Token::T_IdentifierDecorator);
                    break;
                } else if (nextCh == '=') {
                    if (d_lex->version.version() >= Version::v3_5)
                        setOperator(i, 2, Python::Token::T_OperatorMatrixMulEqual);
                    else
                        setSyntaxError(i, 2);
                    break;
                }

                 setDelimiter(i, 1, Python::Token::T_Delimiter);
            } break;
            // illegal chars
            case '$': case '?': case '`':
                setSyntaxError(i, 1);
                break;
            // text or number?
            default:
            {
                // check for string prefixes
                // this works by setting prefixlen and let ordinary string detection handle it.
                // see case '"': above
                if (nextCh == '"' || nextCh == '\'') {
                    thisCh = static_cast<char>(std::tolower(thisCh));

                    if (thisCh == 'r' || thisCh == 'b' ||
                        thisCh == 'f' || thisCh == 'u')
                    {
                        prefixLen = 1;
                        break; // let string start code handle it
                    }

                } else if (thirdCh == '"' || thirdCh == '\'') {
                    std::string twoCh = text.substr(i, 2);
                    // make lowercase
                    std::transform(twoCh.begin(), twoCh.end(), twoCh.begin(),
                                   [](unsigned char ch){ return std::tolower(ch);});

                    if (twoCh == "fr" || twoCh == "rf" ||
                        twoCh == "br" || twoCh == "rb")
                    {
                        prefixLen = 2;
                        i += 1;
                        break; // let string start code handle it
                    }
                }

                // Check for normal text
                if (isLetter(ch) || ch =='_')
                {
                    uint len = lastWordCh(i, text);
                    std::size_t hash = cstrToHash(text.substr(i, len).c_str(), len);

                    auto it = d_lex->keywords.find(hash);
                    if (it != d_lex->keywords.end()) {
                        uint startI = i;
                        switch (len) {
                        case 8:
                            if (hash == d_lex->continueHash) {
                                setToToken(i, len, Python::Token::T_KeywordContinue);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                             } break;
                        case 7:
                            if (hash == d_lex->finallyHash) {
                                setToToken(i, len, Python::Token::T_KeywordFinally);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 6:
                            if (hash == d_lex->importHash) {
                                setToToken(i, len, Python::Token::T_KeywordImport);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_IdentifierModule; // step out to handle module name
                                isModuleLine = true;
                            } else if (hash == d_lex->returnHash) {
                                setToToken(i, len, Python::Token::T_KeywordReturn);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->exceptHash) {
                                setToToken(i, len, Python::Token::T_KeywordExcept);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->globalHash) {
                                setToToken(i, len, Python::Token::T_KeywordGlobal);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->lambdaHash) {
                                setToToken(i, len, Python::Token::T_KeywordLambda);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->assertHash) {
                                setToToken(i, len, Python::Token::T_KeywordAssert);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 5:
                            if (hash == d_lex->classHash) {
                                setToToken(i, len, Python::Token::T_KeywordClass);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_IdentifierClass; // step out to handle class name
                            } else if (hash == d_lex->yieldHash) {
                                setToToken(i, len, Python::Token::T_KeywordYield);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->falseHash) {
                                setIdentifier(i, len, Python::Token::T_IdentifierFalse);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->whileHash) {
                                setToToken(i, len, Python::Token::T_KeywordWhile);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->breakHash) {
                                setToToken(i, len, Python::Token::T_KeywordBreak);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->raiseHash) {
                                setToToken(i, len, Python::Token::T_KeywordRaise);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 4:
                            if (hash == d_lex->fromHash) {
                                setToToken(i, len, Python::Token::T_KeywordFrom);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_IdentifierModulePackage; // step out handle module name
                                isModuleLine = true;
                             } else if (hash == d_lex->trueHash) {
                                setIdentifier(i, len, Python::Token::T_IdentifierTrue);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->noneHash) {
                                setIdentifier(i, len, Python::Token::T_IdentifierNone);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->elifHash) {
                                setToToken(i, len, Python::Token::T_KeywordElIf);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->elseHash) {
                                setToToken(i, len, Python::Token::T_KeywordElse);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->withHash) {
                                setToToken(i, len, Python::Token::T_KeywordWith);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->passHash) {
                                setToToken(i, len, Python::Token::T_KeywordPass);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 3:
                            if (hash == d_lex->defHash) {
                                tokLine->m_isParamLine = true;
                                setToToken(i, len, Python::Token::T_KeywordDef);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_IdentifierDefUnknown; // step out to handle def name

                            } else if (hash == d_lex->notHash) {
                                setToToken(i, len, Python::Token::T_OperatorNot);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->forHash) {
                                setToToken(i, len, Python::Token::T_KeywordFor);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->tryHash) {
                                setToToken(i, len, Python::Token::T_KeywordTry);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 2:
                            if (hash == d_lex->andHash) {
                                setToToken(i, len, Python::Token::T_OperatorAnd);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->asHash) {
                                setToToken(i, len, Python::Token::T_KeywordAs);
                                if (isModuleLine)
                                    d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_IdentifierModuleAlias;
                                else
                                    d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->inHash) {
                                setToToken(i, len, Python::Token::T_OperatorIn);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->isHash) {
                                setToToken(i, len, Python::Token::T_OperatorIs);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->orHash) {
                                setToToken(i, len, Python::Token::T_OperatorOr);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->ifHash) {
                                setToToken(i, len, Python::Token::T_KeywordIf);
                                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        default:; // nothing
                        }

                        if (startI == i) {
                            setToToken(i, len, Python::Token::T_Keyword);
                        }

                    } else {
                        auto it = d_lex->builtins.find(hash);
                        if (it != d_lex->builtins.end()) {
                            if (d_lex->activeLine->back() &&
                                d_lex->activeLine->back()->type() == Python::Token::T_DelimiterPeriod)
                            {
                                // avoid match against someObj.print
                                setToToken(i, len, Python::Token::T_IdentifierUnknown);
                            } else
                                setToToken(i, len, Python::Token::T_IdentifierBuiltin);
                        } else if (isModuleLine) {
                            i -= 1; // jump to module block
                            d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_IdentifierModule;
                        } else {
                            setUndeterminedIdentifier(i, len, Python::Token::T_IdentifierUnknown);
                        }
                    }

                    break;
                    //i += word.length(); setWord increments
                }

                // is it the beginning of a number?
                {
                    Token::Type tp;
                    uint len = lastNumberCh(i, text, tp);
                    if ( len > 0) {
                        setNumber(i, len, tp);
                        break;
                    } else if (thisCh == '\\') {
                        setDelimiter(i, 1, Python::Token::T_DelimiterBackSlash);
                        break;
                    }
                }
                // its a error if we ever get here
                setSyntaxError(i, 1);
                d_lex->activeLine->setSyntaxErrorMsg(d_lex->activeLine->back(),
                                                     "Illegal char at col:" + std::to_string(i) +
                                                     " row: "+ std::to_string(d_lex->activeLine->lineNr()));

                std::clog << "Lexer error on char: " << ch <<
                          " row:"<< std::to_string(d_lex->activeLine->lineNr() + 1) <<
                          " col:" << std::to_string(i) << std::endl;

            } // end default:
            } // end switch

        } break;// end T_Undetermined

        // we should only arrive here when we start on a new line
        case Python::Token::T_LiteralBlockDblQuote: FALLTHROUGH
        case Python::Token::T_LiteralBlockSglQuote:
        {
            // multiline string continued from previous line
            scanIndentation(i, text);
            d_lex->isCodeLine = true;

            const char *endMarker;
            if (d_lex->activeLine->m_endStateOfLastPara == Token::T_LiteralBlockDblQuote)
                endMarker = "\"\"\"";
            else
                endMarker = "'''";
            auto endPos = text.find(endMarker, i);
            if (endPos != text.npos) {
                // we dont store a token on this line as it was set on previous line
                // should only have one token for each string regardless of number of lines
                // but we do store endpos on this line in the token that started us
                uint guard = d_lex->tokenList.max_size();
                TokenLine *line = d_lex->activeLine;
                while(line && line->m_previousLine && (--guard)) {
                    if (line->m_previousLine->endState() != d_lex->activeLine->m_endStateOfLastPara) {
                        if (line->back())
                            line->back()->m_endPos = static_cast<uint16_t>(endPos + 3);
                        break;
                    }
                    line = line->m_previousLine;
                }

                i = static_cast<uint>(endPos + 2);
                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;

            } else {
                i = static_cast<uint>(text.length());
            }

        } break;
        case Python::Token::T_IdentifierDefUnknown:
        {   // before we now if its a class member or a function
            for (; i < text.length(); ++i) {
                if (!isSpace(text.at(i)))
                    break;
            }
            if (i == text.length()) {
                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                break; // not yet finished this line?
            }

            uint len = lastWordCh(i, text);
            if (d_lex->activeLine->indent() == 0)
                // no indent, it can't be a method
                setIdentifier(i, len, Python::Token::T_IdentifierFunction);
            else
                setUndeterminedIdentifier(i, len, Python::Token::T_IdentifierDefUnknown);
            d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;

        } break;
        case Python::Token::T_IdentifierClass:
        {
            for (; i < text.length(); ++i) {
                if (!isSpace(text.at(i)))
                    break;
            }
            if (i == text.length()) {
                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                break; // not yet finished this line?
            }

            uint len = lastWordCh(i, text);
            setIdentifier(i, len, Python::Token::T_IdentifierClass);
            d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;

        } break;
        case Python::Token::T_IdentifierModuleAlias:
        case Python::Token::T_IdentifierModuleGlob:
        case Python::Token::T_IdentifierModule:  // fallthrough
        case Python::Token::T_IdentifierModulePackage:
        {
            // can import multiple modules separated  with ',' so we jump here between here and T_Undertermined
            for (; i < text.length(); ++i) {
                if (!isSpace(text.at(i)))
                    break;
            }
            if (i == text.length()) {
                d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;
                break; // not yet finished this line?
            }

            uint len = lastWordCh(i, text);
            if (len < 1) {
                if (ch == '*') // globs aren't a word char so they don't get caught
                    len += 1;
                else
                    break;
            }

            setIdentifier(i, len, d_lex->activeLine->m_endStateOfLastPara);
            d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined;

        } break;
        default:
        {
            // let subclass sort it out
            Python::Token::Type nextState = unhandledState(i, d_lex->activeLine->m_endStateOfLastPara, text);
            if (nextState != Python::Token::T_Invalid) {
                d_lex->activeLine->m_endStateOfLastPara = nextState;
                break; // break switch
            }

            setSyntaxError(i, lastWordCh(i, text));
            std::clog << "Python::Lexer unknown state for: " << ch <<
                         " row:"<< std::to_string(tokLine->lineNr() + 1) <<
                         " col:" << std::to_string(i) << std::endl;
        }
        } // end switch

        //prev = ch;
        assert(i >= lastI); // else infinite loop
        lastI = i++;

    } // end loop


    // only block comments can have several lines
    if ( d_lex->activeLine->m_endStateOfLastPara != Python::Token::T_LiteralBlockDblQuote &&
         d_lex->activeLine->m_endStateOfLastPara != Python::Token::T_LiteralBlockSglQuote )
    {
        d_lex->activeLine->m_endStateOfLastPara = Python::Token::T_Undetermined ;
    }

    return i;
}

Python::Token::Type Python::Lexer::unhandledState(uint &pos, int state,
                                                      const std::string &text)
{
    (void)pos;
    (void)state;
    (void)text;
    return Python::Token::T_Invalid;
}

void Python::Lexer::tokenUpdated(const Python::Token *tok)
{
    (void)tok; // handle in subclass, squelsh compile warning
}

Python::Token::Type &Python::Lexer::endStateOfLastParaRef() const
{
    return d_lex->activeLine->m_endStateOfLastPara;
}

Python::TokenLine *Python::Lexer::activeLine() const
{
    return d_lex->activeLine;
}

void Python::Lexer::setActiveLine(Python::TokenLine *line)
{
    d_lex->activeLine = line;
}

uint Python::Lexer::lastWordCh(uint startPos, const std::string &text) const
{
    uint len = 0;
    for (auto pos = text.begin() + startPos;
         pos != text.end(); ++pos, ++len)
    {
        // is letter or number or _
        if (!isNumber(*pos) && !isLetter(*pos) && *pos != '_')
            break;
    }

    return len;
}

uint Python::Lexer::lastNumberCh(uint startPos, const std::string &text, Token::Type &type) const
{
    auto pos = text.begin() + startPos;
    int first = *pos, prevCh = 0, ch = 0;
    uint len = 0;
    type = Token::T_NumberDecimal;
    for (; pos != text.end(); ++pos, ++len) {
        ch = std::tolower(*pos);
        switch (ch) {
        case '0': case '1': case '_':
            break;
        case '2': case '3': case '4': case '5':
        case '6': case '7':
            if (type == Token::T_NumberBinary)
                type = Token::T_SyntaxError;
            break;
        case '8': case '9':
            if (type == Token::T_NumberBinary || type == Token::T_NumberOctal)
                type = Token::T_SyntaxError;
            break;
        case 'e':
            if (type == Token::T_NumberHex)
                break;
            type = (type == Token::T_NumberDecimal || type == Token::T_NumberFloat) ?
                        Token::T_NumberFloat : Token::T_SyntaxError;
            break;
        case 'a': case 'c': case 'd': case 'f':
            type = (type != Token::T_NumberHex) ? Token::T_NumberHex : Token::T_SyntaxError;
            break;
        case '.':
            type = (type == Token::T_NumberDecimal) ? Token::T_NumberFloat : Token::T_SyntaxError;
            break;
        case '-': case '+':
            type = (len > 2 && prevCh == 'e' && type == Token::T_NumberFloat) ?
                        Token::T_NumberFloat : Token::T_SyntaxError;
            break;
        case 'x':
            type = (len == 1 && first == '0' && type != Token::T_SyntaxError) ?
                        Token::T_NumberHex : Token::T_SyntaxError;
            break;
        case 'o':
            type = (len == 1 && first == '0' && type != Token::T_SyntaxError) ?
                        Token::T_NumberOctal : Token::T_SyntaxError;
            break;
        case 'b':
            type = (len == 1 && first == '0' && type != Token::T_SyntaxError) ?
                        Token::T_NumberBinary : Token::T_SyntaxError;
            break;
        default:
            goto out;
        }

        prevCh = ch;
    }

out:
    // python 2 octal ie. 01234 != 1234
    if (first == '0' &&  type == Token::T_NumberDecimal && len > 1) {
        if (d_lex->version.version() < Version::v3_0)
            type = Token::T_NumberOctal;
        else
            type = Token::T_SyntaxError; // invalid in py3
    }

    // long integer or imaginary?
    if (prevCh != 0 && (ch == 'l' || ch == 'j')) {
        len += 1;
        if (d_lex->version.version() >= Version::v3_0 && ch == 'l')
            type = Token::T_SyntaxError;
    }

    return len;
}

uint Python::Lexer::lastStringCh(uint startAt, const std::string &text,
                                 Token::Type &type, uint32_t &stringOptions) const
{
    uint len = static_cast<uint>(text.length());
    if (len <= startAt)
        return 0;

    char prevCh = 0, ch = 0, prevPrefix = 0,closingCh = 0, closingChCnt = 0;
    bool isBlockString = false;

    len = 0;
    for (auto pos = text.begin() + startAt; pos != text.end(); ++pos, ++len) {
        ch = static_cast<char>(tolower(*pos));
        if (ch == '\\') {
            // escape a string char
            if (stringOptions & STRING_IS_FORMAT_TYPE)
                type = Token::T_SyntaxError; // we can't escape format strings
            ++len; ++pos;
        } else if (ch == '"' || ch == '\'') {
            if (closingCh == 0)
                closingCh = ch;
            if (closingCh == ch) {
                closingChCnt = ((prevCh == ch || prevCh == 0) && !isBlockString) ?
                                    closingChCnt +1 : closingChCnt -1;
                if (closingChCnt == 0) {
                    ++len;
                    break; // finished
                }
            }
        } else if (!closingCh) {
            // string not yet started, its a string prefix?
            if (ch == 'r' && (prevPrefix == 'f' || prevPrefix == 0) && len < 2) {
                stringOptions |= STRING_IS_RAW_TYPE;
            } else if (ch == 'f' && (prevPrefix == 'r' || prevPrefix == 0) && len < 2) {
                stringOptions |= STRING_IS_FORMAT_TYPE;
            } else if (ch == 'b' && prevPrefix == 'r' && len < 2) {
                if (d_lex->version.version() < Version::v3_3)
                    type = Token::T_SyntaxError;
                stringOptions |= STRING_IS_BYTES_TYPE;
            } else if (ch == 'b' && prevPrefix == 0 && len < 1) {
                stringOptions |= STRING_IS_BYTES_TYPE;
            } else if (ch == 'r' && prevPrefix == 'b' && len < 2) {
                stringOptions |= STRING_IS_RAW_TYPE;
            } else if (ch == 'r' && prevPrefix == 0 && len < 1) {
                stringOptions |= STRING_IS_RAW_TYPE;
            }  else if (ch == 'u' && len == 0) {
                if (d_lex->version.version() < Version::v3_3 &&
                    d_lex->version.version() >= Version::v3_0)
                {
                    type = Token::T_SyntaxError;
                }
                stringOptions |= STRING_IS_UNICODE_TYPE;
            }  else {
                // more than 2 format chars or invalid format
                type = Token::T_SyntaxError;
                return len;
            }
            prevPrefix = ch;
            continue; // don't set prevCh
        } else {
            // ordinary string chars
            isBlockString = closingChCnt == 3;
        }
        prevCh = ch;
    }

    if (type != Token::T_SyntaxError) {
        if (closingCh == '"')
            type = isBlockString ? Token::T_LiteralBlockDblQuote : Token::T_LiteralDblQuote;
        else
            type = isBlockString ? Token::T_LiteralBlockSglQuote : Token::T_LiteralSglQuote;

        // is this block continued on next line?
        if (closingChCnt != 0 && isBlockString) {
            d_lex->activeLine->m_endStateOfLastPara = type;
            stringOptions |= STRING_IS_MULTILINE_TYPE;
        }
    }

    return len;
}

Python::Token*
Python::Lexer::setRestOfLine(uint &pos, const std::string &text, Python::Token::Type tokType)
{
    uint len = static_cast<uint>(text.size()) - pos;
    return setToToken(pos, len, tokType);
}

void Python::Lexer::scanIndentation(uint &pos, const std::string &text)
{
    if (d_lex->activeLine->empty()) {

        uint count = 0, j = pos;

        for (auto it = text.begin() + pos; it != text.end(); ++it, ++j) {
            if (*it == ' ') {
                ++count;
            } else if (*it == '\t') {
                count += 8; // according to python lexical documentaion tab is eight spaces
            } else {
                break;
            }
        }
        if (count > 0) {
            //setIndentation(pos, j, count);
            pos += j - 1;
            d_lex->activeLine->m_indentCharCount = static_cast<uint16_t>(count);
        }
    }
}


Python::Token*
Python::Lexer::setToToken(uint &pos, uint len, Python::Token::Type tokType)
{
    Python::Token *tok = d_lex->activeLine->newDeterminedToken(tokType, pos, len);
    tokenUpdated(tok);
    if (!d_lex->isCodeLine)
        d_lex->isCodeLine = tok->isCode();
    if (len > 0)
        pos += len -1;
    return tok;
}


Python::Token*
Python::Lexer::setIdentifier(uint &pos, uint len, Python::Token::Type tokType)
{
    return setToToken(pos, len, tokType);
}


Python::Token*
Python::Lexer::setUndeterminedIdentifier(uint &pos, uint len, Python::Token::Type tokType)
{
    // let parse tree figure out and color at a later stage
    Python::Token *tok = d_lex->activeLine->newUndeterminedToken(tokType, pos, len);
    tokenUpdated(tok);
    pos += len -1;
    return tok;
}


Python::Token*
Python::Lexer::setNumber(uint &pos, uint len, Python::Token::Type tokType)
{
    return setToToken(pos, len, tokType);
}


Python::Token*
Python::Lexer::setOperator(uint &pos, uint len, Python::Token::Type tokType)
{
    return setToToken(pos, len, tokType);
}


Python::Token*
Python::Lexer::setDelimiter(uint &pos, uint len, Python::Token::Type tokType)
{
    return setToToken(pos, len, tokType);
}


Python::Token*
Python::Lexer::setSyntaxError(uint &pos, uint len)
{
    return setToToken(pos, len, Python::Token::T_SyntaxError);
}


Python::Token*
Python::Lexer::setLiteral(uint &pos, uint len, Python::Token::Type tokType)
{
    return setToToken(pos, len, tokType);
}

void Python::Lexer::setIndentation()
{
    // get the nearest sibling above that has code
    Python::TokenLine *prevLine = previousCodeLine(d_lex->activeLine->m_previousLine);
    while (prevLine && prevLine->m_isContinuation)
        prevLine = previousCodeLine(prevLine->m_previousLine);

    uint guard = TokenList::max_size();
    Python::Token *tok = nullptr;

    do { // break out block
        if (!prevLine) {
            if (d_lex->activeLine->indent() > 0)
                tok = createIndentError("Unexpected Indent at begining of file");
            break;
        }
        if (d_lex->activeLine->indent() == prevLine->m_indentCharCount) {
            if (prevLine->back() && prevLine->back()->type() == Token::T_Dedent) {
                // remove a previous dedent token at the same level
                prevLine->remove(prevLine->back(), true);
                prevLine->incBlockState();
                d_lex->insertDedent = true; // defer insert to last token scanned
            }
            break;
        }

        // its not a blockstart/blockend
        if (d_lex->activeLine->m_isContinuation)
            break;

        // when we get here we have a clean
        // when we get here we have a change in indentation to previous line
        if (prevLine->m_indentCharCount < d_lex->activeLine->indent()) {
            // increase indent by one block

            // check so we have a valid ':'
            Python::TokenLine *line = previousCodeLine(d_lex->activeLine->m_previousLine);
            if (!line) break;
            Python::Token *lookupTok = line->m_backTok;
            while(lookupTok && lookupTok->ownerLine() == line &&
                  lookupTok->type() != Python::Token::T_DelimiterColon && (--guard))
            {
                lookupTok = lookupTok->previous();
            }

            if (!lookupTok || lookupTok->ownerLine() != line) {
                tok = createIndentError("Blockstart without ':'");
            } else {
                prevLine->incBlockState();
                tok = new Python::Token(Python::Token::T_Indent, 0,
                                        d_lex->activeLine->front()->startPos(),
                                        d_lex->activeLine);
                d_lex->activeLine->insert(d_lex->activeLine->front()->previous(),
                                          tok);
            }
        }
    } while(false); // breakout block

    if (tok)
        tokenUpdated(tok);
}

Python::Token *Python::Lexer::createIndentError(const std::string &msg)
{
    Python::Token *tok = d_lex->activeLine->m_frontTok;
    if (!tok || tok->type() != Python::Token::T_IndentError) {
        tok = new Python::Token(Python::Token::T_IndentError, 0, d_lex->activeLine->indent(), d_lex->activeLine);
        d_lex->activeLine->push_front(tok);
    }
    d_lex->activeLine->setIndentErrorMsg(tok, msg);
    return tok;
}

Python::Token *Python::Lexer::insertDedent()
{
    d_lex->insertDedent = false;
    d_lex->activeLine->decBlockState();
    return d_lex->activeLine->newDeterminedToken(Python::Token::T_Dedent,
                                                 d_lex->activeLine->back()->endPos(), 0);
}

void Python::Lexer::checkForDedent()
{
    Python::TokenLine *prevLine = previousCodeLine(d_lex->activeLine->m_previousLine);
    uint guard = d_lex->tokenList.max_size();

    if (prevLine && prevLine->back() &&
        prevLine->m_indentCharCount > d_lex->activeLine->m_indentCharCount &&
        prevLine->back()->type() != Token::T_Dedent)
    {
        // decrease indent
        // first find out the indent count that triggered the indent
        Python::TokenLine *startLine = prevLine;
        int dedentCnt = 0, indentCnt = 0;
        uint lastIndent = prevLine->m_indentCharCount;
        while (startLine && (--guard)) {
            if (startLine->m_indentCharCount <= lastIndent &&
                startLine->m_indentCharCount > d_lex->activeLine->m_indentCharCount)
            {
                if (startLine->back() && startLine->back()->type() == Python::Token::T_Dedent) {
                    --dedentCnt;
                    lastIndent = previousCodeLine(startLine->m_previousLine)->m_indentCharCount;
                }
                if (startLine->front() && startLine->front()->type() == Python::Token::T_Indent) {
                    ++indentCnt;
                    lastIndent = previousCodeLine(startLine->m_previousLine)->m_indentCharCount;
                }
            }

            // we break here if we have found a indent on current level or unconditionally if we are at a lower level
            if (startLine->isCodeLine() &&
                (startLine->m_indentCharCount <= d_lex->activeLine->m_indentCharCount))
            {
                break; // we are finished
            }

            startLine = startLine->m_previousLine;
        }

        // insert dedents
        while ((indentCnt + dedentCnt) > 0) {
            --dedentCnt;
            prevLine->decBlockState();
            prevLine->newDeterminedToken(Python::Token::T_Dedent, prevLine->back()->endPos(), 0);
        }

    }
}

void Python::Lexer::checkLineEnd()
{
    if (d_lex->insertDedent)
        insertDedent();
    if (d_lex->activeLine->indent() > 0 && d_lex->isCodeLine)
        setIndentation();
    checkForDedent(); // this line might dedent a previous line
}

// -------------------------------------------------------------------------------------

Python::LexerReader::LexerReader(const std::string &pyPath) :
    Lexer()
{
    // split by ':'
    std::size_t left = 0, right = 0;
    while(left != std::string::npos) {
        right = pyPath.find(":", left);
        if (right != std::string::npos) {
            m_pyPaths.push_back(pyPath.substr(left, right - left));
            left = right +2;
        } else {
            m_pyPaths.push_back(pyPath.substr(left));
            break;
        }
    }
}

Python::LexerReader::~LexerReader()
{
}

bool Python::LexerReader::readFile(const std::string &file)
{
    if (!file.empty())
        d_lex->filePath = file;
    else if (d_lex->filePath.empty())
        return false;

    FileInfo fi(d_lex->filePath);
    if (!FileInfo::fileExists(fi.absolutePath()))
        return false;


    std::ifstream pyFile(fi.absolutePath(), std::ios::in);
    if (pyFile.is_open()) {
        if (!d_lex->tokenList.empty())
            d_lex->tokenList.clear();
        std::string line;

        std::chrono::duration<double> elapsed_seconds;

        while (std::getline(pyFile, line)) {
            d_lex->tokenList.appendLine(new TokenLine(nullptr, line));

            auto start = std::chrono::system_clock::now();

            tokenize(d_lex->tokenList.lastLine());

            auto end = std::chrono::system_clock::now();
            elapsed_seconds += end - start;
        }

        std::cout << "time to tokenize " << d_lex->filePath << ": " << elapsed_seconds.count() << "s\n";

        pyFile.close();

        return true;
    }

    return false;
}

std::string Python::LexerReader::pythonPath() const
{
    // join with ':'
    auto it = m_pyPaths.begin();
    std::string paths = m_pyPaths.empty() ? "" : *it;
    while (++it != m_pyPaths.end())
        paths += *it;
    return paths;
}

const std::list<std::string> &Python::LexerReader::paths() const
{
    return m_pyPaths;
}

// -------------------------------------------------------------------------------------


// fileformat for dumpstring:
// each line has one thing in it, linetext, one token etc
// firstline is filename and python version separated by ';'
// then comes lineNr;#firstline in code
// then comes all properties of this line, one property per line
//  propertylines starts with space ' '
// then comes all tokens in this line, one token per line
//  tokenlines starts with TAB '\t'
//  then: starPos;endPos;TokenName
// ex:
// def firstLine():
//     return "end of example"
//
// becomes:
// /home/me/file1.py\n
// def firstLine():\n
//  indent=0\n
//  paren=0\n
//          0;3;T_KeywordDef\n <- TAB or 8 spaces indent
//          4;13;T_IdentifierFunction\n
//          13;14;T_DelimiterOpenParen\n
//          14;15;T_DelimiterCloseParen\n
//          15;16;T_DelimiterColon\n
//          16;16;T_DelimiterNewLine\n
//     return "end of example"\n
//  indent=4\n
//  paren=0\n
//      4;10;T_KewordReturn\n
//      11;27;T_LiteralDblQuote\n
Python::LexerPersistent::LexerPersistent(Lexer *lexer) :
    m_lexer(lexer)
{
}

Python::LexerPersistent::~LexerPersistent()
{
}

const std::string Python::LexerPersistent::dumpToString() const
{
    assert(m_lexer && "Must have a vaid lexer");
    std::string dmp = m_lexer->filePath() + ";" +
                      m_lexer->version().versionAsString() + "\n";

    auto line = m_lexer->list().firstLine();
    uint guard = m_lexer->list().max_size();
    while (line && (--guard)) {
        dmp += "\n---------------------------------------------------------------------\n"
                + std::to_string(line->lineNr()) + ";" + line->text(); // newline from line->text()
        // space marks this line is a line property line
        if (line->indent())
            dmp += " indent=" + std::to_string(line->indent()) + "\n";
        if (line->bracketCnt())
            dmp += " bracket=" + std::to_string(line->bracketCnt()) + "\n";
        if (line->braceCnt())
            dmp += " brace=" + std::to_string(line->braceCnt()) + "\n";
        if (line->parenCnt())
            dmp += " paren=" + std::to_string(line->parenCnt()) + "\n";
        if (line->isContinuation())
            dmp += " continue=" + std::to_string(line->isContinuation()) + "\n";
        if (line->isParamLine())
            dmp += " param=" + std::to_string(line->isParamLine()) + "\n";
        if (line->blockState())
            dmp += " blockstate=" + std::to_string(line->blockState()) + "\n";
        if (!line->unfinishedTokens().empty()) {
            std::string str;
            for(auto idx : line->unfinishedTokens())
                str += std::to_string(idx) + ";";
            dmp += " unfinished=" + str.substr(0, str.length()-1) + "\n"; // trim the last ';'
        }
        for (auto &tok : *line) {
            dmp +=  "        " +
                    std::to_string(tok.startPos()) + ";" + std::to_string(tok.endPos()) +
                    ";" + std::string(Token::tokenToCStr(tok.type())) + "\n";
        }
        // this one must be inserted after tokens have been dumped so we can recreate in this order
        if (line->tokenScanInfo() && !line->tokenScanInfo()->allMessages().empty()) {
            for(auto msg : line->tokenScanInfo()->allMessages()) {
                auto msgList = split(msg->message(), "\n");
                dmp += " scaninfo=" + msg->msgTypeAsString() + ";" +
                        std::to_string(line->tokenPos(msg->token())) + ";" +
                        join(msgList, "\033") + "\n";
            }

        }
        line = line->nextLine();
    }
    // prevent the last '\n'
    if (dmp.back() == '\n')
        return dmp.substr(0, dmp.length() - 1);
    return dmp;
}

bool Python::LexerPersistent::dumpToFile(const std::string &file) const
{
    FileInfo fi(file);
    if (!FileInfo::dirExists(fi.absolutePath()))
        return false;

    std::ofstream dmpFile(fi.absolutePath(), std::ios::out | std::ios::trunc);
    if (dmpFile.is_open()){
        dmpFile << dumpToString();
        return true;
    }

    return false;
}

int Python::LexerPersistent::reconstructFromString(const std::string &dmpStr) const
{
    int readLines = 1;
    assert(m_lexer && "Must have a valid lexer");
    m_lexer->list().clear();

    auto strList = split(dmpStr, "\n");
    auto lineIt = strList.begin();

    if (lineIt == strList.end())
        return -readLines;
    LexerPTokenLine *activeLine = nullptr;
    // first line should always be filepath and version
    auto firstLineParts = split(*lineIt, ";");
    if (firstLineParts.size() < 1)
        return -readLines;
    m_lexer->setFilePath(firstLineParts.front());
    if (firstLineParts.size() == 2)
        m_lexer->setVersion(Version::strToVersion(firstLineParts.back()));
    ++lineIt; ++readLines;
    try {
        uint guard = m_lexer->list().max_size();
        // iterate the rows, one
        while (lineIt != strList.end() && (--guard)) {
            if ((lineIt->front() == '\t') ||
                (lineIt->length() > 8 && lineIt->substr(0, 8) == "        "))
            {
                // its a token line
                if (!activeLine)
                    return -readLines;
                auto lineStr = lineIt->substr(1);
                auto itmList = split(lineStr, ";");
                auto itmIt = itmList.begin();
                if (itmList.size() != 3)
                    return -readLines;
                uint16_t startPos = static_cast<uint16_t>(std::stoul(*itmIt)),
                        endPos   = static_cast<uint16_t>(std::stoul(*(++itmIt)));
                auto tok = new Token(Token::strToToken(*(++itmIt)), startPos, endPos, nullptr);
                activeLine->push_back(tok);
            } else if(lineIt->front() == ' ') {
                // its a line parameter line
                // space marks this line is a line property line
                auto list = split(lineIt->substr(1), "=");
                if (list.size() != 2 || !activeLine)
                    return -readLines;

                if (list.front() == "indent")
                    activeLine->setIndentCnt(static_cast<uint16_t>(std::stoi(list.back())));
                else if (list.front() == "bracket")
                    activeLine->setBracketCnt(static_cast<int16_t>(std::stoi(list.back())));
                else if (list.front() == "brace")
                    activeLine->setBraceCnt(static_cast<int16_t>(std::stoi(list.back())));
                else if (list.front() == "paren")
                    activeLine->setParenCnt(static_cast<int16_t>(std::stoi(list.back())));
                else if (list.front() == "continue")
                    activeLine->setIsContinuation(static_cast<bool>(std::stoi(list.back())));
                else if (list.front() == "param")
                    activeLine->setIsParamLine(static_cast<bool>(std::stoi(list.back())));
                else if (list.front() == "blockstate")
                    activeLine->setBlockStateCnt(static_cast<int16_t>(std::stoi(list.back())));
                else if (list.front() == "unfinished") {
                    std::list<int> idxs;
                    for(auto &strIdx : split(list.back(), ";"))
                        idxs.push_back(std::stoi(strIdx));
                    activeLine->setUnfinishedTokenIdx(idxs);
                } else if (list.front() == "scaninfo") {
                    auto scanParts = split(list.back(), ";");
                    if (scanParts.size() != 3u)
                        return -readLines;
                    auto scanPartIt = scanParts.begin();
                    auto msgType = TokenScanInfo::ParseMsg::strToMsgType(*(scanPartIt++));
                    Token *tok = (*activeLine)[std::stoi(*(scanPartIt++))];
                    if (!tok || msgType == TokenScanInfo::Invalid)
                        return -readLines;
                    auto msgParts = split(*scanPartIt, "\033");
                    std::string msg = join(msgParts, "\n");
                    activeLine->tokenScanInfo(true)->setParseMessage(tok, msg, msgType);
                }
            } else if (!lineIt->empty()) {
                // ignore comments
                if ((*lineIt)[0] != '#' && (*lineIt)[0] != '-') {
                    // its a row line, code might contain ';' so we join all parts
                    auto list = split(*lineIt, ";");
                    if (list.size() < 1)
                        return -readLines;
                    ulong lineNr = std::stoul(list.front());
                    list.pop_front(); // pop linenr
                    activeLine = new LexerPTokenLine(join(list, ";"));
                    m_lexer->list().appendLine(activeLine);
                    if (!lineIt->empty() && activeLine->lineNr() != lineNr)
                        return -readLines;
                }
            }
            ++lineIt;
            ++readLines;
        }
    } catch(...) {
        return -readLines;
    }
    return readLines;
}

int Python::LexerPersistent::reconstructFromDmpFile(const std::string &file) const
{
    FileInfo fi(file);
    if (!FileInfo::fileExists(fi.absolutePath()))
        return false;


    std::ifstream dmpFile(fi.absolutePath(), std::ios::in);
    if (dmpFile.is_open()) {
        if (!m_lexer->list().empty())
            m_lexer->list().clear();

        std::string dmpStr((std::istreambuf_iterator<char>(dmpFile)),
                           (std::istreambuf_iterator<char>()));

        dmpFile.close();

        return reconstructFromString(dmpStr);
    }

    return false;
}


