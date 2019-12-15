#ifndef PYTHONSOURCEDEBUGTOOLS_H
#define PYTHONSOURCEDEBUGTOOLS_H

#include "PythonToken.h"

namespace Python {

class SourceModule;
class SourceFrame;


const char* tokenToCStr(Token::Type tok);


// --------------------------------------------------------------------

class DumpToFileBaseClass
{
public:
    explicit DumpToFileBaseClass(const char *outfile = "stdout");
    explicit DumpToFileBaseClass(FILE *fp);
    virtual ~DumpToFileBaseClass();

    const char *datetimeStr();
protected:
    FILE *m_file;
};

// -----------------------------------------------------------------

// dumps all tokens from pythonsyntaxhighlighter
class DumpSyntaxTokens : public DumpToFileBaseClass
{
public:
    explicit DumpSyntaxTokens(TokenLine *firstLine, const char *outfile = "stdout");
    ~DumpSyntaxTokens();
};

// --------------------------------------------------------------------

// dump identifiers etc from a module and frame
class DumpModule : public DumpToFileBaseClass
{
public:
    explicit DumpModule(SourceModule *module, const char *outfile = "stdout");
    explicit DumpModule(SourceModule *module, FILE *fp);
    ~DumpModule();

    void dumpFrame(const SourceFrame *frm, int indent);
private:
    SourceModule *m_module;
    void create();
};


// --------------------------------------------------------------------

} // namespace Python

#endif // PYTHONSOURCEDEBUGTOOLS_H
