#include "PythonSourceParser.h"
#include "PythonSource.h"
#include "PythonSourceModule.h"
#include "PythonSourceFrames.h"

DBG_TOKEN_FILE



Python::SourceParser::SourceParser() :
    m_activeModule(nullptr), m_activeFrame(nullptr),
    m_tok(nullptr), m_beforeTok(nullptr)
{
}

Python::SourceParser::~SourceParser()
{
}

void Python::SourceParser::setActiveModule(SourceModule *module)
{
    if (m_activeModule != module) {
        m_activeFrame = nullptr;
        m_tok = m_beforeTok = nullptr;
        m_activeModule = module;
    }
}

void Python::SourceParser::setActiveFrame(Python::SourceFrame *frame)
{
    if (m_activeFrame != frame) {
        m_activeFrame = frame;
        if (m_activeFrame->m_module)
            m_activeModule = m_activeFrame->m_module;
        m_tok = m_beforeTok = nullptr;
    }
}

Python::SourceParser *Python::SourceParser::instance()
{
    static Python::SourceParser *inst = new Python::SourceParser();
    return inst;
}

void Python::SourceParser::moveNext()
{
    if (m_tok)
        m_beforeTok = m_tok;
    m_tok = m_tok ? m_tok->next() :  nullptr;
}

void Python::SourceParser::movePrevious()
{
    if (m_tok)
        m_tok = m_tok->previous();
    if (m_tok)
        m_beforeTok = m_tok->previous();
}

void Python::SourceParser::moveToken(Python::Token *newPos)
{
    m_tok = newPos;
    m_beforeTok = newPos ? newPos->previous() : nullptr;
}

void Python::SourceParser::startSubFrame(Python::SourceIndent &indent,
                                              Python::SourceRoot::DataTypes type)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(m_tok)

    // add function frame
    bool classFrm = type == Python::SourceRoot::ClassType;
    Python::SourceFrame *funcFrm = new Python::SourceFrame(m_activeFrame, m_activeModule, m_activeFrame, classFrm),
                        *oldFrm = m_activeFrame;
    funcFrm->setToken(m_tok);
    m_activeFrame->insert(funcFrm);

    // store this as a function identifier
    Python::SourceRoot::TypeInfo typeInfo;
    typeInfo.type = type;
    m_activeFrame->m_identifiers.setIdentifier(m_tok, typeInfo);

    // find out indent and push frameblock
    // def func(arg1):     <- frameIndent
    //     var1 = None     <- previousBlockIndent
    //     if (arg1):      <- triggers change in currentBlockIndent
    //         var1 = arg1 <- currentBlockIndent
    uint frameIndent = m_tok->ownerLine()->indent();
    const Python::Token *tokIt = m_tok;
    uint guard = 1000;
    while (tokIt && (tokIt = tokIt->next()) && (--guard)) {
        if (tokIt->type() == Token::T_Indent) {
            indent.pushFrameBlock(frameIndent,
                                  tokIt->ownerLine()->indent());
            break;
        }
    }

    // scan this frame
    m_activeFrame = funcFrm;
    scanFrame(m_tok, indent);
    m_activeFrame = oldFrm;
    DBG_TOKEN(m_tok)
}

Python::Token *Python::SourceParser::scanFrame(Python::Token *startToken, Python::SourceIndent &indent)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(startToken)

    if (!startToken)
        return startToken;

    // freshly created frame?
    if (!m_activeFrame->token())
        m_activeFrame->setToken(startToken);
    if (!m_activeFrame->m_lastToken.token())
        m_activeFrame->m_lastToken.setToken(startToken);

    m_tok = startToken;
    DBG_TOKEN(m_tok)

    uint guard = m_tok->ownerLine()->ownerList()->max_size(); // max number of rows scanned
    while (m_tok && (--guard)) {
        m_tok = scanLine(m_tok, indent);

        if (indent.framePopCnt() > 0) {
            indent.framePopCntDecr();
            break;
        }

        moveNext();
    }

    if (m_tok)
        m_activeFrame->m_lastToken.setToken(m_tok);
    else {
        assert(m_beforeTok != nullptr);
        m_activeFrame->m_lastToken.setToken(m_beforeTok);
    }

    if (guard == 0)
        std::cout << "***Warning !! scanFrame loopguard" << std::endl;

    return m_tok;
}


