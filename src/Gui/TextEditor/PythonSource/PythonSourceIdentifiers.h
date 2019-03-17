#ifndef PYTHONSOURCEIDENTIFIERS_H
#define PYTHONSOURCEIDENTIFIERS_H

#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceRoot.h"
//#include "PythonSourceImports.h"
//#include "PythonSourceModule.h"

#include <TextEditor/PythonSyntaxHighlighter.h>

namespace Gui {
class PythonSourceImports;
class PythonSourceModule;
class PythonSourceIdentifier;
class PythonSourceTypeHint;

/// a class for each assignment within frame (variable type might mutate)
class PythonSourceIdentifierAssignment : public PythonSourceListNodeBase {
    PythonSourceRoot::TypeInfo m_type;
public:
    explicit PythonSourceIdentifierAssignment(PythonSourceIdentifier *owner,
                                              PythonToken *startToken,
                                              PythonSourceRoot::TypeInfo typeInfo);
    explicit PythonSourceIdentifierAssignment(PythonSourceTypeHint *owner,
                                              PythonToken *startToken,
                                              PythonSourceRoot::TypeInfo typeInfo);
    ~PythonSourceIdentifierAssignment();
    PythonSourceRoot::TypeInfo typeInfo() const { return m_type; }
    void setType(PythonSourceRoot::TypeInfo &type) { m_type = type; } // type is implicitly copied
    int linenr() const;
    int position() const; // as in pos in document line
};
// --------------------------------------------------------------------

typedef PythonSourceIdentifierAssignment PythonSourceTypeHintAssignment;

// --------------------------------------------------------------------


class PythonSourceTypeHint : public PythonSourceListParentBase
{
    const PythonSourceModule *m_module;
    const PythonSourceIdentifier *m_identifer;
    PythonSourceRoot::TypeInfo m_type;
public:
    explicit PythonSourceTypeHint(PythonSourceListParentBase *owner,
                                  const PythonSourceIdentifier *identifer,
                                  const PythonSourceModule *module);
    ~PythonSourceTypeHint();

    const PythonSourceModule *module() const { return m_module; }
    const PythonSourceFrame *frame() const;
    const PythonSourceIdentifier *identifier() const { return m_identifer; }

    /// gets last typehint assignment up til line and pos in document
    /// or nullptr if not found
    PythonSourceTypeHintAssignment *getFromPos(int line, int pos) const;
    PythonSourceTypeHintAssignment *getFromPos(const PythonToken *tok) const;

    /// get name of typeHint
    QString name() const;

    /// get the type of typeHint
    PythonSourceRoot::TypeInfo type() const { return m_type; }

protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------


// ----------------------------------------------------------------------

/// identifierClass, one for each variable in each frame
class PythonSourceIdentifier : public PythonSourceListParentBase
{
    const PythonSourceModule *m_module;
    PythonSourceTypeHint m_typeHint;
public:
    explicit PythonSourceIdentifier(PythonSourceListParentBase *owner,
                                    const PythonSourceModule *module);
    ~PythonSourceIdentifier();

    /// module for this identifier
    const PythonSourceModule *module() const { return m_module; }
    /// frame for this identifier
    const PythonSourceFrame *frame() const;

    /// get the last assigment up til line and pos in document
    /// or nullptr if not found
    PythonSourceIdentifierAssignment *getFromPos(int line, int pos) const;
    PythonSourceIdentifierAssignment *getFromPos(const PythonToken *tok) const;

    /// get the name of identifier
    QString name() const;

    /// get last inserted TypeHint for identifier up to including line
    PythonSourceTypeHintAssignment *getTypeHintAssignment(const PythonToken *tok) const;
    PythonSourceTypeHintAssignment *getTypeHintAssignment(int line) const;
    /// returns true if identifier has a typehint up to including line
    bool hasTypeHint(int line) const;
    /// set typeHint
    PythonSourceTypeHintAssignment *setTypeHint(PythonToken *tok,
                                                PythonSourceRoot::TypeInfo typeInfo);

protected:
    // should sort by linenr and (if same line also token startpos)
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// ----------------------------------------------------------------------------
/// a root container for all identifiers
class PythonSourceIdentifierList : public PythonSourceListParentBase
{
    const PythonSourceModule *m_module;
public:
    explicit PythonSourceIdentifierList(const PythonSourceModule *module);
    ~PythonSourceIdentifierList();

    /// get the module for this identifierList
    const PythonSourceModule *module() const { return m_module; }
    /// get the frame contained for these collections
    const PythonSourceFrame *frame() const;
    /// get the identifier with name or nullptr if not contained
    const PythonSourceIdentifier *getIdentifier(const QString name) const;
    bool hasIdentifier(const QString name) const { return getIdentifier(name) != nullptr; }
    /// sets a new assignment, creates identifier if not exists
    PythonSourceIdentifier *setIdentifier(PythonToken *tok,
                                          PythonSourceRoot::TypeInfo typeInfo);

    /// get the identifier that is pointed to by token
    /// This traverses chain upwards until it finds identifier
    /// with non refernce type (discrete assignment)
    /// limitChain limits lookup chain
    ///     var1 = None <- none is discrete
    ///     var2 = var1 <- getIdentifierReferencedBy(var2token) returns var1
    ///     var3 = var2 <- getIdentifierReferencedBy(var3token) returns var1
    ///     var4 = var3 <- getIdentifierReferencedBy(var4token) also returns var1
    ///     var5 = var4 <- getIdentifierReferencedBy(var5token, limitChain = 1) returns var4
    const PythonSourceIdentifier *getIdentifierReferencedBy(const PythonToken *tok,
                                                            int limitChain = 10) const;
    const PythonSourceIdentifier *getIdentifierReferencedBy(const PythonSourceIdentifier *ident,
                                                            int line, int pos,
                                                            int limitChain = 10) const;

    /// returns a list containing all identifiers dependent on
    const PythonSourceIdentifierList getIdentifierDependants(const PythonSourceIdentifier *ident,
                                                             int line, int pos,
                                                             int limitChain = 10) const;

    const PythonSourceIdentifierList getIdentifierDependants(const PythonToken *tok,
                                                             int limitChain = 10) const;protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

} // namespace Gui

#endif // PYTHONSOURCEIDENTIFIERS_H
