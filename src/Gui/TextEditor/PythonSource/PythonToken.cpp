
#include "PythonToken.h"
#include "PythonSourceListBase.h"
#include "PythonLexer.h"

#include <algorithm>
#include <assert.h>
//#include <cctype>
#include <string>
#include <iostream>
#include <unordered_map>
#include <cstring>


#ifdef BUILD_PYTHON_DEBUGTOOLS
//# include <TextEditor/PythonCodeDebugTools.h>
//# define DEBUG_DELETES
#endif

std::unordered_map<std::string, Python::Token::Type> createMapOfTokens()
{
    std::unordered_map<std::string, Python::Token::Type> map;
    for(uint t = 0; t < Python::Token::T__EndOfTokensMarker; ++t)
    {
        auto type = static_cast<Python::Token::Type>(t);
        std::string tokName = Python::Token::tokenToCStr(type);
        if (strncmp(tokName.c_str(), "Unknown(", 8) != 0)
            map[tokName] = type;
    }
    return map;
}

// -------------------------------------------------------------------------

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

Python::Version::versions Python::Version::strToVersion(const std::string &versionStr)
{
    for (auto &ver : availableVersions()) {
        if (ver.second == versionStr)
            return ver.first;
    }
    return Invalid;
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

Python::TokenIterator::TokenIterator(Python::Token *startTok) :
    m_start(startTok)
{
}

Python::TokenIterator::TokenIterator(const Python::TokenIterator &other) :
    m_start(other.m_start)
{
}

Python::TokenIterator::~TokenIterator()
{
}

Python::TokenIterator &Python::TokenIterator::operator++()
{
    // preincrement '++it'
    m_start = m_start->next();
    return *this;
}

Python::TokenIterator Python::TokenIterator::operator++(int)
{
    // postincrement 'it++'
    TokenIterator beforeIt(*this);
    m_start = m_start->next();
    return beforeIt;
}

Python::TokenIterator &Python::TokenIterator::operator--()
{
    // predecrement
    m_start = m_start->previous();
    return *this;
}

Python::TokenIterator Python::TokenIterator::operator--(int)
{
    // postdecrement
    TokenIterator beforeIt(*this);
    m_start = m_start->previous();
    return beforeIt;
}


// -------------------------------------------------------------------------------------------
// static
const char *Python::Token::tokenToCStr(Python::Token::Type tokType)
{
    switch(tokType) {
    case T_Undetermined:        return "T_Undetermined";     // Parser looks tries to figure out next char also Standard text
        // python
    case T_Indent:              return "T_Indent";
    case T_Dedent:              return "T_Dedent";
    case T_Comment:             return "T_Comment";     // Comment begins with #
    case T_SyntaxError:         return "T_SyntaxError";
    case T_IndentError:         return "T_IndentError";  // to signify that we have a indent error, set by PythonSourceRoot

        // numbers
    case T_NumberDecimal:       return "T_NumberDecimal";     // Normal number
    case T_NumberHex:           return "T_NumberHex";
    case T_NumberBinary:        return "T_NumberBinary";
    case T_NumberOctal:         return "T_NumberOctal";    // starting with 0 ie 011 = 9, different color to let it stand out
    case T_NumberFloat:         return "T_NumberFloat";

        // strings
    case T_LiteralDblQuote:     return "T_LiteralDblQote";     // String literal beginning with "
    case T_LiteralSglQuote:     return "T_LiteralSglQuote";     // Other string literal beginning with '
    case T_LiteralBlockDblQuote: return "T_LiteralBlockDblQuote";     // Block comments beginning and ending with """
    case T_LiteralBlockSglQuote: return "T_LiteralBlockSglQuote";     // Other block comments beginning and ending with '''

        // Keywords
    case T_Keyword:             return "T_Keyword";
    case T_KeywordClass:        return "T_KeywordClass";
    case T_KeywordDef:          return "T_KeywordDef";
    case T_KeywordImport:       return "T_KeywordImport";
    case T_KeywordFrom:         return "T_KeywordFrom";
    case T_KeywordAs:           return "T_KeywordAs";
    case T_KeywordYield:        return "T_KeywordYield";
    case T_KeywordReturn:       return "T_KeywordReturn";
    case T_KeywordRaise:        return "T_KeywordRaise";
    case T_KeywordWith:         return "T_KeywordWith";
    case T_KeywordGlobal:       return "T_KeywordGlobal";
    case T_KeywordLambda:       return "T_KeywordLambda";
    case T_KeywordPass:         return "T_KeywordPass";
    case T_KeywordAssert:       return "T_KeywordAssert";
    case T_KeywordIf:           return "T_KeywordIf";
    case T_KeywordElIf:         return "T_KeywordElIf";
    case T_KeywordElse:         return "T_KeywordElse";
    case T_KeywordFor:          return "T_KeywordFor";
    case T_KeywordWhile:        return "T_KeywordWhile";
    case T_KeywordBreak:        return "T_KeywordBreak";
    case T_KeywordContinue:     return "T_KeywordContinue";
    case T_KeywordTry:          return "T_KeywordTry";
    case T_KeywordExcept:       return "T_KeywordExcept";
    case T_KeywordFinally:      return "T_KeywordFinally";
        // leave some room for future keywords

        // operators
       // arithmetic
    case T_OperatorPlus:          return     "T_OperatorPlus";           // +,
    case T_OperatorMinus:         return     "T_OperatorMinus";          // -,
    case T_OperatorMul:           return     "T_OperatorMul";            // *,
    case T_OperatorExponential:   return     "T_OperatorExponential";    // **,
    case T_OperatorDiv:           return     "T_OperatorDiv";            // /,
    case T_OperatorFloorDiv:      return     "T_OperatorFloorDiv";       // //,
    case T_OperatorModulo:        return     "T_OperatorModulo";         // %,
    case T_OperatorMatrixMul:     return     "T_OperatorMatrixMul";      // @
        //bitwise

    case T_OperatorBitShiftLeft:  return     "T_OperatorBitShiftLeft";   // <<,
    case T_OperatorBitShiftRight: return     "T_OperatorBitShiftRight";  // >>,
    case T_OperatorBitAnd:        return     "T_OperatorBitAnd";         // &,
    case T_OperatorBitOr:         return     "T_OperatorBitOr";          // |,
    case T_OperatorBitXor:        return     "T_OperatorBitXor";         // ^,
    case T_OperatorBitNot:        return     "T_OperatorBitNot";         // ~,

        // assigment
    case T_OperatorEqual:         return     "T_OperatorEqual";          // =,
    case T_OperatorWalrus:        return     "T_OperatorWalrus";
    case T_OperatorPlusEqual:     return     "T_OperatorPlusEqual";      // +=,
    case T_OperatorMinusEqual:    return     "T_OperatorMinusEqual";     // -=,
    case T_OperatorMulEqual:      return     "T_OperatorMulEqual";       // *=,
    case T_OperatorDivEqual:      return     "T_OperatorDivEqual";       // /=,
    case T_OperatorModuloEqual:   return     "T_OperatorModuloEqual";    // %=,
    case T_OperatorFloorDivEqual: return     "T_OperatorFloorDivEqual";  // //=,
    case T_OperatorExpoEqual:     return     "T_OperatorExpoEqual";      // **=,
    case T_OperatorMatrixMulEqual:return     "T_OperatorMatrixMulEqual"; // @= introduced in py 3.5

        // assigment bitwise
    case T_OperatorBitAndEqual:   return     "T_OperatorBitAndEqual";    // &=,
    case T_OperatorBitOrEqual:    return     "T_OperatorBitOrEqual";     // |=,
    case T_OperatorBitXorEqual:   return     "T_OperatorBitXorEqual";    // ^=,
    case T_OperatorBitNotEqual:   return     "T_OperatorBitNotEqual";    // ~=,
    case T_OperatorBitShiftRightEqual:return "T_OperatorBitShiftRightEqual"; // >>=,
    case T_OperatorBitShiftLeftEqual: return "T_OperatorBitShiftLeftEqual";  // <<=,

        // compare
    case T_OperatorCompareEqual:  return     "T_OperatorCompareEqual";   // ==,
    case T_OperatorNotEqual:      return     "T_OperatorNotEqual";       // !=,
    case T_OperatorLessEqual:     return     "T_OperatorLessEqual";      // <=,
    case T_OperatorMoreEqual:     return     "T_OperatorMoreEqual";      // >=,
    case T_OperatorLess:          return     "T_OperatorLess";           // <,
    case T_OperatorMore:          return     "T_OperatorMore";           // >,
    case T_OperatorAnd:           return     "T_OperatorAnd";            // 'and',
    case T_OperatorOr:            return     "T_OperatorOr";             // 'or',
    case T_OperatorNot:           return     "T_OperatorNot";            // 'not',
    case T_OperatorIs:            return     "T_OperatorIs";             // 'is',
    case T_OperatorIn:            return     "T_OperatorIn";             // 'in',

        // special
    case T_OperatorVariableParam: return     "T_OperatorVariableParam";  // * for function parameters ie (*arg1)
    case T_OperatorKeyWordParam:  return     "T_OperatorKeyWordParam";   // ** for function parameters ir (**arg1)

        // delimiters
    case T_Delimiter:             return "T_Delimiter";   // all other non specified
    case T_DelimiterOpenParen:    return "T_DelimiterOpenParen";   // (
    case T_DelimiterCloseParen:   return "T_DelimiterCloseParen";   // )
    case T_DelimiterOpenBracket:  return "T_DelimiterOpenBracket";   // [
    case T_DelimiterCloseBracket: return "T_DelimiterCloseBracket";   // ]
    case T_DelimiterOpenBrace:    return "T_DelimiterOpenBrace";   // {
    case T_DelimiterCloseBrace:   return "T_DelimiterCloseBrace";   // }
    case T_DelimiterPeriod:       return "T_DelimiterPeriod";   // .
    case T_DelimiterComma:        return "T_DelimiterComma";   // ,
    case T_DelimiterColon:        return "T_DelimiterColon";   // :
    case T_DelimiterSemiColon:    return "T_DelimiterSemiColon";   // ;
    case T_DelimiterEllipsis:     return "T_DelimiterEllipsis";    // ...
        // metadata such def funcname(arg: "documentation") ->
        //                            "returntype documentation":
    case T_DelimiterMetaData:     return "T_DelimiterMetaData";   // -> might also be ':' inside arguments
    case T_DelimiterBackSlash:    return "T_DelimiterBackSlash";   // when end of line is escaped like so '\'
    case T_DelimiterNewLine:      return "T_DelimiterNewLine";   // each new line
        // identifiers
    case T_IdentifierUnknown:     return "T_IdentifierUnknown"; // name is not identified at this point
    case T_IdentifierDefined:     return "T_IdentifierDefined"; // variable is in current context
    case T_IdentifierModule:      return "T_IdentifierModule"; // its a module definition
    case T_IdentifierModuleAlias: return "T_IdentifierModuleAlias"; // alias for import. ie: import Something as Alias
    case T_IdentifierModulePackage: return "T_IdentifierModulePackage"; // identifier is a package, ie: root for other modules
    case T_IdentifierModuleGlob:  return "T_IdentifierModuleGlob"; // from mod import * <- glob
    case T_IdentifierFunction:    return "T_IdentifierFunction"; // its a function definition
    case T_IdentifierMethod:      return "T_IdentifierMethod"; // its a method definition
    case T_IdentifierClass:       return "T_IdentifierClass"; // its a class definition
    case T_IdentifierSuperMethod: return "T_IdentifierSuperMethod"; // its a method with name: __**__
    case T_IdentifierBuiltin:     return "T_IdentifierBuiltin"; // its builtin
    case T_IdentifierDecorator:   return "T_IdentifierDecorator"; // member decorator like: @property
    case T_IdentifierDefUnknown:  return "T_IdentifierDefUnknown"; // before system has determined if its a
                                                                                            // method or function yet
    case T_IdentifierNone:        return "T_IdentifierNone";  // The None keyword
    case T_IdentifierTrue:        return "T_IdentifierTrue";  // The bool True
    case T_IdentifierFalse:       return "T_IdentifierFalse"; // The bool False
    case T_IdentifierSelf:        return "T_IdentifierSelf";
    case T_IdentifierInvalid:     return "T_IdentifierInvalid";

        // metadata such def funcname(arg: "documentaion") -> "returntype documentation":
    case T_MetaData:              return "T_MetaData";
        // these are inserted by PythonSourceRoot
    case T_BlockStart:            return "T_BlockStart"; // indicate a new block ie if (a is True):
                                                                                  //                                       ^
    case T_BlockEnd:              return "T_BlockEnd";   // indicate block end ie:    dosomething
                                                                                  //                        dosomethingElse
                                                                                  //                       ^


    case T_Invalid:               return "T_Invalid";
    case T_PythonConsoleOutput:   return "T_PythonConsoleOutput";
    case T_PythonConsoleError:    return "T_PythonConsoleError";
    default:
        static char str[50];
        sprintf(str, "Unknown(%d)", static_cast<int>(tokType));
        char *closeParenCh = strchr(str, ')');
        closeParenCh++;
        closeParenCh = nullptr; // terminate str
        return str;
    }
}

Python::Token::Type Python::Token::strToToken(const std::string &tokName)
{
    static std::unordered_map<std::string, Token::Type> tokens = createMapOfTokens();
    if (tokens.find(tokName) != tokens.end())
        return tokens[tokName];
    return T_Invalid;
}


Python::Token::Token(Python::Token::Type token, uint16_t startPos,
                     uint16_t endPos, TokenLine *line) :
    m_type(token), m_startPos(startPos), m_endPos(endPos),
    m_customMask(0), m_hash(0), m_next(nullptr),
    m_previous(nullptr), m_ownerLine(line)
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
    m_customMask(0), m_hash(other.m_hash), m_next(other.m_next),
    m_previous(other.m_previous), m_ownerLine(other.m_ownerLine)
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

bool Python::Token::isStringRaw() const
{
    return isString() && (m_customMask & STRING_IS_RAW_TYPE);
}

bool Python::Token::isStringBytes() const
{
    return isString() && (m_customMask & STRING_IS_BYTES_TYPE);
}

bool Python::Token::isStringUnicode() const
{
    return isString() && (m_customMask & STRING_IS_UNICODE_TYPE);
}

bool Python::Token::isStringFormat() const
{
    return isString() && (m_customMask & STRING_IS_FORMAT_TYPE);
}

bool Python::Token::isBoolean() const {
    return m_type == T_IdentifierTrue ||
           m_type == T_IdentifierFalse;
}

bool Python::Token::isMultilineString() const
{
    return (m_type > T__LiteralsMultilineStart &&
            m_type < T__LiteralsMultilineEnd &&
            (m_customMask & STRING_IS_MULTILINE_TYPE));
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

Python::TokenList &Python::TokenList::operator=(const Python::TokenList &other)
{
    (void)other;
    return *this;
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
             iter && i < idx;
             iter = iter->m_next, ++i)
        { /* intentionally empty*/ }
        return iter;
    }
    // lookup from end (reverse)
    Python::Token *iter = m_last;
    for (int32_t i = cnt -1;
         iter && i > idx;
         iter = iter->m_previous,--i)
    { /* intentionally empty*/ }
    return iter;
}

