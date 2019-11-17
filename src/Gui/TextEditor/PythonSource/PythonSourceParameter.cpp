#include "PythonSourceParameter.h"
#include "PythonSourceListBase.h"
#include "PythonSource.h"
#include "PythonSourceFrames.h"
#include "PythonSourceIdentifiers.h"

#include <PythonSyntaxHighlighter.h>

DBG_TOKEN_FILE

using namespace Gui;


Python::SourceParameter::SourceParameter(Python::SourceParameterList *parent, Python::Token *tok) :
    Python::SourceListNodeBase(parent),
    m_paramType(InValid)
{
    setToken(tok);
}

Python::SourceParameter::~SourceParameter()
{
}

const Python::SourceFrame *Python::SourceParameter::frame() const
{
    Python::SourceParameterList *parentList = dynamic_cast<Python::SourceParameterList*>(m_owner);
    if (!parentList)
        return nullptr;
    return parentList->frame();
}

Python::SourceIdentifierAssignment *Python::SourceParameter::identifierAssignment() const
{
    // lookup with same token
    const Python::SourceFrame *frm = frame();
    assert(frm != nullptr && "Expected a Python::SourceFrame as parent to Python::SourceParameterList");
    assert(m_token != nullptr && "A non valid token stored to Python::SourceParameter");

    Python::SourceListNodeBase *itm = nullptr;
    for (itm = frm->identifiers().begin();
         itm != frm->identifiers().end();
         itm = itm->next())
    {
        Python::SourceIdentifier *ident = dynamic_cast<Python::SourceIdentifier*>(itm);
        if (ident && ident->name() == m_token->text()) {
            return dynamic_cast<Python::SourceIdentifierAssignment*>(ident->findExact(m_token));
        }
    }

    return nullptr;
}

// -----------------------------------------------------------------------------


Python::SourceParameterList::SourceParameterList(Python::SourceFrame *frame) :
    Python::SourceListParentBase(frame),
    m_frame(frame)
{
    m_preventAutoRemoveMe = true;
}

Python::SourceParameterList::~SourceParameterList()
{
}

const Python::SourceParameter *Python::SourceParameterList::getParameter(const std::string &name) const
{
    for (Python::SourceListNodeBase *itm = m_first;
         itm != nullptr;
         itm->next())
    {
        if (itm->text() == name)
            return dynamic_cast<Python::SourceParameter*>(itm);
    }
    return nullptr;
}

Python::SourceParameter *Python::SourceParameterList::setParameter(Python::Token *tok,
                                                                Python::SourceRoot::TypeInfo typeInfo,
                                                                Python::SourceParameter::ParameterType paramType)
{
    assert(tok && "Expected a valid pointer");
    const std::string name = tok->text();

    Python::SourceParameter *parameter = nullptr;

    // find in our list
    for(Python::SourceListNodeBase *itm = begin();
        itm != nullptr;
        itm = itm->next())
    {
        Python::SourceParameter *param = dynamic_cast<Python::SourceParameter*>(itm);
        if (param->text() == name) {
            parameter = param;
            break;
        }
    }

    // create new parameter
    if (!parameter) {
        parameter = new Python::SourceParameter(this, tok);
        insert(parameter);
    }

    if (parameter->type() != typeInfo)
        parameter->setType(typeInfo); // type differ
    if (parameter->parameterType() != paramType)
        parameter->setParameterType(paramType);

   return parameter; // already have this one and it is equal
}

int Python::SourceParameterList::compare(const Python::SourceListNodeBase *left,
                                       const Python::SourceListNodeBase *right) const
{
    const Python::SourceParameter *l = dynamic_cast<const Python::SourceParameter*>(left),
                                *r = dynamic_cast<const Python::SourceParameter*>(right);
    assert(l != nullptr && r != nullptr && "Non Python::SourceArgument items in agrmumentslist");
    if (*l->token() < *r->token())
        return +1;
    return -1; // r must be bigger
}
