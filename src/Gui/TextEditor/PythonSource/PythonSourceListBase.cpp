#include "PythonSourceListBase.h"
#include "PythonSource.h"

DBG_TOKEN_FILE

using namespace Gui;

PythonSourceListNodeBase::PythonSourceListNodeBase(PythonSourceListParentBase *owner) :
    m_previous(nullptr), m_next(nullptr),
    m_owner(owner), m_token(nullptr)
{
    assert(owner != nullptr && "Must have valid owner");
    //assert(owner != this && "Can't own myself");
}

PythonSourceListNodeBase::PythonSourceListNodeBase(const PythonSourceListNodeBase &other) :
    m_previous(nullptr), m_next(nullptr),
    m_owner(other.m_owner), m_token(other.m_token)
{
    assert(other.m_owner != nullptr && "Trying to copy PythonSourceListNodeBase with null as owner");
    m_token->attachReference(this);
}

PythonSourceListNodeBase::~PythonSourceListNodeBase()
{
    if (m_token)
        const_cast<PythonToken*>(m_token)->detachReference(this);
    if (m_owner && m_owner != this)
        m_owner->remove(this);
}

void PythonSourceListNodeBase::setToken(PythonToken *token) {
    if (m_token)
        const_cast<PythonToken*>(m_token)->detachReference(this);
    token->attachReference(this);
    m_token = token;
}

QString PythonSourceListNodeBase::text() const
{
    if (m_token)
        return m_token->text();
    return QString();
}

void PythonSourceListNodeBase::setOwner(PythonSourceListParentBase *owner)
{
    //assert(owner != this && "Can't own myself");
    m_owner = owner;
}

// this should only be called from PythonToken destructor when it gets destroyed
void PythonSourceListNodeBase::tokenDeleted()
{
    m_token = nullptr;
    if (m_owner && m_owner != this)
        m_owner->remove(this, true); // remove me from owner and let him delete me
}

// -------------------------------------------------------------------------


PythonSourceListParentBase::PythonSourceListParentBase(PythonSourceListParentBase *owner) :
    PythonSourceListNodeBase(owner),
    m_first(nullptr), m_last(nullptr),
    m_preventAutoRemoveMe(false)
{
}

PythonSourceListParentBase::PythonSourceListParentBase(const PythonSourceListParentBase &other) :
    PythonSourceListNodeBase(this),
    m_first(other.m_first), m_last(other.m_last),
    m_preventAutoRemoveMe(other.m_preventAutoRemoveMe)
{
}

PythonSourceListParentBase::~PythonSourceListParentBase()
{
    // delete all children
    PythonSourceListNodeBase *n = m_first,
                             *tmp = nullptr;
    while(n) {
        n->setOwner(nullptr);
        tmp = n;
        n = n->next();
        delete tmp;
    }
}

void PythonSourceListParentBase::insert(PythonSourceListNodeBase *node)
{
    DEFINE_DBG_VARS
    assert(node->next() == nullptr && "Node already owned");
    assert(node->previous() == nullptr && "Node already owned");
    assert(contains(node) == false && "Node already stored");
    if (m_last) {
        // look up the place for this
        PythonSourceListNodeBase *n = m_last,
                                 *tmp = nullptr;
        while (n != nullptr) {
            int res = compare(node, n);
            DBG_TOKEN(n->token())
            if (res < 0) {
                // n is less than node, insert after n
                tmp = n->next();
                n->setNext(node);
                node->setPrevious(n);
                if (tmp) {
                    tmp->setPrevious(node);
                    node->setNext(tmp);
                } else {
                    node->setNext(nullptr);
                    m_last = node;
                }
                break;
            } else if (n->previous() == nullptr) {
                // n is more than node and we are at the beginning (won't iterate further)
                node->setNext(n);
                n->setPrevious(node);
                m_first = node;
                node->setPrevious(nullptr);
                break;
            }
            n = n->previous();
        }
    } else {
        m_first = node;
        m_last = node;
    }
}

