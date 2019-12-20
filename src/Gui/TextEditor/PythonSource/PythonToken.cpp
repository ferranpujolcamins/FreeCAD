
#include "PreCompiled.h"
#include "PythonToken.h"
#include <TextEditor/PythonSource/PythonSourceListBase.h>
#include "PythonSyntaxHighlighter.h"

#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <string>


#ifdef BUILD_PYTHON_DEBUGTOOLS
# include <TextEditor/PythonCodeDebugTools.h>
//# define DEBUG_DELETES
#endif

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
        endStateOfLastPara(Python::Token::T_Invalid),
        activeLine(nullptr), insertDedent(false),
        isCodeLine(false)
    {
        reGenerate();
    }
    ~LexerP()
    {
    }

    static void reGenerate()
    {
        if (version.version() == activeVersion)
            return;

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
    Python::Token::Type endStateOfLastPara;
    std::string filePath;
    Python::TokenLine *activeLine;
    bool insertDedent, isCodeLine;

    // these hashes are constant and wont change, use for quicker lookup
    static const std::size_t defHash, classHash, importHash, fromHash, andHash,
                             asHash, inHash, isHash, notHash, orHash,yieldHash,
                             returnHash, trueHash, falseHash, noneHash, ifHash,
                             elifHash, elseHash, forHash, whileHash, breakHash,
                             continueHash, tryHash, exceptHash, finallyHash;
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
const std::size_t Python::LexerP::tryHash = cstrToHash("try", 3);
const std::size_t Python::LexerP::exceptHash = cstrToHash("except", 6);
const std::size_t Python::LexerP::finallyHash = cstrToHash("finally", 7);

} // namespace Python

// -------------------------------------------------------------------------------------------

Python::Version::Version(uint8_t major, uint8_t minor)
{
    if (major == 3) {
        switch (minor) {
        case 9: m_version = v3_9; break;
        case 8: m_version = v3_8; break;
        case 7: m_version = v3_7; break;
        case 6: m_version = v3_6; break;
        case 5: m_version = v3_5; break;
        case 4: m_version = v3_4; break;
        case 3: m_version = v3_3; break;
        case 2: m_version = v3_2; break;
        case 1: m_version = v3_1; break;
        case 0: m_version = v3_0; break;
        default:
            assert(minor > 0 && "unknown python minor version");
        }
    } else if (major == 2) {
        switch (minor) {
        case 7: m_version = v2_7; break;
        case 6: m_version = v2_6; break;
        default:
            assert(minor > 0 && "unknown python minor version");
        }
    } else
        assert(minor > 0 && "unknown python major version");
}

Python::Version::Version(Python::Version::versions version) :
    m_version(version)
{
    assert(version <= Latest);
    assert(version >= v2_6);
}

Python::Version::Version(const Python::Version &other) :
    m_version(other.m_version)
{
}

Python::Version::~Version()
{
}

void Python::Version::setVersion(versions version)
{
    assert(version <= Latest);
    assert(version >= v2_6);
    m_version = version;
}

uint8_t Python::Version::majorVersion() const
{
    if (m_version == Latest)
        return 3;
    if (m_version >= v3_0 && m_version <= v3_9)
        return 3;
    else if (m_version >= v2_6 && m_version <= v2_7)
        return 2;
    return 0;
}

uint8_t Python::Version::minorVersion() const
{
    switch (m_version) {
    case Latest: // FALLTHROUGH
    case v3_9: return 9;
    case v3_8: return 8;
    case v3_7: return 7;
    case v3_6: return 6;
    case v3_5: return 5;
    case v3_4: return 4;
    case v3_3: return 3;
    case v3_2: return 2;
    case v3_1: return 1;
    case v3_0: return 0;
    case EOL: // fallthrough
    case v2_7: return 7;
    case v2_6: return 6;
    case Invalid: return 0xFF;
    }
    return 0xFF;
}

Python::Version::versions Python::Version::version() const
{
    return m_version;
}

std::string Python::Version::versionAsString() const
{
    return versionAsString(m_version);
}

// static
std::string Python::Version::versionAsString(Python::Version::versions version)
{
    switch (version) {
    case Invalid: return "invalid";
    case v2_6: return "2.6";
    case EOL: // Fallthrough
    case v2_7: return "2.7";
    case v3_0: return "3.0";
    case v3_1: return "3.1";
    case v3_2: return "3.2";
    case v3_3: return "3.3";
    case v3_4: return "3.4";
    case v3_5: return "3.5";
    case v3_6: return "3.6";
    case v3_7: return "3.7";
    case v3_8: return "3.8";
    case v3_9: return "3.9";
    case Latest: return "Latest";
    }
    return "invalid";
}

// static
std::map<Python::Version::versions, const std::string> Python::Version::availableVersions()
{
    std::map<versions, const std::string> map;
    for (uint v = v2_6; v <= v2_7; ++v) {
        versions ver = static_cast<versions>(v);
        map.insert(std::pair<versions, const std::string>(ver, versionAsString(ver)));
    }

    for (uint v = v3_0; v <= Latest; ++v){
        versions ver = static_cast<versions>(v);
        map.insert(std::pair<versions, const std::string>(ver, versionAsString(ver)));
    }
    return map;
}


// -------------------------------------------------------------------------------------------
Python::Token::Token(Python::Token::Type token, uint startPos, uint endPos, TokenLine *line) :
    m_type(token), m_startPos(startPos), m_endPos(endPos),
    m_hash(0), m_next(nullptr), m_previous(nullptr),
    m_ownerLine(line)
{
#ifdef BUILD_PYTHON_DEBUGTOOLS
    m_nameDbg = text();
    if (m_ownerLine)
        m_lineDbg = m_ownerLine->lineNr();
#endif

    if (m_startPos != m_endPos)
        m_hash = cstrToHash(text().c_str(), m_endPos - m_startPos);
}

Python::Token::Token(const Python::Token &other) :
    m_type(other.m_type), m_startPos(other.m_startPos), m_endPos(other.m_endPos),
    m_hash(other.m_hash), m_next(other.m_next), m_previous(other.m_previous),
    m_ownerLine(other.m_ownerLine)
{
#ifdef BUILD_PYTHON_DEBUGTOOLS
    m_nameDbg = text();
    if (m_ownerLine)
        m_lineDbg = m_ownerLine->lineNr();
#endif
}

Python::Token::~Token()
{
#ifdef DEBUG_DELETES
    std::cout << "del Token: " << std::hex << this << " '" << m_nameDbg <<
                 "' ownerLine:" << std::hex << m_ownerLine <<
                 " type" << Python::tokenToCStr(m_type)  << std::endl;
#endif

    // notify our listeners
    for (auto n = m_wrappers.begin(); n != m_wrappers.end();) {
        auto p = n++; // need to advance because n might get deleted by tokenDeleted
        (*p)->tokenDeleted(); // we don't explicitly delete p here as owner of p might
                              // decide to some other thing with it
                              // and we are not the owner of p
    }

    if (m_ownerLine)
        m_ownerLine->remove(this, false);
}

void Python::Token::changeType(Python::Token::Type tokType)
{
    if (m_type == T_IdentifierUnknown && m_ownerLine) {
        if (tokType != T_IdentifierInvalid)
            m_ownerLine->unfinishedTokens().remove(m_ownerLine->tokenPos(this));
    }
    m_type = tokType;
}

Python::TokenList *Python::Token::ownerList() const
{
    return m_ownerLine->ownerList();
}

Python::TokenLine *Python::Token::ownerLine() const
{
    return m_ownerLine;
}

const std::string Python::Token::text() const
{
    if (m_ownerLine && m_endPos - m_startPos)
        return m_ownerLine->text().substr(m_startPos, m_endPos - m_startPos);
    return std::string();
}

