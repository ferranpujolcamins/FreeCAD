#ifndef PYTHONSOURCEDEBUGTOOLS_H
#define PYTHONSOURCEDEBUGTOOLS_H

#include <TextEditor/PythonCodeDebugTools.h>


namespace Gui {
namespace Python {


class SourceModule;
class SourceFrame;

// dump identifiers etc from a module and frame
class DumpModule : public DumpToFileBaseClass
{
public:
    explicit DumpModule(Python::SourceModule *module, const char *outfile = "stdout");
    explicit DumpModule(Python::SourceModule *module, FILE *fp);
    ~DumpModule();

    void dumpFrame(const Python::SourceFrame *frm, int indent);
private:
    Python::SourceModule *m_module;
    void create();
};

} // namespace Python
} // namespace Gui

#endif // PYTHONSOURCEDEBUGTOOLS_H
