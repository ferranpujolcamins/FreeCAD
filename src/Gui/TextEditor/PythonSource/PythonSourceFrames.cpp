#include "PythonSourceFrames.h"

#include "PythonSource.h"
#include "PythonSourceRoot.h"
#include "PythonSourceModule.h"
#include <QDebug>

DBG_TOKEN_FILE

using namespace Gui;

PythonSourceFrameReturnType::PythonSourceFrameReturnType(PythonSourceListParentBase *owner,
                                       const PythonSourceModule *module,
                                       PythonToken *tok) :
    PythonSourceListNodeBase(owner),
    m_module(module)
{
    assert(module != nullptr && "Must get a valid owner");
    assert(module != nullptr && "Must get a vaild module");
    setToken(tok);
}

PythonSourceFrameReturnType::~PythonSourceFrameReturnType()
{
}

void PythonSourceFrameReturnType::setTypeInfo(PythonSourceRoot::TypeInfo typeInfo)
{
    m_typeInfo = typeInfo;
}

PythonSourceRoot::TypeInfo PythonSourceFrameReturnType::returnType() const
{
    if (isYield()) {
        PythonSourceRoot::TypeInfo typeInfo;
        typeInfo.type = PythonSourceRoot::GeneratorType;
        return typeInfo;
    }

    // compute return type of statement
    return m_module->root()->statementResultType(m_token,
                                                 m_module->getFrameForToken(
                                                     m_token, m_module->rootFrame()));
}

bool PythonSourceFrameReturnType::isYield() const
{
    return m_token->token == PythonSyntaxHighlighter::T_KeywordYield;
}

PythonSourceRoot::TypeInfo PythonSourceFrameReturnType::yieldType() const
{
    PythonSourceRoot::TypeInfo typeInfo;
    if (isYield())
        return m_module->root()->statementResultType(m_token,
                                                     m_module->getFrameForToken(
                                                         m_token, m_module->rootFrame()));

    return typeInfo;
}

// ----------------------------------------------------------------------------


PythonSourceFrameReturnTypeList::PythonSourceFrameReturnTypeList(PythonSourceListParentBase *owner,
                                                                const PythonSourceModule *module) :
    PythonSourceListParentBase(owner),
    m_module(module)
{
    m_preventAutoRemoveMe = true;
}

PythonSourceFrameReturnTypeList::PythonSourceFrameReturnTypeList(const PythonSourceFrameReturnTypeList &other) :
    PythonSourceListParentBase(other),
    m_module(other.m_module)
{
}

PythonSourceFrameReturnTypeList::~PythonSourceFrameReturnTypeList()
{
}

int PythonSourceFrameReturnTypeList::compare(const PythonSourceListNodeBase *left, const PythonSourceListNodeBase *right) const
{
    const PythonSourceFrameReturnType *l = dynamic_cast<const PythonSourceFrameReturnType*>(left),
                             *r = dynamic_cast<const PythonSourceFrameReturnType*>(right);
    assert(l != nullptr && r != nullptr && "Non PythonSourceReturnList items in returnList");
    if (*l->token() < *r->token())
        return +1;
    return -1; // r must be bigger
}



// -----------------------------------------------------------------------------

PythonSourceFrame::PythonSourceFrame(PythonSourceFrame *owner,
                                     PythonSourceModule *module,
                                     PythonSourceFrame *parentFrame,
                                     bool isClass):
    PythonSourceListParentBase(owner),
    m_identifiers(module),
    m_parameters(this),
    m_imports(this, module),
    m_returnTypes(this, module),
    m_typeHint(nullptr),
    m_parentFrame(parentFrame),
    m_module(module),
    m_lastToken(this),
    m_isClass(isClass)
{
}

PythonSourceFrame::PythonSourceFrame(PythonSourceModule *owner,
                                     PythonSourceModule *module,
                                     PythonSourceFrame *parentFrame,
                                     bool isClass):
    PythonSourceListParentBase(owner),
    m_identifiers(module),
    m_parameters(this),
    m_imports(this, module),
    m_returnTypes(this, module),
    m_typeHint(nullptr),
    m_parentFrame(parentFrame),
    m_module(module),
    m_lastToken(this),
    m_isClass(isClass)
{
}

PythonSourceFrame::~PythonSourceFrame()
{
    if (m_typeHint)
        delete m_typeHint;
}

QString PythonSourceFrame::name() const
{
    if (isModule())
        return QString::fromLatin1("<%1>").arg(m_module->moduleName());
    const PythonSourceIdentifier *ident = parentFrame()->identifiers()
                                        .getIdentifier(token()->text());
    if (ident)
        return ident->name();

    return QLatin1String("<unbound>") + m_token->text();
}

