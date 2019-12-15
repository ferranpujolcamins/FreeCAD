#include "PythonSourceDebugTools.h"

#include <TextEditor/PythonCodeDebugTools.h>

#include "PythonSource.h"
#include "PythonSourceRoot.h"
#include "PythonSourceModule.h"
#include "PythonSourceFrames.h"

DBG_TOKEN_FILE

using namespace Gui;
using namespace Python;

#include <time.h>
#include <cstdint>

// for DBG_TOKEN_FILE
char TOKEN_TEXT_BUF[350];
char TOKEN_NAME_BUF[50];
char TOKEN_INFO_BUF[40];
char TOKEN_SRC_LINE_BUF[350];


const char *Gui::Python::tokenToCStr(Token::Type tok)
{
    switch(tok) {
    case Token::T_Undetermined:        return "T_Undetermined";     // Parser looks tries to figure out next char also Standard text
        // python
    case Token::T_Indent:              return "T_Indent";
    case Token::T_Dedent:              return "T_Dedent";
    case Token::T_Comment:             return "T_Comment";     // Comment begins with #
    case Token::T_SyntaxError:         return "T_SyntaxError";
    case Token::T_IndentError:         return "T_IndentError";  // to signify that we have a indent error, set by PythonSourceRoot

        // numbers
    case Token::T_NumberDecimal:       return "T_NumberDecimal";     // Normal number
    case Token::T_NumberHex:           return "T_NumberHex";
    case Token::T_NumberBinary:        return "T_NumberBinary";
    case Token::T_NumberOctal:         return "T_NumberOctal";    // starting with 0 ie 011 = 9, different color to let it stand out
    case Token::T_NumberFloat:         return "T_NumberFloat";

        // strings
    case Token::T_LiteralDblQuote:     return "T_LiteralDblQote";     // String literal beginning with "
    case Token::T_LiteralSglQuote:     return "T_LiteralSglQuote";     // Other string literal beginning with '
    case Token::T_LiteralBlockDblQuote: return "T_LiteralBlockDblQuote";     // Block comments beginning and ending with """
    case Token::T_LiteralBlockSglQuote: return "T_LiteralBlockSglQuote";     // Other block comments beginning and ending with '''

        // Keywords
    case Token::T_Keyword:             return "T_Keyword";
    case Token::T_KeywordClass:        return "T_KeywordClass";
    case Token::T_KeywordDef:          return "T_KeywordDef";
    case Token::T_KeywordImport:       return "T_KeywordImport";
    case Token::T_KeywordFrom:         return "T_KeywordFrom";
    case Token::T_KeywordAs:           return "T_KeywordAs";
    case Token::T_KeywordYield:        return "T_KeywordYield";
    case Token::T_KeywordReturn:       return "T_KeywordReturn";
    case Token::T_KeywordIf:           return "T_KeywordIf";
    case Token::T_KeywordElIf:         return "T_KeywordElIf";
    case Token::T_KeywordElse:         return "T_KeywordElse";
    case Token::T_KeywordFor:          return "T_KeywordFor";
    case Token::T_KeywordWhile:        return "T_KeywordWhile";
    case Token::T_KeywordBreak:        return "T_KeywordBreak";
    case Token::T_KeywordContinue:     return "T_KeywordContinue";
    case Token::T_KeywordTry:          return "T_KeywordTry";
    case Token::T_KeywordExcept:       return "T_KeywordExcept";
    case Token::T_KeywordFinally:      return "T_KeywordFinally";
        // leave some room for future keywords

        // operators
       // arithmetic
    case Token::T_OperatorPlus:          return     "T_OperatorPlus";           // +,
    case Token::T_OperatorMinus:         return     "T_OperatorMinus";          // -,
    case Token::T_OperatorMul:           return     "T_OperatorMul";            // *,
    case Token::T_OperatorExponential:   return     "T_OperatorExponential";    // **,
    case Token::T_OperatorDiv:           return     "T_OperatorDiv";            // /,
    case Token::T_OperatorFloorDiv:      return     "T_OperatorFloorDiv";       // //,
    case Token::T_OperatorModulo:        return     "T_OperatorModulo";         // %,
    case Token::T_OperatorMatrixMul:     return     "T_OperatorMatrixMul";      // @
        //bitwise

    case Token::T_OperatorBitShiftLeft:  return     "T_OperatorBitShiftLeft";   // <<,
    case Token::T_OperatorBitShiftRight: return     "T_OperatorBitShiftRight";  // >>,
    case Token::T_OperatorBitAnd:        return     "T_OperatorBitAnd";         // &,
    case Token::T_OperatorBitOr:         return     "T_OperatorBitOr";          // |,
    case Token::T_OperatorBitXor:        return     "T_OperatorBitXor";         // ^,
    case Token::T_OperatorBitNot:        return     "T_OperatorBitNot";         // ~,

        // assigment
    case Token::T_OperatorEqual:         return     "T_OperatorEqual";          // =,
    case Token::T_OperatorPlusEqual:     return     "T_OperatorPlusEqual";      // +=,
    case Token::T_OperatorMinusEqual:    return     "T_OperatorMinusEqual";     // -=,
    case Token::T_OperatorMulEqual:      return     "T_OperatorMulEqual";       // *=,
    case Token::T_OperatorDivEqual:      return     "T_OperatorDivEqual";       // /=,
    case Token::T_OperatorModuloEqual:   return     "T_OperatorModuloEqual";    // %=,
    case Token::T_OperatorFloorDivEqual: return     "T_OperatorFloorDivEqual";  // //=,
    case Token::T_OperatorExpoEqual:     return     "T_OperatorExpoEqual";      // **=,
    case Token::T_OperatorMatrixMulEqual:return     "T_OperatorMatrixMulEqual"; // @= introduced in py 3.5

        // assigment bitwise
    case Token::T_OperatorBitAndEqual:   return     "T_OperatorBitAndEqual";    // &=,
    case Token::T_OperatorBitOrEqual:    return     "T_OperatorBitOrEqual";     // |=,
    case Token::T_OperatorBitXorEqual:   return     "T_OperatorBitXorEqual";    // ^=,
    case Token::T_OperatorBitNotEqual:   return     "T_OperatorBitNotEqual";    // ~=,
    case Token::T_OperatorBitShiftRightEqual:return "T_OperatorBitShiftRightEqual"; // >>=,
    case Token::T_OperatorBitShiftLeftEqual: return "T_OperatorBitShiftLeftEqual";  // <<=,

        // compare
    case Token::T_OperatorCompareEqual:  return     "T_OperatorCompareEqual";   // ==,
    case Token::T_OperatorNotEqual:      return     "T_OperatorNotEqual";       // !=,
    case Token::T_OperatorLessEqual:     return     "T_OperatorLessEqual";      // <=,
    case Token::T_OperatorMoreEqual:     return     "T_OperatorMoreEqual";      // >=,
    case Token::T_OperatorLess:          return     "T_OperatorLess";           // <,
    case Token::T_OperatorMore:          return     "T_OperatorMore";           // >,
    case Token::T_OperatorAnd:           return     "T_OperatorAnd";            // 'and',
    case Token::T_OperatorOr:            return     "T_OperatorOr";             // 'or',
    case Token::T_OperatorNot:           return     "T_OperatorNot";            // 'not',
    case Token::T_OperatorIs:            return     "T_OperatorIs";             // 'is',
    case Token::T_OperatorIn:            return     "T_OperatorIn";             // 'in',

        // special
    case Token::T_OperatorVariableParam: return     "T_OperatorVariableParam";  // * for function parameters ie (*arg1)
    case Token::T_OperatorKeyWordParam:  return     "T_OperatorKeyWordParam";   // ** for function parameters ir (**arg1)

        // delimiters
    case Token::T_Delimiter:             return "T_Delimiter";   // all other non specified
    case Token::T_DelimiterOpenParen:    return "T_DelimiterOpenParen";   // (
    case Token::T_DelimiterCloseParen:   return "T_DelimiterCloseParen";   // )
    case Token::T_DelimiterOpenBracket:  return "T_DelimiterOpenBracket";   // [
    case Token::T_DelimiterCloseBracket: return "T_DelimiterCloseBracket";   // ]
    case Token::T_DelimiterOpenBrace:    return "T_DelimiterOpenBrace";   // {
    case Token::T_DelimiterCloseBrace:   return "T_DelimiterCloseBrace";   // }
    case Token::T_DelimiterPeriod:       return "T_DelimiterPeriod";   // .
    case Token::T_DelimiterComma:        return "T_DelimiterComma";   // ,
    case Token::T_DelimiterColon:        return "T_DelimiterColon";   // :
    case Token::T_DelimiterSemiColon:    return "T_DelimiterSemiColon";   // ;
    case Token::T_DelimiterEllipsis:     return "T_DelimiterEllipsis";    // ...
        // metadata such def funcname(arg: "documentation") ->
        //                            "returntype documentation":
    case Token::T_DelimiterMetaData:     return "T_DelimiterMetaData";   // -> might also be ':' inside arguments
    case Token::T_DelimiterBackSlash:    return "T_DelimiterBackSlash";   // when end of line is escaped like so '\'
    case Token::T_DelimiterNewLine:      return "T_DelimiterNewLine";   // each new line
        // identifiers
    case Token::T_IdentifierUnknown:     return "T_IdentifierUnknown"; // name is not identified at this point
    case Token::T_IdentifierDefined:     return "T_IdentifierDefined"; // variable is in current context
    case Token::T_IdentifierModule:      return "T_IdentifierModule"; // its a module definition
    case Token::T_IdentifierModuleAlias: return "T_IdentifierModuleAlias"; // alias for import. ie: import Something as Alias
    case Token::T_IdentifierModulePackage: return "T_IdentifierModulePackage"; // identifier is a package, ie: root for other modules
    case Token::T_IdentifierModuleGlob:  return "T_IdentifierModuleGlob"; // from mod import * <- glob
    case Token::T_IdentifierFunction:    return "T_IdentifierFunction"; // its a function definition
    case Token::T_IdentifierMethod:      return "T_IdentifierMethod"; // its a method definition
    case Token::T_IdentifierClass:       return "T_IdentifierClass"; // its a class definition
    case Token::T_IdentifierSuperMethod: return "T_IdentifierSuperMethod"; // its a method with name: __**__
    case Token::T_IdentifierBuiltin:     return "T_IdentifierBuiltin"; // its builtin
    case Token::T_IdentifierDecorator:   return "T_IdentifierDecorator"; // member decorator like: @property
    case Token::T_IdentifierDefUnknown:  return "T_IdentifierDefUnknown"; // before system has determined if its a
                                                                                            // method or function yet
    case Token::T_IdentifierNone:        return "T_IdentifierNone";  // The None keyword
    case Token::T_IdentifierTrue:        return "T_IdentifierTrue";  // The bool True
    case Token::T_IdentifierFalse:       return "T_IdentifierFalse"; // The bool False
    case Token::T_IdentifierSelf:        return "T_IdentifierSelf";
    case Token::T_IdentifierInvalid:     return "T_IdentifierInvalid";

        // metadata such def funcname(arg: "documentaion") -> "returntype documentation":
    case Token::T_MetaData:              return "T_MetaData";
        // these are inserted by PythonSourceRoot
    case Token::T_BlockStart:            return "T_BlockStart"; // indicate a new block ie if (a is True):
                                                                                  //                                       ^
    case Token::T_BlockEnd:              return "T_BlockEnd";   // indicate block end ie:    dosomething
                                                                                  //                        dosomethingElse
                                                                                  //                       ^


    case Token::T_Invalid:               return "T_Invalid";
    case Token::T_PythonConsoleOutput:   return "T_PythonConsoleOutput";
    case Token::T_PythonConsoleError:    return "T_PythonConsoleError";
    default:
        static char str[50];
        sprintf(str, "Unknown(%d)", (int)tok);
        char *closeParenCh = strchr(str, ')');
        closeParenCh++;
        closeParenCh = 0; // terminate str
        return str;
    }
}