int Python::Token::line() const
{
    if (m_ownerLine)
        return static_cast<int>(m_ownerLine->lineNr());
    return -1;
}

bool Python::Token::isNumber() const { // is a number in src file
    return m_type > T__NumbersStart &&
           m_type < T__NumbersEnd;
}

bool Python::Token::isInt() const { // is a integer (dec) in src file
    return m_type > T__NumbersIntStart &&
           m_type < T__NumbersIntEnd;
}

bool Python::Token::isFloat() const {
    return m_type == T_NumberFloat;
}

bool Python::Token::isString() const {
    return m_type > T__LiteralsStart &&
            m_type < T__LiteralsEnd;
}

bool Python::Token::isBoolean() const {
    return m_type == T_IdentifierTrue ||
           m_type == T_IdentifierFalse;
}

bool Python::Token::isMultilineString() const
{
    return m_type > T__LiteralsMultilineStart &&
           m_type < T__LiteralsMultilineEnd;
}

bool Python::Token::isKeyword() const {
    return m_type > T__KeywordsStart &&
           m_type <  T__KeywordsEnd;
}

bool Python::Token::isOperator() const {
    return m_type > T__OperatorStart &&
           m_type <  T__OperatorEnd;
}

bool Python::Token::isOperatorArithmetic() const {
    return m_type > T__OperatorArithmeticStart &&
           m_type < T__OperatorArithmeticEnd;
}

bool Python::Token::isOperatorBitwise() const {
    return m_type > T__OperatorBitwiseStart &&
           m_type < T__OperatorBitwiseEnd;
}

bool Python::Token::isOperatorAssignment() const { // modifies lvalue
    return m_type > T__OperatorAssignmentStart &&
           m_type < T__OperatorAssignmentEnd;
}

bool Python::Token::isOperatorAssignmentBitwise() const { // modifies through bit twiddling
    return m_type > T__OperatorBitwiseStart &&
           m_type < T__OperatorBitwiseEnd;
}

bool Python::Token::isOperatorCompare() const {
    return m_type > T__OperatorAssignBitwiseStart &&
           m_type < T__OperatorAssignBitwiseEnd;
}

bool Python::Token::isOperatorCompareKeyword() const {
    return m_type > T__OperatorCompareKeywordStart &&
           m_type < T__OperatorCompareKeywordEnd;
}

bool Python::Token::isOperatorParam() const {
    return m_type > T__OperatorParamStart &&
           m_type <  T__OperatorParamEnd;
}

bool Python::Token::isDelimiter() const {
    return m_type > T__DelimiterStart &&
           m_type < T__DelimiterEnd;
}

bool Python::Token::isIdentifier() const {
    return m_type > T__IdentifierStart &&
           m_type < T__IdentifierEnd;
}

bool Python::Token::isIdentifierVariable() const {
    return m_type > T__IdentifierVariableStart &&
           m_type < T__IdentifierVariableEnd;
}

bool Python::Token::isIdentifierDeclaration() const {
    return m_type > T__IdentifierDeclarationStart &&
           m_type < T__IdentifierDeclarationEnd;
}

bool Python::Token::isIdentifierFrameStart() const
{
    return m_type > T__IdentifierFrameStartTokenStart &&
           m_type < T__IdentifierFrameStartTokenEnd;
}

bool Python::Token::isNewLine() const
{
    if (m_type != T_DelimiterNewLine)
        return false;

    // else check for escape char
    const Python::Token *prev = previous();
    return prev && prev->type() != T_DelimiterBackSlash;
}

bool Python::Token::isInValid() const  {
    return m_type == T_Invalid;
}

bool Python::Token::isUndetermined() const {
    return m_type == T_Undetermined;
}

bool Python::Token::isCode() const
{
    if (m_type < T__NumbersStart)
        return false;
    if (m_type > T__DelimiterTextLineStart &&
        m_type < T__DelimiterTextLineEnd)
    {
        return false;
    }
    if (m_type >= T_BlockStart &&
        m_type <= T_BlockEnd)
    {
        return false;
    }
    return true;
}

bool Python::Token::isImport() const
{
    return m_type > T__IdentifierImportStart &&
           m_type < T__IdentifierImportEnd;
}
bool Python::Token::isText() const
{
    return m_type == T_Comment || isCode();
}

void Python::Token::attachReference(Python::TokenWrapperBase *tokWrapper)
{
    std::list<Python::TokenWrapperBase*>::iterator pos =
            std::find(m_wrappers.begin(), m_wrappers.end(), tokWrapper);
    if (pos == m_wrappers.end())
        m_wrappers.push_back(tokWrapper);
}

void Python::Token::detachReference(Python::TokenWrapperBase *tokWrapper)
{
    m_wrappers.remove(tokWrapper);
}

// -------------------------------------------------------------------------------------------

Python::TokenList::TokenList(Lexer *lexer) :
    m_first(nullptr), m_last(nullptr),
    m_firstLine(nullptr), m_lastLine(nullptr),
    m_size(0), m_lexer(lexer)
{
#ifdef DEBUG_DELETES
    std::cout << "new TokenLine: " << std::hex << this << std::endl;
#endif
}

Python::TokenList::TokenList(const Python::TokenList &other) :
    m_first(other.m_first), m_last(other.m_last),
    m_firstLine(other.m_firstLine), m_lastLine(other.m_lastLine),
    m_size(other.m_size), m_lexer(other.m_lexer)
{
#ifdef DEBUG_DELETES
    std::cout << "cpy TokenLine: " << std::hex << this
              << " tokens:" << m_size << " lines:" << lineCount() << std::endl;
#endif
}

Python::TokenList::~TokenList()
{
#ifdef DEBUG_DELETES
    std::cout << "del TokenLine: " << std::hex << this
              << " tokens:" << m_size << " lines:" << lineCount() << std::endl;
#endif
    clear();
}

Python::Token *Python::TokenList::operator[](int32_t idx)
{
    int32_t cnt = static_cast<int32_t>(count());

    while (idx < 0)
        idx = cnt - idx;

    if (cnt / 2 > idx) {
        // lookup from beginning
        Python::Token *iter = m_first;
        for (int32_t i = 0;
             iter && i < cnt;
             iter = iter->m_next, ++i)
        { /* intentionally empty*/ }
        return iter;
    }
    // lookup from end (reverse)
    Python::Token *iter = m_last;
    for (int32_t i = cnt -1;
         iter && i >= cnt;
         iter = iter->m_previous,--i)
    { /* intentionally empty*/ }
    return iter;
}

uint32_t Python::TokenList::count() const
{
    return m_size;
}

void Python::TokenList::clear()
{
    uint guard = max_size();
    while(m_firstLine && (--guard))
        removeLine(m_firstLine, true);
}

void Python::TokenList::push_back(Python::Token *tok)
{
    assert(tok->m_ownerLine && "Token has no line attached");
    if (m_last) {
        m_last->m_next = tok;
        tok->m_previous = m_last;
    }

    m_last = tok;
    //tok->m_ownerLine = this;
    ++m_size;

    if (!m_first)
        m_first = m_last;
}

void Python::TokenList::push_front(Python::Token *tok)
{
    assert(tok->m_ownerLine && "Token has no line attached");
    if (m_first) {
        m_first->m_previous = tok;
        tok->m_next = m_first;
    }

    m_first = tok;
    //tok->m_ownerLine = this;
    ++m_size;

    if (!m_last)
        m_last = m_first;
}

Python::Token *Python::TokenList::pop_back()
{
    Python::Token *tok = m_last;
    if (tok){
        m_last = tok->m_previous;
        tok->m_next = tok->m_previous = nullptr;
        tok->m_ownerLine = nullptr;
        assert(m_size > 0 && "Size cache out of order");
        --m_size;
    }
    return tok;
}

