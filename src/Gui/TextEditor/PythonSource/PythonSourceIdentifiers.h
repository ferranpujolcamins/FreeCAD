#ifndef PYTHONSOURCEIDENTIFIERS_H
#define PYTHONSOURCEIDENTIFIERS_H

#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceRoot.h"
//#include "PythonSourceImports.h"
//#include "PythonSourceModule.h"

#include <TextEditor/PythonSyntaxHighlighter.h>

namespace Gui {
namespace Python {


class SourceImports;
class SourceModule;
class SourceIdentifier;
class SourceTypeHint;

/// a class for each assignment within frame (variable type might mutate)
class SourceIdentifierAssignment : public Python::SourceListNodeBase {
    Python::SourceRoot::TypeInfo m_type;
public:
    explicit SourceIdentifierAssignment(Python::SourceIdentifier *owner,
                                        Python::Token *startToken,
                                        Python::SourceRoot::TypeInfo typeInfo);
    explicit SourceIdentifierAssignment(Python::SourceTypeHint *owner,
                                        Python::Token *startToken,
                                        Python::SourceRoot::TypeInfo typeInfo);
    ~SourceIdentifierAssignment();
    Python::SourceRoot::TypeInfo typeInfo() const { return m_type; }
    void setType(Python::SourceRoot::TypeInfo &type) { m_type = type; } // type is implicitly copied
    int linenr() const;
    int position() const; // as in pos in document line
};
// --------------------------------------------------------------------

typedef Python::SourceIdentifierAssignment SourceTypeHintAssignment;

// --------------------------------------------------------------------


class SourceTypeHint : public Python::SourceListParentBase
{
    const Python::SourceModule *m_module;
    const Python::SourceIdentifier *m_identifer;
    Python::SourceRoot::TypeInfo m_type;
public:
    explicit SourceTypeHint(Python::SourceListParentBase *owner,
                            const Python::SourceIdentifier *identifer,
                            const Python::SourceModule *module);
    ~SourceTypeHint();

    const Python::SourceModule *module() const { return m_module; }
    const Python::SourceFrame *frame() const;
    const Python::SourceIdentifier *identifier() const { return m_identifer; }

    /// gets last typehint assignment up til line and pos in document
    /// or nullptr if not found
    Python::SourceTypeHintAssignment *getFromPos(int line, int pos) const;
    Python::SourceTypeHintAssignment *getFromPos(const Python::Token *tok) const;

    /// get name of typeHint
    QString name() const;

    /// get the type of typeHint
    Python::SourceRoot::TypeInfo type() const { return m_type; }

protected:
    int compare(const Python::SourceListNodeBase *left,
                const Python::SourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------


// ----------------------------------------------------------------------

/// identifierClass, one for each variable in each frame
class SourceIdentifier : public Python::SourceListParentBase
{
    const Python::SourceModule *m_module;
    Python::SourceTypeHint m_typeHint;
public:
    explicit SourceIdentifier(Python::SourceListParentBase *owner,
                              const Python::SourceModule *module);
    ~SourceIdentifier();

    /// module for this identifier
    const Python::SourceModule *module() const { return m_module; }
    /// frame for this identifier
    const Python::SourceFrame *frame() const;

    /// get the last assigment up til line and pos in document
    /// or nullptr if not found
    Python::SourceIdentifierAssignment *getFromPos(int line, int pos) const;
    Python::SourceIdentifierAssignment *getFromPos(const Python::Token *tok) const;

    /// returns true if identifier is callable from line
    bool isCallable(const Python::Token *tok) const;
    bool isCallable(int line, int pos) const;

    /// returns the frame assosiated with this identifier
    const Python::SourceFrame *callFrame(const Python::Token *tok) const;

    /// returns true if identifier is imported
    bool isImported(const Python::Token *tok) const;
    bool isImported(int line, int pos) const;

    /// get the name of identifier
    QString name() const;

    /// get last inserted TypeHint for identifier up to including line
    Python::SourceTypeHintAssignment *getTypeHintAssignment(const Python::Token *tok) const;
    Python::SourceTypeHintAssignment *getTypeHintAssignment(int line) const;
    /// returns true if identifier has a typehint up to including line
    bool hasTypeHint(int line) const;
    /// set typeHint
    Python::SourceTypeHintAssignment *setTypeHint(Python::Token *tok,
                                                Python::SourceRoot::TypeInfo typeInfo);

protected:
    // should sort by linenr and (if same line also token startpos)
    int compare(const Python::SourceListNodeBase *left,
                const Python::SourceListNodeBase *right) const;

#ifdef BUILD_PYTHON_DEBUGTOOLS
public:
    QString m_name;
#endif
};

// ----------------------------------------------------------------------------
/// a root container for all identifiers
class SourceIdentifierList : public Python::SourceListParentBase
{
    const Python::SourceModule *m_module;
public:
    explicit SourceIdentifierList(const Python::SourceModule *module);
    ~SourceIdentifierList();

    /// get the module for this identifierList
    const Python::SourceModule *module() const { return m_module; }
    /// get the frame contained for these collections
    const Python::SourceFrame *frame() const;
    /// get the identifier with name or nullptr if not contained
    const Python::SourceIdentifier *getIdentifier(const QString name) const;
    const Python::SourceIdentifier *getIdentifier(const Python::Token *tok) const {
        return getIdentifier(tok->text());
    }
    bool hasIdentifier(const QString name) const { return getIdentifier(name) != nullptr; }
    bool hasIdentifier(const Python::Token *tok) const {
        return getIdentifier(tok->text()) == nullptr;
    }
    /// sets a new assignment, creates identifier if not exists
    Python::SourceIdentifier *setIdentifier(Python::Token *tok,
                                            Python::SourceRoot::TypeInfo typeInfo);

    /// get the identifier that is pointed to by token
    /// This traverses chain upwards until it finds identifier
    /// with non refernce type (discrete assignment)
    /// limitChain limits lookup chain
    ///     var1 = None <- none is discrete
    ///     var2 = var1 <- getIdentifierReferencedBy(var2token) returns var1
    ///     var3 = var2 <- getIdentifierReferencedBy(var3token) returns var1
    ///     var4 = var3 <- getIdentifierReferencedBy(var4token) also returns var1
    ///     var5 = var4 <- getIdentifierReferencedBy(var5token, limitChain = 1) returns var4 due to limitChain
    const Python::SourceIdentifier *getIdentifierReferencedBy(const Python::Token *tok,
                                                              int limitChain = 10) const;
    const Python::SourceIdentifier *getIdentifierReferencedBy(const Python::SourceIdentifier *ident,
                                                              int line, int pos,
                                                              int limitChain = 10) const;

    /// returns a list containing all identifiers dependent on
    const Python::SourceIdentifierList getIdentifierDependants(const Python::SourceIdentifier *ident,
                                                               int line, int pos,
                                                               int limitChain = 10) const;

    const Python::SourceIdentifierList getIdentifierDependants(const Python::Token *tok,
                                                               int limitChain = 10) const;

protected:
    int compare(const Python::SourceListNodeBase *left,
                const Python::SourceListNodeBase *right) const;
};

} // namespace Python
} // namespace Gui

#endif // PYTHONSOURCEIDENTIFIERS_H