bool PythonSourceListParentBase::remove(PythonSourceListNodeBase *node, bool deleteNode)
{
    if (contains(node)) {
        // remove from list
        if (node->previous())
            node->previous()->setNext(node->next());
        if (node->next())
            node->next()->setPrevious(node->previous());
        if (m_first == node)
            m_first = node->next();
        if (m_last == node)
            m_last = node->previous();

        node->setNext(nullptr);
        node->setPrevious(nullptr);
        if (deleteNode)
            delete node;

        // if we are empty we should remove ourselfs
        if (m_first == nullptr && m_last == nullptr &&
            m_owner != this && !m_preventAutoRemoveMe)
        {
            m_owner->remove(this, true);
        }
        return true;

    }

    return false;
}

bool PythonSourceListParentBase::contains(PythonSourceListNodeBase *node) const
{
    DEFINE_DBG_VARS
    PythonSourceListNodeBase *f = m_first,
                             *l = m_last;
    // we search front and back to be 1/2 n at worst lookup time
    while(f && l) {
        DBG_TOKEN(f->token())
        DBG_TOKEN(l->token())
        if (f == node || l == node)
            return true;
        f = f->next();
        l = l->previous();
    }

    return false;
}

bool PythonSourceListParentBase::empty() const
{
    return m_first == nullptr && m_last == nullptr;
}

std::size_t PythonSourceListParentBase::size() const
{
    DEFINE_DBG_VARS
    PythonSourceListNodeBase *f = m_first;
    std::size_t cnt = 0;
    while(f) {
        DBG_TOKEN(f->token())
        ++cnt;
        f = f->next();
    }

    return cnt;
}

std::size_t PythonSourceListParentBase::indexOf(PythonSourceListNodeBase *node) const
{
    std::size_t idx = -1;
    PythonSourceListNodeBase *f = m_first;
    while (f) {
        ++idx;
        if (f == node)
            break;
        f = f->next();
    }

    return idx;
}

PythonSourceListNodeBase *PythonSourceListParentBase::operator [](std::size_t idx) const
{
    PythonSourceListNodeBase *n = m_first;
    for (std::size_t i = 0; i < idx; ++i) {
        if (n == nullptr)
            return nullptr;
        n = n->next();
    }
    return n;
}

PythonSourceListNodeBase *PythonSourceListParentBase::findExact(const PythonToken *tok) const
{
    DEFINE_DBG_VARS
    for (PythonSourceListNodeBase *node = m_first;
         node != nullptr;
         node = node->next())
    {
        DBG_TOKEN(node->token())
        if (*node->token() == *tok)
            return node;
    }

    return nullptr;
}

PythonSourceListNodeBase *PythonSourceListParentBase::findFirst(PythonSyntaxHighlighter::Tokens token) const
{
    DEFINE_DBG_VARS

    if (!token)
        return nullptr;

    for (PythonSourceListNodeBase *node = m_first;
         node != nullptr;
         node = node->next())
    {
        DBG_TOKEN(node->token())
        if (node->token() && node->token()->token == token)
            return node;
    }

    return nullptr;
}

PythonSourceListNodeBase *PythonSourceListParentBase::findLast(PythonSyntaxHighlighter::Tokens token) const
{
    DEFINE_DBG_VARS

    for (PythonSourceListNodeBase *node = m_last;
         node != nullptr;
         node = node->previous())
    {
        DBG_TOKEN(node->token())
        if (node->token() && node->token()->token == token)
            return node;
    }

    return nullptr;
}

bool PythonSourceListParentBase::hasOtherTokens(PythonSyntaxHighlighter::Tokens token) const
{
    for (PythonSourceListNodeBase *node = m_last;
         node != nullptr;
         node = node->previous())
    {
        if (node->token() && node->token()->token != token)
            return true;
    }

    return false;
}

int PythonSourceListParentBase::compare(const PythonSourceListNodeBase *left,
                                        const PythonSourceListNodeBase *right) const
{
    if (!left || ! right || !left->token() || !right->token())
        return 0;
    if (*left->token() > *right->token())
        return +1;
    else if (*left->token() < *right->token())
        return -1;
    return 0;
}
