#include "PythonSourceDebugTools.h"

#include <TextEditor/PythonCodeDebugTools.h>

#include "PythonSource.h"
#include "PythonSourceRoot.h"
#include "PythonSourceModule.h"
#include "PythonSourceFrames.h"

DBG_TOKEN_FILE

using namespace Gui;

DumpModule::DumpModule(PythonSourceModule *module, const char *outfile) :
    DumpToFileBaseClass(outfile),
    m_module(module)
{
    fprintf(m_file, "dump for module %s at %s\n", m_module->moduleName().toLatin1().data(), datetimeStr());

    const PythonSourceFrame *rootFrm =  m_module->rootFrame();
    fprintf(m_file, "Startframe->isModule:%d\n", rootFrm->isModule());

    dumpFrame(rootFrm, 0);
}

DumpModule::~DumpModule()
{
}

void DumpModule::dumpFrame(const PythonSourceFrame *frm, int indent)
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

    DBG_TOKEN(frm->lastToken());

    fprintf(m_file, "%sFrame name:%s isClass:%d\n", indentStr, frm->name().toLatin1().data(),
                                                    frm->isClass());
    fprintf(m_file, "%ssubframes count:%lu\n", indentStr, frm->size());

    // parameters
    fprintf(m_file, "%sFrame parameters:\n", indentStr);
    if (!frm->isModule()) {
        for (PythonSourceParameter *itm = dynamic_cast<PythonSourceParameter*>(frm->parameters().begin());
             itm != frm->parameters().end();
             itm = dynamic_cast<PythonSourceParameter*>(itm->next()))
        {
            const char *paramType = nullptr;
            switch(itm->parameterType()){
            case PythonSourceParameter::Positional:
                paramType = "Positional"; break;
            case PythonSourceParameter::PositionalDefault:
                paramType = "PositionalDefault"; break;
            case PythonSourceParameter::Keyword:
                paramType = "KeyWord"; break;
            case PythonSourceParameter::Variable:
                paramType = "Variable"; break;
            case PythonSourceParameter::InValid:
                paramType = "InValid"; break;
            default:
                paramType = "Unknown"; break;
            }
            fprintf(m_file, "%s %s(%s)", indentStr, itm->identifierAssignment()->text().toLatin1().data(), paramType);

            PythonSourceIdentifierAssignment *assign = itm->identifierAssignment();
            fprintf(m_file, " : %s\n", assign->typeInfo().typeAsStr().toLatin1().data());
        }
    }

    // iterate through subFrames, recursive
    for(PythonSourceFrame *subFrm = dynamic_cast<PythonSourceFrame*>(frm->begin());
        subFrm != frm->end();
        subFrm = dynamic_cast<PythonSourceFrame*>(subFrm->next()))
    {
        dumpFrame(subFrm, indent + 4);
    }


    // finished print newline
    fprintf(m_file, "\n");
}
