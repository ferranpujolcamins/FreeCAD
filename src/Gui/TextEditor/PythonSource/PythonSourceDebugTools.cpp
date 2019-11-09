#include "PythonSourceDebugTools.h"

#include <TextEditor/PythonCodeDebugTools.h>

#include "PythonSource.h"
#include "PythonSourceRoot.h"
#include "PythonSourceModule.h"
#include "PythonSourceFrames.h"

DBG_TOKEN_FILE

using namespace Gui;
using namespace Python;

DumpModule::DumpModule(Python::SourceModule *module, const char *outfile) :
    DumpToFileBaseClass(outfile),
    m_module(module)
{
    create();
}

DumpModule::DumpModule(Python::SourceModule *module, FILE *fp) :
    DumpToFileBaseClass(fp),
    m_module(module)
{
    create();
}

void DumpModule::create()
{
    if (m_module) {
        fprintf(m_file, "dump for module %s at %s\n", m_module->moduleName().toLatin1().data(), datetimeStr());

        const Python::SourceFrame *rootFrm =  m_module->rootFrame();
        fprintf(m_file, "Startframe->isModule:%d\n", rootFrm->isModule());

        dumpFrame(rootFrm, 0);
    }
}

DumpModule::~DumpModule()
{
}

void DumpModule::dumpFrame(const Python::SourceFrame *frm, int indent)
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
            Syntax::tokenToCStr(frm->token()->token), frm->token()->line() +1,
            frm->lastToken() ? Syntax::tokenToCStr(frm->lastToken()->token) : "-1",
            frm->lastToken() ? frm->lastToken()->line() +1 : -1);

    DBG_TOKEN(frm->lastToken())

    fprintf(m_file, "%sFrame name:%s isClass:%d\n", indentStr, frm->name().toLatin1().data(),
                                                    frm->isClass());
    fprintf(m_file, "%ssubframes count:%x\n", indentStr, frm->size());

    // parameters
    fprintf(m_file, "%sFrame parameters:\n", indentStr);
    if (!frm->isModule()) {
        for (Python::SourceParameter *itm = dynamic_cast<Python::SourceParameter*>(frm->parameters().begin());
             itm != frm->parameters().end();
             itm = dynamic_cast<Python::SourceParameter*>(itm->next()))
        {
            const char *paramType = nullptr;
            switch(itm->parameterType()){
            case Python::SourceParameter::Positional:
                paramType = "Positional"; break;
            case Python::SourceParameter::PositionalDefault:
                paramType = "PositionalDefault"; break;
            case Python::SourceParameter::Keyword:
                paramType = "KeyWord"; break;
            case Python::SourceParameter::Variable:
                paramType = "Variable"; break;
            case Python::SourceParameter::InValid:
                paramType = "InValid"; break;
            default:
                paramType = "Unknown"; break;
            }
            Python::SourceIdentifierAssignment *assign = itm->identifierAssignment();
            QString txt = assign ? assign->text() : QString();
            fprintf(m_file, "%s %s(%s)", indentStr, txt.toLatin1().data(), paramType);

            QString typeInfo = assign ? assign->typeInfo().typeAsStr() : QString();
            fprintf(m_file, " : %s\n", typeInfo.toLatin1().data());
        }
    }

    // iterate through subFrames, recursive
    for(Python::SourceFrame *subFrm = dynamic_cast<Python::SourceFrame*>(frm->begin());
        subFrm != frm->end();
        subFrm = dynamic_cast<Python::SourceFrame*>(subFrm->next()))
    {
        dumpFrame(subFrm, indent + 4);
    }


    // finished print newline
    fprintf(m_file, "\n");
}
