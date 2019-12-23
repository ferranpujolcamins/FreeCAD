#include "PythonSourceDebugTools.h"

//#include <TextEditor/PythonCodeDebugTools.h>

#include "PythonSource.h"
#include "PythonSourceRoot.h"
#include "PythonSourceModule.h"
#include "PythonSourceFrames.h"

DBG_TOKEN_FILE

using namespace Python;

#include <time.h>
#include <cstdint>

// for DBG_TOKEN_FILE
char TOKEN_TEXT_BUF[350];
char TOKEN_NAME_BUF[50];
char TOKEN_INFO_BUF[40];
char TOKEN_SRC_LINE_BUF[350];

// ---------------------------------------------------------------------------------------

DumpToFileBaseClass::DumpToFileBaseClass(const char *outfile)
{
    if (strcmp(outfile, "stdout") == 0)
        m_file = stdout;
    else if (strcmp(outfile, "stderr"))
        m_file = stderr;
    else {
        FileInfo fi(outfile);
        m_file = fopen(fi.absolutePath().c_str(), "w");
    }
}

DumpToFileBaseClass::DumpToFileBaseClass(FILE *fp) :
    m_file(fp)
{
}

DumpToFileBaseClass::~DumpToFileBaseClass()
{
    if (m_file && m_file != stdout && m_file != stderr)
        fclose(m_file);
}

const char *DumpToFileBaseClass::datetimeStr()
{
    static char dateStr[30];
    struct tm tmNow;
    time_t now = time(nullptr);
    tmNow = *(localtime(&now));
    strftime(dateStr, 30, "%Y-%m-%d %H:%M:%S", &tmNow);
    return dateStr;
}

// ---------------------------------------------------------------------------------------

DumpSyntaxTokens::DumpSyntaxTokens(TokenLine *firstLine, const char *outfile):
    DumpToFileBaseClass(outfile)
{
    fprintf(m_file, "dump tokens at %s\n", datetimeStr());

    TokenLine *line = firstLine;
    while(line) {
        // first print the complete srctext as is
        fprintf(m_file, "%s¶\n", line->text().c_str());
        for (Python::Token *tok : line->tokens()) {
            fprintf(m_file, " %s", Token::tokenToCStr(tok->type()));
        }
        fprintf(m_file, "¶\n");
        line = line->nextLine();
    }
}

DumpSyntaxTokens::~DumpSyntaxTokens()
{
}

// ----------------------------------------------------------------------------------------


DumpModule::DumpModule(SourceModule *module, const char *outfile) :
    DumpToFileBaseClass(outfile),
    m_module(module)
{
    create();
}

DumpModule::DumpModule(SourceModule *module, FILE *fp) :
    DumpToFileBaseClass(fp),
    m_module(module)
{
    create();
}

void DumpModule::create()
{
    if (m_module) {
        fprintf(m_file, "dump for module %s at %s\n", m_module->moduleName().c_str(), datetimeStr());

        const SourceFrame *rootFrm =  m_module->rootFrame();
        fprintf(m_file, "Startframe->isModule:%d\n", rootFrm->isModule());

        dumpFrame(rootFrm, 0);
    }
}

DumpModule::~DumpModule()
{
}

void DumpModule::dumpFrame(const SourceFrame *frm, int indent)
{
    DEFINE_DBG_VARS

    // build up indent string
    char indentStr[200];
    int i = 0;
    for (i = 0; i < indent; ++i)
        indentStr[i] = ' ';
    indentStr[i] = 0;

    if (!frm->token()) {
        fprintf(m_file, "%s ***** tried a new frame but it hadnt any startToken, bailing....\n",  indentStr);
        return;
    }
    DBG_TOKEN(frm->token())


    fprintf(m_file, "%s---------------------------------------------------\n", indentStr);
    fprintf(m_file, "%sStarting new frame, startToken:%s(line:%d), endToken:%s(line:%d)\n", indentStr,
            Python::Token::tokenToCStr(frm->token()->type()), frm->token()->line() +1,
            frm->lastToken() ? Python::Token::tokenToCStr(frm->lastToken()->type()) : "-1",
            frm->lastToken() ? frm->lastToken()->line() +1 : -1);

    DBG_TOKEN(frm->lastToken())

    fprintf(m_file, "%sFrame name:%s isClass:%d\n", indentStr, frm->name().c_str(),
                                                    frm->isClass());
    fprintf(m_file, "%ssubframes count:%x\n", indentStr, frm->size());

    // parameters
    fprintf(m_file, "%sFrame parameters:\n", indentStr);
    if (!frm->isModule()) {
        for (auto itm = frm->parameters().begin();
             itm != frm->parameters().end();
             ++itm)
        {
            const char *paramType = nullptr;
            switch((*itm)->parameterType()){
            case SourceParameter::Positional:
                paramType = "Positional"; break;
            case SourceParameter::PositionalDefault:
                paramType = "PositionalDefault"; break;
            case SourceParameter::Keyword:
                paramType = "KeyWord"; break;
            case SourceParameter::Variable:
                paramType = "Variable"; break;
            case SourceParameter::InValid:
                paramType = "InValid"; break;
            default:
                paramType = "Unknown"; break;
            }
            SourceIdentifierAssignment *assign = (*itm)->identifierAssignment();
            const std::string txt = assign ? assign->text() : std::string();
            fprintf(m_file, "%s %s(%s)", indentStr, txt.c_str(), paramType);

            const std::string  typeInfo = assign ? assign->typeInfo().typeAsStr() : std::string();
            fprintf(m_file, " : %s\n", typeInfo.c_str());
        }
    }

    // iterate through subFrames, recursive
    for(SourceFrame *subFrm = dynamic_cast<SourceFrame*>(frm->begin());
        subFrm != frm->end();
        subFrm = dynamic_cast<SourceFrame*>(subFrm->next()))
    {
        dumpFrame(subFrm, indent + 4);
    }


    // finished print newline
    fprintf(m_file, "\n");
}
