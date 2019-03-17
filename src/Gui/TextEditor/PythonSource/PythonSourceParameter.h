#ifndef PYTHONSOURCEPARAMETER_H
#define PYTHONSOURCEPARAMETER_H

#include "PythonSource.h"
#include "PythonSourceRoot.h"
#include "PythonSourceListBase.h"

namespace Gui {
class PythonSourceFrame;
class PythonSourceIdentifierAssignment;
class PythonSourceParameterList;

// class for function/method parameters ie: def func(arg1, arg2=None, *arg3, **arg4)
//                                                   ^     ^           ^       ^
class PythonSourceParameter : public PythonSourceListNodeBase
{
public:
    /// type for this parameter, ie (arg1, arg2=0, *arg3, **arg4)
    ///  translates to      (Positional, PositionalDefault, Variable, Keyword)
    enum ParameterType {
        InValid,
        Positional,
        PositionalDefault,
        Variable,
        Keyword
    };
    explicit PythonSourceParameter(PythonSourceParameterList *parent, PythonToken *tok);
    ~PythonSourceParameter();

    /// get the frame of this parameter
    const PythonSourceFrame *frame() const;

    /// get the identifierAssignment of this argument
    PythonSourceIdentifierAssignment *identifierAssignment() const;

    /// type for this parameter, ie (arg1, arg2=0, *arg3, **arg4)
    ///  translates to      (Positional, PositionalDefault, Variable, Keyword)
    ParameterType parameterType() const { return m_paramType; }
    void setParameterType(ParameterType paramType) { m_paramType = paramType; }

    const PythonSourceRoot::TypeInfo type() const { return m_type; }
    void setType(PythonSourceRoot::TypeInfo &type) { m_type = type; } // type is implicitly copied


private:
    PythonSourceRoot::TypeInfo m_type;
    ParameterType m_paramType;
};

// ----------------------------------------------------------------------

class PythonSourceParameterList : public PythonSourceListParentBase
{
    PythonSourceFrame *m_frame;
public:
    explicit PythonSourceParameterList(PythonSourceFrame *frame);
    ~PythonSourceParameterList();

    /// get the frame contained for these collections
    const PythonSourceFrame *frame() const { return m_frame; }
    /// get the parameter with name or nullptr if not contained
    const PythonSourceParameter *getParameter(const QString name) const;
    bool hasParameter(const QString name) const { return getParameter(name) != nullptr; }
    /// updates param type and or creates parameter if not exists
    PythonSourceParameter *setParameter(PythonToken *tok,
                                         PythonSourceRoot::TypeInfo typeInfo,
                                         PythonSourceParameter::ParameterType paramType);

protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

} // namespace Gui

#endif // PYTHONSOURCEPARAMETER_H
