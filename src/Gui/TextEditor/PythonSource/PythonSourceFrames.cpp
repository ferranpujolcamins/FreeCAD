#include "PythonSourceFrames.h"

#include "PythonSource.h"
#include "PythonSourceRoot.h"
#include "PythonSourceModule.h"
#include <list>
#include <QDebug>

DBG_TOKEN_FILE

using namespace Gui;

Python::SourceFrameReturnType::SourceFrameReturnType(Python::SourceListParentBase *owner,
                                       const Python::SourceModule *module,
                                       Python::Token *tok) :
    Python::SourceListNodeBase(owner),
    m_module(module)
{
    assert(module != nullptr && "Must get a valid owner");
    assert(module != nullptr && "Must get a vaild module");
    setToken(tok);
}

Python::SourceFrameReturnType::~SourceFrameReturnType()
{
}

void Python::SourceFrameReturnType::setTypeInfo(Python::SourceRoot::TypeInfo typeInfo)
{
    m_typeInfo = typeInfo;
}

Python::SourceRoot::TypeInfo Python::SourceFrameReturnType::returnType() const
{
    if (isYield()) {
        Python::SourceRoot::TypeInfo typeInfo;
        typeInfo.type = Python::SourceRoot::GeneratorType;
        return typeInfo;
    }

    // compute return type of statement
    return m_module->root()->statementResultType(m_token,
                                                 m_module->getFrameForToken(
                                                     m_token, m_module->rootFrame()));
}

bool Python::SourceFrameReturnType::isYield() const
{
    return m_token->type() == Python::Token::T_KeywordYield;
}

Python::SourceRoot::TypeInfo Python::SourceFrameReturnType::yieldType() const
{
    Python::SourceRoot::TypeInfo typeInfo;
    if (isYield())
        return m_module->root()->statementResultType(m_token,
                                                     m_module->getFrameForToken(
                                                         m_token, m_module->rootFrame()));

    return typeInfo;
}

// ----------------------------------------------------------------------------


Python::SourceFrameReturnTypeList::SourceFrameReturnTypeList(Python::SourceListParentBase *owner,
                                                                const Python::SourceModule *module) :
    Python::SourceListParentBase(owner),
    m_module(module)
{
    m_preventAutoRemoveMe = true;
}

Python::SourceFrameReturnTypeList::SourceFrameReturnTypeList(const Python::SourceFrameReturnTypeList &other) :
    Python::SourceListParentBase(other),
    m_module(other.m_module)
{
}

Python::SourceFrameReturnTypeList::~SourceFrameReturnTypeList()
{
}

int Python::SourceFrameReturnTypeList::compare(const Python::SourceListNodeBase *left, const Python::SourceListNodeBase *right) const
{
    const Python::SourceFrameReturnType *l = dynamic_cast<const Python::SourceFrameReturnType*>(left),
                             *r = dynamic_cast<const Python::SourceFrameReturnType*>(right);
    assert(l != nullptr && r != nullptr && "Non Python::SourceReturnList items in returnList");
    if (*l->token() < *r->token())
        return +1;
    return -1; // r must be bigger
}



// -----------------------------------------------------------------------------

Python::SourceFrame::SourceFrame(Python::SourceFrame *owner,
                                 Python::SourceModule *module,
                                 Python::SourceFrame *parentFrame,
                                 bool isClass):
    Python::SourceListParentBase(owner),
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

Python::SourceFrame::SourceFrame(Python::SourceModule *owner,
                                     Python::SourceModule *module,
                                     Python::SourceFrame *parentFrame,
                                     bool isClass):
    Python::SourceListParentBase(owner),
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

Python::SourceFrame::~SourceFrame()
{
    if (m_typeHint)
        delete m_typeHint;
}

const std::string Python::SourceFrame::name() const
{
    if (isModule())
        return "<" + m_module->moduleName() + ">";

    const Python::SourceIdentifier *ident = parentFrame()->identifiers()
                                                .getIdentifier(token()->text());
    if (ident)
        return ident->name();

    return "<unbound>" + m_token->text();
}

