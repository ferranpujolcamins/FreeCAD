#ifndef PYTHONSOURCEFRAMES_H
#define PYTHONSOURCEFRAMES_H

#include "PythonSourceIdentifiers.h"
#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceParameter.h"
#include "PythonSourceImports.h"
#include "PythonSourceIndent.h"
#include "PythonSourceRoot.h"
#include "PythonToken.h"


namespace Gui {
namespace Python {

class SourceParser;

/// a return can be built up by several Identifiers and operators
///  ie: return (var1 + var3) * 4 or 5
///              ^    ^ ^       ^    ^
/// we store pos of return, and compute type when we need it
class SourceFrameReturnType : public Python::SourceListNodeBase
{
    const Python::SourceModule *m_module;
    Python::SourceRoot::TypeInfo m_typeInfo;
public:
    explicit SourceFrameReturnType(Python::SourceListParentBase *owner,
                                   const Python::SourceModule *module,
                                   Python::Token *tok);
    virtual ~SourceFrameReturnType();

    // getter/setters for typeInfo
    void setTypeInfo(Python::SourceRoot::TypeInfo typeInfo);
    Python::SourceRoot::TypeInfo typeInfo() const { return m_typeInfo; }

    /// return this type
    Python::SourceRoot::TypeInfo returnType() const;
    /// true if it is a yield
    bool isYield() const;
    /// yields this type
    Python::SourceRoot::TypeInfo yieldType() const;
};

// --------------------------------------------------------------------

class SourceFrameReturnTypeList : public Python::SourceListParentBase
{
    const Python::SourceModule *m_module;
public:
    explicit SourceFrameReturnTypeList(Python::SourceListParentBase *owner,
                                    const Python::SourceModule *module);
    SourceFrameReturnTypeList(const Python::SourceFrameReturnTypeList &other);
    virtual ~SourceFrameReturnTypeList();
protected:
    int compare(const Python::SourceListNodeBase *left,
                const Python::SourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------

/// for function or method frames, contains all identifiers within this frame
class SourceFrame : public Python::SourceListParentBase
{
    friend class Python::SourceParser;
    Python::SourceListParentBase *m_owner;
    /// stores all identifiers contained within each frame
    Python::SourceIdentifierList m_identifiers;
    Python::SourceParameterList m_parameters;
    Python::SourceImportList    m_imports;
    Python::SourceFrameReturnTypeList m_returnTypes; // as in return types
    Python::SourceRoot::TypeInfo m_type;
    Python::SourceTypeHint *m_typeHint;
    Python::SourceFrame *m_parentFrame;
    Python::SourceModule *m_module;
    Python::SourceListNodeBase m_lastToken; // we need to use ListNodeBase as a wrapper
                                          // handle gracefully when Python::Token gets deleted in document
    bool m_isClass;

public:
    explicit SourceFrame(Python::SourceFrame *owner, Python::SourceModule *module,
                               Python::SourceFrame *parentFrame = nullptr, bool isClass = false);
    explicit SourceFrame(Python::SourceModule *owner, Python::SourceModule *module,
                               Python::SourceFrame *parentFrame = nullptr, bool isClass = false);
    ~SourceFrame();


    /// get the parent function frame
    Python::SourceFrame *parentFrame() const { return m_parentFrame; }
    void setParentFrame(Python::SourceFrame *frame) { m_parentFrame = frame; }

    const Python::SourceModule *module() const { return m_module; }
    Python::SourceRoot::TypeInfo type() const { return m_type; }

    const std::string name() const;

    /// true if frame is itself a module, ie: rootframe
    bool isModule() const { return m_parentFrame == nullptr; }

    /// get the docstring for this function/method
    const std::string docstring();

    /// get the return type hint
    ///  def funt(arg1, arg2) -> Module.Class.Custom:
    ///    would return:         ^
    ///    CustomType with CustomTypeName == Module.Class.Custom
    /// returns Typeinfo::InValidType if no typeHint given
    Python::SourceRoot::TypeInfo returnTypeHint() const;

    /// true if we have identifier
    bool hasIdentifier(int hash) const { return getIdentifier(hash) != nullptr; }
    bool hasIdentifierByName(const std::string &name) const {
        return getIdentifierByName(name) != nullptr;
    }

    const Python::SourceTypeHint *typeHint() const { return m_typeHint; }

    /// is root frame for class
    bool isClass() const { return m_isClass; }

    /// looks up name among identifiers (variables/constants)
    const Python::SourceIdentifier *getIdentifier(int hash) const;
    const Python::SourceIdentifier *getIdentifierByName(const std::string &name) const {
        return getIdentifier(Python::strToHash(name));
    }

    /// get reference to all identifiers within this frame
    const Python::SourceIdentifierList &identifiers() const { return m_identifiers; }

    // get reference to all parameters for this frame
    const Python::SourceParameterList &parameters() const { return m_parameters; }

    /// returns a list with all the types for this frame
    ///   returnType Might differ
    const Python::SourceFrameReturnTypeList returnTypes() const;

    /// the token (unindent) that ends this frame
    const Python::Token *lastToken() const { return m_lastToken.token(); }
    void setLastToken(Python::Token *tok) { m_lastToken.setToken(tok); }

private:

#ifdef BUILD_PYTHON_DEBUGTOOLS
    std::string m_name;
#endif
};

} // namespace Python
} // namespace Gui

#endif // PYTHONSOURCEFRAMES_H