QString PythonSourceFrame::docstring()
{
    DEFINE_DBG_VARS

    // retrive docstring for this function
    QStringList docStrs;
    if (m_token) {
        const PythonToken *token = scanAllParameters(const_cast<PythonToken*>(m_token), false, false);
        DBG_TOKEN(token)
        int guard = 20;
        while(token && token->token != PythonSyntaxHighlighter::T_DelimiterSemiColon) {
            if ((--guard) <= 0)
                return QString();
            NEXT_TOKEN(token)
        }

        if (!token)
            return QString();

        // now we are at semicolon (even with typehint)
        //  def func(arg1,arg2) -> str:
        //                            ^
        // find first string, before other stuff
        while (token) {
            if (!token->txtBlock())
                continue;
            switch (token->token) {
            case PythonSyntaxHighlighter::T_LiteralBlockDblQuote:// fallthrough
            case PythonSyntaxHighlighter::T_LiteralBlockSglQuote: {
                // multiline
                QString tmp = token->text();
                docStrs << tmp.mid(3, tmp.size() - 6); // clip """
            }   break;
            case PythonSyntaxHighlighter::T_LiteralDblQuote: // fallthrough
            case PythonSyntaxHighlighter::T_LiteralSglQuote: {
                // single line
                QString tmp = token->text();
                docStrs << tmp.mid(1, tmp.size() - 2); // clip "
            }   break;
            case PythonSyntaxHighlighter::T_Indent: // fallthrough
            case PythonSyntaxHighlighter::T_Comment:
                NEXT_TOKEN(token)
                break;
            default:
                // some other token, probably some code
                token = nullptr; // bust out of while
                DBG_TOKEN(token)
            }
        }

        // return what we have collected
        return docStrs.join(QLatin1String("\n"));
    }

    return QString(); // no token
}

PythonSourceRoot::TypeInfo PythonSourceFrame::returnTypeHint() const
{
    PythonSourceRoot::TypeInfo typeInfo;
    if (parentFrame()) {
        // get typehint from parentframe identifier for this frame
        const PythonSourceIdentifier *ident = parentFrame()->getIdentifier(m_token->text());
        if (ident) {
            PythonSourceTypeHintAssignment *assign = ident->getTypeHintAssignment(m_token->line());
            if (assign)
                typeInfo = assign->typeInfo();
        }
    }

    return typeInfo;


    // FIXME Move this into scanParameters
/*
    DEFINE_DBG_VARS

    PythonSourceRoot::TypeInfo tpInfo;
    if (m_token) {
        PythonToken *token = scanAllParameters(m_token),
                          *commentToken = nullptr;
        DBG_TOKEN(token)
        int guard = 10;
        while(token && token->token != PythonSyntaxHighlighter::T_DelimiterMetaData) {
            if ((--guard) <= 0)
                return tpInfo;

            if (token->token == PythonSyntaxHighlighter::T_DelimiterSemiColon) {
                // no metadata
                // we may still have type hint in a comment in row below
                while (token && token->token == PythonSyntaxHighlighter::T_Indent) {
                    NEXT_TOKEN(token)
                    if (token && token->token == PythonSyntaxHighlighter::T_Comment) {
                        commentToken = token;
                        token = nullptr; // bust out of loops
                        DBG_TOKEN(token)
                    }
                }

                if (!commentToken)
                    return tpInfo; // have no metadata, might have commentdata
            } else
                NEXT_TOKEN(token)
        }

        QString annotation;

        if (token) {
            // now we are at metadata token
            //  def func(arg1: int, arg2: bool) -> MyType:
            //                                  ^
            // step one more further
            NEXT_TOKEN(token)
            if (token)
                annotation = token->text();

        } else if (commentToken) {
            // extract from comment
            // # type: def func(int, bool) -> MyType
            QRegExp re(QLatin1String("type\\s*:\\s*def\\s+[_a-zA-Z]+\\w*\\(.*\\)\\s*->\\s*(\\w+)"));
            if (re.indexIn(token->text()) > -1)
                annotation = re.cap(1);
        }

        if (!annotation.isEmpty()) {
            // set type and customname
            PythonSourceRoot *sr = PythonSourceRoot::instance();
            tpInfo.type = sr->mapMetaDataType(annotation);
            if (tpInfo.type == PythonSourceRoot::CustomType) {
                tpInfo.customNameIdx = sr->indexOfCustomTypeName(annotation);
                if (tpInfo.customNameIdx < 0)
                    tpInfo.customNameIdx = sr->addCustomTypeName(annotation);
            }
        }
    }

    return tpInfo;
    */
}

PythonToken *PythonSourceFrame::scanFrame(PythonToken *startToken, PythonSourceIndent &indent)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(startToken)

    if (!startToken)
        return startToken;

    // freshly created frame?
    if (!m_token)
        setToken(startToken);
    if (!m_lastToken.token())
        m_lastToken.setToken(startToken);

    PythonToken *tok = startToken;
    PythonTextBlockData *txtData = tok->txtBlock();
    DBG_TOKEN(tok)

    int guard = txtData->block().document()->blockCount(); // max number of rows scanned
    while (tok && (guard--)) {
        txtData = tok->txtBlock();
        tok = scanLine(tok, indent);

        if (indent.framePopCnt() > 0) {
            indent.framePopCntDecr();
            if (tok && m_parentFrame == nullptr)
                m_lastToken.setToken(tok);
            break;
        }

        if (tok && !tok->next())
            m_lastToken.setToken(tok);
        else
            NEXT_TOKEN(tok)
    }

    if (guard == 0)
        qDebug() << QLatin1String("***Warning !! scanFrame loopguard") << endl;

    return tok;
}