const std::string Python::SourceFrame::docstring()
{
    DEFINE_DBG_VARS

    // retrive docstring for this function
    std::list<const std::string> docStrs;
    if (m_token) {
        const Python::Token *token = scanAllParameters(const_cast<Python::Token*>(m_token), false, false);
        DBG_TOKEN(token)
        int guard = 20;
        while(token && token->type() != Python::Token::T_DelimiterSemiColon) {
            if ((--guard) <= 0)
                return std::string();
            NEXT_TOKEN(token)
        }

        if (!token)
            return std::string();

        // now we are at semicolon (even with typehint)
        //  def func(arg1,arg2) -> str:
        //                            ^
        // find first string, before other stuff
        while (token) {
            if (!token->ownerLine())
                continue;
            switch (token->type()) {
            case Python::Token::T_LiteralBlockDblQuote:// fallthrough
            case Python::Token::T_LiteralBlockSglQuote: {
                // multiline
                docStrs.push_back(token->text().substr(3, token->text().length() - 6)); // clip """
            }   break;
            case Python::Token::T_LiteralDblQuote: // fallthrough
            case Python::Token::T_LiteralSglQuote: {
                // single line;
                docStrs.push_back(token->text().substr(1, token->text().length() - 2)); // clip "
            }   break;
            case Python::Token::T_Indent: // fallthrough
            case Python::Token::T_Comment:
                NEXT_TOKEN(token)
                break;
            default:
                // some other token, probably some code
                token = nullptr; // bust out of while
                DBG_TOKEN(token)
            }
        }

        // return what we have collected, why doesn't std::list have a join?
        std::string ret;
        for (const std::string &s : docStrs) {
            if (!ret.empty())
                ret += '\n';
            ret += s;
        }
        return ret;
    }

    return std::string(); // no token
}

Python::SourceRoot::TypeInfo Python::SourceFrame::returnTypeHint() const
{
    Python::SourceRoot::TypeInfo typeInfo;
    if (parentFrame()) {
        // get typehint from parentframe identifier for this frame
        const Python::SourceIdentifier *ident = parentFrame()->getIdentifier(m_token->text());
        if (ident) {
            Python::SourceTypeHintAssignment *assign = ident->getTypeHintAssignment(m_token->line());
            if (assign)
                typeInfo = assign->typeInfo();
        }
    }

    return typeInfo;

}

const Python::SourceIdentifier *Python::SourceFrame::getIdentifier(const std::string &name) const {
    const Python::SourceIdentifier *ident = m_identifiers.getIdentifier(name);
    if (ident)
        return ident;
    // recurse upwards to lookup
    if (!m_parentFrame)
        return nullptr;
    return parentFrame()->getIdentifier(name);
}