Python::Token *Python::SourceParser::scanLine(Python::Token *startToken,
                                               Python::SourceIndent &indent)
{
    DEFINE_DBG_VARS

    if (!startToken)
        return startToken;

    moveToken(startToken);
    Python::Token *frmStartTok = startToken;
    DBG_TOKEN(m_tok)


    bool indentCached = indent.isValid();
    // we don't do this on each row when scanning all frame
    if (!indentCached) {
        indent = m_activeModule->currentBlockIndent(m_tok);
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

    if (m_activeFrame->isModule())
        m_activeFrame->m_type.type = Python::SourceRoot::ModuleType;

    // do framestart if it is first row in document
    bool isFrameStarter = m_tok->line() == 0;

    if (m_tok->isIdentifierFrameStart() || isFrameStarter)
    {
#ifdef BUILD_PYTHON_DEBUGTOOLS
        if (isFrameStarter)
            m_activeFrame->m_name = m_activeModule->moduleName();
        else
            m_activeFrame->m_name = frmStartTok->text();
#endif
        isFrameStarter = true;
        // set initial start token
        m_activeFrame->setToken(frmStartTok);

        // scan parameterLists
        if (m_activeFrame->m_parentFrame){
            // not rootFrame, parse argumentslist
            if (!m_activeFrame->m_isClass) {
                // not __init__ as that is scanned in parentframe
                if (m_activeFrame->m_parentFrame->isClass() && frmStartTok->text() == "__init__")
                    moveToken(frmStartTok->next());
                else
                    scanAllParameters(true, false);
            } else {
                // FIXME implement inheritance
                //m_tok = scanInheritance(tok);
                moveToken(frmStartTok->next()); // TODO remove when inheritance is implemented
            }
            DBG_TOKEN(m_tok)
        }
    }

    // do increasing indents
    handleIndent(indent);

    guard = 200; // max number of tokens in this line, unhang locked loop
    // scan document
    while (m_tok && (guard--)) {
        DBG_TOKEN(m_tok)
        switch(m_tok->type()) {
        case Python::Token::T_KeywordReturn:
            scanReturnStmt();
            continue;
        case Python::Token::T_KeywordYield:
            scanYieldStmt();
            continue;
        case Python::Token::T_DelimiterMetaData: {
            // return typehint -> 'Type'
            // make sure previous char was a ')'
            Python::Token *tmpTok = m_beforeTok;// m_tok->previous();
            int guardTmp = 20;
            while (tmpTok && !tmpTok->isCode() && guardTmp--)
                PREV_TOKEN(tmpTok)

            if (tmpTok && tmpTok->type() != Python::Token::T_DelimiterCloseParen)
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, std::string("Unexpected '") +
                                                    tmpTok->text() + "' before '->'");
            else {
                // its a valid typehint
                Python::SourceRoot::TypeInfo typeInfo;
                const Python::SourceIdentifier *ident;
                Python::Token *typeHintTok = m_tok->next();
                // store this typehint
                scanRValue(typeInfo, true);
                if (typeInfo.isValid() && typeHintTok && m_activeFrame->parentFrame()) {
                    // we don't store typehint in this frame, rather we lookup
                    // our parentframe identifier for this function and stes typehint on that
                    ident = m_activeFrame->parentFrame()->getIdentifier(m_activeFrame->hash());
                    if (ident)
                        const_cast<Python::SourceIdentifier*>(ident)->setTypeHint(typeHintTok, typeInfo);
                }
                DBG_TOKEN(m_tok)
            }

        } break;
        case Python::Token::T_DelimiterNewLine:
            if (!Python::SourceRoot::instance()->isLineEscaped(m_tok)) {
                // store the last token
                if (m_activeFrame->m_lastToken.token() && (*m_activeFrame->m_lastToken.token() < *m_tok))
                    m_activeFrame->m_lastToken.setToken(m_tok);

                // pop indent stack
                while (m_tok->next() && m_tok->next()->type() == Token::T_Dedent) {
                    indent.popBlock();
                    moveNext();
                }

                return m_tok;
            }
            break;
        case Python::Token::T_IdentifierDefUnknown: {
            // a function that can be function or method
            // determine what it is
            if (m_activeFrame->m_isClass && indent.frameIndent() == m_tok->ownerLine()->indent()) {
                if (m_tok->text().substr(0, 2) == "__" &&
                    m_tok->text().substr(m_tok->text().length()-3, 2) == "__")
                {
                    const_cast<Python::Token*>(m_tok)->changeType(Python::Token::T_IdentifierSuperMethod);
                } else {
                    const_cast<Python::Token*>(m_tok)->changeType(Python::Token::T_IdentifierMethod);
                }
                m_activeModule->tokenTypeChanged(m_tok);
                continue; // switch on new token state
            }
            const_cast<Python::Token*>(m_tok)->changeType(Python::Token::T_IdentifierFunction);
            m_activeModule->tokenTypeChanged(m_tok);
            continue; // switch on new token state
        }
        case Python::Token::T_IdentifierClass: {
            Python::SourceRoot::TypeInfo tpInfo(Python::SourceRoot::ClassType);
            m_activeFrame->m_identifiers.setIdentifier(m_tok, tpInfo);
            startSubFrame(indent, Python::SourceRoot::ClassType);
            DBG_TOKEN(m_tok)
            if (indent.framePopCnt()> 0 || (m_tok && m_tok->type() == Token::T_Dedent))
                return m_tok;
            continue;
        }
        case Python::Token::T_IdentifierFunction: {
            startSubFrame(indent, Python::SourceRoot::FunctionType);
            DBG_TOKEN(m_tok)
            if (indent.framePopCnt()> 0 || (m_tok && m_tok->type() == Token::T_Dedent))
                return m_tok;
            continue;
        }
        case Python::Token::T_IdentifierSuperMethod:
            if (m_activeFrame->m_isClass &&
                m_tok->text() == "__init__")
            {
                // parameters for this class, we scan in class scope
                // ie before we create a new frame due to parameters for this class must be
                // owned by class frame
                scanAllParameters(true, true);
                DBG_TOKEN(m_tok)
            }
            FALLTHROUGH
        case Python::Token::T_IdentifierMethod: {
            if (!m_activeFrame->m_isClass)
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Method without class");

            startSubFrame(indent, Python::SourceRoot::MethodType);
            DBG_TOKEN(m_tok)
            if (indent.framePopCnt()> 0 || (m_tok && m_tok->type() == Token::T_Dedent))
                return m_tok;
            continue;
        }
        default:
            if (m_tok->isImport()) {
                scanImports();
                DBG_TOKEN(m_tok)
                continue;

            } else if (m_tok->isIdentifier()) {
                scanIdentifier();
                DBG_TOKEN(m_tok)
                continue;

            }

        } // end switch

        moveNext();

    } // end while

    return m_tok;
}