PythonToken *PythonSourceFrame::scanLine(PythonToken *startToken,
                                               PythonSourceIndent &indent)
{
    DEFINE_DBG_VARS

    if (!startToken)
        return startToken;

    PythonToken *tok = startToken,
                *frmStartTok = startToken;
    DBG_TOKEN(tok)


    bool indentCached = indent.isValid();
    // we don't do this on each row when scanning all frame
    if (!indentCached) {
        indent = m_module->currentBlockIndent(tok);
    }

    // move up to first code token
    // used to determine firsttoken
    int guard = 100;
    while(frmStartTok && (guard--)) {
        if (frmStartTok->isNewLine())
            return frmStartTok; // empty row
        if (frmStartTok->isText())
            break;
        NEXT_TOKEN(frmStartTok)
    }

    if (!frmStartTok)
        return frmStartTok;

    // T_Indent or code on pos 0
    tok = handleIndent(tok, indent, -1);
    if (indent.framePopCnt())
        return tok->previous(); // exit a frame and continue with
                                // caller frame on this token (incr in scanFrame)

    if (isModule())
        m_type.type = PythonSourceRoot::ModuleType;

    // do framestart if it is first row in document
    bool isFrameStarter = tok->txtBlock()->block().blockNumber() == 0;

    if (tok->token == PythonSyntaxHighlighter::T_IdentifierFunction ||
        tok->token == PythonSyntaxHighlighter::T_IdentifierMethod ||
        tok->token == PythonSyntaxHighlighter::T_IdentifierSuperMethod ||
        tok->token == PythonSyntaxHighlighter::T_IdentifierClass ||
        isFrameStarter)
    {
#ifdef BUILD_PYTHON_DEBUGTOOLS
        if (isFrameStarter)
            m_name = m_module->moduleName();
        else
            m_name = frmStartTok->text();
#endif
        isFrameStarter = true;
        // set initial start token
        setToken(frmStartTok);

        // scan parameterLists
        if (m_parentFrame){
            // not rootFrame, parse argumentslist
            if (!isClass()) {
                // not __init__ as that is scanned in parentframe
                if (m_parentFrame->isClass() && frmStartTok->text() == QLatin1String("__init__"))
                    tok = frmStartTok->next();
                else
                    tok = scanAllParameters(frmStartTok, true, false);
            } else {
                // FIXME implement inheritance
                //tok = scanInheritance(tok);
                tok = frmStartTok->next(); // TODO remove when inheritance is implemented
            }
            DBG_TOKEN(tok)
        }
    }

    // do increasing indents
    tok = handleIndent(tok, indent, 1);

    PythonSourceModule *noConstModule = const_cast<PythonSourceModule*>(m_module);

    guard = 200; // max number of tokens in this line, unhang locked loop
    // scan document
    while (tok && (guard--)) {
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_KeywordReturn:
            tok = scanReturnStmt(tok);
            continue;
        case PythonSyntaxHighlighter::T_KeywordYield:
            tok = scanYieldStmt(tok);
            continue;
        case PythonSyntaxHighlighter::T_DelimiterMetaData: {
            // return typehint -> 'Type'
            // make sure previous char was a ')'
            PythonToken *tmpTok = tok->previous();
            int guardTmp = 20;
            while (tmpTok && !tmpTok->isCode() && guardTmp--)
                PREV_TOKEN(tmpTok)

            if (tmpTok && tmpTok->token != PythonSyntaxHighlighter::T_DelimiterCloseParen)
                m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected '%1' before '->'")
                                                    .arg(tmpTok->text()));
            else {
                // its a valid typehint

                PythonSourceRoot::TypeInfo typeInfo;
                const PythonSourceIdentifier *ident;
                PythonToken *typeHintTok = tok->next();
                // store this typehint
                tok = scanRValue(typeHintTok, typeInfo, true);
                if (typeInfo.isValid() && typeHintTok && parentFrame()) {
                    // we don't store typehint in this frame, rather we lookup
                    // our parentframe identifier for this function and stes typehint on that
                    ident = parentFrame()->getIdentifier(m_token->text());
                    if (ident)
                        const_cast<PythonSourceIdentifier*>(ident)->setTypeHint(typeHintTok, typeInfo);
                }
                DBG_TOKEN(tok)
            }

        } break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (!m_module->root()->isLineEscaped(tok)){
                // insert Blockstart if we have a semicolon, non code tokens might exist before
                PythonToken *prevTok = tok->previous();
                int guard = 10;
                while (prevTok && (--guard)) {
                    if (prevTok->isCode()) {
                        if (prevTok->token == PythonSyntaxHighlighter::T_DelimiterColon)
                            m_module->insertBlockStart(prevTok);
                        break;
                    }
                    PREV_TOKEN(prevTok)
                }

                if (m_lastToken.token() && (*m_lastToken.token() < *tok))
                    m_lastToken.setToken(tok);
                return tok;
            }
            break;
        case PythonSyntaxHighlighter::T_IdentifierDefUnknown: {
            // first handle indents to get de-dent
            tok = handleIndent(tok, indent, -1);
            if (indent.framePopCnt())
                return tok->previous();

            // a function that can be function or method
            // determine what it is
            if (m_isClass && indent.frameIndent() == tok->txtBlock()->indent()) {
                if (tok->text().left(2) == QLatin1String("__") &&
                    tok->text().right(2) == QLatin1String("__"))
                {
                    const_cast<PythonToken*>(tok)->token = PythonSyntaxHighlighter::T_IdentifierSuperMethod;
                } else {
                    const_cast<PythonToken*>(tok)->token = PythonSyntaxHighlighter::T_IdentifierMethod;
                }
                noConstModule->setFormatToken(tok);
                continue; // switch on new token state
            }
            tok->token = PythonSyntaxHighlighter::T_IdentifierFunction;
            noConstModule->setFormatToken(tok);
            continue; // switch on new token state
        } break;
        case PythonSyntaxHighlighter::T_IdentifierClass: {

            tok = startSubFrame(tok, indent, PythonSourceRoot::ClassType);
            DBG_TOKEN(tok)
            if (indent.framePopCnt()> 0)
                return tok;
            continue;
        } break;
        case PythonSyntaxHighlighter::T_IdentifierFunction: {
            tok = startSubFrame(tok, indent, PythonSourceRoot::FunctionType);
            DBG_TOKEN(tok)
            if (indent.framePopCnt()> 0)
                return tok;
            continue;
        } break;
        case PythonSyntaxHighlighter::T_IdentifierSuperMethod:
            if (isClass() &&
                tok->text() == QLatin1String("__init__"))
            {
                // parameters for this class, we scan in class scope
                // ie before we create a new frame due to parameters for this class must be
                // owned by class frame
                scanAllParameters(tok->next(), true, true);
                DBG_TOKEN(tok)
            }
            // fallthrough
        case PythonSyntaxHighlighter::T_IdentifierMethod: {
            if (!isClass())
                m_module->setSyntaxError(tok, QLatin1String("Method without class"));

            tok = startSubFrame(tok, indent, PythonSourceRoot::MethodType);
            DBG_TOKEN(tok)
            if (indent.framePopCnt()> 0)
                return tok;
            continue;
        } break;
        default:
            if (tok->isImport()) {
                tok = scanImports(tok);
                DBG_TOKEN(tok)
                continue;

            } else if (tok->isIdentifier()) {
                tok = scanIdentifier(tok);
                DBG_TOKEN(tok)
                continue;

            }

        } // end switch

        NEXT_TOKEN(tok)

    } // end while

    return tok;
}

