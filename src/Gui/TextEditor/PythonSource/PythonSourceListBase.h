#ifndef PYTHONSOURCELISTBASE_H
#define PYTHONSOURCELISTBASE_H

#include "PythonSource.h"
#include <TextEditor/PythonSyntaxHighlighter.h>


namespace  Gui {
namespace Python {

class SourceListParentBase;

/// We do a Collection implementation duobly inked list as it lets us cleanly handle
/// insert, deletions, changes in source document

///
/// Base class for items in collections
class SourceListNodeBase {
public:
    explicit SourceListNodeBase(Python::SourceListParentBase *owner);
    SourceListNodeBase(const Python::SourceListNodeBase &other);
    virtual ~SourceListNodeBase();
    Python::SourceListNodeBase *next() const { return m_next; }
    Python::SourceListNodeBase *previous() const { return m_previous; }

    void setNext(Python::SourceListNodeBase *next) { m_next = next; }
    void setPrevious(Python::SourceListNodeBase *previous) { m_previous = previous; }
    /// python token
    const Python::Token *token() const { return m_token; }
    void setToken(Python::Token *token);

    /// gets text for token (gets from document)
    QString text() const;

    /// owner node, setOwner is essential during cleanup, or ownership swaps
    Python::SourceListParentBase *owner() const { return m_owner; }
    void setOwner(Python::SourceListParentBase *owner);

    /// called by PythonToken when it gets destroyed
    virtual void tokenDeleted();

protected:
    Python::SourceListNodeBase *m_previous;
    Python::SourceListNodeBase *m_next;
    Python::SourceListParentBase *m_owner;
    Python::Token *m_token;
};

// -------------------------------------------------------------------------

/// base class for collection, manages a linked list, might itself be a node i a linked list
class SourceListParentBase : public Python::SourceListNodeBase
{
public:
    typedef SourceListNodeBase iterator_t;
    explicit SourceListParentBase(Python::SourceListParentBase *owner);

    // Note!! this doesn't copy the elements in the list
    SourceListParentBase(const Python::SourceListParentBase &other);
    virtual ~SourceListParentBase();

    /// adds node to current collection
    virtual void insert(Python::SourceListNodeBase *node);
    /// remove node from current collection
    /// returns true if node was removed (was in collection)
    /// NOTE! if deleteNode is false: caller is responsible for deleting node
    virtual bool remove(Python::SourceListNodeBase *node, bool deleteNode = false);
    /// true if node is contained
    bool contains(Python::SourceListNodeBase *node) const;
    bool empty() const;
    /// returns size of list
    int32_t size() const;
    /// returns index of node or -1 if not found
    int32_t indexOf(Python::SourceListNodeBase *node) const;
    /// returns node at idx
    Python::SourceListNodeBase *operator [](std::size_t idx) const;

    /// finds the node that has token this exact token
    Python::SourceListNodeBase *findExact(const Python::Token *tok) const;
    /// finds the first token of this type
    Python::SourceListNodeBase *findFirst(Python::Token::Tokens token) const;
    /// finds the last token of this type (reverse lookup)
    Python::SourceListNodeBase *findLast(Python::Token::Tokens token) const;
    /// returns true if all elements are of token type false otherwise
    /// if list is empty it returns false
    bool hasOtherTokens(Python::Token::Tokens token) const;

    /// stl iterator stuff
    Python::SourceListNodeBase* begin() const { return m_first; }
    Python::SourceListNodeBase* rbegin() const { return m_last; }
    Python::SourceListNodeBase* last()  const { return m_last; }
    Python::SourceListNodeBase* end() const { return nullptr; }
    Python::SourceListNodeBase* rend() const { return nullptr; }

protected:
    /// subclass may implement, used to sort insert order
    ///     should return -1 if left is more than right
    ///                   +1 if right is more than left
    ///                    0 if both equal
    virtual int compare(const Python::SourceListNodeBase *left,
                        const Python::SourceListNodeBase *right) const;
    Python::SourceListNodeBase *m_first;
    Python::SourceListNodeBase *m_last;
    bool m_preventAutoRemoveMe; // set to true on root container,
                                // sub containers removes themselfs from here
                                // if container gets empty default behaviour is
                                // to let parent delete me
                                // true here sets prevents this
};

} // namespace Python

} // namespace Gui


#endif // PYTHONSOURCELISTBASE_H