// scans a for aditional TypeHint and scans rvalue
void Python::SourceParser::scanIdentifier()
{
    DEFINE_DBG_VARS

    int guard = 20;
    Python::SourceIdentifier *ident = nullptr;
    const Python::SourceIdentifier *parentIdent = nullptr;
    Python::Token *identifierTok = nullptr; // the identifier that gets assigned
    Python::SourceRoot::TypeInfo typeInfo;

    while (m_tok && (guard--)) {
        // TODO figure out how to do tuple assignment

        switch (m_tok->type()) {
        case Python::Token::T_IdentifierFalse: FALLTHROUGH
        case Python::Token::T_IdentifierTrue:
            typeInfo.type = Python::SourceRoot::BoolType;
            m_activeFrame->m_identifiers.setIdentifier(m_tok, typeInfo);
            break;
        case Python::Token::T_IdentifierNone:
            typeInfo.type = Python::SourceRoot::NoneType;
            m_activeFrame->m_identifiers.setIdentifier(m_tok, typeInfo);
            break;
        case Python::Token::T_DelimiterOpenParen:
            if (identifierTok) {
                if (identifierTok->type() == Python::Token::T_IdentifierUnknown) {
                    const Python::SourceIdentifier *tmpIdent = lookupIdentifierReference(identifierTok);

                    // default to invalid
                    identifierTok->changeType(Python::Token::T_IdentifierInvalid);

                    if (tmpIdent && !tmpIdent->isCallable(identifierTok)) {
                        std::string msg = "Calling '" + identifierTok->text() +
                                                "' is a non callable";
                        m_tok->ownerLine()->setLookupErrorMsg(identifierTok, msg);
                    }

                } else if (identifierTok->type() == Python::Token::T_IdentifierBuiltin) {
                    Python::SourceRoot::TypeInfoPair tpPair =
                            Python::SourceRoot::instance()->builtinType(identifierTok, m_activeFrame);
                   if (!tpPair.thisType.isCallable()) {
                       std::string msg = "Calling builtin '" + identifierTok->text() +
                                                "' is a non callable";
                       m_tok->ownerLine()->setLookupErrorMsg(identifierTok, msg);
                   }
                }

            }
            return;
            //break;
        case Python::Token::T_DelimiterPeriod:
            if (identifierTok) {
                if (parentIdent)
                    goto parent_check;
                else if (identifierTok->type() == Python::Token::T_IdentifierUnknown)
                    parentIdent = lookupIdentifierReference(identifierTok);
                else
                    parentIdent = m_activeFrame->getIdentifier(m_tok->hash());

                if (!parentIdent) {
                    identifierTok->changeType(Python::Token::T_IdentifierInvalid);
                    m_tok->ownerLine()->setLookupErrorMsg(identifierTok);
                }
            } else {
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Unexpected '.'");
                return;
            }
            break;
        case Python::Token::T_IdentifierBuiltin: {
            Python::SourceRoot::TypeInfoPair tpPair =
                    Python::SourceRoot::instance()->builtinType(m_tok, m_activeFrame);
            m_activeFrame->m_identifiers.setIdentifier(m_tok, tpPair.thisType);
            identifierTok = m_tok;
        }   break;
        case Python::Token::T_OperatorNot:
        case Python::Token::T_OperatorIs:
            // booltest
            if (!identifierTok) {
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Unexpected " + m_tok->text());
            } else {
                typeInfo.type = Python::SourceRoot::BoolType;
                ident = m_activeFrame->m_identifiers.setIdentifier(identifierTok, typeInfo);
                if (identifierTok->type() == Python::Token::T_IdentifierUnknown) {
                    identifierTok->changeType(Python::Token::T_IdentifierDefined);
                    m_activeModule->tokenTypeChanged(identifierTok);
                }
            }
            break;
        case Python::Token::T_OperatorIn: FALLTHROUGH
        case Python::Token::T_OperatorEqual:
            // assigment
            if (!identifierTok) {
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Unexpected " + m_tok->text());
            } else {
                scanRValue(typeInfo, false);
                ident = m_activeFrame->m_identifiers.setIdentifier(identifierTok, typeInfo);
            }
            if (identifierTok && identifierTok->type() == Python::Token::T_IdentifierUnknown) {
                identifierTok->changeType(Python::Token::T_IdentifierDefined);
                m_activeModule->tokenTypeChanged(identifierTok);
            }
            break;
        case Python::Token::T_DelimiterColon:
            // type hint or blockstart
            if (m_tok->next() && !m_tok->next()->isCode()) {
                // its not a typehint, its probably a blockstart
                if (identifierTok)
                    lookupIdentifierReference(identifierTok);
                return;
            } else if (ident) {
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Unexpected ':'");
                return;
            } else if (!identifierTok) {
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Unexpected =");
            } else {
                ident = m_activeFrame->m_identifiers.setIdentifier(identifierTok, typeInfo);
                scanRValue(typeInfo, true);
                DBG_TOKEN(m_tok)
                if (m_tok)
                    ident->setTypeHint(m_tok, typeInfo);
                if (!m_tok)
                    return;
                DBG_TOKEN(m_tok)
            }
            break;
        default:
            assert(!m_tok->isImport() && "Should never get a import in scanIdentifier");
parent_check:
            if (parentIdent && identifierTok) {
                const Python::SourceFrame *frm = parentIdent->callFrame(m_tok->previous());
                if (frm) {
                    parentIdent = frm->getIdentifier(parentIdent->hash());
                    if (!parentIdent) {
                        identifierTok->changeType(Python::Token::T_IdentifierInvalid);
                        m_tok->ownerLine()->setLookupErrorMsg(m_tok->previous());
                    } else {
                        identifierTok->changeType(Python::Token::T_IdentifierDefined);
                    }
                    m_activeModule->tokenTypeChanged(identifierTok);
                }
            }

            if (!m_tok->isCode()) {
                if (identifierTok && !parentIdent)
                    lookupIdentifierReference(identifierTok);
                return;
            } else if (!m_tok->isIdentifierVariable() &&
                       m_tok->type() != Python::Token::T_DelimiterPeriod)
            {
                lookupIdentifierReference(identifierTok);
                return;
            } else if (m_tok->isIdentifierVariable())
                identifierTok = m_tok;

            break;
        }

        moveNext();
    }
}