PythonToken *PythonSourceFrame::startSubFrame(PythonToken *tok,
                                              PythonSourceIndent &indent,
                                              PythonSourceRoot::DataTypes type)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    // first handle indents to get de-dent
    tok = handleIndent(tok, indent, -1);
    if (indent.framePopCnt())
        return tok->previous();

    // store this as a function identifier
    PythonSourceRoot::TypeInfo typeInfo;
    typeInfo.type = type;
    m_identifiers.setIdentifier(tok, typeInfo);

    // add function frame
    bool classFrm = type == PythonSourceRoot::ClassType;
    PythonSourceFrame *funcFrm = new PythonSourceFrame(this, m_module, this, classFrm);
    funcFrm->setToken(tok);
    insert(funcFrm);

    // scan this frame
    int lineIndent = tok->txtBlock()->indent();
    if (indent.frameIndent() < lineIndent)
         indent.pushFrameBlock(lineIndent, lineIndent);
    tok = funcFrm->scanFrame(tok, indent);
    DBG_TOKEN(tok)
    //if (tok && m_lastToken.token() && (*m_lastToken.token() < *tok))
    //    m_lastToken.setToken(tok);

    return tok;
}


// scans a for aditional TypeHint and scans rvalue
PythonToken *PythonSourceFrame::scanIdentifier(PythonToken *tok)
{
    DEFINE_DBG_VARS

    int guard = 20;
    PythonSourceIdentifier *ident = nullptr;
    PythonToken *identifierTok = nullptr; // the identifier that gets assigned
    PythonSourceRoot::TypeInfo typeInfo;

    while (tok && (guard--)) {
        // TODO figure out how to do tuple assignment
        if (!tok->isCode())
            return tok;
        switch (tok->token) {
        case PythonSyntaxHighlighter::T_IdentifierFalse:
            typeInfo.type = PythonSourceRoot::BoolType;
            m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case PythonSyntaxHighlighter::T_IdentifierTrue:
            typeInfo.type = PythonSourceRoot::BoolType;
            m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case PythonSyntaxHighlighter::T_IdentifierNone:
            typeInfo.type = PythonSourceRoot::NoneType;
            m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case PythonSyntaxHighlighter::T_OperatorEqual:
            // assigment
            if (!identifierTok) {
                m_module->setSyntaxError(tok, QLatin1String("Unexpected ="));
            } else {
                tok = scanRValue(tok, typeInfo, false);
                ident = m_identifiers.setIdentifier(identifierTok, typeInfo);
            }
            if (identifierTok && identifierTok->token == PythonSyntaxHighlighter::T_IdentifierUnknown) {
                identifierTok->token = PythonSyntaxHighlighter::T_IdentifierDefined;
                m_module->setFormatToken(identifierTok);
            }
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            // type hint or blockstart
            if (tok->next() && !tok->next()->isCode()) {
                // its not a typehint, its probably a blockstart
                return tok;
            } else if (ident) {
                m_module->setSyntaxError(tok, QLatin1String("Unexpected ':'"));
                return tok;
            } else if (!identifierTok) {
                m_module->setSyntaxError(tok, QLatin1String("Unexpected ="));
            } else {
                ident = m_identifiers.setIdentifier(identifierTok, typeInfo);
                tok = scanRValue(tok, typeInfo, true);
                DBG_TOKEN(tok)
                if (tok)
                    ident->setTypeHint(tok, typeInfo);
                if (!tok)
                    return nullptr;
                DBG_TOKEN(tok)
            }
            break;
        default:
            assert(!tok->isImport() && "Should never get a import in scanIdentifier");
            if (tok->isIdentifierVariable())
                identifierTok = tok;
            break;
        }


        NEXT_TOKEN(tok)
    }

    return tok;
}