Python::Token *Python::TokenList::pop_front()
{
    Python::Token *tok = m_first;
    if (tok) {
        m_first = tok->m_next;
        tok->m_next = tok->m_previous = nullptr;
        tok->m_ownerLine = nullptr;
        assert(m_size > 0 && "Size cache out of order");
        --m_size;
    }
    return tok;
}

void Python::TokenList::insert(Python::Token *previousTok, Python::Token *insertTok)
{
    assert(insertTok->m_ownerLine && "insertTok has no line attached");
    assert(!insertTok->m_previous && !insertTok->m_next && "insertTok is already inserted");
    assert(insertTok != previousTok && "inserting the same token");

    if (previousTok) {
        assert(previousTok->m_ownerLine->m_ownerList == this && "previousTok not in this list");
        insertTok->m_next = previousTok->m_next;
        if (previousTok->m_next)
            previousTok->m_next->m_previous = insertTok;
        previousTok->m_next = insertTok;
        insertTok->m_previous = previousTok;
        if (m_last == previousTok)
            m_last = insertTok;
    } else {
        // the very first token
        assert(m_first != insertTok && "Trying to insert the same token before already inserted place");
        if (m_first)
            insertTok->m_next = m_first;
        if (!m_last)
            m_last = insertTok;
        m_first = insertTok;
    }

    ++m_size;
}

bool Python::TokenList::remove(Python::Token *tok, bool deleteTok)
{
    if (!tok)
        return false;

    if (tok->m_previous)
        tok->m_previous->m_next = tok->m_next;
    if (tok->m_next)
        tok->m_next->m_previous = tok->m_previous;
    if (m_first == tok)
        m_first = nullptr;
    if (m_last == tok)
        m_last = nullptr;

    assert(m_size > 0 && "Size cache out of order");
    --m_size;

    if (tok->m_ownerLine->m_tokenScanInfo)
        tok->m_ownerLine->m_tokenScanInfo->clearParseMessages(tok);

    if (deleteTok)
        delete tok;
    else
        tok->m_ownerLine = nullptr;
    return true;
}

bool Python::TokenList::remove(Python::Token *tok, Python::Token *endTok, bool deleteTok)
{
    if (!tok || !endTok ||
        tok->m_ownerLine != endTok->m_ownerLine)
    {
        return false;
    }

    if (tok->m_previous)
        tok->m_previous->m_next = endTok;
    if (endTok->m_previous) {
        endTok->m_previous->m_next = nullptr;
        endTok->m_previous = tok->m_previous;
    }
    tok->m_previous = nullptr;

    uint32_t guard = max_size();

    Python::Token *iter = tok;
    while (iter && guard--) {
        Python::Token *tmp = iter;
        iter = iter->m_next;

        assert(m_size > 0 && "Size cache out of order");
        --m_size;

        if (tmp->m_ownerLine->m_tokenScanInfo)
            tmp->m_ownerLine->m_tokenScanInfo->clearParseMessages(tmp);

        if (deleteTok)
            delete tmp;
        else
            tmp->m_ownerLine = nullptr;
    }

    assert(guard > 0 && "Line iteration guard circular nodes in List");
    return true;
}

Python::TokenLine *Python::TokenList::lineAt(int32_t lineNr) const
{
    if (!m_firstLine || !m_lastLine)
        return nullptr;

    Python::TokenLine *line;

    if (lineNr < 0) {
        // lookup from the back
        line = m_lastLine;
        for (int32_t idx = lineNr; idx < 0 && line; ++idx) {
            line = line->m_previousLine;
        }
        return line;
    }

    // lookup from start
    line = m_firstLine;
    for (int32_t idx = 0; idx < lineNr && line; ++idx) {
        line = line->m_nextLine;
    }
    return line;
}

uint32_t Python::TokenList::lineCount() const
{
    uint32_t cnt = 0;
    uint32_t guard = max_size();
    for (Python::TokenLine *line = m_firstLine;
         line && guard;
         line = line->m_nextLine, --guard)
    {
        ++cnt;
    }
    assert(guard > 0 && "Line iteration guard circular nodes in List");
    return cnt;
}

void Python::TokenList::swapLine(int32_t lineNr, Python::TokenLine *swapIn)
{
    swapLine(lineAt(lineNr), swapIn);
}

void Python::TokenList::swapLine(Python::TokenLine *swapOut, Python::TokenLine *swapIn)
{
    assert(swapIn != nullptr);
    assert(swapOut != nullptr);
    assert(swapIn != swapOut);
    assert(swapOut->m_ownerList == this && "swapOut not in this list");
    assert((!swapIn->m_ownerList || swapIn->m_ownerList == this) &&
                                "swapIn already attached to a list");

    assert(swapOut->m_nextLine != swapIn);
    assert(swapOut->m_previousLine != swapIn);

    swapIn->m_ownerList = this;
    swapIn->m_line = swapOut->m_line;

    if (m_firstLine == swapOut->instance())
        m_firstLine = swapIn->instance();
    if (m_lastLine == swapOut->instance())
        m_lastLine = swapIn->instance();
    if (swapOut->m_previousLine)
        swapOut->m_previousLine->m_nextLine = swapIn;
    if (swapOut->m_nextLine)
        swapOut->m_nextLine->m_previousLine = swapIn;
    swapIn->m_previousLine = swapOut->m_previousLine;
    swapIn->m_nextLine     = swapOut->m_nextLine;

    // clear so this line don't mess with list when it is deleted
    swapOut->m_previousLine = swapOut->m_nextLine = nullptr;

    assert(swapIn->m_nextLine != swapIn);
    assert(swapIn->m_previousLine != swapIn);
}

void Python::TokenList::insertLine(int32_t lineNr, Python::TokenLine *insertLine)
{
    Python::TokenLine *lineObj = lineAt(lineNr);
    if (lineObj)
        lineObj = lineObj->m_previousLine; // if nullptr = first line in list
    this->insertLine(lineObj, insertLine);
}

void Python::TokenList::insertLine(Python::TokenLine *previousLine,
                                   Python::TokenLine *insertLine)
{
    assert(insertLine != nullptr);
    assert(insertLine != previousLine);
    assert(insertLine->m_ownerList == this && "insertLine contained in another list");

    if (previousLine) {
        // insert after previousLine
        assert(previousLine->m_ownerList == this && "previousLine not stored in this list");
        insertLine->m_previousLine = previousLine->instance();
        insertLine->m_nextLine = previousLine->m_nextLine->instance();
        if (previousLine->m_nextLine) {
            // insert in the middle
            assert(m_lastLine != previousLine->instance() && "*m_lastLine out of sync");
            insertLine->m_nextLine->m_previousLine = insertLine->instance();
        } else {
            // last line
            assert(m_lastLine == previousLine);
            m_lastLine = insertLine->instance();
        }
        previousLine->m_nextLine = insertLine;

        insertLine->m_line = previousLine->m_line;
        incLineCount(insertLine);
    } else if(m_firstLine) {
        // has a line stored
        insertLine->m_nextLine = m_firstLine;
        insertLine->m_previousLine = nullptr;
        m_firstLine->m_previousLine = insertLine->instance();
        m_firstLine = insertLine->instance();
        insertLine->m_line = 0;
        incLineCount(insertLine->m_nextLine);
    } else {
        // at beginning (ie. first row)
        insertLine->m_nextLine = nullptr;
        insertLine->m_previousLine = nullptr;
        m_firstLine = insertLine->instance();
        m_lastLine = insertLine->instance();
        insertLine->m_line = 0;
    }

    insertLine->m_ownerList = this;

    assert(insertLine->m_nextLine != insertLine);
    assert(insertLine->m_previousLine != insertLine);
}