const Python::SourceIdentifier *Python::SourceParser::lookupIdentifierReference(Python::Token *tok)
{
    const Python::SourceIdentifier *tmpIdent = nullptr;
    if (tok &&
        (tok->type() == Python::Token::T_IdentifierUnknown ||
         tok->type() == Python::Token::T_IdentifierInvalid))
    {
        Python::SourceRoot::TypeInfo typeInfo;
        const Python::Token *lookFromToken = tok;
        Python::SourceFrame *frm = m_activeFrame;
        Python::SourceIdentifierAssignment *assign = nullptr;

        // lookup in this frame, from cursor position upwards
        if ((tmpIdent = frm->getIdentifier(tok->hash())))
            assign = tmpIdent->getFromPos(tok);

        // else lookup in praentframe (complete context)
        while(!assign && (frm = frm->parentFrame())) {
            // lookup in parent frames if not found
            tmpIdent = frm->identifiers().getIdentifier(tok);
            if (tmpIdent) {
                lookFromToken = frm->lastToken();
                break;
            }
        }

        if (!tmpIdent && tok->type() == Python::Token::T_IdentifierInvalid)
            return nullptr; // error is already set

        // default to invalid
        tok->changeType(Python::Token::T_IdentifierInvalid);

        if (!tmpIdent) {
            tok->ownerLine()->setLookupErrorMsg(tok);
        } else {
            assign = tmpIdent->getFromPos(lookFromToken);
            if (assign && assign->typeInfo().isValid()) {
                if (assign->token()->type() == Python::Token::T_IdentifierSelf)
                    tok->changeType(Python::Token::T_IdentifierSelf);
                else
                    tok->changeType(Python::Token::T_IdentifierDefined);
                if (tmpIdent->isImported(tok))
                    typeInfo.type = Python::SourceRoot::ReferenceImportType;
                else if (tmpIdent->isCallable(tok))
                    typeInfo.type = Python::SourceRoot::ReferenceCallableType;
                else
                    typeInfo.type = Python::SourceRoot::ReferenceType;
                m_activeFrame->m_identifiers.setIdentifier(tok, typeInfo);
                tok->ownerLine()->unfinishedTokens().remove(tok->ownerLine()->tokenPos(tok));
            }
        }
        m_activeModule->tokenTypeChanged(tok);
    }
    return tmpIdent;
}