PythonToken *PythonSourceFrame::scanRValue(PythonToken *tok,
                                           PythonSourceRoot::TypeInfo &typeInfo,
                                           bool isTypeHint)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    typeInfo = PythonSourceRoot::TypeInfo(); // init with clean type

    int guard = 20;
    int parenCnt = 0,
        bracketCnt = 0,
        braceCnt = 0;

    const PythonSourceIdentifier *ident = nullptr;

    while (tok && (guard--)) {
        if (!tok->isCode())
            return tok;
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_DelimiterOpenParen:
            if (!isTypeHint)
                return tok;
            ++parenCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseParen:
            if (!isTypeHint)
                return tok;
            --parenCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBracket:
            if (!isTypeHint)
                return tok;
            ++bracketCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseBracket:
            if (!isTypeHint)
                return tok;
            --bracketCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBrace:
            if (!isTypeHint)
                return tok;
            ++braceCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterCloseBrace:
            if (!isTypeHint)
                return tok;
            --braceCnt; break;
        case PythonSyntaxHighlighter::T_DelimiterPeriod:
            // might have obj.sub1.sub2
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            if (!isTypeHint) {
                m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected '%1'").arg(tok->text()));
                return tok;
            }
            // else do next token
            break;
        case PythonSyntaxHighlighter::T_OperatorEqual:
            if (!isTypeHint && tok->next()) {
                // might have chained assignments ie: vlu1 = valu2 = vlu3 = 0
                tok = scanRValue(tok->next(), typeInfo, isTypeHint);
                DBG_TOKEN(tok)
            }

            if (ident && tok)
                typeInfo = ident->getFromPos(tok)->typeInfo();
            return tok;
        default:
            if (parenCnt == 0 && braceCnt == 0 && braceCnt == 0) {
                QString text = tok->text();// one roundtrip to document
                if (!isTypeHint) {
                    // ordinary RValue
                    if (tok->isInt())
                        typeInfo.type = PythonSourceRoot::IntType;
                    else if (tok->isFloat())
                        typeInfo.type = PythonSourceRoot::FloatType;
                    else if (tok->isString())
                        typeInfo.type = PythonSourceRoot::StringType;
                    else if (tok->isKeyword()) {
                        if (text == QLatin1String("None"))
                            typeInfo.type = PythonSourceRoot::NoneType;
                        else if (text == QLatin1String("False") ||
                                 text == QLatin1String("True"))
                        {
                            typeInfo.type = PythonSourceRoot::BoolType;
                        }
                    }
                }

                if (tok->isIdentifierVariable()){
                    if (tok->token == PythonSyntaxHighlighter::T_IdentifierBuiltin) {
                        typeInfo.type = PythonSourceRoot::ReferenceBuiltInType;
                    } else {
                        PythonSourceFrame *frm = this;
                        do {
                            if (!frm->isClass())
                                ident = frm->identifiers().getIdentifier(text);
                        } while (!ident && (frm = frm->parentFrame()));

                        // finished lookup
                        if (ident) {
                            typeInfo.type = PythonSourceRoot::ReferenceType;
                            typeInfo.referenceName = text;
                            if (tok->token == PythonSyntaxHighlighter::T_IdentifierUnknown) {
                                tok->token = PythonSyntaxHighlighter::T_IdentifierDefined;
                                m_module->setFormatToken(tok);
                            }
                        } else if (isTypeHint)
                            m_module->setLookupError(tok, QString::fromLatin1("Unknown type '%1'").arg(text));
                        else {
                            // new identifier
                            m_module->setLookupError(tok, QString::fromLatin1("Unbound variable in RValue context '%1'").arg(tok->text()));
                            typeInfo.type = PythonSourceRoot::ReferenceType;
                        }
                    }
                    return tok;

                } else if (tok->isIdentifierDeclaration()) {
                    if (tok->isBoolean())
                        typeInfo.type = PythonSourceRoot::BoolType;
                    else if (tok->token == PythonSyntaxHighlighter::T_IdentifierNone)
                        typeInfo.type = PythonSourceRoot::NoneType;
                } else if (tok->isCode()) {
                    if (isTypeHint)
                        typeInfo.type = PythonSourceRoot::instance()->mapMetaDataType(tok->text());
                    if (tok->isNumber())
                        typeInfo.type = PythonSourceRoot::instance()->numberType(tok);
                    else if (tok->isString())
                        typeInfo.type = PythonSourceRoot::StringType;
                    else
                        m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected code (%1)").arg(text));
                    return tok;
                }
            }
        }
        NEXT_TOKEN(tok)
    }

    return tok;
}