void Python::TokenList::appendLine(Python::TokenLine *lineToPush)
{
    assert(lineToPush);
    assert(lineToPush->m_ownerList == nullptr && "lineToPush is contained in another list");
    if (m_lastLine) {
        lineToPush->m_previousLine = m_lastLine;
        lineToPush->m_line = m_lastLine->m_line +1;
    }

    m_lastLine = lineToPush->instance();
    if (!m_firstLine)
        m_firstLine = lineToPush->instance();

    lineToPush->m_ownerList = this;
}

void Python::TokenList::removeLine(int32_t lineNr, bool deleteLine)
{
    removeLine(lineAt(lineNr), deleteLine);
}

void Python::TokenList::removeLine(Python::TokenLine *lineToRemove, bool deleteLine)
{
    assert(lineToRemove);
    assert(lineToRemove->m_ownerList == this && "lineToRemove not contained in this list");

    if (lineToRemove->instance() == m_firstLine)
        m_firstLine = lineToRemove->m_nextLine ?
                    lineToRemove->m_nextLine->instance() : nullptr;
    if (lineToRemove->instance() == m_lastLine)
        m_lastLine =  lineToRemove->m_previousLine ?
                    lineToRemove->m_previousLine->instance() : nullptr;
    if (lineToRemove->m_previousLine)
        lineToRemove->m_previousLine->m_nextLine = lineToRemove->m_nextLine->instance();
    if (lineToRemove->m_nextLine) {
        lineToRemove->m_nextLine->m_previousLine = lineToRemove->m_previousLine->instance();
        decLineCount(lineToRemove->m_nextLine);
    }

    if (lineToRemove->m_frontTok == m_first) {
        if (lineToRemove->m_nextLine)
            m_first = lineToRemove->m_nextLine->m_frontTok;
        else
            m_first = nullptr;
    }

    if (m_last && lineToRemove->back() == m_last) {
        if (lineToRemove->m_previousLine)
            m_last = lineToRemove->m_previousLine->back();
        else
            m_last = m_first;
    }

    // we dont remove tokens here, we just detach them from this list
    // line destructor is responsible for cleanup
    //if (!lineToRemove->empty())
    //    remove(lineToRemove->front(), lineToRemove->back(), true);

    if (deleteLine)
        delete lineToRemove;
}

void Python::TokenList::incLineCount(Python::TokenLine *firstLineToInc) const
{
    uint guard = max_size();
    while(firstLineToInc && (--guard)) {
        ++firstLineToInc->m_line;
        firstLineToInc = firstLineToInc->m_nextLine;
    }
    assert(guard > 0);
}

void Python::TokenList::decLineCount(Python::TokenLine *firstLineToDec) const
{
    uint guard = max_size();
    while(firstLineToDec && (--guard)) {
        ++firstLineToDec->m_line;
        firstLineToDec = firstLineToDec->m_nextLine;
    }
    assert(guard > 0);
}

// ----------------------------------------------------------------------------------------

Python::TokenLine::TokenLine(Python::TokenList *ownerList,
                             Python::Token *startTok,
                             const std::string &text) :
    m_ownerList(ownerList), m_frontTok(startTok), m_backTok(startTok),
    m_nextLine(nullptr), m_previousLine(nullptr), m_tokenScanInfo(nullptr),
    m_indentCharCount(0), m_parenCnt(0), m_bracketCnt(0), m_braceCnt(0),
    m_blockStateCnt(0), m_line(-1),  m_isParamLine(false)
{
    // strip newline chars
    size_t trimChrs = 0;

    // should match "\r\n", "\n\r", "\r", "\n"
    for (size_t i = 1; text.length() >= i && i < 2; ++i) {
        if (text[text.length() - i] == '\n' || text[text.length() - i] == '\r')
            ++trimChrs;
        else
            break;
    }

    m_text = text.substr(0, text.length() - trimChrs) + "\n";

#ifdef DEBUG_DELETES
    std::cout << "new TokenLine: " << std::hex << this << " " << m_text << std::endl;
#endif
}

Python::TokenLine::TokenLine(const TokenLine &other) :
    m_ownerList(other.m_ownerList), m_frontTok(other.m_frontTok),
    m_backTok(other.m_backTok), m_nextLine(other.m_nextLine),
    m_previousLine(other.m_previousLine), m_tokenScanInfo(other.m_tokenScanInfo),
    m_text(other.m_text), m_indentCharCount(other.m_indentCharCount),
    m_parenCnt(other.m_parenCnt), m_bracketCnt(other.m_bracketCnt),
    m_braceCnt(other.m_braceCnt), m_blockStateCnt(other.m_blockStateCnt),
    m_line(-1), m_isParamLine(false)
{
#ifdef DEBUG_DELETES
    std::cout << "cpy TokenLine: " << std::hex << this << " " << m_text << std::endl;
#endif
}

Python::TokenLine::~TokenLine()
{
#ifdef DEBUG_DELETES
    std::cout << "del TokenLine: " << std::hex << this << " '" << m_text <<
                 "' tokCnt:" << count() << std::endl;
#endif

    if (m_tokenScanInfo) {
        delete m_tokenScanInfo;
        m_tokenScanInfo = nullptr;
    }

    Python::Token *tok = m_frontTok;
    while (tok && tok->m_ownerLine == this)
    {
        Python::Token *tmp = tok;
        tok = tok->m_next;
        delete tmp;
    }

    m_ownerList->removeLine(this, false);
}

uint32_t Python::TokenLine::lineNr() const
{
    return static_cast<uint>(m_line);
}

uint Python::TokenLine::count() const
{
    uint cnt = 0;
    Python::Token *iter = m_frontTok;
    uint guard = m_ownerList->max_size();
    while(iter && (--guard)) {
        if (iter->m_ownerLine != this)
            break;
        ++cnt;
        iter = iter->m_next;
    }

    assert(guard > 0 && "Line iteration guard circular nodes in List");
    return cnt;
}

uint Python::TokenLine::indent() const
{
    return m_indentCharCount;
}

bool Python::TokenLine::isCodeLine() const
{
    Python::Token *tok = m_frontTok;
    uint guard = m_ownerList->max_size();
    while (tok && tok->m_ownerLine == this && (--guard)) {
        if (tok->isCode())
            return true;
        tok = tok->next();
    }
    return false;
}

Python::Token *Python::TokenLine::back() const {

    return m_backTok;
}

Python::Token *Python::TokenLine::end() const {
    return m_backTok ? m_backTok->m_next : nullptr;
}

Python::Token *Python::TokenLine::rend() const {
    return m_frontTok ? m_frontTok->m_previous : nullptr;
}

Python::Token *Python::TokenLine::operator[](int idx) const
{
    // allow lookup from the end
    while (idx < 0)
        idx = static_cast<int>(count()) - idx;

    int cnt = 0;
    Python::Token *iter = m_frontTok;
    uint guard = m_ownerList->max_size();
    while(iter && (--guard)) {
        if (iter->m_ownerLine != this)
            break;
        if (idx == cnt++)
            return iter;
        iter = iter->m_next;
    }
    assert(guard > 0 && "Line iteration guard circular nodes in List");

    return nullptr;
}

Python::Token *Python::TokenLine::tokenAt(int idx) const
{
    Python::Token *iter = m_frontTok;
    uint guard = m_ownerList->max_size();
    while(iter && iter->m_ownerLine == this && (--guard)) {
        if (iter->startPosInt() <= idx && iter->endPosInt() > idx)
            return iter;
        iter = iter->m_next;
    }
    assert(guard > 0 && "Line iteration guard circular nodes in List");

    return nullptr;
}

int Python::TokenLine::tokenPos(const Python::Token *tok) const
{
    int idx = 0;
    uint guard = m_ownerList->max_size();
    const Python::Token *iter = m_frontTok;
    while (iter && iter->m_ownerLine == this && (--guard)) {
        if (iter == tok)
            return idx;
        ++idx;
        iter = iter->m_next;
    }
    return -1;
}