// ---------------------------------------------------------------------------------------

DumpToFileBaseClass::DumpToFileBaseClass(const char *outfile)
{
    if (strcmp(outfile, "stdout") == 0)
        m_file = stdout;
    else if (strcmp(outfile, "stderr"))
        m_file = stderr;
    else {
        FileInfo fi(outfile);
        m_file = fopen(fi.absolutePath().c_str(), "w");
    }
}

DumpToFileBaseClass::DumpToFileBaseClass(FILE *fp) :
    m_file(fp)
{
}

DumpToFileBaseClass::~DumpToFileBaseClass()
{
    if (m_file && m_file != stdout && m_file != stderr)
        fclose(m_file);
}

const char *DumpToFileBaseClass::datetimeStr()
{
    static char dateStr[30];
    struct tm tmNow;
    time_t now = time(nullptr);
    tmNow = *(localtime(&now));
    strftime(dateStr, 30, "%Y-%m-%d %H:%M:%S", &tmNow);
    return dateStr;
}

// ---------------------------------------------------------------------------------------

DumpSyntaxTokens::DumpSyntaxTokens(TokenLine *firstLine, const char *outfile):
    DumpToFileBaseClass(outfile)
{
    fprintf(m_file, "dump tokens at %s\n", datetimeStr());

    TokenLine *line = firstLine;
    while(line) {
        // first print the complete srctext as is
        fprintf(m_file, "%s¶\n", line->text().c_str());
        for (Python::Token *tok : line->tokens()) {
            fprintf(m_file, " %s", tokenToCStr(tok->type()));
        }
        fprintf(m_file, "¶\n");
        line = line->nextLine();
    }
}

