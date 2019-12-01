#ifndef PYTHONSOURCEITEMBASE_H
#define PYTHONSOURCEITEMBASE_H

#include "PythonToken.h"

namespace Gui {
namespace Python {


/// base class for items in Abstract syntax tree
template <typename TOwner, void (TOwner::*tokenDelCallback)(TokenWrapperBase *wrapper)>
class SourceItemAbstract : public TokenWrapperInherit
{
public:
    //typedef TokenWrapper<T> WrapperType;
    //typedef void (T::*onTokenDeleted)(WrapperType *wrapper);

    explicit SourceItemAbstract(Python::Token *tok,
                                SourceItemAbstract<TOwner, tokenDelCallback> *owner) :
        TokenWrapperInherit(tok, owner)
    { }
    virtual ~SourceItemAbstract()
    { }

    /// gets text for token (gets from document)
    const std::string text() const {
        if (m_token)
            return m_token->text();
        return std::string();
    }

    /// gets the hash for this tokens text
    virtual int hash() const {
        if (m_token)
            return m_token->hash();
        return 0;
    }
};

} // namespace Python
} // namespace Gui

#endif // PYTHONSOURCEITEMBASE_H
