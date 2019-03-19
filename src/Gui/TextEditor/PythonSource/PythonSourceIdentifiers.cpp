#include "PythonSourceIdentifiers.h"
#include "PythonSource.h"
#include "PythonSourceModule.h"


#include <TextEditor/PythonSyntaxHighlighter.h>

using namespace Gui;

DBG_TOKEN_FILE

PythonSourceIdentifierAssignment::PythonSourceIdentifierAssignment(PythonSourceIdentifier *owner,
                                                                   PythonToken *startToken,
                                                                   PythonSourceRoot::TypeInfo typeInfo) :
    PythonSourceListNodeBase(owner)
{
    m_type = typeInfo;
    assert(startToken != nullptr && "Must have valid token");
    m_token = startToken;
    m_token->attachReference(this);
}

PythonSourceIdentifierAssignment::PythonSourceIdentifierAssignment(PythonSourceTypeHint *owner,
                                                                   PythonToken *startToken,
                                                                   PythonSourceRoot::TypeInfo typeInfo) :
        PythonSourceListNodeBase(owner)
{
    m_type = typeInfo;
    assert(startToken != nullptr && "Must have valid token");
    m_token = startToken;
    m_token->attachReference(this);
}

PythonSourceIdentifierAssignment::~PythonSourceIdentifierAssignment()
{
}

int PythonSourceIdentifierAssignment::linenr() const
{
    return m_token->line();
}

int PythonSourceIdentifierAssignment::position() const
{
    return m_token->startPos;
}

// --------------------------------------------------------------------------------

PythonSourceIdentifier::PythonSourceIdentifier(PythonSourceListParentBase *owner,
                                               const PythonSourceModule *module) :
    PythonSourceListParentBase(owner),
    m_module(module),
    m_typeHint(this, this, module)
{
    assert(m_module != nullptr && "Must have a valid module");
}

PythonSourceIdentifier::~PythonSourceIdentifier()
{
}

const PythonSourceFrame *PythonSourceIdentifier::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
}

PythonSourceIdentifierAssignment *PythonSourceIdentifier::getFromPos(int line, int pos) const
{
    for (PythonSourceListNodeBase *lstElem = last();
         lstElem != end();
         lstElem = lstElem->previous())
    {
        if ((lstElem->token()->line() == line && lstElem->token()->endPos < pos) ||
            (lstElem->token()->line() < line))
        {
            return dynamic_cast<PythonSourceIdentifierAssignment*>(lstElem);
        }
    }

    // not found
    return nullptr;
}

PythonSourceIdentifierAssignment *PythonSourceIdentifier::getFromPos(const PythonToken *tok) const
{
    return getFromPos(tok->line(), tok->startPos);
}

QString PythonSourceIdentifier::name() const
{
    if (m_first)
        return m_first->text();

    return QLatin1String("<lookup error>");
}

PythonSourceTypeHintAssignment *PythonSourceIdentifier::getTypeHintAssignment(const PythonToken *tok) const
{
    return getTypeHintAssignment(tok->line());
}

PythonSourceTypeHintAssignment *PythonSourceIdentifier::getTypeHintAssignment(int line) const
{
    for (PythonSourceListNodeBase *lstElem = m_typeHint.last();
         lstElem != end();
         lstElem = lstElem->previous())
    {
        if (lstElem->token()->line() <= line)
            return dynamic_cast<PythonSourceTypeHintAssignment*>(lstElem);
    }

    return nullptr;
}

bool PythonSourceIdentifier::hasTypeHint(int line) const {
    return getTypeHintAssignment(line) != nullptr;
}


PythonSourceTypeHintAssignment *PythonSourceIdentifier::setTypeHint(PythonToken *tok,
                                                                    PythonSourceRoot::TypeInfo typeInfo)
{
    PythonSourceTypeHintAssignment *typeHint = getTypeHintAssignment(tok->line());
    if (typeHint) {
        // we found the last declared type hint, update type
        typeHint->setType(typeInfo);
        return typeHint;
    }

    // create new assigment
    typeHint = new PythonSourceTypeHintAssignment(&m_typeHint, tok, typeInfo);
    m_typeHint.insert(typeHint);
    return typeHint;
}

int PythonSourceIdentifier::compare(const PythonSourceListNodeBase *left,
                                    const PythonSourceListNodeBase *right) const
{
    // should sort by linenr and (if same line also token startpos)
    const PythonSourceIdentifierAssignment *l = dynamic_cast<const PythonSourceIdentifierAssignment*>(left),
                                           *r = dynamic_cast<const PythonSourceIdentifierAssignment*>(right);
    assert(l != nullptr && r != nullptr && "PythonSourceIdentifier stored non compatible node");
    if (l->linenr() > r->linenr()) {
        return -1;
    } else if (l->linenr() < r->linenr()) {
        return +1;
    } else {
        // line nr equal
        if (l->token()->startPos > r->token()->startPos)
            return -1;
        else
            return +1; // can't be at same source position
    }
}

// -------------------------------------------------------------------------------


PythonSourceIdentifierList::PythonSourceIdentifierList(const PythonSourceModule *module):
    PythonSourceListParentBase(this),
    m_module(module)
{
    m_preventAutoRemoveMe = true;
    assert(module != nullptr && "Must have a valid module");
}

PythonSourceIdentifierList::~PythonSourceIdentifierList()
{
}

const PythonSourceFrame *PythonSourceIdentifierList::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
}

const PythonSourceIdentifier *PythonSourceIdentifierList::getIdentifier(const QString name) const
{
    for (PythonSourceIdentifier *itm = dynamic_cast<PythonSourceIdentifier*>(m_first);
         itm != end();
         itm = dynamic_cast<PythonSourceIdentifier*>(itm->next()))
    {
        QString itmName = itm->name();
        if (itmName == name)
            return itm;
    }
    return nullptr;
}

