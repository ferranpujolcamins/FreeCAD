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
 //   m_parameters(this),
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
//    m_parameters(this),
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
    for(auto &p : m_parameters)
        delete p;
    if (m_typeHint)
        delete m_typeHint;
}

const std::string Python::SourceFrame::name() const
{
    if (isModule())
        return "<" + m_module->moduleName() + ">";

    const Python::SourceIdentifier *ident = parentFrame()->identifiers()
                                                .getIdentifier(token()->hash());
    if (ident)
        return ident->name();

    return "<unbound>" + text();
}

const std::string Python::SourceFrame::docstring()
{
    /*
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
            case Python::Token::T_LiteralBlockDblQuote: FALLTHROUGH
            case Python::Token::T_LiteralBlockSglQuote: {
                // multiline
                docStrs.push_back(token->text().substr(3, token->text().length() - 6)); // clip """
            }   break;
            case Python::Token::T_LiteralDblQuote: FALLTHROUGH
            case Python::Token::T_LiteralSglQuote: {
                // single line;
                docStrs.push_back(token->text().substr(1, token->text().length() - 2)); // clip "
            }   break;
            case Python::Token::T_Indent: FALLTHROUGH
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
*/
    return std::string(); // no token
}


Python::SourceRoot::TypeInfo Python::SourceFrame::returnTypeHint() const
{
    Python::SourceRoot::TypeInfo typeInfo;
    if (parentFrame()) {
        // get typehint from parentframe identifier for this frame
        const Python::SourceIdentifier *ident = parentFrame()->getIdentifier(this->hash());
        if (ident) {
            Python::SourceTypeHintAssignment *assign =
                    ident->getTypeHintAssignment(m_token->line());
            if (assign)
                typeInfo = assign->typeInfo();
        }
    }

    return typeInfo;

}

const Python::SourceIdentifier *Python::SourceFrame::getIdentifier(std::size_t hash) const
{
    const Python::SourceIdentifier *ident = m_identifiers.getIdentifier(hash);
    if (ident)
        return ident;
    // recurse upwards to lookup
    if (!m_parentFrame)
        return nullptr;
    return parentFrame()->getIdentifier(hash);
}

const Python::SourceFrameReturnTypeList Python::SourceFrame::returnTypes() const
{
    return m_returnTypes;
}

void Python::SourceFrame::deleteParameter(Python::SourceParameter *param)
{
    m_parameters.remove(param);
    delete param;
//    auto pos = std::find(m_parameters.begin(), m_parameters.end(), param);
//    if (pos != m_parameters.end()) {
//        auto p = *pos;
//        m_parameters.erase(pos);
//        delete p;
//    }

}
