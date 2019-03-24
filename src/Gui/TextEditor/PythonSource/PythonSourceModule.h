#ifndef PYTHONSOURCEMODULE_H
#define PYTHONSOURCEMODULE_H

#include "PythonSource.h"
#include "PythonSourceFrames.h"
#include "PythonSourceListBase.h"

namespace Gui {
class PythonSourceRoot;
class PythonSourceIndent;

/// for modules contains module global identifiers and methods
/// Don't confuse with PythonSourceImport that stares each import in code
class PythonSourceModule : public PythonSourceListParentBase
{
    PythonSourceRoot *m_root;
    PythonSourceFrame m_rootFrame;
    QString m_filePath;
    QString m_moduleName;
    PythonSyntaxHighlighter *m_highlighter;
    bool m_rehighlight;
public:

    explicit PythonSourceModule(PythonSourceRoot *root,
                                PythonSyntaxHighlighter *highlighter);
    ~PythonSourceModule();

    PythonSourceRoot *root() const { return m_root; }
    const PythonSourceFrame *rootFrame() const { return &m_rootFrame; }

    /// filepath for this module
    QString filePath() const { return m_filePath; }
    void setFilePath(QString filePath) { m_filePath = filePath; }

    /// modulename, ie sys or PartDesign
    QString moduleName() const { return m_moduleName; }
    void setModuleName(QString moduleName) { m_moduleName = moduleName; }

    /// rescans all lines in frame that encapsulates tok
    void scanFrame(PythonToken *tok);
    /// rescans a single row
    void scanLine(PythonToken *tok);

    bool shouldRehighlight() const { return m_rehighlight; }

    /// returns indent info for block where tok is
    PythonSourceIndent currentBlockIndent(const PythonToken *tok) const;

    // syntax highlighter stuff
    PythonSyntaxHighlighter *highlighter() const { return m_highlighter; }
    void setFormatToken(const PythonToken *tok, QTextCharFormat format);
    void setFormatToken(const PythonToken *tok);

    /// stores a syntax error at tok with message and sets up for repaint
    void setSyntaxError(const PythonToken *tok, QString parseMessage) const;

    /// sets a indenterror (sets token to indentError and sets up for style document)
    void setIndentError(const PythonToken *tok) const;

    /// sets a lookup error (variable not found) with message, or default message
    void setLookupError(const PythonToken *tok, QString parseMessage = QString()) const;

    /// stores a message at tok
    void setMessage(const PythonToken *tok, QString parseMessage) const;

    /// returns the frame for given token
    const PythonSourceFrame *getFrameForToken(const PythonToken *tok,
                                              const PythonSourceFrame *parentFrame) const;


    /// inserts a blockStartTok after colonTok
    void insertBlockStart(const PythonToken *colonTok) const;
    /// inserts a blockEnd token before newLineTok
    PythonToken *insertBlockEnd(const PythonToken *newLineTok) const;


protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right);


private:
    int _currentBlockIndent(const PythonToken *tok) const;
};

// --------------------------------------------------------------------------

/// collection class for all modules
class PythonSourceModuleList : public PythonSourceListParentBase
{
    PythonSourceModule *m_main;
public:
    explicit PythonSourceModuleList();
    ~PythonSourceModuleList();

    PythonSourceModule *main() const { return m_main; }
    void setMain(PythonSourceModule *main);
    virtual bool remove(PythonSourceListNodeBase *node, bool deleteNode = false);
};


} // namespace Gui

#endif // PYTHONSOURCEMODULE_H