uint32_t Python::TokenList::count() const
{
    return m_size;
}

void Python::TokenList::clear(bool deleteLine)
{
    uint guard = max_size();
    while(m_firstLine && (--guard))
        removeLine(m_firstLine, deleteLine);
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
        if (m_first) {
            insertTok->m_next = m_first;
            m_first->m_previous = insertTok;
        }
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
        for (int32_t idx = lineNr; idx < -1 && line; ++idx) {
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
    if (m_lastLine)
        return static_cast<uint32_t>(m_lastLine->m_line +1);
    return 0;

    /*
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
    */
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
    assert(swapIn->m_ownerList == nullptr && "swapIn already attached to a list");

    assert(swapOut->m_nextLine != swapIn);
    assert(swapOut->m_previousLine != swapIn);
    assert(swapIn->m_nextLine == nullptr);
    assert(swapIn->m_previousLine == nullptr);

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

    // update m_first or m_last pointer to our tokens
    if (m_first == swapOut->m_frontTok) {
        if (swapIn->m_frontTok)
            m_first = swapIn->m_frontTok;
        else
            m_first = swapOut->m_backTok ? swapOut->m_backTok->m_next : nullptr;
    }
    if (m_last == swapOut->m_backTok) {
        if (swapIn->m_backTok)
            m_last = swapIn->m_backTok;
        else
            m_last = swapOut->m_frontTok ? swapOut->m_frontTok->m_previous : nullptr;
    }

    // clear so this line don't mess with list when it is deleted
    swapOut->m_previousLine = swapOut->m_nextLine = nullptr;

    assert(swapIn->m_nextLine != swapIn);
    assert(swapIn->m_previousLine != swapIn);
}

void Python::TokenList::insertLine(int32_t lineNr, Python::TokenLine *lineToInsert)
{
    Python::TokenLine *lineObj = lineAt(lineNr);
    if (lineObj)
        lineObj = lineObj->m_previousLine; // if nullptr = first line in list
    else if (lineNr > 0 && static_cast<uint>(lineNr) == lineCount())
        lineObj = m_lastLine;
    insertLine(lineObj, lineToInsert);
}

void Python::TokenList::insertLine(Python::TokenLine *previousLine,
                                   Python::TokenLine *insertLine)
{
    assert(insertLine != nullptr);
    assert(insertLine != previousLine);
    assert(insertLine->m_previousLine == nullptr);
    assert(insertLine->m_nextLine == nullptr);
    assert(insertLine->m_ownerList == nullptr && "insertLine contained in another list");

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
    assert(lineToPush->m_previousLine == nullptr);
    assert(lineToPush->m_nextLine == nullptr);
    assert(lineToPush->m_ownerList == nullptr && "lineToPush is contained in another list");
    if (m_lastLine) {
        lineToPush->m_previousLine = m_lastLine;
        lineToPush->m_line = m_lastLine->m_line +1;
        m_lastLine->m_nextLine = lineToPush->instance();
    }

    m_lastLine = lineToPush->instance();
    if (!m_firstLine) {
        m_firstLine = lineToPush->instance();
        lineToPush->m_line = 0;
    }

    lineToPush->m_ownerList = this;
}

void Python::TokenList::removeLine(int32_t lineNr, bool deleteLine)
{
    removeLine(lineAt(lineNr), deleteLine);
}

void Python::TokenList::removeLine(Python::TokenLine *lineToRemove, bool deleteLine)
{
    assert(lineToRemove && "lineToRemove==nulltpr");
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

    lineToRemove->m_nextLine = lineToRemove->m_previousLine = nullptr;

    if (deleteLine)
        delete lineToRemove;
    else
        lineToRemove->m_ownerList = nullptr;
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
        --firstLineToDec->m_line;
        firstLineToDec = firstLineToDec->m_nextLine;
    }
    assert(guard > 0);
}

