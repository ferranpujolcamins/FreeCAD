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

/// a return can be built up by several Identifiers and operators
///  ie: return (var1 + var3) * 4 or 5
///              ^    ^ ^       ^    ^
/// we store pos of return, and compute type when we need it
class PythonSourceFrameReturnType : public PythonSourceListNodeBase
{
    const PythonSourceModule *m_module;
    PythonSourceRoot::TypeInfo m_typeInfo;
public:
    explicit PythonSourceFrameReturnType(PythonSourceListParentBase *owner,
                                         const PythonSourceModule *module,
                                         PythonToken *tok);
    virtual ~PythonSourceFrameReturnType();

    // getter/setters for typeInfo
    void setTypeInfo(PythonSourceRoot::TypeInfo typeInfo);
    PythonSourceRoot::TypeInfo typeInfo() const { return m_typeInfo; }

    /// return this type
    PythonSourceRoot::TypeInfo returnType() const;
    /// true if it is a yield
    bool isYield() const;
    /// yields this type
    PythonSourceRoot::TypeInfo yieldType() const;
};

// --------------------------------------------------------------------

class PythonSourceFrameReturnTypeList : public PythonSourceListParentBase
{
    const PythonSourceModule *m_module;
public:
    explicit PythonSourceFrameReturnTypeList(PythonSourceListParentBase *owner,
                                    const PythonSourceModule *module);
    PythonSourceFrameReturnTypeList(const PythonSourceFrameReturnTypeList &other);
    virtual ~PythonSourceFrameReturnTypeList();
protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------

/// for function or method frames, contains all identifiers within this frame
class PythonSourceFrame : public PythonSourceListParentBase
{
    PythonSourceListParentBase *m_owner;
    /// stores all identifiers contained within each frame
    PythonSourceIdentifierList m_identifiers;
    PythonSourceParameterList m_parameters;
    PythonSourceImportList    m_imports;
    PythonSourceFrameReturnTypeList m_returnTypes; // as in return types
    PythonSourceRoot::TypeInfo m_type;
    PythonSourceTypeHint *m_typeHint;
    PythonSourceFrame *m_parentFrame;
    const PythonSourceModule *m_module;
    PythonSourceListNodeBase m_lastToken; // we need to use ListNodeBase as a wrapper
                                          // handle gracefully when Pythontoken gets deleted in document
    bool m_isClass;

public:
    explicit PythonSourceFrame(PythonSourceFrame *owner, const PythonSourceModule *module,
                               PythonSourceFrame *parentFrame = nullptr, bool isClass = false);
    explicit PythonSourceFrame(PythonSourceModule *owner, const PythonSourceModule *module,
                               PythonSourceFrame *parentFrame = nullptr, bool isClass = false);
    ~PythonSourceFrame();


    /// get the parent function frame
    PythonSourceFrame *parentFrame() const { return m_parentFrame; }
    void setParentFrame(PythonSourceFrame *frame) { m_parentFrame = frame; }

    const PythonSourceModule *module() const { return m_module; }
    PythonSourceRoot::TypeInfo type() const { return m_type; }

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
    PythonSourceRoot::TypeInfo returnTypeHint() const;

    /// true if we have identifier
    bool hasIdentifier(const QString name) const {
        return m_identifiers.getIdentifier(name);
    }

    const PythonSourceTypeHint *typeHint() const { return m_typeHint; }

    /// is root frame for class
    bool isClass() const { return m_isClass; }

    /// looks up name among identifiers (variables/constants)
    const PythonSourceIdentifier *getIdentifier(const QString name) const {
        return m_identifiers.getIdentifier(name);
    }
    /// get reference to all identifiers within this frame
    const PythonSourceIdentifierList &identifiers() const { return m_identifiers; }

    // get reference to all parameters for this frame
    const PythonSourceParameterList &parameters() const { return m_parameters; }

    /// returns a list with all the types for this frame
    ///   returnType Might differ
    const PythonSourceFrameReturnTypeList returnTypes() const;

    /// the token (unindent) that ends this frame
    const PythonToken *lastToken() const { return m_lastToken.token(); }

    // scan* functions might mutate PythonToken, ie change from Undetermined -> determined etc.
    /// on complete rescan, returns lastToken->next()
    PythonToken *scanFrame(PythonToken *startToken, PythonSourceIndent &indent);

    /// on single row rescan
    PythonToken *scanLine(PythonToken *startToken, PythonSourceIndent &indent);


private:
    // moves token til next line with tokens
    PythonToken *gotoNextLine(PythonToken *tok);

    PythonToken *handleIndent(PythonToken *tok,
                              PythonSourceIndent &indent);

    // set identifier and sets up reference to RValue
    PythonToken *scanIdentifier(PythonToken *tok);
    // scans the RValue ie after '='
    // or scans type hint ie after :
    PythonToken *scanRValue(PythonToken *tok,
                            PythonSourceRoot::TypeInfo &typeInfo,
                            bool isTypeHint);
    PythonToken *scanImports(PythonToken *tok);

    // scan return statement
    PythonToken *scanReturnStmt(PythonToken *tok);
    // scan yield statement
    PythonToken *scanYieldStmt(PythonToken *tok);
    // sanity check after for code after a return or yield
    void scanCodeAfterReturnOrYield(PythonToken *tok);

    // used to traverse to semicolon after argumentslist for typehint
    // if storeParameters, we add found parameters to parametersList
    PythonToken *scanAllParameters(PythonToken *token, bool storeParameters, bool isInitFunc);
    // sets a parameter
    PythonToken *scanParameter(PythonToken *paramToken, int &parenCount, bool isInitFunc);
    // guess type for identifier
    PythonSourceRoot::TypeInfo guessIdentifierType(const PythonToken *token);
    // goto end of line
    PythonToken *gotoEndOfLine(PythonToken *tok);
};

} // namespace Gui

#endif // PYTHONSOURCEFRAMES_H
