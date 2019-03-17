#ifndef PYTHONSOURCELISTBASE_H
#define PYTHONSOURCELISTBASE_H

#include "PythonSource.h"
#include <TextEditor/PythonSyntaxHighlighter.h>


namespace  Gui {

class PythonSourceListParentBase;

/// We do a Collection implementation duobly inked list as it lets us cleanly handle
/// insert, deletions, changes in source document

///
/// Base class for items in collections
class PythonSourceListNodeBase {
public:
    explicit PythonSourceListNodeBase(PythonSourceListParentBase *owner);
    PythonSourceListNodeBase(const PythonSourceListNodeBase &other);
    virtual ~PythonSourceListNodeBase();
    PythonSourceListNodeBase *next() const { return m_next; }
    PythonSourceListNodeBase *previous() const { return m_previous; }

    void setNext(PythonSourceListNodeBase *next) { m_next = next; }
    void setPrevious(PythonSourceListNodeBase *previous) { m_previous = previous; }
    /// python token
    const PythonToken *token() const { return m_token; }
    void setToken(PythonToken *token);

    /// gets text for token (gets from document)
    QString text() const;

    /// owner node, setOwner is essential during cleanup, or ownership swaps
    PythonSourceListParentBase *owner() const { return m_owner; }
    void setOwner(PythonSourceListParentBase *owner);

    /// called by PythonToken when it gets destroyed
    virtual void tokenDeleted();

protected:
    PythonSourceListNodeBase *m_previous;
    PythonSourceListNodeBase *m_next;
    PythonSourceListParentBase *m_owner;
    PythonToken *m_token;
};

// -------------------------------------------------------------------------

/// base class for collection, manages a linked list, might itself be a node i a linked list
class PythonSourceListParentBase : public PythonSourceListNodeBase
{
public:
    typedef PythonSourceListNodeBase iterator_t;
    explicit PythonSourceListParentBase(PythonSourceListParentBase *owner);

    // Note!! this doesn't copy the elements in the list
    PythonSourceListParentBase(const PythonSourceListParentBase &other);
    virtual ~PythonSourceListParentBase();

    /// adds node to current collection
    virtual void insert(PythonSourceListNodeBase *node);
    /// remove node from current collection
    /// returns true if node was removed (was in collection)
    /// NOTE! if deleteNode is false: caller is responsible for deleting node
    virtual bool remove(PythonSourceListNodeBase *node, bool deleteNode = false);
    /// true if node is contained
    bool contains(PythonSourceListNodeBase *node) const;
    bool empty() const;
    /// returns size of list
    std::size_t size() const;
    /// returns index of node or -1 if not found
    std::size_t indexOf(PythonSourceListNodeBase *node) const;
    /// returns node at idx
    PythonSourceListNodeBase *operator [](std::size_t idx) const;

    /// finds the node that has token this exact token
    PythonSourceListNodeBase *findExact(const PythonToken *tok) const;
    /// finds the first token of this type
    PythonSourceListNodeBase *findFirst(PythonSyntaxHighlighter::Tokens token) const;
    /// finds the last token of this type (reverse lookup)
    PythonSourceListNodeBase *findLast(PythonSyntaxHighlighter::Tokens token) const;
    /// returns true if all elements are of token type false otherwise
    /// if list is empty it returns false
    bool hasOtherTokens(PythonSyntaxHighlighter::Tokens token) const;

    /// stl iterator stuff
    PythonSourceListNodeBase* begin() const { return m_first; }
    PythonSourceListNodeBase* rbegin() const { return m_last; }
    PythonSourceListNodeBase* last()  const { return m_last; }
    PythonSourceListNodeBase* end() const { return nullptr; }
    PythonSourceListNodeBase* rend() const { return nullptr; }

protected:
    /// subclass may implement, used to sort insert order
    ///     should return -1 if left is more than right
    ///                   +1 if right is more than left
    ///                    0 if both equal
    virtual int compare(const PythonSourceListNodeBase *left,
                        const PythonSourceListNodeBase *right) const;
    PythonSourceListNodeBase *m_first;
    PythonSourceListNodeBase *m_last;
    bool m_preventAutoRemoveMe; // set to true on root container,
                                // sub containers removes themselfs from here
                                // if container gets empty default behaviour is
                                // to let parent delete me
                                // true here sets prevents this
};

} // namespace Gui


#endif // PYTHONSOURCELISTBASE_H
