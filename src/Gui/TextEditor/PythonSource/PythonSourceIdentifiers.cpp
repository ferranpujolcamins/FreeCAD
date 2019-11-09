#include "PythonSourceIdentifiers.h"
#include "PythonSource.h"
#include "PythonSourceModule.h"


#include <TextEditor/PythonSyntaxHighlighter.h>

using namespace Gui;

DBG_TOKEN_FILE

Python::SourceIdentifierAssignment::SourceIdentifierAssignment(Python::SourceIdentifier *owner,
                                                                   Python::Token *startToken,
                                                                   Python::SourceRoot::TypeInfo typeInfo) :
    Python::SourceListNodeBase(owner)
{
    m_type = typeInfo;
    assert(startToken != nullptr && "Must have valid token");
    m_token = startToken;
    m_token->attachReference(this);
}

Python::SourceIdentifierAssignment::SourceIdentifierAssignment(Python::SourceTypeHint *owner,
                                                                   Python::Token *startToken,
                                                                   Python::SourceRoot::TypeInfo typeInfo) :
        Python::SourceListNodeBase(owner)
{
    m_type = typeInfo;
    assert(startToken != nullptr && "Must have valid token");
    m_token = startToken;
    m_token->attachReference(this);
}

Python::SourceIdentifierAssignment::~SourceIdentifierAssignment()
{
}

int Python::SourceIdentifierAssignment::linenr() const
{
    return m_token->line();
}

int Python::SourceIdentifierAssignment::position() const
{
    return m_token->startPos;
}

// --------------------------------------------------------------------------------

Python::SourceIdentifier::SourceIdentifier(Python::SourceListParentBase *owner,
                                               const Python::SourceModule *module) :
    Python::SourceListParentBase(owner),
    m_module(module),
    m_typeHint(this, this, module)
{
    assert(m_module != nullptr && "Must have a valid module");
}

Python::SourceIdentifier::~SourceIdentifier()
{
}

const Python::SourceFrame *Python::SourceIdentifier::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
}

Python::SourceIdentifierAssignment *Python::SourceIdentifier::getFromPos(int line, int pos) const
{
    for (Python::SourceListNodeBase *lstElem = last();
         lstElem != end();
         lstElem = lstElem->previous())
    {
        if ((lstElem->token()->line() == line && lstElem->token()->endPos < pos) ||
            (lstElem->token()->line() < line))
        {
            return dynamic_cast<Python::SourceIdentifierAssignment*>(lstElem);
        }
    }

    // not found
    return nullptr;
}

Python::SourceIdentifierAssignment *Python::SourceIdentifier::getFromPos(const Python::Token *tok) const
{
    return getFromPos(tok->line(), tok->startPos);
}

bool Python::SourceIdentifier::isCallable(const Python::Token *tok) const
{
    return isCallable(tok->line(), tok->startPos);
}

bool Python::SourceIdentifier::isCallable(int line, int pos) const
{
    const Python::SourceIdentifierAssignment *assign =
            getTypeHintAssignment(line);

    // try with ordinary assignments ie "var=1"
    if (!assign) {
        assign = getFromPos(line, pos);
    }

    if (!assign)
        return false;

    return assign->typeInfo().isCallable();
}

const Python::SourceFrame *Python::SourceIdentifier::callFrame(const Python::Token *tok) const
{
    // should be directly callable, not a typehint
    // try with ordinary assignments ie "var=1"
    const Python::SourceIdentifierAssignment *assign = getFromPos(tok);

    if (!assign)
        return nullptr;

    if (assign->typeInfo().isCallable())
        return m_module->getFrameForToken(tok, m_module->rootFrame());
    return nullptr;
}

bool Python::SourceIdentifier::isImported(const Python::Token *tok) const
{
    return isImported(tok->line(), tok->startPos);
}

bool Python::SourceIdentifier::isImported(int line, int pos) const
{
    const Python::SourceIdentifierAssignment *assign =
            getFromPos(line, pos);

    if (!assign)
        return false;

    return assign->token()->isImport();
}

QString Python::SourceIdentifier::name() const
{
    if (m_first)
        return m_first->text();

    return QLatin1String("<lookup error>");
}