void Python::SourceParser::scanRValue(Python::SourceRoot::TypeInfo &typeInfo,
                                      bool isTypeHint)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(m_tok)

    typeInfo = Python::SourceRoot::TypeInfo(); // init with clean type

    int guard = 20;
    int parenCnt = 0,
        bracketCnt = 0,
        braceCnt = 0;

    const Python::SourceIdentifier *ident = nullptr;

    while (m_tok && (guard--)) {
        if (!m_tok->isCode())
            return;
        switch(m_tok->type()) {
        case Python::Token::T_DelimiterOpenParen:
            if (!isTypeHint)
                return;
            ++parenCnt; break;
        case Python::Token::T_DelimiterCloseParen:
            if (!isTypeHint)
                return;
            --parenCnt; break;
        case Python::Token::T_DelimiterOpenBracket:
            if (!isTypeHint)
                return;
            ++bracketCnt; break;
        case Python::Token::T_DelimiterCloseBracket:
            if (!isTypeHint)
                return;
            --bracketCnt; break;
        case Python::Token::T_DelimiterOpenBrace:
            if (!isTypeHint)
                return;
            ++braceCnt; break;
        case Python::Token::T_DelimiterCloseBrace:
            if (!isTypeHint)
                return;
            --braceCnt; break;
        case Python::Token::T_DelimiterPeriod:
            // might have obj.sub1.sub2
            break;
        case Python::Token::T_DelimiterColon:
            if (!isTypeHint) {
                std::string msg = "Unexpected '" + m_tok->text() + "'";
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, msg);
                return;
            }
            // else do next token
            break;
        case Python::Token::T_OperatorIn:
            if (ident && m_tok)
                typeInfo = ident->getFromPos(m_tok)->typeInfo();
            return;
        case Python::Token::T_OperatorEqual:
            if (!isTypeHint && m_tok->next()) {
                // might have chained assignments ie: vlu1 = valu2 = vlu3 = 0
                ident = lookupIdentifierReference(m_tok);
                moveNext();
                scanRValue(typeInfo, isTypeHint);
                DBG_TOKEN(m_tok)
            }

            if (ident && m_tok)
                typeInfo = ident->getFromPos(m_tok)->typeInfo();
            return;
        default:
            if (parenCnt == 0 && braceCnt == 0 && braceCnt == 0) {
                std::string text = m_tok->text();// one roundtrip to document
                if (!isTypeHint) {
                    // ordinary RValue
                    if (m_tok->isInt())
                        typeInfo.type = Python::SourceRoot::IntType;
                    else if (m_tok->isFloat())
                        typeInfo.type = Python::SourceRoot::FloatType;
                    else if (m_tok->isString())
                        typeInfo.type = Python::SourceRoot::StringType;
                    else if (m_tok->isKeyword()) {
                        if (text == "None")
                            typeInfo.type = Python::SourceRoot::NoneType;
                        else if (text == "False" ||
                                 text == "True")
                        {
                            typeInfo.type = Python::SourceRoot::BoolType;
                        }
                    }
                }

                if (m_tok->isIdentifierVariable()){
                    if (m_tok->type() == Python::Token::T_IdentifierBuiltin) {
                        typeInfo.type = Python::SourceRoot::ReferenceBuiltInType;
                    } else {
                        Python::SourceFrame *frm = m_activeFrame;
                        do {
                            if (!frm->isClass())
                                ident = frm->identifiers().getIdentifier(m_tok->hash());
                        } while (!ident && (frm = frm->parentFrame()));

                        // finished lookup
                        if (ident) {
                            typeInfo.type = Python::SourceRoot::ReferenceType;
                            typeInfo.referenceName = text;
                            if (m_tok->type() == Python::Token::T_IdentifierUnknown) {
                                m_tok->changeType(Python::Token::T_IdentifierDefined);
                                m_activeModule->tokenTypeChanged(m_tok);
                            }
                        } else if (isTypeHint) {
                            std::string msg = "Unknown type '" + text + "'";
                            m_tok->ownerLine()->setLookupErrorMsg(m_tok, msg);
                        } else {
                            // new identifier
                            std::string msg = "Unbound variable in RValue context '" + text + "'";
                            m_tok->ownerLine()->setLookupErrorMsg(m_tok, msg);
                            typeInfo.type = Python::SourceRoot::ReferenceType;
                        }
                    }
                    return;

                } else if (m_tok->isIdentifierDeclaration()) {
                    if (m_tok->isBoolean())
                        typeInfo.type = Python::SourceRoot::BoolType;
                    else if (m_tok->type() == Python::Token::T_IdentifierNone)
                        typeInfo.type = Python::SourceRoot::NoneType;
                } else if (m_tok->isCode()) {
                    if (isTypeHint)
                        typeInfo.type = Python::SourceRoot::instance()->mapMetaDataType(text);
                    if (m_tok->isNumber())
                        typeInfo.type = Python::SourceRoot::instance()->numberType(m_tok);
                    else if (m_tok->isString())
                        typeInfo.type = Python::SourceRoot::StringType;
                    else if (m_tok->type() != Python::Token::T_DelimiterComma &&
                             !m_tok->isOperatorArithmetic())
                    {
                        std::string msg =  "Unexpected code (" + text + ")";
                        m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, msg);
                    }
                    return;
                }
            }
        }
        moveNext();
    }
}

void Python::SourceParser::scanImports()
{
    DEFINE_DBG_VARS

    int guard = 20;
    std::list<const std::string> fromPackages, importPackages, modules;
    std::string alias;
    bool isAlias = false;
    bool isImports = m_tok->previous()->type() == Python::Token::T_KeywordImport;
    Python::Token *moduleTok = nullptr,
                *aliasTok = nullptr;

    while (m_tok && (guard--)) {
        std::string text = m_tok->text();
        switch(m_tok->type()) {
        case Python::Token::T_KeywordImport:
            isImports = true;
            break;
        case Python::Token::T_KeywordFrom:
            if (isImports) {
                std::string msg = "Unexpected token '" + text + "'";
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, msg);
            }
            break;
        case Python::Token::T_KeywordAs:
            isAlias = true;
            break;
        case Python::Token::T_DelimiterNewLine:
            if (!m_activeFrame->m_module->root()->isLineEscaped(m_tok)) {
                guard = 0; // we are finished, bail out
                if (modules.size() > 0) {
                    if (isAlias)
                        m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Expected aliasname before newline");
                    goto store_module;
                }
            }
            break;
        case Python::Token::T_DelimiterComma:
            if (!modules.empty())
                goto store_module;
            break;
        case Python::Token::T_IdentifierModuleGlob: {
            std::list<const std::string> all;
            for (auto &pkg : fromPackages) all.push_back(pkg);
            for (auto &pkg : importPackages) all.push_back(pkg);
            m_activeFrame->m_imports.setModuleGlob(all);
            modules.clear();
            importPackages.clear();
        }   break;
        case Python::Token::T_IdentifierModulePackage:
            if (!isImports) {
                fromPackages.push_back(text);
                break;
            }
            goto set_identifier;
        case Python::Token::T_IdentifierModule:
            goto set_identifier;
        case Python::Token::T_IdentifierModuleAlias:
            if (!isAlias) {
                std::string msg = "Parsed as Alias but analyzer disagrees. '" + text + "'";
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, msg);
            }
            goto set_identifier;
        default:
            if (m_tok->type() == Python::Token::T_DelimiterPeriod &&
                m_tok->previous() && m_tok->previous()->isImport())
                break; // might be "import foo.bar ..."
                       //                     ^
            if (m_tok->isCode()) {
                std::string msg = "Unexpected token '" + text + "'";
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, msg);
            }
            break;
        }

