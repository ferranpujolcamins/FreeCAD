#include "PythonSourceListBase.h"
#include "PythonSource.h"

DBG_TOKEN_FILE

using namespace Gui;

Python::SourceListNodeBase::SourceListNodeBase(Python::SourceListParentBase *owner) :
    m_previous(nullptr), m_next(nullptr),
    m_owner(owner), m_token(nullptr)
{
    assert(owner != nullptr && "Must have valid owner");
    //assert(owner != this && "Can't own myself");
}

Python::SourceListNodeBase::SourceListNodeBase(const Python::SourceListNodeBase &other) :
    m_previous(nullptr), m_next(nullptr),
    m_owner(other.m_owner), m_token(other.m_token)
{
    assert(other.m_owner != nullptr && "Trying to copy Python::SourceListNodeBase with null as owner");
    m_token->attachReference(this);
}

Python::SourceListNodeBase::~SourceListNodeBase()
{
    if (m_token)
        const_cast<Python::Token*>(m_token)->detachReference(this);
    if (m_owner && m_owner != this)
        m_owner->remove(this);
}

void Python::SourceListNodeBase::setToken(Python::Token *token) {
    if (m_token)
        const_cast<Python::Token*>(m_token)->detachReference(this);
    token->attachReference(this);
    m_token = token;
}

const std::string Python::SourceListNodeBase::text() const
{
    if (m_token)
        return m_token->text();
    return std::string();
}

int Python::SourceListNodeBase::hash() const
{
    if (m_token)
        return m_token->hash();
    return 0;
}

void Python::SourceListNodeBase::setOwner(Python::SourceListParentBase *owner)
{
    //assert(owner != this && "Can't own myself");
    m_owner = owner;
}

// this should only be called from PythonToken destructor when it gets destroyed
void Python::SourceListNodeBase::tokenDeleted()
{
    m_token = nullptr;
    if (m_owner && m_owner != this)
        m_owner->remove(this, true); // remove me from owner and let him delete me
}

// -------------------------------------------------------------------------


Python::SourceListParentBase::SourceListParentBase(Python::SourceListParentBase *owner) :
    Python::SourceListNodeBase(owner),
    m_first(nullptr), m_last(nullptr),
    m_preventAutoRemoveMe(false)
{
}

Python::SourceListParentBase::SourceListParentBase(const Python::SourceListParentBase &other) :
    Python::SourceListNodeBase(this),
    m_first(other.m_first), m_last(other.m_last),
    m_preventAutoRemoveMe(other.m_preventAutoRemoveMe)
{
}

Python::SourceListParentBase::~SourceListParentBase()
{
    // delete all children
    Python::SourceListNodeBase *n = m_first,
                             *tmp = nullptr;
    while(n) {
        n->setOwner(nullptr);
        tmp = n;
        n = n->next();
        delete tmp;
    }
}

void Python::SourceListParentBase::insert(Python::SourceListNodeBase *node)
{
    DEFINE_DBG_VARS
    assert(node->next() == nullptr && "Node already owned");
    assert(node->previous() == nullptr && "Node already owned");
    assert(contains(node) == false && "Node already stored");
    if (m_last) {
        // look up the place for this
        Python::SourceListNodeBase *n = m_last,
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

bool Python::SourceListParentBase::remove(Python::SourceListNodeBase *node, bool deleteNode)
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

bool Python::SourceListParentBase::contains(Python::SourceListNodeBase *node) const
{
    DEFINE_DBG_VARS
    Python::SourceListNodeBase *f = m_first,
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

bool Python::SourceListParentBase::empty() const
{
    return m_first == nullptr && m_last == nullptr;
}

int32_t Python::SourceListParentBase::size() const
{
    DEFINE_DBG_VARS
    Python::SourceListNodeBase *f = m_first;
    int32_t cnt = 0;
    while(f) {
        DBG_TOKEN(f->token())
        ++cnt;
        f = f->next();
    }

    return cnt;
}

int32_t Python::SourceListParentBase::indexOf(Python::SourceListNodeBase *node) const
{
    int32_t idx = -1;
    Python::SourceListNodeBase *f = m_first;
    while (f) {
        ++idx;
        if (f == node)
            break;
        f = f->next();
    }

    return idx;
}

Python::SourceListNodeBase *Python::SourceListParentBase::operator [](std::size_t idx) const
{
    Python::SourceListNodeBase *n = m_first;
    for (std::size_t i = 0; i < idx; ++i) {
        if (n == nullptr)
            return nullptr;
        n = n->next();
    }
    return n;
}

Python::SourceListNodeBase *Python::SourceListParentBase::findExact(const Python::Token *tok) const
{
    DEFINE_DBG_VARS
    for (Python::SourceListNodeBase *node = m_first;
         node != nullptr;
         node = node->next())
    {
        DBG_TOKEN(node->token())
        if (*node->token() == *tok)
            return node;
    }

    return nullptr;
}

Python::SourceListNodeBase *Python::SourceListParentBase::findFirst(Python::Token::Type tokType) const
{
    DEFINE_DBG_VARS

    for (Python::SourceListNodeBase *node = m_first;
         node != nullptr;
         node = node->next())
    {
        DBG_TOKEN(node->token())
        if (node->token() && node->token()->type() == tokType)
            return node;
    }

    return nullptr;
}

Python::SourceListNodeBase *Python::SourceListParentBase::findLast(Python::Token::Type tokType) const
{
    DEFINE_DBG_VARS

    for (Python::SourceListNodeBase *node = m_last;
         node != nullptr;
         node = node->previous())
    {
        DBG_TOKEN(node->token())
        if (node->token() && node->token()->type() == tokType)
            return node;
    }

    return nullptr;
}

bool Python::SourceListParentBase::hasOtherTokens(Python::Token::Type tokType) const
{
    for (Python::SourceListNodeBase *node = m_last;
         node != nullptr;
         node = node->previous())
    {
        if (node->token() && node->token()->type() != tokType)
            return true;
    }

    return false;
}

int Python::SourceListParentBase::compare(const Python::SourceListNodeBase *left,
                                          const Python::SourceListNodeBase *right) const
{
    if (!left || ! right || !left->token() || !right->token())
        return 0;
    if (*left->token() > *right->token())
        return -1;
    else if (*left->token() < *right->token())
        return +1;
    return 0;
}
