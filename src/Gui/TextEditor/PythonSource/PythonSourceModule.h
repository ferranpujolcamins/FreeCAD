#ifndef PYTHONSOURCEMODULE_H
#define PYTHONSOURCEMODULE_H

#include "PythonSource.h"
#include "PythonSourceFrames.h"
#include "PythonSourceListBase.h"

#include <QList>

namespace Gui {
namespace Python {

class SourceRoot;
class SourceIndent;

/// for modules contains module global identifiers and methods
/// Don't confuse with Python::SourceImport that stares each import in code
class SourceModule : public Python::SourceListParentBase
{
    Python::SourceRoot *m_root;
    Python::SourceFrame m_rootFrame;
    QString m_filePath;
    QString m_moduleName;
    Python::SyntaxHighlighter *m_highlighter;
    QList<int> m_rehighlightRows;
public:

    explicit SourceModule(Python::SourceRoot *root,
                                Python::SyntaxHighlighter *highlighter);
    ~SourceModule();

    Python::SourceRoot *root() const { return m_root; }
    const Python::SourceFrame *rootFrame() const { return &m_rootFrame; }

    /// filepath for this module
    QString filePath() const { return m_filePath; }
    void setFilePath(QString filePath) { m_filePath = filePath; }

    /// modulename, ie sys or PartDesign
    QString moduleName() const { return m_moduleName; }
    void setModuleName(QString moduleName) { m_moduleName = moduleName; }

    /// rescans all lines in frame that encapsulates tok
    void scanFrame(Python::Token *tok);
    /// rescans a single row
    void scanLine(Python::Token *tok);

    bool shouldRehighlight() const { return m_rehighlightRows.size() > 0; }

    /// returns indent info for block where tok is
    Python::SourceIndent currentBlockIndent(const Python::Token *tok) const;

    // syntax highlighter stuff
    Python::SyntaxHighlighter *highlighter() const { return m_highlighter; }
    void setFormatToken(const Python::Token *tok, QTextCharFormat format);
    void setFormatToken(const Python::Token *tok);

    /// stores a syntax error at tok with message and sets up for repaint
    void setSyntaxError(const Python::Token *tok, QString parseMessage) const;

    /// sets a indenterror (sets token to indentError and sets up for style document)
    void setIndentError(const Python::Token *tok) const;

    /// sets a lookup error (variable not found) with message, or default message
    void setLookupError(const Python::Token *tok, QString parseMessage = QString()) const;

    /// stores a message at tok
    void setMessage(const Python::Token *tok, QString parseMessage) const;

    /// returns the frame for given token
    const Python::SourceFrame *getFrameForToken(const Python::Token *tok,
                                              const Python::SourceFrame *parentFrame) const;


    /// inserts a blockStartTok after colonTok
    void insertBlockStart(const Python::Token *colonTok) const;
    /// inserts a blockEnd token before newLineTok
    Python::Token *insertBlockEnd(const Python::Token *newLineTok) const;


protected:
    int compare(const Python::SourceListNodeBase *left,
                const Python::SourceListNodeBase *right);


private:
    int _currentBlockIndent(const Python::Token *tok) const;
    void reHighLight(const Python::Token *tok);
};

// --------------------------------------------------------------------------

/// collection class for all modules
class SourceModuleList : public Python::SourceListParentBase
{
    Python::SourceModule *m_main;
public:
    explicit SourceModuleList();
    ~SourceModuleList();

    Python::SourceModule *main() const { return m_main; }
    void setMain(Python::SourceModule *main);
    virtual bool remove(Python::SourceListNodeBase *node, bool deleteNode = false);
};


} // namespace Python
} // namespace Gui

#endif // PYTHONSOURCEMODULE_H
