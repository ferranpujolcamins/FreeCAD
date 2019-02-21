/***************************************************************************
 *   Copyright (c) 2019 Fredrik Johansson github.com/mumme74               *
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


#include "PreCompiled.h"

#include "PythonCodeDebugTools.h"
#include <QTextBlock>
#include <QTextBlockUserData>

using namespace Gui;
using namespace Syntax;

const char *Gui::Syntax::tokenToCStr(Gui::PythonSyntaxHighlighter::Tokens tok)
{
    switch(tok) {
    case PythonSyntaxHighlighter::T_Undetermined:        return "T_Undetermined";     // Parser looks tries to figure out next char also Standard text
        // python
    case PythonSyntaxHighlighter::T_Indent:              return "T_Indent";
    case PythonSyntaxHighlighter::T_Comment:             return "T_Comment";     // Comment begins with #
    case PythonSyntaxHighlighter::T_SyntaxError:         return "T_SyntaxError";
    case PythonSyntaxHighlighter::T_IndentError:         return "T_IndentError";  // to signify that we have a indent error, set by PythonSourceRoot

        // numbers
    case PythonSyntaxHighlighter::T_NumberDecimal:       return "T_NumberDecimal";     // Normal number
    case PythonSyntaxHighlighter::T_NumberHex:           return "T_NumberHex";
    case PythonSyntaxHighlighter::T_NumberBinary:        return "T_NumberBinary";
    case PythonSyntaxHighlighter::T_NumberOctal:         return "T_NumberOctal";    // starting with 0 ie 011 = 9, different color to let it stand out
    case PythonSyntaxHighlighter::T_NumberFloat:         return "T_NumberFloat";

        // strings
    case PythonSyntaxHighlighter::T_LiteralDblQuote:     return "T_LiteralDblQote";     // String literal beginning with "
    case PythonSyntaxHighlighter::T_LiteralSglQuote:     return "T_LiteralSglQuote";     // Other string literal beginning with '
    case PythonSyntaxHighlighter::T_LiteralBlockDblQuote: return "T_LiteralBlockDblQuote";     // Block comments beginning and ending with """
    case PythonSyntaxHighlighter::T_LiteralBlockSglQuote: return "T_LiteralBlockSglQuote";     // Other block comments beginning and ending with '''

        // Keywords
    case PythonSyntaxHighlighter::T_Keyword:             return "T_Keyword";
    case PythonSyntaxHighlighter::T_KeywordClass:        return "T_KeywordClass";
    case PythonSyntaxHighlighter::T_KeywordDef:          return "T_KeywordDef";
    case PythonSyntaxHighlighter::T_KeywordImport:       return "T_KeywordImport";
    case PythonSyntaxHighlighter::T_KeywordFrom:         return "T_KeywordFrom";
    case PythonSyntaxHighlighter::T_KeywordAs:           return "T_KeywordAs";
    case PythonSyntaxHighlighter::T_KeywordYield:        return "T_KeywordYield";
    case PythonSyntaxHighlighter::T_KeywordReturn:       return "T_KeywordReturn";
        // leave some room for future keywords

        // operators
       // arithmetic
    case PythonSyntaxHighlighter::T_OperatorPlus:          return     "T_OperatorPlus";           // +,
    case PythonSyntaxHighlighter::T_OperatorMinus:         return     "T_OperatorMinus";          // -,
    case PythonSyntaxHighlighter::T_OperatorMul:           return     "T_OperatorMul";            // *,
    case PythonSyntaxHighlighter::T_OperatorExponential:   return     "T_OperatorExponential";    // **,
    case PythonSyntaxHighlighter::T_OperatorDiv:           return     "T_OperatorDiv";            // /,
    case PythonSyntaxHighlighter::T_OperatorFloorDiv:      return     "T_OperatorFloorDiv";       // //,
    case PythonSyntaxHighlighter::T_OperatorModulo:        return     "T_OperatorModulo";         // %,
    case PythonSyntaxHighlighter::T_OperatorMatrixMul:     return     "T_OperatorMatrixMul";      // @
        //bitwise

    case PythonSyntaxHighlighter::T_OperatorBitShiftLeft:  return     "T_OperatorBitShiftLeft";   // <<,
    case PythonSyntaxHighlighter::T_OperatorBitShiftRight: return     "T_OperatorBitShiftRight";  // >>,
    case PythonSyntaxHighlighter::T_OperatorBitAnd:        return     "T_OperatorBitAnd";         // &,
    case PythonSyntaxHighlighter::T_OperatorBitOr:         return     "T_OperatorBitOr";          // |,
    case PythonSyntaxHighlighter::T_OperatorBitXor:        return     "T_OperatorBitXor";         // ^,
    case PythonSyntaxHighlighter::T_OperatorBitNot:        return     "T_OperatorBitNot";         // ~,

        // assigment
    case PythonSyntaxHighlighter::T_OperatorEqual:         return     "T_OperatorEqual";          // =,
    case PythonSyntaxHighlighter::T_OperatorPlusEqual:     return     "T_OperatorPlusEqual";      // +=,
    case PythonSyntaxHighlighter::T_OperatorMinusEqual:    return     "T_OperatorMinusEqual";     // -=,
    case PythonSyntaxHighlighter::T_OperatorMulEqual:      return     "T_OperatorMulEqual";       // *=,
    case PythonSyntaxHighlighter::T_OperatorDivEqual:      return     "T_OperatorDivEqual";       // /=,
    case PythonSyntaxHighlighter::T_OperatorModuloEqual:   return     "T_OperatorModuloEqual";    // %=,
    case PythonSyntaxHighlighter::T_OperatorFloorDivEqual: return     "T_OperatorFloorDivEqual";  // //=,
    case PythonSyntaxHighlighter::T_OperatorExpoEqual:     return     "T_OperatorExpoEqual";      // **=,
    case PythonSyntaxHighlighter::T_OperatorMatrixMulEqual:return     "T_OperatorMatrixMulEqual"; // @= introduced in py 3.5

        // assigment bitwise
    case PythonSyntaxHighlighter::T_OperatorBitAndEqual:   return     "T_OperatorBitAndEqual";    // &=,
    case PythonSyntaxHighlighter::T_OperatorBitOrEqual:    return     "T_OperatorBitOrEqual";     // |=,
    case PythonSyntaxHighlighter::T_OperatorBitXorEqual:   return     "T_OperatorBitXorEqual";    // ^=,
    case PythonSyntaxHighlighter::T_OperatorBitNotEqual:   return     "T_OperatorBitNotEqual";    // ~=,
    case PythonSyntaxHighlighter::T_OperatorBitShiftRightEqual:return "T_OperatorBitShiftRightEqual"; // >>=,
    case PythonSyntaxHighlighter::T_OperatorBitShiftLeftEqual: return "T_OperatorBitShiftLeftEqual";  // <<=,

        // compare
    case PythonSyntaxHighlighter::T_OperatorCompareEqual:  return     "T_OperatorCompareEqual";   // ==,
    case PythonSyntaxHighlighter::T_OperatorNotEqual:      return     "T_OperatorNotEqual";       // !=,
    case PythonSyntaxHighlighter::T_OperatorLessEqual:     return     "T_OperatorLessEqual";      // <=,
    case PythonSyntaxHighlighter::T_OperatorMoreEqual:     return     "T_OperatorMoreEqual";      // >=,
    case PythonSyntaxHighlighter::T_OperatorLess:          return     "T_OperatorLess";           // <,
    case PythonSyntaxHighlighter::T_OperatorMore:          return     "T_OperatorMore";           // >,
    case PythonSyntaxHighlighter::T_OperatorAnd:           return     "T_OperatorAnd";            // 'and',
    case PythonSyntaxHighlighter::T_OperatorOr:            return     "T_OperatorOr";             // 'or',
    case PythonSyntaxHighlighter::T_OperatorNot:           return     "T_OperatorNot";            // 'not',
    case PythonSyntaxHighlighter::T_OperatorIs:            return     "T_OperatorIs";             // 'is',
    case PythonSyntaxHighlighter::T_OperatorIn:            return     "T_OperatorIn";             // 'in',

        // special
    case PythonSyntaxHighlighter::T_OperatorVariableParam: return     "T_OperatorVariableParam";  // * for function parameters ie (*arg1)
    case PythonSyntaxHighlighter::T_OperatorKeyWordParam:  return     "T_OperatorKeyWordParam";   // ** for function parameters ir (**arg1)

        // delimiters
    case PythonSyntaxHighlighter::T_Delimiter:             return "T_Delimiter";   // all other non specified
    case PythonSyntaxHighlighter::T_DelimiterOpenParen:    return "T_DelimiterOpenParen";   // (
    case PythonSyntaxHighlighter::T_DelimiterCloseParen:   return "T_DelimiterCloseParen";   // )
    case PythonSyntaxHighlighter::T_DelimiterOpenBracket:  return "T_DelimiterOpenBracket";   // [
    case PythonSyntaxHighlighter::T_DelimiterCloseBracket: return "T_DelimiterCloseBracket";   // ]
    case PythonSyntaxHighlighter::T_DelimiterOpenBrace:    return "T_DelimiterOpenBrace";   // {
    case PythonSyntaxHighlighter::T_DelimiterCloseBrace:   return "T_DelimiterCloseBrace";   // }
    case PythonSyntaxHighlighter::T_DelimiterPeriod:       return "T_DelimiterPeriod";   // .
    case PythonSyntaxHighlighter::T_DelimiterComma:        return "T_DelimiterComma";   // ,
    case PythonSyntaxHighlighter::T_DelimiterColon:        return "T_DelimiterColon";   // :
    case PythonSyntaxHighlighter::T_DelimiterSemiColon:    return "T_DelimiterSemiColon";   // ;
        // metadata such def funcname(arg: "documentation") ->
        //                            "returntype documentation":
    case PythonSyntaxHighlighter::T_DelimiterMetaData:     return "T_DelimiterMetaData";   // -> might also be ':' inside arguments
    case PythonSyntaxHighlighter::T_DelimiterLineContinue: return "T_DelimiterLineContinue";   // when end of line is escaped like so '\'
    case PythonSyntaxHighlighter::T_DelimiterNewLine:      return "T_DelimiterNewLine";   // each new line
        // identifiers
    case PythonSyntaxHighlighter::T_IdentifierUnknown:     return "T_IdentifierUnknown"; // name is not identified at this point
    case PythonSyntaxHighlighter::T_IdentifierDefined:     return "T_IdentifierDefined"; // variable is in current context
    case PythonSyntaxHighlighter::T_IdentifierModule:      return "T_IdentifierModule"; // its a module definition
    case PythonSyntaxHighlighter::T_IdentifierModuleGlob:  return "T_IdentifierModuleGlob"; // from mod import * <- glob
    case PythonSyntaxHighlighter::T_IdentifierFunction:    return "T_IdentifierFunction"; // its a function definition
    case PythonSyntaxHighlighter::T_IdentifierMethod:      return "T_IdentifierMethod"; // its a method definition
    case PythonSyntaxHighlighter::T_IdentifierClass:       return "T_IdentifierClass"; // its a class definition
    case PythonSyntaxHighlighter::T_IdentifierSuperMethod: return "T_IdentifierSuperMethod"; // its a method with name: __**__
    case PythonSyntaxHighlighter::T_IdentifierBuiltin:     return "T_IdentifierBuiltin"; // its builtin
    case PythonSyntaxHighlighter::T_IdentifierDecorator:   return "T_IdentifierDecorator"; // member decorator like: @property
    case PythonSyntaxHighlighter::T_IdentifierDefUnknown:  return "T_IdentifierDefUnknown"; // before system has determined if its a
                                                                                            // method or function yet
    case PythonSyntaxHighlighter::T_IdentifierSelf:        return "T_IdentifierSelf";

        // metadata such def funcname(arg: "documentaion") -> "returntype documentation":
    case PythonSyntaxHighlighter::T_MetaData:              return "T_MetaData";


    case PythonSyntaxHighlighter::T_Invalid:               return "T_Invalid";
    case PythonSyntaxHighlighter::T_PythonConsoleOutput:   return "T_PythonConsoleOutput";
    case PythonSyntaxHighlighter::T_PythonConsoleError:    return "T_PythonConsoleError";
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

DumpSyntaxTokens::DumpSyntaxTokens(QTextBlock firstBlock, FILE *out):
    m_file(out)
{
    QTextBlock txtBlock = firstBlock;
    while(txtBlock.isValid()) {
        // first print the complete srctext as is
        fprintf(m_file, "%s¶\n", txtBlock.text().toLatin1().data());
        PythonTextBlockData *txtData = reinterpret_cast<PythonTextBlockData*>(txtBlock.userData());
        if (txtData == nullptr)
            break;
        for (PythonToken *tok : txtData->tokens()) {
            fprintf(m_file, " %s", tokenToCStr(tok->token));
        }
        fprintf(m_file, "¶\n");
        txtBlock = txtBlock.next();
    }
}

DumpSyntaxTokens::~DumpSyntaxTokens()
{
}


