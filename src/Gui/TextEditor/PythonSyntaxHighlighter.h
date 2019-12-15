#ifndef PYTHONSYNTAXHIGHLIGHTER_H
#define PYTHONSYNTAXHIGHLIGHTER_H

#include "PreCompiled.h"
#include "SyntaxHighlighter.h"
#include <Gui/TextEditor/PythonSource/PythonToken.h>
#include "TextEditor.h"
#include <Base/Parameter.h>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Base {
class PyException;
class PyExceptionInfo;
}

namespace Gui {
class TextEditBlockScanInfo;
class PythonEditor;
class PythonConsoleTextEdit;

class TextBlockData;
class PythonSyntaxHighlighterP;

/**
 * Syntax highlighter for Python.
 */
class GuiExport PythonSyntaxHighlighter : public Gui::SyntaxHighlighter,
                                          public Python::Lexer
{
    Q_OBJECT
public:
    PythonSyntaxHighlighter(QObject* parent);
    ~PythonSyntaxHighlighter() override;

    void highlightBlock (const QString & text) override;

    void rehighlight();

/*
    /// masks for decoding userState()
    enum {
        ParamLineShiftPos = 17,
        ParenCntShiftPos  = 28,
        TokensMASK      = 0x0000FFFF,
        ParamLineMASK   = 0x00010000, // when we are whithin a function parameter line
        ParenCntMASK    = 0xF0000000, // how many parens we have open from previous block
        PreventTokenize = 0x00020000, // prevents this call from running tokenize, used to repaint this block
        Rehighlighted   = 0x00040000, // used to determine if we already have rehighlighted
    };
    */

    /// returns the format to color this token
    QTextCharFormat getFormatToken(const Python::Token *token) const;

    /// update how this token is rendered
    void tokenTypeChanged(const Python::Token *tok) const override;

    /// these formats a line in a predefined maner to to highlight were the error occured
    void setMessage(const Python::Token *tok) const override;
    void setIndentError(const Python::Token *tok) const override;
    void setSyntaxError(const Python::Token *tok) const override;

    /// inserts a new format for token
    void newFormatToken(const Python::Token *tok, QTextCharFormat format) const;

    // used by code analyzer, set by editor
    void setFilePath(QString filePath);
    QString filePath() const;

protected:
    /// sets (re-colors) txt contained from token
    void tokenUpdated(const Python::Token *tok) override;

private Q_SLOTS:
    void sourceScanTmrCallback();

private:
    PythonSyntaxHighlighterP* d;

};


// ------------------------------------------------------------------------

struct PythonMatchingCharInfo
{
    char character;
    int position;
    PythonMatchingCharInfo();
    PythonMatchingCharInfo(const PythonMatchingCharInfo &other);
    PythonMatchingCharInfo(char chr, int pos);
    ~PythonMatchingCharInfo();
    char matchingChar() const;
    static char matchChar(char match);
};

// --------------------------------------------------------------------



/**
 * @brief The PythonMatchingChars highlights the opposite (), [] or {}
 */
class PythonMatchingChars : public QObject
{
    Q_OBJECT

public:
    PythonMatchingChars(PythonEditor *parent);
    PythonMatchingChars(PythonConsoleTextEdit *parent);
    ~PythonMatchingChars();

private Q_SLOTS:
    void cursorPositionChange();

private:
    QPlainTextEdit *m_editor;
};

// -----------------------------------------------------------------------
class PythonTextBlockScanInfo;
class PythonTextBlockData : public Gui::TextEditBlockData,
                      public Python::TokenLine
{
public:
    //typedef QVector<Python::Token*> tokens_t;
    explicit PythonTextBlockData(QTextBlock block, Python::TokenList *tokenList,
                           Python::Token *startTok = nullptr);
    PythonTextBlockData(const PythonTextBlockData &other);
    ~PythonTextBlockData() override;

    static PythonTextBlockData *pyBlockDataFromCursor(const QTextCursor &cursor);

    PythonTextBlockData *nextBlock() const override;
    PythonTextBlockData *previousBlock() const override;

    /**
     * @brief tokenAt returns the token defined pointed to by cursor
     * @param cursor that hovers the token to lookup
     * @return pointer to token or nullptr
     */
    static Python::Token* tokenAt(const QTextCursor &cursor);

    Python::Token *tokenAt(int pos) const;


    /**
     * @brief isMatchAt compares token at given pos
     * @param pos position i document
     * @param tokType match against this token
     * @return true if tokType are the same
     */
    bool isMatchAt(int pos, Python::Token::Type tokType) const;


    /**
     * @brief isMatchAt compares multiple tokens at given pos
     * @param pos position i document
     * @param tokTypes a list of tokens to match against this token
     * @return true if any of the tokTypes are the same
     */
    bool isMatchAt(int pos, const QList<Python::Token::Type> tokTypes) const;

    // FIXME: Sould factor scanInfo to  be disconnected from TextBlockData
    /**
     * @brief scanInfo contains parse messages set by code analyzer
     * @return nullptr if no parsemessages  or PythonTextBlockScanInfo*
     */
    PythonTextBlockScanInfo *scanInfo() const;

    /**
     * @brief setScanInfo set class with parsemessages
     * @param scanInfo instance of PythonTextBlockScanInfo
     */
    void setScanInfo(PythonTextBlockScanInfo *scanInfo);


    int blockState() const override;
    int incBlockState() override;
    int decBlockState() override;

protected:


private:

#ifdef BUILD_PYTHON_DEBUGTOOLS
    QString m_textDbg;
#endif

    friend class PythonSyntaxHighlighter; // in order to hide some api
};

// -------------------------------------------------------------------

class PythonTextBlockScanInfo : public Gui::TextEditBlockScanInfo
{
public:
    explicit PythonTextBlockScanInfo();
    virtual ~PythonTextBlockScanInfo();


    /// set message for token
    void setParseMessage(const Python::Token *tok, QString message, MsgType type = Message);

    /// get the ParseMsg for tok, filter by type
    /// nullptr if not found
    const ParseMsg *getParseMessage(const Python::Token *tok, MsgType type = AllMsgTypes) const;

    /// get parseMessage for token, filter by type
    QString parseMessage(const Python::Token *tok, MsgType type = AllMsgTypes) const;

    /// clear message
    void clearParseMessage(const Python::Token *tok);
};

// -------------------------------------------------------------------


/**
 * @brief helper to determine what icon to use based on exception
 */
class PyExceptionInfoGui
{
public:
    explicit PyExceptionInfoGui(Base::PyExceptionInfo *exc);
    ~PyExceptionInfoGui();
    const char *iconName() const;
private:
    Base::PyExceptionInfo *m_exc;
};

} // namespace Gui


#endif // PYTHONSYNTAXHIGHLIGHTER_H
