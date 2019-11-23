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
#include "PythonCode.h"
#include <PythonSource/PythonSource.h>
#include <PythonSource/PythonSourceRoot.h>
#include "PythonSource/PythonSourceDebugTools.h"
#include "PythonSyntaxHighlighter.h"
#include "PythonEditor.h"
#include <QTextBlock>
#include <QTextBlockUserData>
#include <QFileInfo>
#include <QTreeView>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <Gui/TextEditor/TextEditor.h>
#include <time.h>
#include <cstdint>

// for DBG_TOKEN_FILE
char TOKEN_TEXT_BUF[350];
char TOKEN_NAME_BUF[50];
char TOKEN_INFO_BUF[40];
char TOKEN_SRC_LINE_BUF[350];

using namespace Gui;
using namespace Syntax;

const char *Gui::Syntax::tokenToCStr(Python::Token::Type tok)
{
    switch(tok) {
    case Python::Token::T_Undetermined:        return "T_Undetermined";     // Parser looks tries to figure out next char also Standard text
        // python
    case Python::Token::T_Indent:              return "T_Indent";
    case Python::Token::T_Comment:             return "T_Comment";     // Comment begins with #
    case Python::Token::T_SyntaxError:         return "T_SyntaxError";
    case Python::Token::T_IndentError:         return "T_IndentError";  // to signify that we have a indent error, set by PythonSourceRoot

        // numbers
    case Python::Token::T_NumberDecimal:       return "T_NumberDecimal";     // Normal number
    case Python::Token::T_NumberHex:           return "T_NumberHex";
    case Python::Token::T_NumberBinary:        return "T_NumberBinary";
    case Python::Token::T_NumberOctal:         return "T_NumberOctal";    // starting with 0 ie 011 = 9, different color to let it stand out
    case Python::Token::T_NumberFloat:         return "T_NumberFloat";

        // strings
    case Python::Token::T_LiteralDblQuote:     return "T_LiteralDblQote";     // String literal beginning with "
    case Python::Token::T_LiteralSglQuote:     return "T_LiteralSglQuote";     // Other string literal beginning with '
    case Python::Token::T_LiteralBlockDblQuote: return "T_LiteralBlockDblQuote";     // Block comments beginning and ending with """
    case Python::Token::T_LiteralBlockSglQuote: return "T_LiteralBlockSglQuote";     // Other block comments beginning and ending with '''

        // Keywords
    case Python::Token::T_Keyword:             return "T_Keyword";
    case Python::Token::T_KeywordClass:        return "T_KeywordClass";
    case Python::Token::T_KeywordDef:          return "T_KeywordDef";
    case Python::Token::T_KeywordImport:       return "T_KeywordImport";
    case Python::Token::T_KeywordFrom:         return "T_KeywordFrom";
    case Python::Token::T_KeywordAs:           return "T_KeywordAs";
    case Python::Token::T_KeywordYield:        return "T_KeywordYield";
    case Python::Token::T_KeywordReturn:       return "T_KeywordReturn";
    case Python::Token::T_KeywordIf:           return "T_KeywordIf";
    case Python::Token::T_KeywordElIf:         return "T_KeywordElIf";
    case Python::Token::T_KeywordElse:         return "T_KeywordElse";
    case Python::Token::T_KeywordFor:          return "T_KeywordFor";
    case Python::Token::T_KeywordWhile:        return "T_KeywordWhile";
    case Python::Token::T_KeywordBreak:        return "T_KeywordBreak";
    case Python::Token::T_KeywordContinue:     return "T_KeywordContinue";
        // leave some room for future keywords

        // operators
       // arithmetic
    case Python::Token::T_OperatorPlus:          return     "T_OperatorPlus";           // +,
    case Python::Token::T_OperatorMinus:         return     "T_OperatorMinus";          // -,
    case Python::Token::T_OperatorMul:           return     "T_OperatorMul";            // *,
    case Python::Token::T_OperatorExponential:   return     "T_OperatorExponential";    // **,
    case Python::Token::T_OperatorDiv:           return     "T_OperatorDiv";            // /,
    case Python::Token::T_OperatorFloorDiv:      return     "T_OperatorFloorDiv";       // //,
    case Python::Token::T_OperatorModulo:        return     "T_OperatorModulo";         // %,
    case Python::Token::T_OperatorMatrixMul:     return     "T_OperatorMatrixMul";      // @
        //bitwise

    case Python::Token::T_OperatorBitShiftLeft:  return     "T_OperatorBitShiftLeft";   // <<,
    case Python::Token::T_OperatorBitShiftRight: return     "T_OperatorBitShiftRight";  // >>,
    case Python::Token::T_OperatorBitAnd:        return     "T_OperatorBitAnd";         // &,
    case Python::Token::T_OperatorBitOr:         return     "T_OperatorBitOr";          // |,
    case Python::Token::T_OperatorBitXor:        return     "T_OperatorBitXor";         // ^,
    case Python::Token::T_OperatorBitNot:        return     "T_OperatorBitNot";         // ~,

        // assigment
    case Python::Token::T_OperatorEqual:         return     "T_OperatorEqual";          // =,
    case Python::Token::T_OperatorPlusEqual:     return     "T_OperatorPlusEqual";      // +=,
    case Python::Token::T_OperatorMinusEqual:    return     "T_OperatorMinusEqual";     // -=,
    case Python::Token::T_OperatorMulEqual:      return     "T_OperatorMulEqual";       // *=,
    case Python::Token::T_OperatorDivEqual:      return     "T_OperatorDivEqual";       // /=,
    case Python::Token::T_OperatorModuloEqual:   return     "T_OperatorModuloEqual";    // %=,
    case Python::Token::T_OperatorFloorDivEqual: return     "T_OperatorFloorDivEqual";  // //=,
    case Python::Token::T_OperatorExpoEqual:     return     "T_OperatorExpoEqual";      // **=,
    case Python::Token::T_OperatorMatrixMulEqual:return     "T_OperatorMatrixMulEqual"; // @= introduced in py 3.5

        // assigment bitwise
    case Python::Token::T_OperatorBitAndEqual:   return     "T_OperatorBitAndEqual";    // &=,
    case Python::Token::T_OperatorBitOrEqual:    return     "T_OperatorBitOrEqual";     // |=,
    case Python::Token::T_OperatorBitXorEqual:   return     "T_OperatorBitXorEqual";    // ^=,
    case Python::Token::T_OperatorBitNotEqual:   return     "T_OperatorBitNotEqual";    // ~=,
    case Python::Token::T_OperatorBitShiftRightEqual:return "T_OperatorBitShiftRightEqual"; // >>=,
    case Python::Token::T_OperatorBitShiftLeftEqual: return "T_OperatorBitShiftLeftEqual";  // <<=,

        // compare
    case Python::Token::T_OperatorCompareEqual:  return     "T_OperatorCompareEqual";   // ==,
    case Python::Token::T_OperatorNotEqual:      return     "T_OperatorNotEqual";       // !=,
    case Python::Token::T_OperatorLessEqual:     return     "T_OperatorLessEqual";      // <=,
    case Python::Token::T_OperatorMoreEqual:     return     "T_OperatorMoreEqual";      // >=,
    case Python::Token::T_OperatorLess:          return     "T_OperatorLess";           // <,
    case Python::Token::T_OperatorMore:          return     "T_OperatorMore";           // >,
    case Python::Token::T_OperatorAnd:           return     "T_OperatorAnd";            // 'and',
    case Python::Token::T_OperatorOr:            return     "T_OperatorOr";             // 'or',
    case Python::Token::T_OperatorNot:           return     "T_OperatorNot";            // 'not',
    case Python::Token::T_OperatorIs:            return     "T_OperatorIs";             // 'is',
    case Python::Token::T_OperatorIn:            return     "T_OperatorIn";             // 'in',

        // special
    case Python::Token::T_OperatorVariableParam: return     "T_OperatorVariableParam";  // * for function parameters ie (*arg1)
    case Python::Token::T_OperatorKeyWordParam:  return     "T_OperatorKeyWordParam";   // ** for function parameters ir (**arg1)

        // delimiters
    case Python::Token::T_Delimiter:             return "T_Delimiter";   // all other non specified
    case Python::Token::T_DelimiterOpenParen:    return "T_DelimiterOpenParen";   // (
    case Python::Token::T_DelimiterCloseParen:   return "T_DelimiterCloseParen";   // )
    case Python::Token::T_DelimiterOpenBracket:  return "T_DelimiterOpenBracket";   // [
    case Python::Token::T_DelimiterCloseBracket: return "T_DelimiterCloseBracket";   // ]
    case Python::Token::T_DelimiterOpenBrace:    return "T_DelimiterOpenBrace";   // {
    case Python::Token::T_DelimiterCloseBrace:   return "T_DelimiterCloseBrace";   // }
    case Python::Token::T_DelimiterPeriod:       return "T_DelimiterPeriod";   // .
    case Python::Token::T_DelimiterComma:        return "T_DelimiterComma";   // ,
    case Python::Token::T_DelimiterColon:        return "T_DelimiterColon";   // :
    case Python::Token::T_DelimiterSemiColon:    return "T_DelimiterSemiColon";   // ;
    case Python::Token::T_DelimiterEllipsis:     return "T_DelimiterEllipsis";    // ...
        // metadata such def funcname(arg: "documentation") ->
        //                            "returntype documentation":
    case Python::Token::T_DelimiterMetaData:     return "T_DelimiterMetaData";   // -> might also be ':' inside arguments
    case Python::Token::T_DelimiterLineContinue: return "T_DelimiterLineContinue";   // when end of line is escaped like so '\'
    case Python::Token::T_DelimiterNewLine:      return "T_DelimiterNewLine";   // each new line
        // identifiers
    case Python::Token::T_IdentifierUnknown:     return "T_IdentifierUnknown"; // name is not identified at this point
    case Python::Token::T_IdentifierDefined:     return "T_IdentifierDefined"; // variable is in current context
    case Python::Token::T_IdentifierModule:      return "T_IdentifierModule"; // its a module definition
    case Python::Token::T_IdentifierModuleAlias: return "T_IdentifierModuleAlias"; // alias for import. ie: import Something as Alias
    case Python::Token::T_IdentifierModulePackage: return "T_IdentifierModulePackage"; // identifier is a package, ie: root for other modules
    case Python::Token::T_IdentifierModuleGlob:  return "T_IdentifierModuleGlob"; // from mod import * <- glob
    case Python::Token::T_IdentifierFunction:    return "T_IdentifierFunction"; // its a function definition
    case Python::Token::T_IdentifierMethod:      return "T_IdentifierMethod"; // its a method definition
    case Python::Token::T_IdentifierClass:       return "T_IdentifierClass"; // its a class definition
    case Python::Token::T_IdentifierSuperMethod: return "T_IdentifierSuperMethod"; // its a method with name: __**__
    case Python::Token::T_IdentifierBuiltin:     return "T_IdentifierBuiltin"; // its builtin
    case Python::Token::T_IdentifierDecorator:   return "T_IdentifierDecorator"; // member decorator like: @property
    case Python::Token::T_IdentifierDefUnknown:  return "T_IdentifierDefUnknown"; // before system has determined if its a
                                                                                            // method or function yet
    case Python::Token::T_IdentifierNone:        return "T_IdentifierNone";  // The None keyword
    case Python::Token::T_IdentifierTrue:        return "T_IdentifierTrue";  // The bool True
    case Python::Token::T_IdentifierFalse:       return "T_IdentifierFalse"; // The bool False
    case Python::Token::T_IdentifierSelf:        return "T_IdentifierSelf";
    case Python::Token::T_IdentifierInvalid:     return "T_IdentifierInvalid";

        // metadata such def funcname(arg: "documentaion") -> "returntype documentation":
    case Python::Token::T_MetaData:              return "T_MetaData";
        // these are inserted by PythonSourceRoot
    case Python::Token::T_BlockStart:            return "T_BlockStart"; // indicate a new block ie if (a is True):
                                                                                  //                                       ^
    case Python::Token::T_BlockEnd:              return "T_BlockEnd";   // indicate block end ie:    dosomething
                                                                                  //                        dosomethingElse
                                                                                  //                       ^


    case Python::Token::T_Invalid:               return "T_Invalid";
    case Python::Token::T_PythonConsoleOutput:   return "T_PythonConsoleOutput";
    case Python::Token::T_PythonConsoleError:    return "T_PythonConsoleError";
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
        QFileInfo fi(QString::fromLatin1(outfile));
        m_file = fopen(fi.absoluteFilePath().toLatin1(), "w");
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

DumpSyntaxTokens::DumpSyntaxTokens(QTextBlock firstBlock, const char *outfile):
    DumpToFileBaseClass(outfile)
{
    fprintf(m_file, "dump tokens at %s\n", datetimeStr());

    QTextBlock txtBlock = firstBlock;
    while(txtBlock.isValid()) {
        // first print the complete srctext as is
        fprintf(m_file, "%s¶\n", txtBlock.text().toLatin1().data());
        Python::TextBlockData *txtData = reinterpret_cast<Python::TextBlockData*>(txtBlock.userData());
        if (txtData == nullptr)
            break;
        for (Python::Token *tok : txtData->tokens()) {
            fprintf(m_file, " %s", tokenToCStr(tok->type()));
        }
        fprintf(m_file, "¶\n");
        txtBlock = txtBlock.next();
    }
}

DumpSyntaxTokens::~DumpSyntaxTokens()
{
}

// ----------------------------------------------------------------------------------------

static const uint8_t SRC_ROW_SHIFT = 8;
static const std::intptr_t SRC_ROW_FLAG = (1 << 7),
                           SRC_ROW_START = (1 << SRC_ROW_SHIFT),
                           SRC_ROW_MASK = 0xFFFFFFFFFFFFFF00,
                           TOK_ROW_MASK = 0x00000000000000FF;

TokenModel::TokenModel(const PythonEditor *editor, QObject *parent) :
    QAbstractItemModel(parent),
    m_editor(editor)
{
}

TokenModel::~TokenModel()
{
}

QVariant TokenModel::data(const QModelIndex &index, int role) const
{
    DEFINE_DBG_VARS

    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::ForegroundRole))
        return QVariant();

    auto idx = reinterpret_cast<std::intptr_t>(index.internalPointer());
    if (idx & SRC_ROW_FLAG) {
        // its a code line
        Python::SyntaxHighlighter *hl = dynamic_cast<Python::SyntaxHighlighter*>(m_editor->syntaxHighlighter());
        if (hl) {
            Python::TokenLine *line = hl->list().lineAt(index.row());
            if (line) {
                if (index.column() == 0) {
                    if (role == Qt::ForegroundRole)
                        return QBrush(QColor(0, 0, 50));
                    return QString::number(line->lineNr() +1);
                } else {
                    if (role == Qt::ForegroundRole)
                        return QBrush(QColor(0, 0, 50));
                    return QString::fromStdString(line->text());
                }
            }
        }

    } else {
        // its a token line, and we have parent
        int parentline = (idx & SRC_ROW_MASK) >> SRC_ROW_SHIFT;
        uint tokIdx = idx & TOK_ROW_MASK;
        auto txtData = getTextBlock(parentline - 1);
        if (txtData) {
            const std::list<Python::Token*> &tokens = txtData->tokens();
            if (txtData->tokens().size() > tokIdx) {
                const Python::Token *tok = *std::next(tokens.begin(), tokIdx);
                if (tok) {
                    DBG_TOKEN(tok)
                    if (index.column() == 0) {
                        if (role == Qt::ForegroundRole)
                            return QBrush(QColor(0, 0, 170));
                        return QString::fromLatin1("s:%1 e:%2")
                                .arg(tok->startPos())
                                .arg(tok->endPos());
                    } else if (index.column() == 1) {
                        if (role == Qt::ForegroundRole)
                            return QBrush(QColor(0, 0, 120));
                        return QString::fromLatin1(tokenToCStr(tok->type()));
                    }
                }
            }
        }
    }

    return QVariant();
}

