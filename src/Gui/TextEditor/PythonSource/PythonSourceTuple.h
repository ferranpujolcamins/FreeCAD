#ifndef PYTHONSOURCETUPLE_H
#define PYTHONSOURCETUPLE_H

#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceRoot.h"

#include <TextEditor/PythonSyntaxHighlighter.h>

namespace Gui {

class PythonSourceTuple : public PythonSourceListNodeBase
{
    const PythonSourceModule *m_module;
public:
    explicit PythonSourceTuple(PythonSourceListParentBase *owner,
                               PythonSourceModule *module);
    ~PythonSourceTuple();

};
}

#endif // PYTHONSOURCETUPLE_H
