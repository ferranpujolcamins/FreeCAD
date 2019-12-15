#ifndef PYTHONSOURCEPARSER_H
#define PYTHONSOURCEPARSER_H


#include "PythonSourceIdentifiers.h"
#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceParameter.h"
#include "PythonSourceImports.h"
#include "PythonSourceFrames.h"
#include "PythonSourceIndent.h"
#include "PythonSourceRoot.h"
#include "PythonToken.h"

namespace Gui {
namespace Python {

/// this class pasers tokens and creates the SyntaxTree
class SourceParser
{
    Python::SourceModule *m_activeModule;
    Python::SourceFrame *m_activeFrame;
                  /// hold the position of current token
    Python::Token *m_tok,
                  /// holds the poistion of currentToken -1
                  *m_beforeTok;
public:
    explicit SourceParser();
    virtual ~SourceParser();

    /// the currently active module
    Python::SourceModule *activeModule() const { return m_activeModule; }
    void setActiveModule(Python::SourceModule *module);

    /// the currently active frame
    Python::SourceFrame *activeFrame() const { return m_activeFrame; }
    void setActiveFrame(Python::SourceFrame *frame);

    static Python::SourceParser *instance();

    // scan* functions might mutate Python::Token, ie change from Undetermined -> determined etc.
    /// on complete rescan, returns lastToken->next()
    Python::Token *scanFrame(Python::Token *startToken, Python::SourceIndent &indent);

    /// on single row rescan
    Python::Token *scanLine(Python::Token *startToken, Python::SourceIndent &indent);

    /// looks up a previously defined identifier with same name and sets token to referenced type
    const Python::SourceIdentifier *lookupIdentifierReference(Python::Token *tok);


private:
    /// advances token internally in parser
    void moveNext();
    void movePrevious();

    void moveToken(Python::Token *newPos);


    /// starts a new frame
    void startSubFrame(Python::SourceIndent &indent,
                               Python::SourceRoot::DataTypes type);

    /// moves token til next line with tokens
    void gotoNextLine();

    /**
     * @brief handleIndent calculates the indent, pushes and pops indent blocks
     * @param tok = Starttoken
     * @param indent = indent class to mutate
     * @return
     */
    void handleIndent(Python::SourceIndent &indent);
    void handleDedent(Python::SourceIndent &indent);
    const TokenLine *frameStartLine(const Python::Token *semiColonTok);

    // set identifier and sets up reference to RValue
    void scanIdentifier();

    // scans the RValue ie after '='
    // or scans type hint ie after :
    void scanRValue(Python::SourceRoot::TypeInfo &typeInfo,
                            bool isTypeHint);
    void scanImports();

    // scan return statement
    void scanReturnStmt();
    // scan yield statement
    void scanYieldStmt();
    // sanity check after for code after a return or yield
    void scanCodeAfterReturnOrYield(const std::string &name);

    // used to traverse to semicolon after argumentslist for typehint
    // if storeParameters, we add found parameters to parametersList
    void scanAllParameters(bool storeParameters, bool isInitFunc);
    // sets a parameter
    void scanParameter(int &parenCount, bool isInitFunc);
    // guess type for identifier
    Python::SourceRoot::TypeInfo guessIdentifierType(const Python::Token *token);
    // goto end of line
    void gotoEndOfLine();
};


} // namespace Python
} // namespace Gui

#endif // PYTHONSOURCEPARSER_H