Python::SourceTypeHintAssignment *Python::SourceIdentifier::getTypeHintAssignment(const Python::Token *tok) const
{
    return getTypeHintAssignment(tok->line());
}

Python::SourceTypeHintAssignment *Python::SourceIdentifier::getTypeHintAssignment(int line) const
{
    for (Python::SourceListNodeBase *lstElem = m_typeHint.last();
         lstElem != nullptr;
         lstElem = lstElem->previous())
    {
        if (lstElem->token()->line() <= line)
            return dynamic_cast<Python::SourceTypeHintAssignment*>(lstElem);
    }

    return nullptr;
}

bool Python::SourceIdentifier::hasTypeHint(int line) const {
    return getTypeHintAssignment(line) != nullptr;
}


Python::SourceTypeHintAssignment *Python::SourceIdentifier::setTypeHint(Python::Token *tok,
                                                                    Python::SourceRoot::TypeInfo typeInfo)
{
    Python::SourceTypeHintAssignment *typeHint = getTypeHintAssignment(tok->line());
    if (typeHint) {
        // we found the last declared type hint, update type
        typeHint->setType(typeInfo);
        return typeHint;
    }

    // create new assigment
    typeHint = new Python::SourceTypeHintAssignment(&m_typeHint, tok, typeInfo);
    m_typeHint.insert(typeHint);
    return typeHint;
}

int Python::SourceIdentifier::compare(const Python::SourceListNodeBase *left,
                                    const Python::SourceListNodeBase *right) const
{
    // should sort by linenr and (if same line also token startpos)
    const Python::SourceIdentifierAssignment *l = dynamic_cast<const Python::SourceIdentifierAssignment*>(left),
                                           *r = dynamic_cast<const Python::SourceIdentifierAssignment*>(right);
    assert(l != nullptr && r != nullptr && "Python::SourceIdentifier stored non compatible node");
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


Python::SourceIdentifierList::SourceIdentifierList(const Python::SourceModule *module):
    Python::SourceListParentBase(this),
    m_module(module)
{
    m_preventAutoRemoveMe = true;
    assert(module != nullptr && "Must have a valid module");
}

Python::SourceIdentifierList::~SourceIdentifierList()
{
}

const Python::SourceFrame *Python::SourceIdentifierList::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
}

const Python::SourceIdentifier *Python::SourceIdentifierList::getIdentifier(const QString name) const
{
    for (Python::SourceIdentifier *itm = dynamic_cast<Python::SourceIdentifier*>(m_first);
         itm != nullptr;
         itm = dynamic_cast<Python::SourceIdentifier*>(itm->next()))
    {
        QString itmName = itm->name();
        if (itmName == name)
            return itm;
    }
    return nullptr;
}

Python::SourceIdentifier
*Python::SourceIdentifierList::setIdentifier(Python::Token *tok,
                                           Python::SourceRoot::TypeInfo typeInfo)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    assert(tok && "Expected a valid pointer");
    QString name = tok->text();

    Python::SourceIdentifier *identifier = nullptr;
    Python::SourceIdentifierAssignment *assign = nullptr;

    // find in our list
    for(Python::SourceListNodeBase *itm = begin();
        itm != nullptr;
        itm = itm->next())
    {
        Python::SourceIdentifier *ident = dynamic_cast<Python::SourceIdentifier*>(itm);
        if (ident && ident->name() == name) {
            identifier = ident;
            break;
        }
    }

    // new indentifier
    if (identifier) {
        // check so we don't double insert
        Python::SourceListNodeBase *itm = identifier->getFromPos(tok);
        if (itm) {
            assign = dynamic_cast<Python::SourceIdentifierAssignment*>(itm);
            assert(assign != nullptr && "Stored value was not a assignment");
            if (assign->typeInfo() == typeInfo)
                return identifier; // already have this one and it is equal
            //else
                // type differ, create new assignment
        }
        // create new assigment
        assign = new Python::SourceIdentifierAssignment(identifier, tok, typeInfo);
        identifier->insert(assign);
        return identifier;
    }

    // create identifier
    identifier = new Python::SourceIdentifier(this, m_module);
