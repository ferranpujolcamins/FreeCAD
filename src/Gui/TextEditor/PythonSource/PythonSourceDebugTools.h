#ifndef PYTHONSOURCEDEBUGTOOLS_H
#define PYTHONSOURCEDEBUGTOOLS_H

#include <TextEditor/PythonCodeDebugTools.h>


namespace Gui {
class PythonSourceModule;
class PythonSourceFrame;

// dump identifiers etc from a module and frame
class DumpModule : public DumpToFileBaseClass
{
public:
    explicit DumpModule(PythonSourceModule *module, const char *outfile = "stdout");
    explicit DumpModule(PythonSourceModule *module, FILE *fp);
    ~DumpModule();

    void dumpFrame(const PythonSourceFrame *frm, int indent);
private:
    PythonSourceModule *m_module;
    void create();
};

} // namespace Gui

#endif // PYTHONSOURCEDEBUGTOOLS_H