Qt::ItemFlags TokenModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;

    return QAbstractItemModel::flags(index);
}

QVariant TokenModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0)
            return QLatin1String("line");
        else
            return QLatin1String("code");
    }

    return QVariant();
}

QModelIndex TokenModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.isValid()) {
        // its the tokens row
        std::intptr_t idx = reinterpret_cast<std::intptr_t>(parent.internalPointer());
        auto txtData = getTextBlock((idx >> SRC_ROW_SHIFT) -1);
        if (txtData && static_cast<uint>(row) < txtData->tokens().size()){
            idx = (idx & SRC_ROW_MASK) | row;
            return createIndex(row, column, reinterpret_cast<void*>(idx));
        }
    } else {
        // its the code line
        auto txtData = getTextBlock(row);
        int idx = ((row + 1) << SRC_ROW_SHIFT) | SRC_ROW_FLAG;
        if (txtData){
            idx |= txtData->tokens().size();
        }
        return createIndex(row, column, reinterpret_cast<void*>(idx));
    }

    return QModelIndex();
}

QModelIndex TokenModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    auto idx = reinterpret_cast<std::intptr_t>(index.internalPointer());
    if (!(idx & SRC_ROW_FLAG)) {
        // its a token line, and we have parent
        int parentline = (idx & SRC_ROW_MASK) >> SRC_ROW_SHIFT;
        idx |= (idx & SRC_ROW_MASK) | SRC_ROW_FLAG;
        auto txtData = getTextBlock(parentline);
        if (txtData)
            idx |= txtData->tokens().size();
        return createIndex(parentline -1, 0 , reinterpret_cast<void*>(idx));
    }

    return QModelIndex();
}

int TokenModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0 || m_editor->document()->isEmpty())
        return 0; // we only have children to col0

    if (parent.isValid()) {
        std::intptr_t idx = reinterpret_cast<std::intptr_t>(parent.internalPointer());
        int line = idx >> SRC_ROW_SHIFT;
        if (idx & SRC_ROW_FLAG) {
            // its a src row
            auto txtData = getTextBlock(line -1);
            if (txtData)
                return static_cast<int>(txtData->tokens().size());
        }
        // its a tokens row or fail txtData
        return 0;
    }
    // root element, number of rows
    if (parent.row() < 0)
        return m_editor->document()->lineCount();
    return 0;
}

int TokenModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 2; // token
    return 2; // line
}

void TokenModel::refreshAll()
{
    beginResetModel();
    endResetModel();
}

const Python::TextBlockData *TokenModel::getTextBlock(long line) const
{
    QTextBlock block = m_editor->document()->findBlockByLineNumber(static_cast<int>(line));
    if (!block.isValid())
        return nullptr;

    return dynamic_cast<Python::TextBlockData*>(block.userData());
}

// -----------------------------------------------------------------------

DebugWindow::DebugWindow(PythonEditor *editor) :
    QWidget(),
    m_editor(editor),
    m_tabWgt(nullptr), m_tokModel(nullptr), m_tokTree(nullptr)
{
    setWindowTitle(QString::fromLatin1("Debug view"));
    resize(800, 680);

    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);
    m_tabWgt = new QTabWidget(this);
    layout->addWidget(m_tabWgt);

    m_tokModel = new TokenModel(m_editor);
    m_tokTree  = new QTreeView();
    m_tokTree->setModel(m_tokModel);

    QWidget *tokPage = new QWidget(this);
    QVBoxLayout *tokLayout = new QVBoxLayout();
    tokPage->setLayout(tokLayout);
    QPushButton *tokRefresh = new QPushButton(QLatin1String("refresh"));
    connect(tokRefresh, SIGNAL(clicked()), m_tokModel, SLOT(refreshAll()));

    tokLayout->addWidget(tokRefresh);
    tokLayout->addWidget(m_tokTree);

    m_tabWgt->addTab(tokPage, QLatin1String("Tokens"));


    // framedump
    QWidget *frameDump = new QWidget(this);
    QVBoxLayout *frameDumpLayout = new QVBoxLayout();
    frameDump->setLayout(frameDumpLayout);

    m_frameDumpEdit = new QPlainTextEdit(this);
    m_frameDumpEdit->setReadOnly(true);

    QPushButton *frameDumpRefresh = new QPushButton(QLatin1String("refresh"));
    connect(frameDumpRefresh, SIGNAL(clicked()), this, SLOT(dumpFrames()));

    frameDumpLayout->addWidget(frameDumpRefresh);
    frameDumpLayout->addWidget(m_frameDumpEdit);

    m_tabWgt->addTab(frameDump, QLatin1String("frames"));

    dumpFrames();
}

DebugWindow::~DebugWindow()
{
    delete m_tokTree;
    delete m_tokModel;
    delete m_tabWgt;
}

void DebugWindow::dumpFrames()
{
    FILE *tmpFilePtr;

    tmpFilePtr = tmpfile();

    //DumpSyntaxTokens dump(document()->begin());
    QString fileName = m_editor->fileName();
    Python::SourceModule *module =
            Python::SourceRoot::instance()->moduleFromPath(fileName.toStdString());
    Python::DumpModule dMod(module, tmpFilePtr);

    rewind(tmpFilePtr);

    QByteArray textBytes;
    while(!feof(tmpFilePtr)) {
        char c = fgetc(tmpFilePtr);
        textBytes.append(c);
    }

    m_frameDumpEdit->setPlainText(QString::fromLatin1(textBytes));

}



#include "moc_PythonCodeDebugTools.cpp"