next_token:
        if (guard > 0)
            moveNext();
        continue;

        // we should only get here by explicit goto statements
set_identifier:
            if (!isAlias) {
                // previous text was a package
                if (modules.size()) {
                    importPackages.push_back(modules.back());
                    modules.pop_back();
                }
                modules.push_back(text);
                moduleTok = m_tok;
                goto next_token;
            } else {
                alias = text;
                aliasTok = m_tok;
            }

// same code used from multiple places, keeps loop running
store_module:
        {
        assert(modules.size() > 0 && "Modules set called by no modules in container");
        // merge the from and import lists
        std::list<const std::string> all;
        for (auto &pkg : fromPackages) all.push_back(pkg);
        for (auto &pkg : importPackages) all.push_back(pkg);
        Python::SourceImportModule *imp = nullptr;

        if (alias.size() > 0) {
            // store import and set identifier as alias
            imp = m_activeFrame->m_imports.setModule(all, modules.front(), alias);
        } else {
            // store import and set identifier as modulename
            imp = m_activeFrame->m_imports.setModule(all, modules.front());
        }
        Python::SourceRoot::TypeInfo typeInfo(imp->type());
        m_activeFrame->m_identifiers.setIdentifier(aliasTok ? aliasTok : moduleTok, typeInfo);

        alias.clear();
        importPackages.clear();
        modules.clear();
        isAlias = false;
        moduleTok = aliasTok = nullptr;
        goto next_token;
        }
    }
}

// scans a single statement
void Python::SourceParser::scanReturnStmt()
{
    DEFINE_DBG_VARS
    DBG_TOKEN(m_tok)
    Python::SourceRoot::TypeInfo typeInfo =
            m_activeModule->root()->statementResultType(m_tok, m_activeFrame);
    // store it
    Python::SourceFrameReturnType *frmType =
            new Python::SourceFrameReturnType(&m_activeFrame->m_returnTypes,
                                              m_activeModule, m_tok);
    frmType->setTypeInfo(typeInfo);
    m_activeFrame->m_returnTypes.insert(frmType);

    // set errormessage if we have code after return on this indentation level
    scanCodeAfterReturnOrYield("return");

    // advance token til next statement
    gotoEndOfLine();
}

void Python::SourceParser::scanYieldStmt()
{
    DEFINE_DBG_VARS
    DBG_TOKEN(m_tok)
    assert(m_tok && m_tok->type() == Python::Token::T_KeywordYield && "Expected a yield token");

    if (!m_activeFrame->m_returnTypes.empty() &&
        m_activeFrame->m_returnTypes.hasOtherTokens(Python::Token::T_KeywordYield))
    {
        // we have other tokens !
        std::string msg = "Setting yield in a function with a return statement\n";
        m_tok->ownerLine()->setMessage(m_tok, msg);
    }
    // TODO implement yield

    // set errormessage if we have code after return on this indentation level
    scanCodeAfterReturnOrYield("yield");

    return gotoEndOfLine();
}

void Python::SourceParser::scanCodeAfterReturnOrYield(const std::string &name)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(m_tok)

    // check if we have code after this return with same block level
    int guard = 10;
    Python::TokenLine *tokLine = m_tok->ownerLine();
    do {
        tokLine = tokLine->nextLine();
        if (tokLine && tokLine->isCodeLine()) {
            if (tokLine->indent() == m_tok->ownerLine()->indent()) {
                std::string msg = "Code after a '" + name + "' will never run.";
                tokLine->setMessage(tokLine->firstCodeToken(), msg);
            }
            break;
        }
    } while (tokLine && (--guard));
}

// may return nullptr on error
void Python::SourceParser::scanAllParameters(bool storeParameters, bool isInitFunc)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(m_tok)

    // safely goes to closingParen of arguments list
    int parenCount = 0,
        safeGuard = 50;
    while(m_tok && (safeGuard--) > 0 &&
          m_tok->type() != Python::Token::T_DelimiterMetaData)
    {
        // first we must make sure we clear all parens
        if (m_tok->type() == Python::Token::T_DelimiterOpenParen)
            ++parenCount;
        else if (m_tok->type() == Python::Token::T_DelimiterCloseParen)
            --parenCount;

        if (parenCount <= 0 && m_tok->type() == Python::Token::T_DelimiterCloseParen) {
            moveNext();
            return; // so next caller knows where it ended
        }

        // advance one token
        moveNext();

        // scan parameters
        if (storeParameters && parenCount > 0) {
            if (m_tok && (m_tok->isIdentifier() ||
                    m_tok->type() == Python::Token::T_OperatorVariableParam ||
                    m_tok->type() == Python::Token::T_OperatorKeyWordParam))
            {
                scanParameter(parenCount, isInitFunc); // also should handle typehints
                DBG_TOKEN(m_tok)
            }
        }
    }
}