PythonToken *PythonSourceFrame::scanImports(PythonToken *tok)
{
    DEFINE_DBG_VARS

    int guard = 20;
    QStringList fromPackages, importPackages, modules;
    QString alias;
    bool isAlias = false;
    bool isImports = tok->previous()->token == PythonSyntaxHighlighter::T_KeywordImport;
    PythonToken *moduleTok = nullptr,
                *aliasTok = nullptr;

    while (tok && (guard--)) {
        QString text = tok->text();
        switch(tok->token) {
        case PythonSyntaxHighlighter::T_KeywordImport:
            isImports = true;
            break;
        case PythonSyntaxHighlighter::T_KeywordFrom:
            if (isImports)
                m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected token '%1'").arg(text));
            break;
        case PythonSyntaxHighlighter::T_KeywordAs:
            isAlias = true;
            break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (!m_module->root()->isLineEscaped(tok)) {
                guard = 0; // we are finished, bail out
                if (modules.size() > 0) {
                    if (isAlias)
                        m_module->setSyntaxError(tok, QLatin1String("Expected aliasname before newline"));
                    goto store_module;
                }
            }
            break;
        case PythonSyntaxHighlighter::T_DelimiterComma:
            if (!modules.isEmpty())
                goto store_module;
            break;
        case PythonSyntaxHighlighter::T_IdentifierModuleGlob:
            m_imports.setModuleGlob(fromPackages + importPackages);
            modules.clear();
            importPackages.clear();
            break;
        case PythonSyntaxHighlighter::T_IdentifierModulePackage:
            if (!isImports) {
                fromPackages << text;
                break;
            }
            goto set_identifier;
        case PythonSyntaxHighlighter::T_IdentifierModule:
            goto set_identifier;
        case PythonSyntaxHighlighter::T_IdentifierModuleAlias:
            if (!isAlias)
                m_module->setSyntaxError(tok, QString::fromLatin1("Parsed as Alias but analyzer disagrees. '%1'").arg(text));
            goto set_identifier;
        default:
            if (tok->token == PythonSyntaxHighlighter::T_DelimiterPeriod &&
                tok->previous() && tok->previous()->isImport())
                break; // might be "import foo.bar ..."
                       //                     ^
            if (tok->isCode())
                m_module->setSyntaxError(tok, QString::fromLatin1("Unexpected token '%1'").arg(text));
            break;
        }

next_token:
        if (guard > 0)
            NEXT_TOKEN(tok)
        continue;

        // we should only get here by explicit goto statements
set_identifier:
            if (!isAlias) {
                // previous text was a package
                if (modules.size())
                    importPackages << modules.takeLast();
                modules << text;
                moduleTok = tok;
                goto next_token;
            } else {
                alias = text;
                aliasTok = tok;
            }

// same code used from multiple places, keeps loop running
store_module:
        assert(modules.size() > 0 && "Modules set called by no modules in container");
        if (alias.size() > 0) {
            // store import and set identifier as alias
            PythonSourceImportModule *imp = m_imports.setModule(fromPackages + importPackages, modules[0], alias);
            PythonSourceRoot::TypeInfo typeInfo(imp->type());
            m_identifiers.setIdentifier(aliasTok, typeInfo);
        } else {
            // store import and set identifier as modulename
            PythonSourceImportModule *imp = m_imports.setModule(fromPackages + importPackages, modules[0]);
            PythonSourceRoot::TypeInfo typeInfo(imp->type());
            m_identifiers.setIdentifier(moduleTok, typeInfo);
        }
        alias.clear();
        importPackages.clear();
        modules.clear();
        isAlias = false;
        moduleTok = aliasTok = nullptr;
        goto next_token;
    }
    return tok;
}

// scans a single statement
PythonToken *PythonSourceFrame::scanReturnStmt(PythonToken *tok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    PythonSourceRoot::TypeInfo typeInfo = m_module->root()->statementResultType(tok, this);
    // store it
    PythonSourceFrameReturnType *frmType = new PythonSourceFrameReturnType(&m_returnTypes, m_module, tok);
    frmType->setTypeInfo(typeInfo);
    m_returnTypes.insert(frmType);

    // set errormessage if we have code after return on this indentation level
    scanCodeAfterReturnOrYield(tok, QLatin1String("return"));

    // advance token til next statement
    return gotoEndOfLine(tok);
}

PythonToken *PythonSourceFrame::scanYieldStmt(PythonToken *tok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    assert(tok && tok->token == PythonSyntaxHighlighter::T_KeywordYield && "Expected a yield token");

    if (!m_returnTypes.empty() && m_returnTypes.hasOtherTokens(PythonSyntaxHighlighter::T_KeywordYield)) {
        // we have other tokens !
        QString msg = QLatin1String("Setting yield in a function with a return statement\n");
        m_module->setMessage(tok, msg);
    }
    // TODO implement yield

    // set errormessage if we have code after return on this indentation level
    scanCodeAfterReturnOrYield(tok, QLatin1String("yield"));

    return gotoEndOfLine(tok);
}

void PythonSourceFrame::scanCodeAfterReturnOrYield(PythonToken *tok, QString name)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    // check if we have code after this return with same block level
    int guard = 10;
    PythonTextBlockData *txtData = tok->txtBlock();
    do {
        txtData = txtData->next();
        if (txtData && txtData->isCodeLine()) {
            if (txtData->indent() == tok->txtBlock()->indent())
                m_module->setMessage(txtData->firstCodeToken(), QString::fromLatin1("Code after a %1 will never run.").arg(name));
            break;
        }
    } while (txtData && (--guard));
}