const std::list<Python::Token *> Python::TokenLine::tokens() const
{
    std::list<Python::Token*> tokList;
    Python::Token *tok = m_frontTok;
    uint guard = m_ownerList->max_size();
    while(tok && tok->m_ownerLine == this && (--guard)) {
        tokList.push_back(tok);
        tok = tok->next();
    }
    return  tokList;
}

std::list<int> &Python::TokenLine::unfinishedTokens()
{
    return m_unfinishedTokenIndexes;
}

const Python::Token *Python::TokenLine::findToken(Python::Token::Type tokType, int searchFrom) const
{
    int lineEnd = static_cast<int>(count());

    if (searchFrom > lineEnd)
        searchFrom = lineEnd;
    else if (searchFrom < -lineEnd)
        searchFrom = -lineEnd;

    if (searchFrom < 0) {
        // search reversed (from lineend)
        Python::Token *tok = back();
        uint guard = m_ownerList->max_size();
        while(tok && tok->m_ownerLine == this && (--guard)) {
            if (tok->type() == tokType &&
                searchFrom <= tok->startPosInt() &&
                searchFrom < tok->endPosInt())
            {
                return tok;
            }
            tok = tok->previous();
        }

    } else {
        // search from front (linestart)
        Python::Token *tok = m_frontTok;
        uint guard = m_ownerList->max_size();
        while(tok && tok->m_ownerLine == this && (--guard)) {
            if (tok->type() == tokType &&
                searchFrom <= tok->startPosInt() &&
                searchFrom < tok->endPosInt())
            {
                return tok;
            }
            tok = tok->next();
        }
    }

    return nullptr;
}

const Python::Token *Python::TokenLine::firstTextToken() const
{
    Python::Token *tok = m_frontTok;
    uint guard = m_ownerList->max_size();
    while(tok && tok->m_ownerLine == this && (--guard)) {
        if (tok->isText())
            return tok;
        tok = tok->next();
    }
    return nullptr;
}

const Python::Token *Python::TokenLine::firstCodeToken() const
{
    Python::Token *tok = m_frontTok;
    uint guard = m_ownerList->max_size();
    while(tok && tok->m_ownerLine == this && (--guard)) {
        if (tok->isCode())
            return tok;
        tok = tok->next();
    }
    return nullptr;
}

void Python::TokenLine::push_back(Python::Token *tok)
{
    assert(!tok->m_next && !tok->m_previous && "tok already attached to container");
    assert(tok->m_ownerLine == this && "tok already added to a Line");
    assert(m_ownerList != nullptr && "Line must be inserted to a list to add tokens");
    tok->m_ownerLine = this;
    Token *beforeTok;
    if (!m_frontTok) {
        m_frontTok = tok;
        Python::TokenLine *line = m_previousLine;
        while (line && !line->m_backTok)
            line = line->m_previousLine;
        beforeTok = line ? line->m_backTok : nullptr;
    } else {
        beforeTok = m_backTok;
    }
    m_ownerList->insert(beforeTok, tok);
    m_backTok = tok;
}

void Python::TokenLine::push_front(Python::Token *tok)
{
    assert(!tok->m_next && !tok->m_previous && "tok already attached to container");
    assert(tok->m_ownerLine == this && "tok already added to a Line");
    assert(m_ownerList != nullptr && "Line must be inserted to a list to add tokens");
    tok->m_ownerLine = this;

    Python::Token *prevTok = nullptr;
    if (m_frontTok)
        prevTok = m_frontTok->m_previous;
    else if (m_previousLine)
        prevTok = m_previousLine->back();
    m_ownerList->insert(prevTok, tok);
    if (!m_backTok)
        m_backTok = tok;
    m_frontTok = tok;
}

Python::Token *Python::TokenLine::pop_back()
{
    assert(m_ownerList != nullptr && "Line must be inserted to a list to remove tokens");
    Python::Token *tok = m_backTok;
    if (tok) {
        m_ownerList->remove(tok, false);
        if (tok == m_frontTok)
            m_frontTok = m_backTok = nullptr;

        if (tok->m_previous  && tok->m_previous->m_ownerLine == instance())
            m_backTok = tok->m_previous;
        else
            m_backTok = m_frontTok;
    }
    return tok;
}

Python::Token *Python::TokenLine::pop_front()
{
    assert(m_ownerList != nullptr && "Line must be inserted to a list to remove tokens");
    if (m_frontTok) {
        m_ownerList->remove(m_frontTok, false);
        m_frontTok = m_frontTok->m_next;
        if (!m_frontTok)
            m_backTok = nullptr;
    }
    return m_frontTok;
}

void Python::TokenLine::insert(Python::Token *tok)
{
    assert(!tok->m_next && !tok->m_previous && "tok already attached to container");
    assert(tok->m_ownerLine == instance() && "tok already added to a Line");
    assert(m_ownerList != nullptr && "Line must be inserted to a list to add tokens");
    tok->m_ownerLine = this;

    //int pos = 0;
    Python::Token *prevTok = m_frontTok;
    if (!prevTok && m_previousLine)
        prevTok = m_previousLine->back();
    // move up to correct place within line
    uint guard = m_ownerList->max_size();
    while (prevTok && prevTok->m_ownerLine == this &&
           *prevTok < *tok && prevTok->m_next &&
           prevTok->m_next->m_ownerLine == this &&
           (--guard))
    {
        prevTok = prevTok->m_next;
        //++pos;
    }
    // insert into list, if prevTok is nullptr its inserted at beginning of list
    m_ownerList->insert(prevTok, tok);
    // our first token in this line
    if (!m_frontTok)
        m_frontTok = m_backTok = tok;
    if (m_backTok == prevTok)
        m_backTok = tok;
    //return pos;
}

void Python::TokenLine::insert(Python::Token *previousTok, Python::Token *insertTok)
{
    assert(insertTok);
    assert(insertTok->m_ownerLine == instance() || insertTok->m_ownerLine == nullptr);
    if (!previousTok || !previousTok->m_next ||
        previousTok->m_next->m_ownerLine != instance())
    {
        insert(insertTok);
        return;
    }

    insertTok->m_ownerLine = instance();
    m_ownerList->insert(previousTok, insertTok);
    if (m_frontTok == insertTok->m_next)
        m_frontTok = insertTok;
    if (m_backTok == previousTok)
        m_backTok = insertTok;
    //return tokenPos(insertTok);
}

bool Python::TokenLine::remove(Python::Token *tok, bool deleteTok)
{
    assert(m_ownerList != nullptr && "Line must be inserted to a list to remove tokens");
    if (tok->m_ownerLine != this)
        return false;

    if (tok == m_frontTok) {
        if (tok->m_next && tok->m_next->m_ownerLine == instance())
            m_frontTok = m_frontTok->m_next;
        else
            m_frontTok =  m_backTok = nullptr;
    }

    return m_ownerList->remove(tok, deleteTok);
}

bool Python::TokenLine::remove(Python::Token *tok, Python::Token *endTok, bool deleteTok)
{
    assert(m_ownerList != nullptr && "Line must be inserted to a list to remove tokens");
    if (tok->m_ownerLine != this || endTok->m_ownerLine != this)
        return false;

    if (tok == m_frontTok) {
        if (endTok->m_next && endTok->m_next->m_ownerLine == instance())
            m_frontTok = endTok->m_next;
        else
            m_frontTok = m_backTok = nullptr;
    }
    if (endTok == m_backTok) {
        if (m_frontTok == m_backTok)
            m_frontTok = nullptr;
        m_backTok = nullptr;
    }

    bool ret = m_ownerList->remove(tok, endTok, deleteTok);
    if (!m_frontTok)
        delete this; // delete me
    return ret;
}