Python::Token *Python::SourceFrame::scanFrame(Python::Token *startToken, Python::SourceIndent &indent)
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

    Python::Token *tok = startToken;
    Python::TokenLine *tokLine = tok->ownerLine();
    DBG_TOKEN(tok)

    uint guard = tokLine->ownerList()->max_size(); // max number of rows scanned
    while (tok && (--guard)) {
        tokLine = tok->ownerLine();
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


Python::Token *Python::SourceFrame::scanLine(Python::Token *startToken,
                                               Python::SourceIndent &indent)
{
    DEFINE_DBG_VARS

    if (!startToken)
        return startToken;

    Python::Token *tok = startToken,
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
        m_type.type = Python::SourceRoot::ModuleType;

    // do framestart if it is first row in document
    bool isFrameStarter = tok->line() == 0;

    if (tok->type() == Python::Token::T_IdentifierFunction ||
        tok->type() == Python::Token::T_IdentifierMethod ||
        tok->type() == Python::Token::T_IdentifierSuperMethod ||
        tok->type() == Python::Token::T_IdentifierClass ||
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
                if (m_parentFrame->isClass() && frmStartTok->text() == "__init__")
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

    Python::SourceModule *noConstModule = const_cast<Python::SourceModule*>(m_module);

    guard = 200; // max number of tokens in this line, unhang locked loop
    // scan document
    while (tok && (guard--)) {
        switch(tok->type()) {
        case Python::Token::T_KeywordReturn:
            tok = scanReturnStmt(tok);
            continue;
        case Python::Token::T_KeywordYield:
            tok = scanYieldStmt(tok);
            continue;
        case Python::Token::T_DelimiterMetaData: {
            // return typehint -> 'Type'
            // make sure previous char was a ')'
            Python::Token *tmpTok = tok->previous();
            int guardTmp = 20;
            while (tmpTok && !tmpTok->isCode() && guardTmp--)
                PREV_TOKEN(tmpTok)

            if (tmpTok && tmpTok->type() != Python::Token::T_DelimiterCloseParen)
                m_module->setSyntaxError(tok, std::string("Unexpected '") +
                                         tmpTok->text() + "' before '->'");
            else {
                // its a valid typehint

                Python::SourceRoot::TypeInfo typeInfo;
                const Python::SourceIdentifier *ident;
                Python::Token *typeHintTok = tok->next();
                // store this typehint
                tok = scanRValue(typeHintTok, typeInfo, true);
                if (typeInfo.isValid() && typeHintTok && parentFrame()) {
                    // we don't store typehint in this frame, rather we lookup
                    // our parentframe identifier for this function and stes typehint on that
                    ident = parentFrame()->getIdentifier(m_token->text());
                    if (ident)
                        const_cast<Python::SourceIdentifier*>(ident)->setTypeHint(typeHintTok, typeInfo);
                }
                DBG_TOKEN(tok)
            }

        } break;
        case Python::Token::T_DelimiterNewLine:
            if (!m_module->root()->isLineEscaped(tok)){
                // insert Blockstart if we have a semicolon, non code tokens might exist before
                Python::Token *prevTok = tok->previous();
                int guard = 10;
                while (prevTok && (--guard)) {
                    if (prevTok->isCode()) {
                        if (prevTok->type() == Python::Token::T_DelimiterColon)
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
        case Python::Token::T_IdentifierDefUnknown: {
            // first handle indents to get de-dent
            tok = handleIndent(tok, indent, -1);
            if (indent.framePopCnt())
                return tok->previous();

            // a function that can be function or method
            // determine what it is
            if (m_isClass && indent.frameIndent() == tok->ownerLine()->indent()) {
                if (tok->text().substr(0, 2) == "__" &&
                    tok->text().substr(tok->text().length()-3, 2) == "__")
                {
                    const_cast<Python::Token*>(tok)->changeType(Python::Token::T_IdentifierSuperMethod);
                } else {
                    const_cast<Python::Token*>(tok)->changeType(Python::Token::T_IdentifierMethod);
                }
                noConstModule->tokenTypeChanged(tok);
                continue; // switch on new token state
            }
            const_cast<Python::Token*>(tok)->changeType(Python::Token::T_IdentifierFunction);
            noConstModule->tokenTypeChanged(tok);
            continue; // switch on new token state
        }
        case Python::Token::T_IdentifierClass: {
            Python::SourceRoot::TypeInfo tpInfo(Python::SourceRoot::ClassType);
            m_identifiers.setIdentifier(tok, tpInfo);
            tok = startSubFrame(tok, indent, Python::SourceRoot::ClassType);
            DBG_TOKEN(tok)
            if (indent.framePopCnt()> 0)
                return tok;
            continue;
        }
        case Python::Token::T_IdentifierFunction: {
            tok = startSubFrame(tok, indent, Python::SourceRoot::FunctionType);
            DBG_TOKEN(tok)
            if (indent.framePopCnt()> 0)
                return tok;
            continue;
        }
        case Python::Token::T_IdentifierSuperMethod:
            if (isClass() &&
                tok->text() == "__init__")
            {
                // parameters for this class, we scan in class scope
                // ie before we create a new frame due to parameters for this class must be
                // owned by class frame
                scanAllParameters(tok->next(), true, true);
                DBG_TOKEN(tok)
            }
            // fallthrough
        [[clang::fallthrough]];
        case Python::Token::T_IdentifierMethod: {
            if (!isClass())
                m_module->setSyntaxError(tok, "Method without class");

            tok = startSubFrame(tok, indent, Python::SourceRoot::MethodType);
            DBG_TOKEN(tok)
            if (indent.framePopCnt()> 0)
                return tok;
            continue;
        }
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

Python::Token *Python::SourceFrame::startSubFrame(Python::Token *tok,
                                              Python::SourceIndent &indent,
                                              Python::SourceRoot::DataTypes type)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    // first handle indents to get de-dent
    tok = handleIndent(tok, indent, -1);
    if (indent.framePopCnt())
        return tok->previous();

    // add function frame
    bool classFrm = type == Python::SourceRoot::ClassType;
    Python::SourceFrame *funcFrm = new Python::SourceFrame(this, m_module, this, classFrm);
    funcFrm->setToken(tok);
    insert(funcFrm);

    // store this as a function identifier
    Python::SourceRoot::TypeInfo typeInfo;
    typeInfo.type = type;
    m_identifiers.setIdentifier(tok, typeInfo);

    // scan this frame
    int lineIndent = tok->ownerLine()->indent();
    if (indent.frameIndent() < lineIndent)
         indent.pushFrameBlock(lineIndent, lineIndent);
    tok = funcFrm->scanFrame(tok, indent);
    DBG_TOKEN(tok)
    //if (tok && m_lastToken.token() && (*m_lastToken.token() < *tok))
    //    m_lastToken.setToken(tok);

    return tok;
}


// scans a for aditional TypeHint and scans rvalue
Python::Token *Python::SourceFrame::scanIdentifier(Python::Token *tok)
{
    DEFINE_DBG_VARS

    int guard = 20;
    Python::SourceIdentifier *ident = nullptr;
    const Python::SourceIdentifier *parentIdent = nullptr;
    Python::Token *identifierTok = nullptr; // the identifier that gets assigned
    Python::SourceRoot::TypeInfo typeInfo;

    while (tok && (guard--)) {
        // TODO figure out how to do tuple assignment

        switch (tok->type()) {
        case Python::Token::T_IdentifierFalse:
            typeInfo.type = Python::SourceRoot::BoolType;
            m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case Python::Token::T_IdentifierTrue:
            typeInfo.type = Python::SourceRoot::BoolType;
            m_identifiers.setIdentifier(tok, typeInfo);
            break;
        case Python::Token::T_IdentifierNone:
            typeInfo.type = Python::SourceRoot::NoneType;
            m_identifiers.setIdentifier(tok, typeInfo);
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
                        m_module->setLookupError(identifierTok, msg);
                    }

                } else if (identifierTok->type() == Python::Token::T_IdentifierBuiltin) {
                    Python::SourceRoot::TypeInfoPair tpPair =
                            Python::SourceRoot::instance()->builtinType(identifierTok, this);
                   if (!tpPair.thisType.isCallable()) {
                       std::string msg = "Calling builtin '" + identifierTok->text() +
                                                "' is a non callable";
                       m_module->setLookupError(identifierTok, msg);
                   }
                }

            }
            return tok;
            //break;
        case Python::Token::T_DelimiterPeriod:
            if (identifierTok) {
                if (parentIdent)
                    goto parent_check;
                else if (identifierTok->type() == Python::Token::T_IdentifierUnknown)
                    parentIdent = lookupIdentifierReference(identifierTok);
                else
                    parentIdent = getIdentifier(tok->text());

                if (!parentIdent) {
                    identifierTok->changeType(Python::Token::T_IdentifierInvalid);
                    m_module->setLookupError(identifierTok);
                }
            } else {
                module()->setSyntaxError(tok, "Unexpected '.'");
                return tok;
            }
            break;
        case Python::Token::T_IdentifierBuiltin: {
            Python::SourceRoot::TypeInfoPair tpPair =
                    Python::SourceRoot::instance()->builtinType(tok, this);
            m_identifiers.setIdentifier(tok, tpPair.thisType);
            identifierTok = tok;
        }   break;
        case Python::Token::T_OperatorEqual:
            // assigment
            if (!identifierTok) {
                m_module->setSyntaxError(tok, "Unexpected =");
            } else {
                tok = scanRValue(tok, typeInfo, false);
                ident = m_identifiers.setIdentifier(identifierTok, typeInfo);
            }
            if (identifierTok && identifierTok->type() == Python::Token::T_IdentifierUnknown) {
                identifierTok->changeType(Python::Token::T_IdentifierDefined);
                m_module->tokenTypeChanged(identifierTok);
            }
            break;
        case Python::Token::T_DelimiterColon:
            // type hint or blockstart
            if (tok->next() && !tok->next()->isCode()) {
                // its not a typehint, its probably a blockstart
                return tok;
            } else if (ident) {
                m_module->setSyntaxError(tok, "Unexpected ':'");
                return tok;
            } else if (!identifierTok) {
                m_module->setSyntaxError(tok, "Unexpected =");
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
parent_check:
            if (parentIdent && identifierTok) {
                const Python::SourceFrame *frm = parentIdent->callFrame(tok->previous());
                if (frm) {
                    parentIdent = frm->getIdentifier(parentIdent->name());
                    if (!parentIdent) {
                        identifierTok->changeType(Python::Token::T_IdentifierInvalid);
                        m_module->setLookupError(tok->previous());
                    } else {
                        identifierTok->changeType(Python::Token::T_IdentifierDefined);
                    }
                    m_module->tokenTypeChanged(identifierTok);
                }
            }

            if (!tok->isCode()) {
                if (identifierTok && !parentIdent)
                    lookupIdentifierReference(identifierTok);
                return tok;
            } else if (!tok->isIdentifierVariable() &&
                       tok->type() != Python::Token::T_DelimiterPeriod)
            {
                lookupIdentifierReference(identifierTok);
                return tok;
            } else if (tok->isIdentifierVariable())
                identifierTok = tok;

            break;
        }

        NEXT_TOKEN(tok)
    }

    return tok;
}

const Python::SourceIdentifier *Python::SourceFrame::lookupIdentifierReference(Python::Token *tok)
{
    const Python::SourceIdentifier *tmpIdent = nullptr;
    if (tok && tok->type() == Python::Token::T_IdentifierUnknown) {
        Python::SourceRoot::TypeInfo typeInfo;
        tmpIdent = getIdentifier(tok->text());

        // default to invalid
        tok->changeType(Python::Token::T_IdentifierInvalid);

        if (!tmpIdent) {
            m_module->setLookupError(tok);
        } else {
            Python::SourceIdentifierAssignment *assign = tmpIdent->getFromPos(tok);
            if (assign && assign->typeInfo().isValid()) {
                tok->changeType(Python::Token::T_IdentifierDefined);
                if (tmpIdent->isImported(tok))
                    typeInfo.type = Python::SourceRoot::ReferenceImportType;
                else if (tmpIdent->isCallable(tok))
                    typeInfo.type = Python::SourceRoot::ReferenceCallableType;
                else
                    typeInfo.type = Python::SourceRoot::ReferenceType;
                m_identifiers.setIdentifier(tok, typeInfo);
            }
        }
        m_module->tokenTypeChanged(tok);
    }
    return tmpIdent;
}

Python::Token *Python::SourceFrame::scanRValue(Python::Token *tok,
                                           Python::SourceRoot::TypeInfo &typeInfo,
                                           bool isTypeHint)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    typeInfo = Python::SourceRoot::TypeInfo(); // init with clean type

    int guard = 20;
    int parenCnt = 0,
        bracketCnt = 0,
        braceCnt = 0;

    const Python::SourceIdentifier *ident = nullptr;

    while (tok && (guard--)) {
        if (!tok->isCode())
            return tok;
        switch(tok->type()) {
        case Python::Token::T_DelimiterOpenParen:
            if (!isTypeHint)
                return tok;
            ++parenCnt; break;
        case Python::Token::T_DelimiterCloseParen:
            if (!isTypeHint)
                return tok;
            --parenCnt; break;
        case Python::Token::T_DelimiterOpenBracket:
            if (!isTypeHint)
                return tok;
            ++bracketCnt; break;
        case Python::Token::T_DelimiterCloseBracket:
            if (!isTypeHint)
                return tok;
            --bracketCnt; break;
        case Python::Token::T_DelimiterOpenBrace:
            if (!isTypeHint)
                return tok;
            ++braceCnt; break;
        case Python::Token::T_DelimiterCloseBrace:
            if (!isTypeHint)
                return tok;
            --braceCnt; break;
        case Python::Token::T_DelimiterPeriod:
            // might have obj.sub1.sub2
            break;
        case Python::Token::T_DelimiterColon:
            if (!isTypeHint) {
                std::string msg = "Unexpected '" + tok->text() + "'";
                m_module->setSyntaxError(tok, msg);
                return tok;
            }
            // else do next token
            break;
        case Python::Token::T_OperatorEqual:
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
                std::string text = tok->text();// one roundtrip to document
                if (!isTypeHint) {
                    // ordinary RValue
                    if (tok->isInt())
                        typeInfo.type = Python::SourceRoot::IntType;
                    else if (tok->isFloat())
                        typeInfo.type = Python::SourceRoot::FloatType;
                    else if (tok->isString())
                        typeInfo.type = Python::SourceRoot::StringType;
                    else if (tok->isKeyword()) {
                        if (text == "None")
                            typeInfo.type = Python::SourceRoot::NoneType;
                        else if (text == "False" ||
                                 text == "True")
                        {
                            typeInfo.type = Python::SourceRoot::BoolType;
                        }
                    }
                }

                if (tok->isIdentifierVariable()){
                    if (tok->type() == Python::Token::T_IdentifierBuiltin) {
                        typeInfo.type = Python::SourceRoot::ReferenceBuiltInType;
                    } else {
                        Python::SourceFrame *frm = this;
                        do {
                            if (!frm->isClass())
                                ident = frm->identifiers().getIdentifier(text);
                        } while (!ident && (frm = frm->parentFrame()));

                        // finished lookup
                        if (ident) {
                            typeInfo.type = Python::SourceRoot::ReferenceType;
                            typeInfo.referenceName = text;
                            if (tok->type() == Python::Token::T_IdentifierUnknown) {
                                tok->changeType(Python::Token::T_IdentifierDefined);
                                m_module->tokenTypeChanged(tok);
                            }
                        } else if (isTypeHint) {
                            std::string msg = "Unknown type '" + text + "'";
                            m_module->setLookupError(tok, msg);
                        } else {
                            // new identifier
                            std::string msg = "Unbound variable in RValue context '" + tok->text() + "'";
                            m_module->setLookupError(tok, msg);
                            typeInfo.type = Python::SourceRoot::ReferenceType;
                        }
                    }
                    return tok;

                } else if (tok->isIdentifierDeclaration()) {
                    if (tok->isBoolean())
                        typeInfo.type = Python::SourceRoot::BoolType;
                    else if (tok->type() == Python::Token::T_IdentifierNone)
                        typeInfo.type = Python::SourceRoot::NoneType;
                } else if (tok->isCode()) {
                    if (isTypeHint)
                        typeInfo.type = Python::SourceRoot::instance()->mapMetaDataType(tok->text());
                    if (tok->isNumber())
                        typeInfo.type = Python::SourceRoot::instance()->numberType(tok);
                    else if (tok->isString())
                        typeInfo.type = Python::SourceRoot::StringType;
                    else {
                        std::string msg =  "Unexpected code (" + text + ")";
                        m_module->setSyntaxError(tok, msg);
                    }
                    return tok;
                }
            }
        }
        NEXT_TOKEN(tok)
    }

    return tok;
}

Python::Token *Python::SourceFrame::scanImports(Python::Token *tok)
{
    DEFINE_DBG_VARS

    int guard = 20;
    std::list<const std::string> fromPackages, importPackages, modules;
    std::string alias;
    bool isAlias = false;
    bool isImports = tok->previous()->type() == Python::Token::T_KeywordImport;
    Python::Token *moduleTok = nullptr,
                *aliasTok = nullptr;

    while (tok && (guard--)) {
        std::string text = tok->text();
        switch(tok->type()) {
        case Python::Token::T_KeywordImport:
            isImports = true;
            break;
        case Python::Token::T_KeywordFrom:
            if (isImports) {
                std::string msg = "Unexpected token '" + text + "'";
                m_module->setSyntaxError(tok, msg);
            }
            break;
        case Python::Token::T_KeywordAs:
            isAlias = true;
            break;
        case Python::Token::T_DelimiterNewLine:
            if (!m_module->root()->isLineEscaped(tok)) {
                guard = 0; // we are finished, bail out
                if (modules.size() > 0) {
                    if (isAlias)
                        m_module->setSyntaxError(tok, "Expected aliasname before newline");
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
            m_imports.setModuleGlob(all);
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
                m_module->setSyntaxError(tok, msg);
            }
            goto set_identifier;
        default:
            if (tok->type() == Python::Token::T_DelimiterPeriod &&
                tok->previous() && tok->previous()->isImport())
                break; // might be "import foo.bar ..."
                       //                     ^
            if (tok->isCode()) {
                std::string msg = "Unexpected token '" + text + "'";
                m_module->setSyntaxError(tok, msg);
            }
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
                if (modules.size()) {
                    importPackages.push_back(modules.back());
                    modules.pop_back();
                }
                modules.push_back(text);
                moduleTok = tok;
                goto next_token;
            } else {
                alias = text;
                aliasTok = tok;
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
            imp = m_imports.setModule(all, modules.front(), alias);
        } else {
            // store import and set identifier as modulename
            imp = m_imports.setModule(all, modules.front());
        }
        Python::SourceRoot::TypeInfo typeInfo(imp->type());
        m_identifiers.setIdentifier(aliasTok ? aliasTok : moduleTok, typeInfo);

        alias.clear();
        importPackages.clear();
        modules.clear();
        isAlias = false;
        moduleTok = aliasTok = nullptr;
        goto next_token;
        }
    }
    return tok;
}

// scans a single statement
Python::Token *Python::SourceFrame::scanReturnStmt(Python::Token *tok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    Python::SourceRoot::TypeInfo typeInfo = m_module->root()->statementResultType(tok, this);
    // store it
    Python::SourceFrameReturnType *frmType = new Python::SourceFrameReturnType(&m_returnTypes, m_module, tok);
    frmType->setTypeInfo(typeInfo);
    m_returnTypes.insert(frmType);

    // set errormessage if we have code after return on this indentation level
    scanCodeAfterReturnOrYield(tok, "return");

    // advance token til next statement
    return gotoEndOfLine(tok);
}

Python::Token *Python::SourceFrame::scanYieldStmt(Python::Token *tok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    assert(tok && tok->type() == Python::Token::T_KeywordYield && "Expected a yield token");

    if (!m_returnTypes.empty() && m_returnTypes.hasOtherTokens(Python::Token::T_KeywordYield)) {
        // we have other tokens !
        std::string msg = "Setting yield in a function with a return statement\n";
        m_module->setMessage(tok, msg);
    }
    // TODO implement yield

    // set errormessage if we have code after return on this indentation level
    scanCodeAfterReturnOrYield(tok, "yield");

    return gotoEndOfLine(tok);
}

void Python::SourceFrame::scanCodeAfterReturnOrYield(Python::Token *tok, const std::string &name)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    // check if we have code after this return with same block level
    int guard = 10;
    Python::TokenLine *tokLine = tok->ownerLine();
    do {
        tokLine = tokLine->nextLine();
        if (tokLine && tokLine->isCodeLine()) {
            if (tokLine->indent() == tok->ownerLine()->indent()) {
                std::string msg = "Code after a '" + name + "' will never run.";
                m_module->setMessage(tokLine->firstCodeToken(), msg);
            }
            break;
        }
    } while (tokLine && (--guard));
}

// may return nullptr on error
Python::Token *Python::SourceFrame::scanAllParameters(Python::Token *tok, bool storeParameters, bool isInitFunc)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    // safely goes to closingParen of arguments list
    int parenCount = 0,
        safeGuard = 50;
    while(tok && (safeGuard--) > 0 &&
          tok->type() != Python::Token::T_DelimiterMetaData)
    {
        // first we must make sure we clear all parens
        if (tok->type() == Python::Token::T_DelimiterOpenParen)
            ++parenCount;
        else if (tok->type() == Python::Token::T_DelimiterCloseParen)
            --parenCount;

        if (parenCount <= 0 && tok->type() == Python::Token::T_DelimiterCloseParen)
            return tok->next(); // so next caller knows where it ended

        // advance one token
        NEXT_TOKEN(tok)

        // scan parameters
        if (storeParameters && parenCount > 0) {
            if (tok->isIdentifier() ||
                tok->type() == Python::Token::T_OperatorVariableParam ||
                tok->type() == Python::Token::T_OperatorKeyWordParam)
            {
                tok = scanParameter(tok, parenCount, isInitFunc); // also should handle typehints
                DBG_TOKEN(tok)
            }
        }
    }

    return tok;
}

Python::Token *Python::SourceFrame::scanParameter(Python::Token *paramToken, int &parenCount, bool isInitFunc)
{
    DEFINE_DBG_VARS

    // safely constructs our parameters list
    int safeGuard = 20;

    // init empty
    Python::SourceParameter *param = nullptr;
    Python::SourceParameter::ParameterType paramType = Python::SourceParameter::InValid;

    // scan up to ',' or closing ')'
    while(paramToken && (safeGuard--))
    {
        // might have subparens
        switch (paramToken->type()) {
        case Python::Token::T_DelimiterComma:
            if (parenCount == 1)
                return paramToken;
            break;
        case Python::Token::T_DelimiterOpenParen:
            ++parenCount;
            break;
        case Python::Token::T_DelimiterCloseParen:
            --parenCount;
            if (parenCount == 0)
                return paramToken;
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
            if (!m_module->root()->isLineEscaped(paramToken))
                return paramToken->previous(); // caller must know where newline is at
            break;
        case Python::Token::T_DelimiterColon:
            // typehint is handled when param is constructed below, just make sure we warn for syntax errors
            if (paramType != Python::SourceParameter::Positional &&
                paramType != Python::SourceParameter::PositionalDefault)
            {
                m_module->setSyntaxError(paramToken, "Unexpected ':'");
            }
            break;
        case Python::Token::T_OperatorEqual:
            // we have a default value, actual reference is handled dynamically when we call getReferenceBy on this identifier
            if (paramType != Python::SourceParameter::PositionalDefault) {
                m_module->setSyntaxError(paramToken, "Unexpected '='");
            }
            if (parenCount == 1) {
                do { // clear this identifier
                    NEXT_TOKEN(paramToken)
                } while (paramToken && paramToken->isIdentifier() && (safeGuard--));
            }
            continue; // switch on this token, hopefully its a ','
        default:
            if (parenCount > 0) {
                if (paramToken->isIdentifier()) {
                    // check if we have default value
                    if (paramType == Python::SourceParameter::InValid) {
                        const Python::Token *nextTok = paramToken->next();
                        if (nextTok && nextTok->type() == Python::Token::T_OperatorEqual)
                            paramType = Python::SourceParameter::PositionalDefault; // have default value
                        else
                            paramType = Python::SourceParameter::Positional;
                    }

                    // set identifier, first try with type hints
                    Python::SourceRoot::TypeInfo typeInfo = guessIdentifierType(paramToken);
                    if (!typeInfo.isValid())
                        typeInfo.type = Python::SourceRoot::ReferenceArgumentType;

                    m_identifiers.setIdentifier(paramToken, typeInfo);

                    // set parameter
                    param = m_parameters.setParameter(paramToken, typeInfo, paramType);

                    // Change tokenValue
                    if (paramToken->type() == Python::Token::T_IdentifierUnknown) {

                        if ((m_parentFrame->isClass() || (isInitFunc && isClass())) &&
                            m_parameters.indexOf(param) == 0)
                        {
                            paramToken->changeType(Python::Token::T_IdentifierSelf);
                        } else {
                            paramToken->changeType(Python::Token::T_IdentifierDefined);
                        }
                    }

                    // repaint in highligher
                    const_cast<Python::SourceModule*>(m_module)->tokenTypeChanged(paramToken);

                } else if(paramType != Python::SourceParameter::InValid) {
                    std::string msg = "Expected identifier, got '" + paramToken->text() + "'";
                    m_module->setSyntaxError(paramToken, msg);
                }
            }
        }

        NEXT_TOKEN(paramToken)
    }

    return paramToken;

}

Python::SourceRoot::TypeInfo Python::SourceFrame::guessIdentifierType(const Python::Token *token)
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

const Python::SourceFrameReturnTypeList Python::SourceFrame::returnTypes() const
{
    return m_returnTypes;
}

Python::Token *Python::SourceFrame::gotoNextLine(Python::Token *tok)
{
    DEFINE_DBG_VARS

    int guard = 200;
    Python::TokenLine *tokLine = tok->ownerLine()->nextLine();
    if (!tokLine)
        return nullptr;
    while(tokLine && (guard--)) {
        if (tokLine->isCodeLine()) {
            tok = tokLine->tokenAt(0);
            DBG_TOKEN(tok)
            break;
        }
        tokLine = tokLine->nextLine();
    }
    return tok;
}

Python::Token *Python::SourceFrame::handleIndent(Python::Token *tok,
                                             Python::SourceIndent &indent,
                                             int direction)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    int ind = tok->ownerLine()->indent();

    if (!indent.validIndentLine(tok))
        return tok;

    if (ind < indent.currentBlockIndent() && direction < 1) {
        // dedent

        // backup until last codeline
        int guard = 20;
        Python::TokenLine *linePrev = tok->ownerLine()->previousLine();
        while(linePrev && !linePrev->isCodeLine() && (guard--))
            linePrev = linePrev->previousLine();

        if (ind > indent.previousBlockIndent()) {
            m_module->setIndentError(tok); // uneven indentation
        } else if (linePrev){

            Python::Token *lastTok = linePrev->back();
            do { // handle when multiple blocks dedent in a single row
                indent.popBlock();
                if (linePrev) {// notify that this block has ended
                    lastTok = m_module->insertBlockEnd(lastTok);
                    DBG_TOKEN(lastTok)
                }
            } while(lastTok && indent.currentBlockIndent() > ind);

            if (indent.framePopCnt() > 0) {
                // save end token for this frame
                Python::TokenLine *line = tok->ownerLine()->previousLine();
                if (line)
                    m_lastToken.setToken(line->back());
                return tok;
            }
        }
    } else if (ind > indent.currentBlockIndent() && direction > -1) {
        // indent
        // find previous ':'
        const Python::Token *prev = tok->previous();
        DBG_TOKEN(prev)
        int guard = 20;
        while(prev && (guard--)) {
            switch (prev->type()) {
            case Python::Token::T_DelimiterColon:
                //m_module->insertBlockStart(tok);
                // fallthrough
            case Python::Token::T_BlockStart:
                indent.pushBlock(ind);
                prev = nullptr; // break while loop
                break;
            case Python::Token::T_Comment:
            case Python::Token::T_LiteralDblQuote:
            case Python::Token::T_LiteralBlockDblQuote:
            case Python::Token::T_LiteralSglQuote:
            case Python::Token::T_LiteralBlockSglQuote:
                PREV_TOKEN(prev)
                break;
            default:
                if (!prev->isCode()) {
                    PREV_TOKEN(prev)
                    break;
                }

                if (prev->ownerLine()->parenCnt() != 0 ||
                    prev->ownerLine()->braceCnt() != 0 ||
                    prev->ownerLine()->bracketCnt() != 0)
                {
                    return tok;
                }
                // syntax error
                m_module->setSyntaxError(tok, "Blockstart without ':'");
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

Python::Token *Python::SourceFrame::gotoEndOfLine(Python::Token *tok)
{
    DEFINE_DBG_VARS

    int guard = 80;
    int newLines = 0;
    while (tok && (guard--)) {
        switch (tok->type()) {
        case Python::Token::T_DelimiterLineContinue:
            --newLines; break;
        case Python::Token::T_DelimiterNewLine:
            if (++newLines > 0)
                return tok;
            // fallthrough
        [[clang::fallthrough]];
        default: ;// nothing
        }
        NEXT_TOKEN(tok)
    }

    return tok;
}