// ----------------------------------------------------------------------------------------

Python::TokenLine::TokenLine(Python::Token *startTok,
                             const std::string &text) :
    m_ownerList(nullptr), m_frontTok(startTok), m_backTok(startTok),
    m_nextLine(nullptr), m_previousLine(nullptr),
    m_endStateOfLastPara(Token::T_Undetermined),
    m_line(-1), m_tokenScanInfo(nullptr), m_indentCharCount(0),
    m_parenCnt(0), m_bracketCnt(0), m_braceCnt(0), m_blockStateCnt(0),
    m_isParamLine(false), m_isContinuation(false)
{
    // strip newline chars
    size_t trimChrs = 0;

    // should match "\r\n", "\n\r", "\r", "\n"
    for (size_t i = 1; text.length() >= i && i < 3; ++i) {
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
    m_previousLine(other.m_previousLine),
    m_text(other.m_text), m_endStateOfLastPara(other.m_endStateOfLastPara),
    m_line(-1), m_tokenScanInfo(other.m_tokenScanInfo),
    m_indentCharCount(other.m_indentCharCount),
    m_parenCnt(other.m_parenCnt), m_bracketCnt(other.m_bracketCnt),
    m_braceCnt(other.m_braceCnt), m_blockStateCnt(other.m_blockStateCnt),
    m_isParamLine(false), m_isContinuation(other.m_isContinuation)
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

    if (m_ownerList)
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

uint16_t Python::TokenLine::indent() const
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
    assert((tok->m_ownerLine == nullptr || tok->m_ownerLine == instance()) &&
           "tok already added to a Line");
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
    assert((tok->m_ownerLine == nullptr || tok->m_ownerLine == instance()) &&
           "tok already added to a Line");
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
        if (m_tokenScanInfo)
            m_tokenScanInfo->clearParseMessages(tok);
        tok->m_ownerLine = nullptr;
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
        if (m_tokenScanInfo)
            m_tokenScanInfo->clearParseMessages(m_frontTok);
        m_frontTok->m_ownerLine = nullptr;
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
    assert((tok->m_ownerLine == nullptr || tok->m_ownerLine == instance()) &&
           "tok already added to a Line");
    assert(m_ownerList != nullptr && "Line must be inserted to a list to add tokens");
    tok->m_ownerLine = this;

    //int pos = 0;
    Python::Token *prevTok = m_frontTok;
    if (!prevTok && m_previousLine)
        prevTok = m_previousLine->back();

    uint guard = m_ownerList->max_size();
    if (prevTok && *prevTok < *tok) {
        while (prevTok && prevTok->m_next && prevTok->m_next->m_ownerLine == this && (--guard)) {
            prevTok = prevTok->m_next;
        }
    } else {
        while (prevTok && prevTok->m_ownerLine == this && (--guard)) {
            prevTok = prevTok->m_previous;
        }
    }


    /*
    // move up to correct place within line
    while (prevTok && prevTok->m_ownerLine == this &&
           *prevTok < *tok && prevTok->m_next &&
           prevTok->m_next->m_ownerLine == this &&
           (--guard))
    {
        prevTok = prevTok->m_next;
        //++pos;
    }
    if (prevTok && prevTok->ownerLine() == m_nextLine && *m_frontTok > *tok)
        prevTok = m_previousLine->back();
        */

    // insert into list, if prevTok is nullptr its inserted at beginning of list
    m_ownerList->insert(prevTok, tok);
    // our first token in this line
    if (!m_frontTok)
        m_frontTok = m_backTok = tok;
    else if (m_frontTok == tok->m_next)
        m_frontTok = tok;
    if (m_backTok == prevTok)
        m_backTok = tok;
    //return pos;
}

void Python::TokenLine::insert(Python::Token *previousTok, Python::Token *insertTok)
{
    assert(insertTok);
    assert((insertTok->m_ownerLine == instance() || insertTok->m_ownerLine == nullptr) &&
           "tok already added to a Line");
    /*if (!previousTok || !previousTok->m_next ||
        previousTok->m_next->m_ownerLine != instance())
    {
        insert(insertTok);
        return;
    }*/

    insertTok->m_ownerLine = instance();
    m_ownerList->insert(previousTok, insertTok);
    if (!m_frontTok || m_frontTok == insertTok->m_next)
        m_frontTok = insertTok;
    if (!m_backTok || m_backTok == previousTok)
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
    if (tok == m_backTok) {
        if (tok->previous() && tok->m_previous->m_ownerLine == instance())
            m_backTok = m_backTok->m_previous;
        else
            m_frontTok = m_backTok = nullptr;
    }

    if (m_tokenScanInfo)
        m_tokenScanInfo->clearParseMessages(tok);

    tok->m_ownerLine = nullptr;
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

    if (m_tokenScanInfo)
        m_tokenScanInfo->clearParseMessages(tok);

    tok->m_ownerLine = nullptr;

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

// ------------------------------------------------------------------------------------

Python::TokenScanInfo::ParseMsg::ParseMsg(const std::string &message, const Token *tok,
                                          TokenScanInfo::MsgType type) :
    m_message(message), m_token(tok), m_type(type)
{
#ifdef DEBUG_DELETES
    std::clog << "new ParseMsg " << std::hex << this << " " << m_message << std::endl;
#endif
}

Python::TokenScanInfo::ParseMsg::~ParseMsg()
{
#ifdef DEBUG_DELETES
    std::clog << "del ParseMsg " << std::hex << this << " " << m_message << std::endl;
#endif
}

const std::string Python::TokenScanInfo::ParseMsg::msgTypeAsString() const
{
    return msgTypeAsString(m_type);
}

// static
const std::string Python::TokenScanInfo::ParseMsg::msgTypeAsString(MsgType msgType)
{
    switch (msgType) {
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

// static
Python::TokenScanInfo::MsgType
Python::TokenScanInfo::ParseMsg::strToMsgType(const std::string &msgTypeStr)
{
    if (msgTypeStr == "Message")
        return Message;
    if (msgTypeStr == "LookupError")
        return LookupError;
    if (msgTypeStr == "SyntaxError")
        return SyntaxError;
    if (msgTypeStr == "IndentError")
        return IndentError;
    if (msgTypeStr == "Warning")
        return Warning;
    if (msgTypeStr == "Issue")
        return Issue;
    return Invalid;
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

    for (auto it = m_parseMsgs.begin();
         it != m_parseMsgs.end();)
    {
        auto tmpIt = it++;
        delete *tmpIt;
    }
}

void Python::TokenScanInfo::setParseMessage(const Python::Token *tok,
                                            const std::string &msg,
                                            Python::TokenScanInfo::MsgType type)
{
    m_parseMsgs.push_back(new ParseMsg(msg, tok, type));
}

const std::list<const Python::TokenScanInfo::ParseMsg *>
Python::TokenScanInfo::parseMessages(const Python::Token *tok,
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

int Python::TokenScanInfo::clearParseMessages(const Python::Token *tok,
                                              MsgType filterType)
{
    int cnt = 0;
    for (auto it = m_parseMsgs.begin();
         it != m_parseMsgs.end();)
    {
        auto tmpIt = it++;
        if ((*tmpIt)->token() == tok &&
            (filterType == AllMsgTypes || (*tmpIt)->type() == filterType))
        {
            m_parseMsgs.remove(*tmpIt);
            delete *tmpIt;
            ++cnt;
        }
    }

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