#ifdef BUILD_PYTHON_DEBUGTOOLS
    identifier->m_name = tok->text();
#endif

    // create new assigment
    assign = new Python::SourceIdentifierAssignment(identifier, tok, typeInfo);
    identifier->insert(assign);

    // must have a assignment before we insert
    insert(identifier);
    return identifier;
}

const Python::SourceIdentifier
*Python::SourceIdentifierList::getIdentifierReferencedBy(const Python::Token *tok,
                                                       int limitChain) const
{
    if (tok->isIdentifier()) {
        // TODO builtin identifiers and modules lookup
        const Python::SourceIdentifier *ident = getIdentifier(tok->text());
        return getIdentifierReferencedBy(ident, tok->line(), tok->startPos, limitChain);
    }

    return nullptr;
}

const Python::SourceIdentifier
*Python::SourceIdentifierList::getIdentifierReferencedBy(const Python::SourceIdentifier *ident,
                                                       int line, int pos, int limitChain) const
{
    if (!ident || limitChain <= 0)
        return nullptr;

    const Python::SourceIdentifier *startIdent = ident;

    // lookup nearest assignment
    const Python::SourceIdentifierAssignment *assign = ident->getFromPos(line, pos);
    if (!assign)
        return startIdent;

    // is this assignment also referenced?
    switch (assign->typeInfo().type) {
    case Python::SourceRoot::ReferenceType:
        // lookup recursive
        ident = getIdentifierReferencedBy(ident, line, pos -1, limitChain -1);
        if (!ident)
            return startIdent;
        return ident;
    // case Python::SourceRoot::ReferenceBuiltinType: // not sure if we need this specialcase?
    default:
        if (assign->typeInfo().isReferenceImported())
            return ident;
        break;
    }

    return ident;
}

int Python::SourceIdentifierList::compare(const Python::SourceListNodeBase *left,
                                        const Python::SourceListNodeBase *right) const
{
    const Python::SourceIdentifier *l = dynamic_cast<const Python::SourceIdentifier*>(left),
                                 *r = dynamic_cast<const Python::SourceIdentifier*>(right);
    assert(l != nullptr && r != nullptr && "Python::SourceIdentifiers contained invalid nodes");
    if (l->name() > r->name())
        return -1;
    else if (r->name() > l->name())
        return +1;
    return 0;
}


// ---------------------------------------------------------------------------------

Python::SourceTypeHint::SourceTypeHint(Python::SourceListParentBase *owner,
                                           const Python::SourceIdentifier *identifer,
                                           const Python::SourceModule *module) :
    Python::SourceListParentBase(owner),
    m_module(module),
    m_identifer(identifer)
{
}

Python::SourceTypeHint::~SourceTypeHint()
{
}

const Python::SourceFrame *Python::SourceTypeHint::frame() const {
    return m_module->getFrameForToken(m_token, m_module->rootFrame());
}

Python::SourceTypeHintAssignment *Python::SourceTypeHint::getFromPos(int line, int pos) const
{
    for (Python::SourceListNodeBase *lstElem = last();
         lstElem != end();
         lstElem = lstElem->previous())
    {
        if ((lstElem->token()->line() == line && lstElem->token()->endPos < pos) ||
            (lstElem->token()->line() < line))
        {
            return dynamic_cast<Python::SourceTypeHintAssignment*>(lstElem);
        }
    }

    // not found
    return nullptr;
}

Python::SourceTypeHintAssignment *Python::SourceTypeHint::getFromPos(const Python::Token *tok) const
{
    return getFromPos(tok->line(), tok->startPos);
}

QString Python::SourceTypeHint::name() const
{
    return m_type.typeAsStr();
}

int Python::SourceTypeHint::compare(const Python::SourceListNodeBase *left, const Python::SourceListNodeBase *right) const
{
    // should sort by linenr and (if same line also token startpos)
    const Python::SourceTypeHintAssignment *l = dynamic_cast<const Python::SourceTypeHintAssignment*>(left),
                                         *r = dynamic_cast<const Python::SourceTypeHintAssignment*>(right);
    assert(l != nullptr && r != nullptr && "Python::SourceTypeHint stored non compatible node");

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