DumpSyntaxTokens::~DumpSyntaxTokens()
{
}

// ----------------------------------------------------------------------------------------


DumpModule::DumpModule(SourceModule *module, const char *outfile) :
    DumpToFileBaseClass(outfile),
    m_module(module)
{
    create();
}

DumpModule::DumpModule(SourceModule *module, FILE *fp) :
    DumpToFileBaseClass(fp),
    m_module(module)
{
    create();
}

void DumpModule::create()
{
    if (m_module) {
        fprintf(m_file, "dump for module %s at %s\n", m_module->moduleName().c_str(), datetimeStr());

        const SourceFrame *rootFrm =  m_module->rootFrame();
        fprintf(m_file, "Startframe->isModule:%d\n", rootFrm->isModule());

        dumpFrame(rootFrm, 0);
    }
}

DumpModule::~DumpModule()
{
}

void DumpModule::dumpFrame(const SourceFrame *frm, int indent)
{
    DEFINE_DBG_VARS

    // build up indent string
    char indentStr[200];
    int i = 0;
    for (i = 0; i < indent; ++i)
        indentStr[i] = ' ';
    indentStr[i] = 0;

    if (!frm->token()) {
        fprintf(m_file, "%s ***** tried a new frame but it hadnt any startToken, bailing....\n",  indentStr);
        return;
    }
    DBG_TOKEN(frm->token())


    fprintf(m_file, "%s---------------------------------------------------\n", indentStr);
    fprintf(m_file, "%sStarting new frame, startToken:%s(line:%d), endToken:%s(line:%d)\n", indentStr,
            Python::tokenToCStr(frm->token()->type()), frm->token()->line() +1,
            frm->lastToken() ? Python::tokenToCStr(frm->lastToken()->type()) : "-1",
            frm->lastToken() ? frm->lastToken()->line() +1 : -1);

    DBG_TOKEN(frm->lastToken())

    fprintf(m_file, "%sFrame name:%s isClass:%d\n", indentStr, frm->name().c_str(),
                                                    frm->isClass());
    fprintf(m_file, "%ssubframes count:%x\n", indentStr, frm->size());

    // parameters
    fprintf(m_file, "%sFrame parameters:\n", indentStr);
    if (!frm->isModule()) {
        for (auto itm = frm->parameters().begin();
             itm != frm->parameters().end();
             ++itm)
        {
            const char *paramType = nullptr;
            switch((*itm)->parameterType()){
            case SourceParameter::Positional:
                paramType = "Positional"; break;
            case SourceParameter::PositionalDefault:
                paramType = "PositionalDefault"; break;
            case SourceParameter::Keyword:
                paramType = "KeyWord"; break;
            case SourceParameter::Variable:
                paramType = "Variable"; break;
            case SourceParameter::InValid:
                paramType = "InValid"; break;
            default:
                paramType = "Unknown"; break;
            }
            SourceIdentifierAssignment *assign = (*itm)->identifierAssignment();
            const std::string txt = assign ? assign->text() : std::string();
            fprintf(m_file, "%s %s(%s)", indentStr, txt.c_str(), paramType);

            const std::string  typeInfo = assign ? assign->typeInfo().typeAsStr() : std::string();
            fprintf(m_file, " : %s\n", typeInfo.c_str());
        }
    }

    // iterate through subFrames, recursive
    for(SourceFrame *subFrm = dynamic_cast<SourceFrame*>(frm->begin());
        subFrm != frm->end();
        subFrm = dynamic_cast<SourceFrame*>(subFrm->next()))
    {
        dumpFrame(subFrm, indent + 4);
    }


    // finished print newline
    fprintf(m_file, "\n");
}