void Python::SourceParser::scanParameter(int &parenCount, bool isInitFunc)
{
    DEFINE_DBG_VARS

    // safely constructs our parameters list
    int safeGuard = 20;

    // init empty
    Python::SourceParameter *param = nullptr;
    Python::SourceParameter::ParameterType paramType = Python::SourceParameter::InValid;

    // scan up to ',' or closing ')'
    while(m_tok && (safeGuard--))
    {
        // might have subparens
        switch (m_tok->type()) {
        case Python::Token::T_DelimiterComma:
            if (parenCount == 1)
                return;
            break;
        case Python::Token::T_DelimiterOpenParen:
            ++parenCount;
            break;
        case Python::Token::T_DelimiterCloseParen:
            --parenCount;
            if (parenCount == 0)
                return;
            break;
        case Python::Token::T_OperatorVariableParam:
            if (parenCount > 0)
                paramType = Python::SourceParameter::Variable;
            break;
        case Python::Token::T_OperatorKeyWordParam:
            if (parenCount > 0)
                paramType = Python::SourceParameter::Keyword;
            break;
        case Python::Token::T_DelimiterNewLine:
            if (!Python::SourceRoot::instance()->isLineEscaped(m_tok)) {
                movePrevious(); // caller must know where newline is at
                return;
            }
            break;
        case Python::Token::T_DelimiterColon:
            // typehint is handled when param is constructed below, just make sure we warn for syntax errors
            if (paramType != Python::SourceParameter::Positional &&
                paramType != Python::SourceParameter::PositionalDefault)
            {
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Unexpected ':'");
            }
            break;
        case Python::Token::T_OperatorEqual:
            // we have a default value, actual reference is handled dynamically when we call getReferenceBy on this identifier
            if (paramType != Python::SourceParameter::PositionalDefault) {
                m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Unexpected '='");
            }
            if (parenCount == 1) {
                do { // clear this identifier
                    moveNext();
                } while (m_tok && m_tok->isIdentifier() && (safeGuard--));
            }
            continue; // switch on this token, hopefully its a ','
        default:
            if (parenCount > 0) {
                if (m_tok->isIdentifier()) {
                    // check if we have default value
                    if (paramType == Python::SourceParameter::InValid) {
                        const Python::Token *nextTok = m_tok->next();
                        if (nextTok && nextTok->type() == Python::Token::T_OperatorEqual)
                            paramType = Python::SourceParameter::PositionalDefault; // have default value
                        else
                            paramType = Python::SourceParameter::Positional;
                    }

                    // set identifier, first try with type hints
                    Python::SourceRoot::TypeInfo typeInfo = guessIdentifierType(m_tok);
                    if (!typeInfo.isValid())
                        typeInfo.type = Python::SourceRoot::ReferenceArgumentType;

                    m_activeFrame->m_identifiers.setIdentifier(m_tok, typeInfo);

                    // set parameter
                    //param = m_activeFrame->m_parameters.setParameter(m_tok, typeInfo, paramType);
                    param = new Python::SourceParameter(m_activeFrame, m_tok);
                    param->setType(typeInfo);
                    param->setParameterType(paramType);
                    m_activeFrame->m_parameters.push_back(param);

                    // Change tokenValue
                    if (m_tok->type() == Python::Token::T_IdentifierUnknown) {

                        if ((m_activeFrame->parentFrame()->isClass() || (isInitFunc && m_activeFrame->isClass())) &&
                            m_activeFrame->m_parameters.size() == 1)
                        {
                            m_tok->changeType(Python::Token::T_IdentifierSelf);
                        } else {
                            m_tok->changeType(Python::Token::T_IdentifierDefined);
                        }
                    }

                    // repaint in highligher
                    const_cast<Python::SourceModule*>(m_activeModule)->tokenTypeChanged(m_tok);

                } else if (!m_tok->isNumber() && !m_tok->isBoolean() &&
                           !m_tok->isString() && !m_tok->isOperatorArithmetic() &&
                           m_tok->type() != Python::Token::T_IdentifierBuiltin &&
                           m_tok->type() != Python::Token::T_DelimiterOpenParen &&
                           m_tok->type() != Python::Token::T_DelimiterOpenBrace &&
                           m_tok->type() != Python::Token::T_DelimiterOpenBracket &&
                           paramType != Python::SourceParameter::InValid)
                {
                    std::string msg = "Expected identifier, got '" + m_tok->text() + "'";
                    m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, msg);
                }
            }
        }
        moveNext();
    }
}

Python::SourceRoot::TypeInfo Python::SourceParser::guessIdentifierType(const Python::Token *token)
{
    DEFINE_DBG_VARS

    Python::SourceRoot::TypeInfo typeInfo;
    if (token && (token = token->next())) {
        DBG_TOKEN(token)
        if (token->type() == Python::Token::T_DelimiterColon) {
            // type hint like foo : None = Nothing
            std::string explicitType = token->next()->text();
            Python::SourceRoot *root = Python::SourceRoot::instance();
            typeInfo.type = root->mapMetaDataType(explicitType);
            if (typeInfo.type == Python::SourceRoot::CustomType) {
                typeInfo.customNameIdx = root->indexOfCustomTypeName(explicitType);
                if (typeInfo.customNameIdx < 0)
                    typeInfo.customNameIdx = root->addCustomTypeName(explicitType);
            }
        } else if (token->type() == Python::Token::T_OperatorEqual) {
            // ordinare assignment foo = Something
            typeInfo.type = Python::SourceRoot::ReferenceType;
        }
    }
    return typeInfo;
}


void Python::SourceParser::gotoNextLine()
{
    DEFINE_DBG_VARS

    int guard = 200;
    Python::TokenLine *tokLine = m_tok->ownerLine()->nextLine();
    if (!tokLine)
        return;
    while(tokLine && (guard--)) {
        if (tokLine->isCodeLine()) {
            moveToken(tokLine->front());
            DBG_TOKEN(m_tok)
            break;
        }
        tokLine = tokLine->nextLine();
    }
}