PythonSourceIdentifier
*PythonSourceIdentifierList::setIdentifier(PythonToken *tok,
                                           PythonSourceRoot::TypeInfo typeInfo)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    assert(tok && "Expected a valid pointer");
    QString name = tok->text();

    PythonSourceIdentifier *identifier = nullptr;
    PythonSourceIdentifierAssignment *assign = nullptr;

    // find in our list
    for(PythonSourceListNodeBase *itm = begin();
        itm != nullptr;
        itm = itm->next())
    {
        PythonSourceIdentifier *ident = dynamic_cast<PythonSourceIdentifier*>(itm);
        if (ident && ident->name() == name) {
            identifier = ident;
            break;
        }
    }

    // new indentifier
    if (!identifier) {
        identifier = new PythonSourceIdentifier(this, m_module);
        insert(identifier);
    } else {
        // check so we don't double insert
        PythonSourceListNodeBase *itm = identifier->findExact(tok);
        if (itm) {
            assign = dynamic_cast<PythonSourceIdentifierAssignment*>(itm);
            assert(assign != nullptr && "Stored value was not a assignment");
            if (assign->typeInfo() == typeInfo)
                return identifier; // already have this one and it is equal
            //else
            //  type differ, create new assignment
        }
    }

    // create new assigment
    assign = new PythonSourceIdentifierAssignment(identifier, tok, typeInfo);
    identifier->insert(assign);
    return identifier;
}

const PythonSourceIdentifier
*PythonSourceIdentifierList::getIdentifierReferencedBy(const PythonToken *tok,
                                                       int limitChain) const
{
    if (tok->isIdentifier()) {
        // TODO builtin identifiers and modules lookup
        const PythonSourceIdentifier *ident = getIdentifier(tok->text());
        return getIdentifierReferencedBy(ident, tok->line(), tok->startPos, limitChain);
    }

    return nullptr;
}

const PythonSourceIdentifier
*PythonSourceIdentifierList::getIdentifierReferencedBy(const PythonSourceIdentifier *ident,
                                                       int line, int pos, int limitChain) const
{
    if (!ident || limitChain <= 0)
        return nullptr;

    const PythonSourceIdentifier *startIdent = ident;

    // lookup nearest assignment
    const PythonSourceIdentifierAssignment *assign = ident->getFromPos(line, pos);
    if (!assign)
        return startIdent;

    // is this assignment also referenced?
    switch (assign->typeInfo().type) {
    case PythonSourceRoot::ReferenceType:
        // lookup recursive
        ident = getIdentifierReferencedBy(ident, line, pos -1, limitChain -1);
        if (!ident)
            return startIdent;
        return ident;
    // case PythonSourceRoot::ReferenceBuiltinType: // not sure if we need this specialcase?
    default:
        if (assign->typeInfo().isReferenceImported())
            return ident;
        break;
    }

    return ident;
}

int PythonSourceIdentifierList::compare(const PythonSourceListNodeBase *left,
                                        const PythonSourceListNodeBase *right) const
{
    const PythonSourceIdentifier *l = dynamic_cast<const PythonSourceIdentifier*>(left),
                                 *r = dynamic_cast<const PythonSourceIdentifier*>(right);
    assert(l != nullptr && r != nullptr && "PythonSourceIdentifiers contained invalid nodes");
    if (l->name() > r->name())
        return -1;
    else if (r->name() > l->name())
        return +1;
    return 0;
}


// ---------------------------------------------------------------------------------

PythonSourceTypeHint::PythonSourceTypeHint(PythonSourceListParentBase *owner,
                                           const PythonSourceIdentifier *identifer,
                                           const PythonSourceModule *module) :
    PythonSourceListParentBase(owner),
    m_module(module),
    m_identifer(identifer)
{
}

PythonSourceTypeHint::~PythonSourceTypeHint()
{
}

const PythonSourceFrame *PythonSourceTypeHint::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
}

PythonSourceTypeHintAssignment *PythonSourceTypeHint::getFromPos(int line, int pos) const
{
    for (PythonSourceListNodeBase *lstElem = last();
         lstElem != end();
         lstElem = lstElem->previous())
    {
        if ((lstElem->token()->line() == line && lstElem->token()->endPos < pos) ||
            (lstElem->token()->line() < line))
        {
            return dynamic_cast<PythonSourceTypeHintAssignment*>(lstElem);
        }
    }

    // not found
    return nullptr;
}

PythonSourceTypeHintAssignment *PythonSourceTypeHint::getFromPos(const PythonToken *tok) const
{
    return getFromPos(tok->line(), tok->startPos);
}

QString PythonSourceTypeHint::name() const
{
    return m_type.typeAsStr();
}

int PythonSourceTypeHint::compare(const PythonSourceListNodeBase *left, const PythonSourceListNodeBase *right) const
{
    // should sort by linenr and (if same line also token startpos)
    const PythonSourceTypeHintAssignment *l = dynamic_cast<const PythonSourceTypeHintAssignment*>(left),
                                         *r = dynamic_cast<const PythonSourceTypeHintAssignment*>(right);
    assert(l != nullptr && r != nullptr && "PythonSourceTypeHint stored non compatible node");

    if (l->linenr() > r->linenr()) {
        return -1;
    } else if (l->linenr() < r->linenr()) {
        return +1;
    } else {
        // line nr equal
        if (l->token()->startPos > r->token()->startPos)
            return -1;
        else
            return +1; // can't be at same source position
    }
}

