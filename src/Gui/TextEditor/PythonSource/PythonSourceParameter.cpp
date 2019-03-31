#include "PythonSourceParameter.h"
#include "PythonSourceListBase.h"
#include "PythonSource.h"
#include "PythonSourceFrames.h"
#include "PythonSourceIdentifiers.h"

#include <PythonSyntaxHighlighter.h>

DBG_TOKEN_FILE

using namespace Gui;


PythonSourceParameter::PythonSourceParameter(PythonSourceParameterList *parent, PythonToken *tok) :
    PythonSourceListNodeBase(parent),
    m_paramType(InValid)
{
    setToken(tok);
}

PythonSourceParameter::~PythonSourceParameter()
{
}

const PythonSourceFrame *PythonSourceParameter::frame() const
{
    PythonSourceParameterList *parentList = dynamic_cast<PythonSourceParameterList*>(m_owner);
    if (!parentList)
        return nullptr;
    return parentList->frame();
}

PythonSourceIdentifierAssignment *PythonSourceParameter::identifierAssignment() const
{
    // lookup with same token
    const PythonSourceFrame *frm = frame();
    assert(frm != nullptr && "Expected a PythonSourceFrame as parent to PythonSourceParameterList");
    assert(m_token != nullptr && "A non valid token stored to PythonSourceParameter");

    PythonSourceListNodeBase *itm = nullptr;
    for (itm = frm->identifiers().begin();
         itm != frm->identifiers().end();
         itm = itm->next())
    {
        PythonSourceIdentifier *ident = dynamic_cast<PythonSourceIdentifier*>(itm);
        if (ident && ident->name() == m_token->text()) {
            return dynamic_cast<PythonSourceIdentifierAssignment*>(ident->findExact(m_token));
        }
    }

    return nullptr;
}

// -----------------------------------------------------------------------------


PythonSourceParameterList::PythonSourceParameterList(PythonSourceFrame *frame) :
    PythonSourceListParentBase(frame),
    m_frame(frame)
{
    m_preventAutoRemoveMe = true;
}

PythonSourceParameterList::~PythonSourceParameterList()
{
}

const PythonSourceParameter *PythonSourceParameterList::getParameter(const QString name) const
{
    for (PythonSourceListNodeBase *itm = m_first;
         itm != nullptr;
         itm->next())
    {
        if (itm->text() == name)
            return dynamic_cast<PythonSourceParameter*>(itm);
    }
    return nullptr;
}

PythonSourceParameter *PythonSourceParameterList::setParameter(PythonToken *tok,
                                                                PythonSourceRoot::TypeInfo typeInfo,
                                                                PythonSourceParameter::ParameterType paramType)
{
    assert(tok && "Expected a valid pointer");
    QString name = tok->text();

    PythonSourceParameter *parameter = nullptr;

    // find in our list
    for(PythonSourceListNodeBase *itm = begin();
        itm != nullptr;
        itm = itm->next())
    {
        PythonSourceParameter *param = dynamic_cast<PythonSourceParameter*>(itm);
        if (param->text() == name) {
            parameter = param;
            break;
        }
    }

    // create new parameter
    if (!parameter) {
        parameter = new PythonSourceParameter(this, tok);
        insert(parameter);
    }

    if (parameter->type() != typeInfo)
        parameter->setType(typeInfo); // type differ
    if (parameter->parameterType() != paramType)
        parameter->setParameterType(paramType);

   return parameter; // already have this one and it is equal
}

int PythonSourceParameterList::compare(const PythonSourceListNodeBase *left,
                                       const PythonSourceListNodeBase *right) const
{
    const PythonSourceParameter *l = dynamic_cast<const PythonSourceParameter*>(left),
                                *r = dynamic_cast<const PythonSourceParameter*>(right);
    assert(l != nullptr && r != nullptr && "Non PythonSourceArgument items in agrmumentslist");
    if (*l->token() < *r->token())
        return +1;
    return -1; // r must be bigger
}
