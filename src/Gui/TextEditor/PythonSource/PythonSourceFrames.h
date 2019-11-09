#ifndef PYTHONSOURCEFRAMES_H
#define PYTHONSOURCEFRAMES_H

#include "PythonSourceIdentifiers.h"
#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceParameter.h"
#include "PythonSourceImports.h"
#include "PythonSourceIndent.h"
#include "PythonSourceRoot.h"
#include <TextEditor/PythonSyntaxHighlighter.h>


namespace Gui {
namespace Python {


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

    QString name() const;

    /// true if frame is itself a module, ie: rootframe
    bool isModule() const { return m_parentFrame == nullptr; }

    /// get the docstring for this function/method
    QString docstring();

    /// get the return type hint
    ///  def funt(arg1, arg2) -> Module.Class.Custom:
    ///    would return:         ^
    ///    CustomType with CustomTypeName == Module.Class.Custom
    /// returns Typeinfo::InValidType if no typeHint given
    Python::SourceRoot::TypeInfo returnTypeHint() const;

    /// true if we have identifier
    bool hasIdentifier(const QString name) const {
        return m_identifiers.getIdentifier(name);
    }

    const Python::SourceTypeHint *typeHint() const { return m_typeHint; }

    /// is root frame for class
    bool isClass() const { return m_isClass; }

    /// looks up name among identifiers (variables/constants)
    const Python::SourceIdentifier *getIdentifier(const QString name) const;

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

    // scan* functions might mutate Python::Token, ie change from Undetermined -> determined etc.
    /// on complete rescan, returns lastToken->next()
    Python::Token *scanFrame(Python::Token *startToken, Python::SourceIndent &indent);

    /// on single row rescan
    Python::Token *scanLine(Python::Token *startToken, Python::SourceIndent &indent);


private:
    /// starts a new frame
    Python::Token *startSubFrame(Python::Token *tok, Python::SourceIndent &indent,
                               Python::SourceRoot::DataTypes type);

    /// moves token til next line with tokens
    Python::Token *gotoNextLine(Python::Token *tok);

    /**
     * @brief handleIndent calculates the indent, pushes and pops indent blocks
     * @param tok = Starttoken
     * @param indent = indent class to mutate
     * @param direction = -1: only calc deindent, 1: only calc indent and push frames instead of block, 0 do both
     * @return
     */
    Python::Token *handleIndent(Python::Token *tok,
                              Python::SourceIndent &indent, int direction);

    // set identifier and sets up reference to RValue
    Python::Token *scanIdentifier(Python::Token *tok);

    /// looks up a previously defined identifier with same name
    const Python::SourceIdentifier *lookupIdentifierReference(Python::Token *tok);

    // scans the RValue ie after '='
    // or scans type hint ie after :
    Python::Token *scanRValue(Python::Token *tok,
                            Python::SourceRoot::TypeInfo &typeInfo,
                            bool isTypeHint);
    Python::Token *scanImports(Python::Token *tok);

    // scan return statement
    Python::Token *scanReturnStmt(Python::Token *tok);
    // scan yield statement
    Python::Token *scanYieldStmt(Python::Token *tok);
    // sanity check after for code after a return or yield
    void scanCodeAfterReturnOrYield(Python::Token *tok, QString name);

    // used to traverse to semicolon after argumentslist for typehint
    // if storeParameters, we add found parameters to parametersList
    Python::Token *scanAllParameters(Python::Token *token, bool storeParameters, bool isInitFunc);
    // sets a parameter
    Python::Token *scanParameter(Python::Token *paramToken, int &parenCount, bool isInitFunc);
    // guess type for identifier
    Python::SourceRoot::TypeInfo guessIdentifierType(const Python::Token *token);
    // goto end of line
    Python::Token *gotoEndOfLine(Python::Token *tok);

#ifdef BUILD_PYTHON_DEBUGTOOLS
    QString m_name;
#endif
};

} // namespace Python
} // namespace Gui

#endif // PYTHONSOURCEFRAMES_H