Python::Token *Python::TokenLine::newDeterminedToken(Python::Token::Type tokType,
                                                     uint startPos, uint len)
{
    Python::Token *tokenObj = new Python::Token(tokType, startPos, startPos + len, this);
    push_back(tokenObj);
    return tokenObj;
}

Python::Token *Python::TokenLine::newUndeterminedToken(Python::Token::Type tokType,
                                                             uint startPos, uint len)
{
    Python::Token *tokenObj = newDeterminedToken(tokType, startPos, len);
    m_unfinishedTokenIndexes.push_back(tokenPos(tokenObj));
    return tokenObj;
}

Python::TokenScanInfo *Python::TokenLine::tokenScanInfo(bool initScanInfo)
{
    if (!m_tokenScanInfo && initScanInfo)
        m_tokenScanInfo = new Python::TokenScanInfo();
    return m_tokenScanInfo;
}

void Python::TokenLine::setIndentErrorMsg(const Python::Token *tok, const std::string &msg)
{
    Python::TokenScanInfo *scanInfo = tokenScanInfo(true);

    scanInfo->setParseMessage(tok, msg, TokenScanInfo::IndentError);
    m_ownerList->lexer()->setIndentError(tok);
}

void Python::TokenLine::setLookupErrorMsg(const Python::Token *tok, const std::string &msg)
{
    assert(tok);
    assert(tok->ownerLine() == instance());
    Python::TokenScanInfo *scanInfo = tokenScanInfo(true);

    if (msg.empty())
        scanInfo->setParseMessage(tok, "Can't lookup identifier '" + tok->text() + "'",
                                  TokenScanInfo::LookupError);
    else
        scanInfo->setParseMessage(tok, msg, TokenScanInfo::LookupError);
}

void Python::TokenLine::setSyntaxErrorMsg(const Python::Token *tok, const std::string &msg)
{
    assert(tok);
    assert(tok->ownerLine() == instance());

    Python::TokenScanInfo *scanInfo = tokenScanInfo(true);
    scanInfo->setParseMessage(tok, msg, TokenScanInfo::SyntaxError);
    m_ownerList->lexer()->setSyntaxError(tok);
}

void Python::TokenLine::setMessage(const Python::Token *tok, const std::string &msg)
{
    assert(tok);
    assert(tok->ownerLine() == instance());

    Python::TokenScanInfo *scanInfo = tokenScanInfo(true);
    scanInfo->setParseMessage(tok, msg, TokenScanInfo::Message);
}