// may return nullptr on error
PythonToken *PythonSourceFrame::scanAllParameters(PythonToken *tok, bool storeParameters, bool isInitFunc)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    // safely goes to closingParen of arguments list
    int parenCount = 0,
        safeGuard = 50;
    while(tok && (safeGuard--) > 0 &&
          tok->token != PythonSyntaxHighlighter::T_DelimiterMetaData)
    {
        // first we must make sure we clear all parens
        if (tok->token == PythonSyntaxHighlighter::T_DelimiterOpenParen)
            ++parenCount;
        else if (tok->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            --parenCount;

        if (parenCount <= 0 && tok->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            return tok->next(); // so next caller knows where it ended

        // advance one token
        NEXT_TOKEN(tok)

        // scan parameters
        if (storeParameters && parenCount > 0) {
            if (tok->isIdentifier() ||
                tok->token == PythonSyntaxHighlighter::T_OperatorVariableParam ||
                tok->token == PythonSyntaxHighlighter::T_OperatorKeyWordParam)
            {
                tok = scanParameter(tok, parenCount, isInitFunc); // also should handle typehints
                DBG_TOKEN(tok)
            }
        }
    }

    return tok;
}

PythonToken *PythonSourceFrame::scanParameter(PythonToken *paramToken, int &parenCount, bool isInitFunc)
{
    DEFINE_DBG_VARS

    // safely constructs our parameters list
    int safeGuard = 20;

    // init empty
    PythonSourceParameter *param = nullptr;
    PythonSourceParameter::ParameterType paramType = PythonSourceParameter::InValid;

    // scan up to ',' or closing ')'
    while(paramToken && (safeGuard--))
    {
        // might have subparens
        switch (paramToken->token) {
        case PythonSyntaxHighlighter::T_DelimiterComma:
            if (parenCount == 1)
                return paramToken;
            break;
        case PythonSyntaxHighlighter::T_DelimiterOpenParen:
            ++parenCount;
            break;
        case PythonSyntaxHighlighter::T_DelimiterCloseParen:
            --parenCount;
            if (parenCount == 0)
                return paramToken;
            break;
        case PythonSyntaxHighlighter::T_OperatorVariableParam:
            if (parenCount > 0)
                paramType = PythonSourceParameter::Variable;
            break;
        case PythonSyntaxHighlighter::T_OperatorKeyWordParam:
            if (parenCount > 0)
                paramType = PythonSourceParameter::Keyword;
            break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (!m_module->root()->isLineEscaped(paramToken))
                return paramToken->previous(); // caller must know where newline is at
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            // typehint is handled when param is constructed below, just make sure we warn for syntax errors
            if (paramType != PythonSourceParameter::Positional &&
                paramType != PythonSourceParameter::PositionalDefault)
            {
                m_module->setSyntaxError(paramToken, QString::fromLatin1("Unexpected ':'"));
            }
            break;
        case PythonSyntaxHighlighter::T_OperatorEqual:
            // we have a default value, actual reference is handled dynamically when we call getReferenceBy on this identifier
            if (paramType != PythonSourceParameter::PositionalDefault) {
                m_module->setSyntaxError(paramToken, QString::fromLatin1("Unexpected '='"));
            }
            if (parenCount == 1) {
                do { // clear this identifier
                    NEXT_TOKEN(paramToken);
                } while (paramToken && paramToken->isIdentifier() && (safeGuard--));
            }
            continue; // switch on this token, hopefully its a ','
        default:
            if (parenCount > 0) {
                if (paramToken->isIdentifier()) {
                    // check if we have default value
                    if (paramType == PythonSourceParameter::InValid) {
                        const PythonToken *nextTok = paramToken->next();
                        if (nextTok && nextTok->token == PythonSyntaxHighlighter::T_OperatorEqual)
                            paramType = PythonSourceParameter::PositionalDefault; // have default value
                        else
                            paramType = PythonSourceParameter::Positional;
                    }

                    // set identifier, first try with type hints
                    PythonSourceRoot::TypeInfo typeInfo = guessIdentifierType(paramToken);
                    if (!typeInfo.isValid())
                        typeInfo.type = PythonSourceRoot::ReferenceArgumentType;

                    m_identifiers.setIdentifier(paramToken, typeInfo);

                    // set parameter
                    param = m_parameters.setParameter(paramToken, typeInfo, paramType);

                    // Change tokenValue
                    if (paramToken->token == PythonSyntaxHighlighter::T_IdentifierUnknown) {

                        if ((m_parentFrame->isClass() || (isInitFunc && isClass())) &&
                            m_parameters.indexOf(param) == 0)
                        {
                            paramToken->token = PythonSyntaxHighlighter::T_IdentifierSelf;
                        } else {
                            paramToken->token = PythonSyntaxHighlighter::T_IdentifierDefined;
                        }
                    }

                    // repaint in highligher
                    const_cast<PythonSourceModule*>(m_module)->setFormatToken(paramToken);

                } else if(paramType != PythonSourceParameter::InValid) {
                    m_module->setSyntaxError(paramToken, QString::fromLatin1("Expected identifier, got '%1'")
                                                                             .arg(paramToken->text()));
                }
            }
        }

        NEXT_TOKEN(paramToken)
    }

    return paramToken;

}

PythonSourceRoot::TypeInfo PythonSourceFrame::guessIdentifierType(const PythonToken *token)
{
    DEFINE_DBG_VARS

    PythonSourceRoot::TypeInfo typeInfo;
    if (token && (token = token->next())) {
        DBG_TOKEN(token)
        if (token->token == PythonSyntaxHighlighter::T_DelimiterColon) {
            // type hint like foo : None = Nothing
            QString explicitType = token->next()->text();
            PythonSourceRoot *root = PythonSourceRoot::instance();
            typeInfo.type = root->mapMetaDataType(explicitType);
            if (typeInfo.type == PythonSourceRoot::CustomType) {
                typeInfo.customNameIdx = root->indexOfCustomTypeName(explicitType);
                if (typeInfo.customNameIdx < 0)
                    typeInfo.customNameIdx = root->addCustomTypeName(explicitType);
            }
        } else if (token->token == PythonSyntaxHighlighter::T_OperatorEqual) {
            // ordinare assignment foo = Something
            typeInfo.type = PythonSourceRoot::ReferenceType;
        }
    }
    return typeInfo;
}

const PythonSourceFrameReturnTypeList PythonSourceFrame::returnTypes() const
{
    return m_returnTypes;
}

PythonToken *PythonSourceFrame::gotoNextLine(PythonToken *tok)
{
    DEFINE_DBG_VARS

    int guard = 200;
    PythonTextBlockData *txtBlock = tok->txtBlock()->next();
    if (!txtBlock)
        return nullptr;
    while(txtBlock && (guard--)) {
        if (txtBlock->isCodeLine()) {
            tok = txtBlock->tokenAt(0);
            DBG_TOKEN(tok)
            break;
        }
        txtBlock = txtBlock->next();
    }
    return tok;
}

PythonToken *PythonSourceFrame::handleIndent(PythonToken *tok,
                                             PythonSourceIndent &indent,
                                             int direction)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    int ind = tok->txtBlock()->indent();

    if (!indent.validIndentLine(tok))
        return tok;

    if (ind < indent.currentBlockIndent() && direction < 1) {
        // dedent

        // backup until last codeline
        int guard = 20;
        PythonTextBlockData *txtPrev = tok->txtBlock()->previous();
        while(txtPrev && !txtPrev->isCodeLine() && (guard--))
            txtPrev = txtPrev->previous();

        if (ind > indent.previousBlockIndent()) {
            m_module->setIndentError(tok); // uneven indentation
        } else if (txtPrev){

            PythonToken *lastTok = txtPrev->tokens().last();
            do { // handle when multiple blocks dedent in a single row
                indent.popBlock();
                if (txtPrev) {// notify that this block has ended
                    lastTok = m_module->insertBlockEnd(lastTok);
                    DBG_TOKEN(lastTok)
                }
            } while(lastTok && indent.currentBlockIndent() > ind);

            if (indent.framePopCnt() > 0) {
                // save end token for this frame
                PythonTextBlockData *txtBlock = tok->txtBlock()->previous();
                if (txtBlock)
                    m_lastToken.setToken(txtBlock->tokens().last());
                return tok;
            }
        }
    } else if (ind > indent.currentBlockIndent() && direction > -1) {
        // indent
        // find previous ':'
        const PythonToken *prev = tok->previous();
        DBG_TOKEN(prev)
        int guard = 20;
        while(prev && (guard--)) {
            switch (prev->token) {
            case PythonSyntaxHighlighter::T_DelimiterColon:
                //m_module->insertBlockStart(tok);
                // fallthrough
            case PythonSyntaxHighlighter::T_BlockStart:
                indent.pushBlock(ind);
                prev = nullptr; // break while loop
                break;
            case PythonSyntaxHighlighter::T_Comment:
            case PythonSyntaxHighlighter::T_LiteralDblQuote:
            case PythonSyntaxHighlighter::T_LiteralBlockDblQuote:
            case PythonSyntaxHighlighter::T_LiteralSglQuote:
            case PythonSyntaxHighlighter::T_LiteralBlockSglQuote:
                PREV_TOKEN(prev)
                break;
            default:
                if (!prev->isCode()) {
                    PREV_TOKEN(prev)
                    break;
                }
                uint userState = static_cast<uint>(prev->txtBlock()->block().userState());
                uint parenCnt = (userState & PythonSyntaxHighlighter::ParenCntMASK) >>
                                          PythonSyntaxHighlighter::ParenCntShiftPos;
                if (parenCnt != 0)
                    return tok;
                // syntax error
                m_module->setSyntaxError(tok, QLatin1String("Blockstart without ':'"));
                if (direction > 0)
                    indent.pushFrameBlock(indent.currentBlockIndent(), ind);
                else
                    indent.pushBlock(ind);
                prev = nullptr; // break while loop
                break;
            }
        }
        DBG_TOKEN(tok)
    }
    return tok;
}

PythonToken *PythonSourceFrame::gotoEndOfLine(PythonToken *tok)
{
    DEFINE_DBG_VARS

    int guard = 80;
    int newLines = 0;
    while (tok && (guard--)) {
        switch (tok->token) {
        case PythonSyntaxHighlighter::T_DelimiterLineContinue:
            --newLines; break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (++newLines > 0)
                return tok;
            // fallthrough
        default: ;// nothing
        }
        NEXT_TOKEN(tok)
    }

    return tok;
}
