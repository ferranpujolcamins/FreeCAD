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
    case PythonSyntaxHighlighter::T_KeywordAs:           return "T_KeywordAs"; // not sure if needed? we arent exactly building a VM...
    case PythonSyntaxHighlighter::T_KeywordIn:           return "T_KeywordIn"; // likwise
    case PythonSyntaxHighlighter::T_KeywordYield:        return "T_KeywordYield";
    case PythonSyntaxHighlighter::T_KeywordReturn:       return "T_KeywordReturn";
        // leave some room for future keywords

        // operators
    case PythonSyntaxHighlighter::T_Operator:            return "T_Operator";   // **, |, &, ^, ~, +, -, *, /, %, ~, @
    case PythonSyntaxHighlighter::T_OperatorCompare:     return "T_OperatorCompare";   // ==, !=, <=, >=, <, >,
    case PythonSyntaxHighlighter::T_OperatorAssign:      return "T_OperatorAssign";   // =, +=, *=, -=, /=, @= introduced in py 3.5

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

        // identifiers
    case PythonSyntaxHighlighter::T_IdentifierUnknown:     return "T_IdentifierUnknown"; // name is not identified at this point
    case PythonSyntaxHighlighter::T_IdentifierDefined:     return "T_IdentifierDefined"; // variable is in current context
    case PythonSyntaxHighlighter::T_IdentifierModule:      return "T_IdentifierModule"; // its a module definition
    case PythonSyntaxHighlighter::T_IdentifierFunction:    return "T_IdentifierFunction"; // its a function definition
    case PythonSyntaxHighlighter::T_IdentifierMethod:      return "T_IdentifierMethod"; // its a method definition
    case PythonSyntaxHighlighter::T_IdentifierClass:       return "T_IdentifierClass"; // its a class definition
    case PythonSyntaxHighlighter::T_IdentifierSuperMethod: return "T_IdentifierSuperMethod"; // its a method with name: __**__
    case PythonSyntaxHighlighter::T_IdentifierBuiltin:     return "T_IdentifierBuiltin"; // its builtin
    case PythonSyntaxHighlighter::T_IdentifierDecorator:   return "T_IdentifierDecorator"; // member decorator like: @property
    case PythonSyntaxHighlighter::T_IdentifierDefUnknown:  return "T_IdentifierDefUnknown"; // before system has determined if its a
        // method or function yet

        // metadata such def funcname(arg: "documentaion") -> "returntype documentation":
    case PythonSyntaxHighlighter::T_MetaData:              return "T_MetaData";


    case PythonSyntaxHighlighter::T_Invalid:               return "T_Invalid";
    case PythonSyntaxHighlighter::T_PythonConsoleOutput:   return "T_PythonConsoleOutput";
    case PythonSyntaxHighlighter::T_PythonConsoleError:    return "T_PythonConsoleError";
    default:
        return "Unknown";
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