void Python::TokenLine::setIndentCount(uint count)
{
    m_indentCharCount = count;
}

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
    LexerP::version.setVersion(value);
    LexerP::reGenerate();
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
    }

    uint prefixLen = 0;

    const std::string text(tokLine->text());

    uint i = 0, lastI = 0;
    char ch; //, prev;

    while ( i < text.length() )
    {
        ch = text.at( i );

        switch ( d_lex->endStateOfLastPara )
        {
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
            case '#':
                // begin a comment
                // it goes on to the end of the row
                setRestOfLine(i, text, Python::Token::T_Comment); // function advances i
                break;
            case '"': case '\'':
            {
                // Begin either string literal or block string
                char compare = thisCh;
                std::string endMarker;
                endMarker.push_back(ch);
                uint startStrPos = 0;
                Python::Token::Type tokType = Python::Token::T_Undetermined;
                uint len = 0;

                if (nextCh == compare && thirdCh == compare) {
                    // block string
                    endMarker += endMarker + endMarker; // makes """ or '''
                    startStrPos = i + 3;
                    if (compare == '"') {
                        tokType = Python::Token::T_LiteralBlockDblQuote;
                        len = lastDblQuoteStringCh(startStrPos, text);
                    } else {
                        tokType = Python::Token::T_LiteralBlockSglQuote;
                        len = lastSglQuoteStringCh(startStrPos, text);
                    }
                    d_lex->endStateOfLastPara = tokType;
                }

                // just a " or ' quoted string
                if (tokType == Python::Token::T_Undetermined) {
                    startStrPos = i + 1;
                    if (compare == '"') {
                        tokType = Python::Token::T_LiteralDblQuote;
                        len = lastDblQuoteStringCh(startStrPos, text);
                    } else {
                        tokType = Python::Token::T_LiteralSglQuote;
                        len = lastSglQuoteStringCh(startStrPos, text);
                    }
                }

                // search for end of string including endMarker
                auto endPos = text.find(endMarker, startStrPos + len);
                if (endPos != text.npos) {
                    len = static_cast<uint>(endPos - i) + prefixLen;
                    d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                }

                // a string might have string prefix, such as r"" or u''
                // detection of that was in a previous iteration
                i -= prefixLen;

                setLiteral(i, len + static_cast<uint>(endMarker.length()) + prefixLen, tokType);
                prefixLen = 0;

                // check if we need indent/dedent Token added
                if (d_lex->endStateOfLastPara != Token::T_Undetermined)
                    checkLineEnd();

            } break;

            // handle indentation
            case ' ': case '\t':
            {
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
                        setWord(i, 2, Python::Token::T_OperatorKeyWordParam);
                        break;
                    }
                    setWord(i, 1, Python::Token::T_OperatorVariableParam);
                    break;
                } else if (isModuleLine) {
                    // specialcase * as it is also used as glob for modules
                    i -= 1;
                    d_lex->endStateOfLastPara = Python::Token::T_IdentifierModuleGlob;
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
                if (nextCh == '.' && thirdCh == '.') {
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
                    setWord(i, len +1, Python::Token::T_IdentifierDecorator);
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
                                setWord(i, len, Python::Token::T_KeywordContinue);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                             } break;
                        case 7:
                            if (hash == d_lex->finallyHash) {
                                setWord(i, len, Python::Token::T_KeywordFinally);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 6:
                            if (hash == d_lex->importHash) {
                                setWord(i, len, Python::Token::T_KeywordImport);
                                d_lex->endStateOfLastPara = Python::Token::T_IdentifierModule; // step out to handle module name
                                isModuleLine = true;
                            } else if (hash == d_lex->returnHash) {
                                setWord(i, len, Python::Token::T_KeywordReturn);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->exceptHash) {
                                setWord(i, len, Python::Token::T_KeywordExcept);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 5:
                            if (hash == d_lex->classHash) {
                                setWord(i, len, Python::Token::T_KeywordClass);
                                d_lex->endStateOfLastPara = Python::Token::T_IdentifierClass; // step out to handle class name
                            } else if (hash == d_lex->yieldHash) {
                                setWord(i, len, Python::Token::T_KeywordYield);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->falseHash) {
                                setIdentifier(i, len, Python::Token::T_IdentifierFalse);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->whileHash) {
                                setWord(i, len, Python::Token::T_KeywordWhile);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->breakHash) {
                                setWord(i, len, Python::Token::T_KeywordBreak);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            }
                            break;
                        case 4:
                            if (hash == d_lex->fromHash) {
                                setWord(i, len, Python::Token::T_KeywordFrom);
                                d_lex->endStateOfLastPara = Python::Token::T_IdentifierModulePackage; // step out handle module name
                                isModuleLine = true;
                             } else if (hash == d_lex->trueHash) {
                                setIdentifier(i, len, Python::Token::T_IdentifierTrue);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->noneHash) {
                                setIdentifier(i, len, Python::Token::T_IdentifierNone);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->elifHash) {
                                setWord(i, len, Python::Token::T_KeywordElIf);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->elseHash) {
                                setWord(i, len, Python::Token::T_KeywordElse);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 3:
                            if (hash == d_lex->defHash) {
                                tokLine->m_isParamLine = true;
                                setWord(i, len, Python::Token::T_KeywordDef);
                                d_lex->endStateOfLastPara = Python::Token::T_IdentifierDefUnknown; // step out to handle def name

                            } else if (hash == d_lex->notHash) {
                                setWord(i, len, Python::Token::T_OperatorNot);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->forHash) {
                                setWord(i, len, Python::Token::T_KeywordFor);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->tryHash) {
                                setWord(i, len, Python::Token::T_KeywordTry);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        case 2:
                            if (hash == d_lex->andHash) {
                                setWord(i, len, Python::Token::T_OperatorAnd);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->asHash) {
                                setWord(i, len, Python::Token::T_KeywordAs);
                                if (isModuleLine)
                                    d_lex->endStateOfLastPara = Python::Token::T_IdentifierModuleAlias;
                                else
                                    d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->inHash) {
                                setWord(i, len, Python::Token::T_OperatorIn);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->isHash) {
                                setWord(i, len, Python::Token::T_OperatorIs);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->orHash) {
                                setWord(i, len, Python::Token::T_OperatorOr);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } else if (hash == d_lex->ifHash) {
                                setWord(i, len, Python::Token::T_KeywordIf);
                                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                            } break;
                        default:; // nothing
                        }

                        if (startI == i) {
                            setWord(i, len, Python::Token::T_Keyword);
                        }

                    } else {
                        auto it = d_lex->builtins.find(hash);
                        if (it != d_lex->builtins.end()) {
                            if (d_lex->activeLine->back() &&
                                d_lex->activeLine->back()->type() == Python::Token::T_DelimiterPeriod)
                            {
                                // avoid match against someObj.print
                                setWord(i, len, Python::Token::T_IdentifierUnknown);
                            } else
                                setWord(i, len, Python::Token::T_IdentifierBuiltin);
                        } else if (isModuleLine) {
                            i -= 1; // jump to module block
                            d_lex->endStateOfLastPara = Python::Token::T_IdentifierModule;
                        } else {
                            setUndeterminedIdentifier(i, len, Python::Token::T_IdentifierUnknown);
                        }
                    }

                    break;
                    //i += word.length(); setWord increments
                }

                // is it the beginning of a number?
                uint len = lastNumberCh(i, text);
                if ( len > 0) {
                    setNumber(i, len, numberType(text.substr(i, len)));
                    break;
                } else if (thisCh == '\\') {
                    setDelimiter(i, 1, Python::Token::T_DelimiterBackSlash);
                    break;
                }
                // its a error if we ever get here
                setSyntaxError(i, 1);


                std::clog << "PythonSyntaxHighlighter error on char: " << ch <<
                          " row:"<< std::to_string(d_lex->activeLine->lineNr() + 1) <<
                          " col:" << std::to_string(i) << std::endl;

            } // end default:
            } // end switch

        } break;// end T_Undetermined

        // we should only arrive here when we start on a new line
        case Python::Token::T_LiteralBlockDblQuote:
        {
            // multiline double quote string continued from previous line
            scanIndentation(i, text);

            auto endPos = text.find("\"\"\"", i);
            if (endPos != text.npos) {
                endPos += 3;
                setLiteral(i, static_cast<uint>(endPos) -i, Python::Token::T_LiteralBlockDblQuote);
                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
            } else {
                setRestOfLine(i, text, Python::Token::T_LiteralBlockDblQuote);
            }

        } break;
        case Python::Token::T_LiteralBlockSglQuote:
        {
            // multiline single quote string continued from previous line
            scanIndentation(i, text);

            auto endPos = text.find("'''", i);
            if (endPos != text.npos) {
                endPos += 3;
                setLiteral(i, static_cast<uint>(endPos) -i, Python::Token::T_LiteralBlockSglQuote);
                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
            }else {
                setRestOfLine(i, text, Python::Token::T_LiteralBlockSglQuote);
            }

        } break;
        case Python::Token::T_IdentifierDefUnknown:
        {   // before we now if its a class member or a function
            for (; i < text.length(); ++i) {
                if (!isSpace(text.at(i)))
                    break;
            }
            if (i == text.length()) {
                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                break; // not yet finished this line?
            }

            uint len = lastWordCh(i, text);
            if (d_lex->activeLine->indent() == 0)
                // no indent, it can't be a method
                setIdentifier(i, len, Python::Token::T_IdentifierFunction);
            else
                setUndeterminedIdentifier(i, len, Python::Token::T_IdentifierDefUnknown);
            d_lex->endStateOfLastPara = Python::Token::T_Undetermined;

        } break;
        case Python::Token::T_IdentifierClass:
        {
            for (; i < text.length(); ++i) {
                if (!isSpace(text.at(i)))
                    break;
            }
            if (i == text.length()) {
                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                break; // not yet finished this line?
            }

            uint len = lastWordCh(i, text);
            setIdentifier(i, len, Python::Token::T_IdentifierClass);
            d_lex->endStateOfLastPara = Python::Token::T_Undetermined;

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
                d_lex->endStateOfLastPara = Python::Token::T_Undetermined;
                break; // not yet finished this line?
            }

            uint len = lastWordCh(i, text);
            if (len < 1) {
                if (ch == '*') // globs aren't a word char so they don't get caught
                    len += 1;
                else
                    break;
            }

            setIdentifier(i, len, d_lex->endStateOfLastPara);
            d_lex->endStateOfLastPara = Python::Token::T_Undetermined;

        } break;
        default:
        {
            // let subclass sort it out
            Python::Token::Type nextState = unhandledState(i, d_lex->endStateOfLastPara, text);
            if (nextState != Python::Token::T_Invalid) {
                d_lex->endStateOfLastPara = nextState;
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
    if ( d_lex->endStateOfLastPara != Python::Token::T_LiteralBlockDblQuote &&
         d_lex->endStateOfLastPara != Python::Token::T_LiteralBlockSglQuote )
    {
        d_lex->endStateOfLastPara = Python::Token::T_Undetermined ;
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
    return d_lex->endStateOfLastPara;
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

uint Python::Lexer::lastNumberCh(uint startPos, const std::string &text) const
{
    std::string lowerText;
    auto pos = text.begin() + startPos;
    int first = *pos;
    uint len = 0;
    for (; pos != text.end(); ++pos, ++len) {
        int ch = std::tolower(*pos);
        if (ch >= '0' && ch <= '9')
            continue;
        if (ch >= 'a' && ch <= 'f')
            continue;
        if (ch == '.')
            continue;
        // hex, binary, octal
        if ((ch == 'x' || ch == 'b' || ch == 'o') &&  first == '0')
            continue;
        break;
    }
    // long integer or imaginary?
    if (pos == text.end()) {
        int ch = std::toupper(text.back());
        if (ch == 'L' || ch == 'J')
            len += 1;
    }

    return len;
}

uint Python::Lexer::lastDblQuoteStringCh(uint startAt, const std::string &text) const
{
    uint len = static_cast<uint>(text.length());
    if (len <= startAt)
        return 0;

    len = 0;
    for (auto pos = text.begin() + startAt; pos != text.end(); ++pos, ++len) {
        if (*pos == '\\') {
            ++len; ++pos;
            continue;
        }
        if (*pos == '"')
            break;
    }

    return len;
}

uint Python::Lexer::lastSglQuoteStringCh(uint startAt, const std::string &text) const
{
    // no escapes '\' possible in this type
    uint len = static_cast<uint>(text.length());
    if (len <= startAt)
        return 0;

    len = 0;
    for (auto pos = text.begin() + startAt; pos != text.end(); ++pos, ++len) {
        if (*pos == '\'')
            break;
    }

    return len;
}

Python::Token::Type Python::Lexer::numberType(const std::string &text) const
{
    if (text.empty())
        return Python::Token::T_Invalid;

    if (text.find('.') != text.npos)
        return Python::Token::T_NumberFloat;
    if (text.length() >= 2) {
        int one = tolower(text[0]),
            two = tolower(text[1]);
        if (one == '0') {
            if (two == 'x')
                return Python::Token::T_NumberHex;
            if (two == 'b')
                return Python::Token::T_NumberBinary;
            if (two == 'o' || one == '0')
                return Python::Token::T_NumberOctal;
            return Python::Token::T_NumberOctal; // python 2 octal ie. 01234 != 1234
        }
    }
    return Python::Token::T_NumberDecimal;
}

void Python::Lexer::setRestOfLine(uint &pos, const std::string &text, Python::Token::Type tokType)
{
    uint len = static_cast<uint>(text.size()) - pos;
    setWord(pos, len, tokType);
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
            d_lex->activeLine->m_indentCharCount = count;
        }
    }
}

void Python::Lexer::setWord(uint &pos, uint len, Python::Token::Type tokType)
{
    Python::Token *tok = d_lex->activeLine->newDeterminedToken(tokType, pos, len);
    tokenUpdated(tok);
    if (!d_lex->isCodeLine)
        d_lex->isCodeLine = tok->isCode();
    pos += len -1;
}

void Python::Lexer::setIdentifier(uint &pos, uint len, Python::Token::Type tokType)
{
    return setWord(pos, len, tokType);
}

void Python::Lexer::setUndeterminedIdentifier(uint &pos, uint len, Python::Token::Type tokType)
{
    // let parse tree figure out and color at a later stage
    Python::Token *tok = d_lex->activeLine->newUndeterminedToken(tokType, pos, len);
    tokenUpdated(tok);
    pos += len -1;
}

void Python::Lexer::setNumber(uint &pos, uint len, Python::Token::Type tokType)
{
    setWord(pos, len, tokType);
}

void Python::Lexer::setOperator(uint &pos, uint len, Python::Token::Type tokType)
{
    setWord(pos, len, tokType);
}

void Python::Lexer::setDelimiter(uint &pos, uint len, Python::Token::Type tokType)
{
    setWord(pos, len, tokType);
}

void Python::Lexer::setSyntaxError(uint &pos, uint len)
{
    setWord(pos, len, Python::Token::T_SyntaxError);
}

void Python::Lexer::setLiteral(uint &pos, uint len, Python::Token::Type tokType)
{
    setWord(pos, len, tokType);
}

void Python::Lexer::setIndentation()
{
    // get the nearest sibling above that has code
    Python::TokenLine *prevLine = previousCodeLine(d_lex->activeLine->m_previousLine);

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
        if (prevLine->m_parenCnt > 0 || prevLine->m_braceCnt > 0 || prevLine->m_bracketCnt > 0)
            break;

        // when we get here we have a cahnge in indentation to previous line
        if (prevLine->m_indentCharCount < d_lex->activeLine->indent()) {
            // increase indent by one block

            // check so we have a valid ':'
            Python::Token *lookupTok = prevLine->back();
            while(lookupTok && lookupTok->ownerLine() == prevLine &&
                  lookupTok->type() != Python::Token::T_DelimiterColon && (--guard))
            {
                lookupTok = lookupTok->previous();
            }

            if (!lookupTok || lookupTok->ownerLine() != prevLine) {
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
                    lastIndent = startLine->m_indentCharCount;
                }
                if (startLine->front() && startLine->front()->type() == Python::Token::T_Indent) {
                    ++indentCnt;
                    lastIndent = startLine->m_indentCharCount;
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

// ------------------------------------------------------------------------------------

Python::TokenScanInfo::ParseMsg::ParseMsg(const std::string &message, const Token *tok,
                                          TokenScanInfo::MsgType type) :
    m_message(message), m_token(tok), m_type(type)
{
#ifdef DEBUG_DELETES
    std::clog << "new ParseMsg " << std::hex << this << std::endl;
#endif
}

Python::TokenScanInfo::ParseMsg::~ParseMsg()
{
#ifdef DEBUG_DELETES
    std::clog << "del ParseMsg " << std::hex << this << std::endl;
#endif
}

const std::string Python::TokenScanInfo::ParseMsg::msgTypeAsString() const
{
    switch (m_type) {
    case Message:     return "Message";
    case LookupError: return "LookupError";
    case SyntaxError: return "SyntaxError";
    case IndentError: return "IndentError";
    case Warning:     return "Warning";
    case Issue:       return "Issue";
    //case AllMsgTypes:
    default: return "";
    }
}

const std::string Python::TokenScanInfo::ParseMsg::message() const
{
    return m_message;
}

const Python::Token *Python::TokenScanInfo::ParseMsg::token() const
{
    return m_token;
}

Python::TokenScanInfo::MsgType Python::TokenScanInfo::ParseMsg::type() const
{
    return m_type;
}

// ------------------------------------------------------------------------------------

Python::TokenScanInfo::TokenScanInfo()
{
#ifdef DEBUG_DELETES
    std::clog << "new TokenScanInfo " << std::hex << this << std::endl;
#endif
}

Python::TokenScanInfo::~TokenScanInfo()
{
#ifdef DEBUG_DELETES
    std::clog << "del TokenScanInfo " << std::hex << this << std::endl;
#endif

    for (auto &msg : m_parseMsgs)
        delete msg;
}

void Python::TokenScanInfo::setParseMessage(const Python::Token *tok,
                                            const std::string &msg,
                                            Python::TokenScanInfo::MsgType type)
{
    m_parseMsgs.push_back(new ParseMsg(msg, tok, type));
}

const std::list<const Python::TokenScanInfo::ParseMsg *>
Python::TokenScanInfo::getParseMessages(const Python::Token *tok,
                                        Python::TokenScanInfo::MsgType type) const
{
    std::list<const ParseMsg*> ret;
    for (auto &msg : m_parseMsgs) {
        if (tok == msg->token() &&
            (type == AllMsgTypes || msg->type() == type))
        {
            ret.push_back(msg);
        }
    }

    return ret;
}

int Python::TokenScanInfo::clearParseMessages(const Python::Token *tok, MsgType filterType)
{
    auto eraseFrom = std::remove_if(m_parseMsgs.begin(), m_parseMsgs.end(),
                   [tok, filterType](const Python::TokenScanInfo::ParseMsg *msg) {
                        if (*msg->token() == *tok) {
                            if (filterType != AllMsgTypes)
                                return msg->type() == filterType;
                            return true;
                        }
                        return false;
    });
    int cnt = static_cast<int>(std::distance(eraseFrom, m_parseMsgs.end()));
    for (auto it = eraseFrom; it != m_parseMsgs.end(); ++it)
        delete *it;
    m_parseMsgs.erase(eraseFrom, m_parseMsgs.end());

    return cnt;
}

std::list<const Python::TokenScanInfo::ParseMsg *> Python::TokenScanInfo::allMessages() const
{
    return m_parseMsgs;
}

// ----------------------------------------------------------------------------------------

Python::TokenWrapperBase::TokenWrapperBase(Token *tok) :
    m_token(nullptr)
{
    setToken(tok);
}

Python::TokenWrapperBase::~TokenWrapperBase()
{
    setToken(nullptr);
}

void Python::TokenWrapperBase::setToken(Python::Token *tok)
{
    if (m_token)
        m_token->detachReference(this);
    if ((m_token = tok))
        m_token->attachReference(this);
}

// ---------------------------------------------------------------------------------------

Python::TokenWrapperInherit::TokenWrapperInherit(Token *token) :
    TokenWrapperBase(token)
{
}

Python::TokenWrapperInherit::~TokenWrapperInherit()
{
}

const std::string Python::TokenWrapperInherit::text() const
{
    if (m_token)
        return m_token->text();
    return std::string();
}

std::size_t Python::TokenWrapperInherit::hash() const
{
    if (m_token)
        return m_token->hash();
    return 0;
}

void Python::TokenWrapperInherit::tokenDeleted()
{
    m_token = nullptr;
    tokenDeletedCallback();
}