void Python::SourceParser::handleDedent(Python::SourceIndent &indent)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(m_tok)
    if (!m_tok)
        return;

    // rationale here is to back up until we find a line with less indent
    // def func(arg1):     <- frameIndent
    //     var1 = None     <- previousBlockIndent
    //     if (arg1):      <- triggers change in currentBlockIndent
    //         var1 = arg1 <- currentBlockIndent

    if (!indent.validIndentLine(m_tok))
        return;

    uint ind = m_tok->ownerLine()->indent();

    if (ind < indent.currentBlockIndent()) {
        // dedent

        // backup until last codeline
        Python::TokenLine *linePrev = m_activeModule->lexer()->previousCodeLine(
                                                m_tok->ownerLine()->previousLine());

        if (ind > indent.previousBlockIndent()) {
            m_tok->ownerLine()->setIndentErrorMsg(m_tok, "Uneven indentation"); // uneven indentation
        } else if (linePrev){

            Python::Token *tok = linePrev->back();
            DBG_TOKEN(tok)
            int indStackSize = indent.atIndentBlock();
            while(tok && tok->type() == Token::T_Dedent) {
                indent.popBlock();
                PREV_TOKEN(tok)
            }

            if (indStackSize <= indent.atIndentBlock())
                m_tok->ownerLine()->setIndentErrorMsg(m_tok, "No previous dedent token");

            if (indent.framePopCnt() > 0) {
                // save end token for this frame
                if (linePrev)
                    m_activeFrame->setLastToken(linePrev->back());
                return;
            }
        }
    }
}

const Python::TokenLine *Python::SourceParser::frameStartLine(const Python::Token *semiColonTok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(semiColonTok)


    // rationale here is to back up until we find a line with less indent
    // def func(arg1):     <- frameIndent
    //     var1 = None     <- previousBlockIndent
    //     if (arg1):      <- triggers change in currentBlockIndent
    //         var1 = arg1 <- currentBlockIndent

    // is it a frame or just a block
    const Python::Token *tok = semiColonTok;
    const Python::TokenLine *line = tok->ownerLine();
    DBG_TOKEN(tok)
    uint guard = 1000;
    while (tok && (--guard)) {
        if (tok->isIdentifierFrameStart())
            return line;
        if (tok->ownerLine() != line) {
            if (tok->ownerLine()->parenCnt() > 0 &&
                tok->ownerLine()->braceCnt() > 0 &&
                tok->ownerLine()->bracketCnt() >0)
            {
                line = tok->ownerLine();
            } else
                return nullptr;
        }
        PREV_TOKEN(tok)
    }

    return nullptr;
}

void Python::SourceParser::handleIndent(Python::SourceIndent &indent)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(m_tok)
    if (!m_tok)
        return;

    if (indent.currentBlockIndent() < m_tok->ownerLine()->indent()) {
        if (m_tok->type() == Token::T_Indent)
            indent.pushBlock(m_tok->ownerLine()->indent());
        else if (m_tok->isCode()) {
            auto prevLine = Python::Lexer::previousCodeLine(
                                       m_tok->ownerLine()->previousLine());
            if (prevLine && prevLine->parenCnt() == 0 &&
                prevLine->bracketCnt() == 0 && prevLine->braceCnt() == 0)
            {
                m_tok->ownerLine()->setIndentErrorMsg(m_tok, "Uneven indentation");
            }
        }
    }

    return;
/*
    //if (!indent.validIndentLine(m_tok))
    //    return;

    uint ind = m_tok->ownerLine()->indent();

    if (ind > indent.currentBlockIndent()) {
        // indent detected
       if (m_tok->ownerLine()->front()->type() != Token::T_Indent)
           m_tok->ownerLine()->setIndentErrorMsg(m_tok, "No indentToken");

        // find previous ':'
        // backup until last codeline
        Python::TokenLine *linePrev = m_activeModule->lexer()->previousCodeLine(
                                                m_tok->ownerLine()->previousLine());
        if (!linePrev)
            return;

        const Python::Token *tokIt = linePrev->back();
        DBG_TOKEN(tokIt)
        int guard = 100;
        while (tokIt && tokIt->ownerLine() == linePrev && (--guard)) {
            if (tokIt->type() == Token::T_DelimiterColon && tokIt->previous() &&
                tokIt->previous()->type() != Token::T_DelimiterBackSlash)
            {
                // is it a frame or just a block
                auto frmLine = frameStartLine(tokIt);
                if (frmLine)
                    indent.pushFrameBlock(indent.currentBlockIndent(), ind);
                else
                    indent.pushBlock(ind);
                return;
            }
            PREV_TOKEN(tokIt)
        }

        // it might be an indent froma continued line
        if (linePrev->parenCnt() != 0 || linePrev->braceCnt() != 0 ||
            linePrev->bracketCnt() != 0)
        {
            return;
        }

        // syntax error
        m_tok->ownerLine()->setSyntaxErrorMsg(m_tok, "Blockstart without ':'");
        //if (direction > 0)
        //    indent.pushFrameBlock(indent.currentBlockIndent(), ind);
        //else
            indent.pushBlock(ind);
    }
    */
}

void Python::SourceParser::gotoEndOfLine()
{
    DEFINE_DBG_VARS

    int guard = 80;
    int newLines = 0;
    while (m_tok && (guard--)) {
        switch (m_tok->type()) {

        case Python::Token::T_DelimiterBackSlash:
            --newLines; break;
        case Python::Token::T_DelimiterNewLine:
            if (++newLines > 0)
                return;
            FALLTHROUGH
        default: ;// nothing
        }
        moveNext();
    }
}
