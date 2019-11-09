#ifndef PYTHONSYNTAXHIGHLIGHTER_H
#define PYTHONSYNTAXHIGHLIGHTER_H

#include "PreCompiled.h"
#include "SyntaxHighlighter.h"
#include "PythonToken.h"
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

namespace Python {

class TextBlockData;
class SyntaxHighlighterP;

/**
 * Syntax highlighter for Python.
 */
class GuiExport SyntaxHighlighter : public Gui::SyntaxHighlighter
{
    Q_OBJECT
public:
    SyntaxHighlighter(QObject* parent);
    virtual ~SyntaxHighlighter();

    void highlightBlock (const QString & text);


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

    /// returns the format to color this token
    QTextCharFormat getFormatToken(const Python::Token *token) const;

    // used by code analyzer, set by editor
    void setFilePath(QString filePath);
    QString filePath() const;

protected:
    // tokenizes block, called by highlightBlock when text have changed
    int tokenize(const QString &text, int &parenCnt, bool &isParamLine);

    /// sets (re-colors) txt contained from token
    void setFormatToken(const Python::Token *tok);

private Q_SLOTS:
    void sourceScanTmrCallback();

private:
    SyntaxHighlighterP* d;

    //const QString getWord(int pos, const QString &text) const;
    int lastWordCh(int startPos, const QString &text) const;
    int lastNumberCh(int startPos, const QString &text) const;
    int lastDblQuoteStringCh(int startAt, const QString &text) const;
    int lastSglQuoteStringCh(int startAt, const QString &text) const;
    Python::Token::Tokens numberType(const QString &text) const;

    void setRestOfLine(int &pos, const QString &text, Python::Token::Tokens token);
    void scanIndentation(int &pos, const QString &text);

    void setWord(int &pos, int len, Python::Token::Tokens token);

    void setIdentifier(int &pos, int len, Python::Token::Tokens token);
    void setUndeterminedIdentifier(int &pos, int len, Python::Token::Tokens token);

    void setNumber(int &pos, int len, Python::Token::Tokens token);

    void setOperator(int &pos, int len, Python::Token::Tokens token);

    void setDelimiter(int &pos, int len, Python::Token::Tokens token);

    void setSyntaxError(int &pos, int len);

    void setLiteral(int &pos, int len, Python::Token::Tokens token);

    // this is special can only be one per line
    // T_Indentation(s) token is generated related when looking at lines above as described in
    // https://docs.python.org/3/reference/lexical_analysis.html#indentation
    void setIndentation(int &pos, int len, int count);

};


// ------------------------------------------------------------------------

struct MatchingCharInfo
{
    char character;
    int position;
    MatchingCharInfo();
    MatchingCharInfo(const MatchingCharInfo &other);
    MatchingCharInfo(char chr, int pos);
    ~MatchingCharInfo();
    char matchingChar() const;
    static char matchChar(char match);
};

// --------------------------------------------------------------------



/**
 * @brief The PythonMatchingChars highlights the opposite (), [] or {}
 */
class MatchingChars : public QObject
{
    Q_OBJECT

public:
    MatchingChars(PythonEditor *parent);
    MatchingChars(PythonConsoleTextEdit *parent);
    ~MatchingChars();

private Q_SLOTS:
    void cursorPositionChange();

private:
    QPlainTextEdit *m_editor;
};

// -----------------------------------------------------------------------
class TextBlockScanInfo;

class TextBlockData : public Gui::TextEditBlockData
{
public:
    typedef QVector<Python::Token*> tokens_t;
    explicit TextBlockData(QTextBlock block);
    TextBlockData(const TextBlockData &other);
    ~TextBlockData();

    static TextBlockData *pyBlockDataFromCursor(const QTextCursor &cursor);
    /**
     * @brief tokens all PythonTokens for this row
     */
    const tokens_t &tokens() const;

    /**
     * @brief undeterminedTokens
     * @return all undetermined tokens for this txtblockdata
     */
    const tokens_t undeterminedTokens() const;

    TextBlockData *next() const;
    TextBlockData *previous() const;
    /**
     * @brief findToken searches for token in this block
     * @param token needle to search for
     * @param searchFrom start position in line to start the search
     *                   if negative start from the back ie -10 searches from pos 10 to 0
     * @return pointer to token if found, else nullptr
     */
    const Python::Token* findToken(Python::Token::Tokens token,
                                 int searchFrom = 0) const;

    /**
     * @brief firstTextToken lookup first token that is non whitespace
     * @return first token that is a Text token (non whitespace)
     */
    const Python::Token *firstTextToken() const;

    /**
     * @brief firstCodeToken lookup first token that is a code token
     * @return first token that is a code token
     */
    const Python::Token *firstCodeToken() const;

    /**
     * @brief tokensBetweenOfType counts tokens to type match between these positions
     *         NOTE! this method ignores T_Indent tokens
     * @param startPos position to start at
     * @param endPos position where it should stop looking
     * @param match against this token
     * @return number of times token was found
     */
    int tokensBetweenOfType(int startPos, int endPos, Token::Tokens match) const;

    /**
     * @brief lastInserted gives the last inserted token
     *         NOTE! this method ignores T_Indent tokens
     * @param lookInPreviousRows if false limits to this textblock only
     * @return last inserted token or nullptr if empty
     */
    Python::Token *lastInserted(bool lookInPreviousRows = true) const;


    /**
     * @brief lastInsertedIsA checks if last token matches
     * @param match token to test against
     * @return true if matched
     */
    bool lastInsertedIsA(Python::Token::Tokens match, bool lookInPreviousRows = true) const;

    /**
     * @brief tokenAt returns the token defined under pos (in block)
     * @param pos from start of line
     * @return pointer to token or nullptr
     */
    Python::Token *tokenAt(int pos) const;

    static Python::Token* tokenAt(const QTextCursor &cursor);

    /**
     * @brief tokenAtAsString returns the token under pos (in block)
     * @param pos from start of line
     * @return token as string
     */
    QString tokenAtAsString(int pos) const;

    QString tokenAtAsString(const Python::Token *tok) const;

    static QString tokenAtAsString(QTextCursor &cursor);


    /**
     * @brief isMatchAt compares token at given pos
     * @param pos position i document
     * @param token match against this token
     * @return true if token are the same
     */
    bool isMatchAt(int pos, Python::Token::Tokens token) const;


    /**
     * @brief isMatchAt compares multiple tokens at given pos
     * @param pos position i document
     * @param token a list of tokens to match against this token
     * @return true if any of the token are the same
     */
    bool isMatchAt(int pos, const QList<Python::Token::Tokens> tokens) const;


    /**
     * @brief isCodeLine checks if line has any code
     *        lines with only indent or comments return false
     * @return true if line has code
     */
    bool isCodeLine() const;

    bool isEmpty() const;
    /**
     * @brief indentString returns the indentation of line
     * @return QString of the raw string in document
     */
    QString indentString() const;

    /**
     * @brief indent the number of indent spaces as interpreted by a python interpreter,
     *        a comment only line returns 0 regardless of indent
     *
     * @return nr of whitespace chars (tab=8) beginning of line
     */
    int indent() const;

    /**
     * @brief insertToken inserts a token at position
     * @param token: Type of token
     * @param pos: where in textducument
     * @param len: token length in document
     */
    Python::Token *insertToken(Python::Token::Tokens token, int pos, int len);

    /**
     * @brief scanInfo contains parse messages set by code analyzer
     * @return nullptr if no parsemessages  or PythonTextBlockScanInfo*
     */
    Python::TextBlockScanInfo *scanInfo() const;

    /**
     * @brief setScanInfo set class with parsemessages
     * @param scanInfo instance of PythonTextBlockScanInfo
     */
    void setScanInfo(Python::TextBlockScanInfo *scanInfo);


    /**
     * @brief setReformat sets up token to be repainted by format
     *        We must later call PythonSyntaxHiglighter::rehighlight(this.block())
     * @param tok PythonToken to be reformated, must be stored in this instance
     * @param format QTextCharFormat to use
     * @return true if token was stored in here
     */
    bool setReformat(const Python::Token *tok, QTextCharFormat format);

    QHash<const Python::Token*, QTextCharFormat> &allReformats() { return m_reformats; }

protected:
    /**
     * @brief insert should only be used by PythonSyntaxHighlighter
     */
    const Python::Token *setDeterminedToken(Python::Token::Tokens token, int startPos, int len);
    /**
     * @brief insert should only be used by PythonSyntaxHighlighter
     *  signifies that parse tree lookup is needed
     */
    const Python::Token *setUndeterminedToken(Python::Token::Tokens token, int startPos, int len);


    /**
     * @brief setIndentCount should be considered private, is
     * @param count
     */
    void setIndentCount(int count);


private:
    tokens_t m_tokens;
    QVector<int> m_undeterminedIndexes; // index to m_tokens where a undetermined is at
                                        //  (so context parser can detemine it later)
    QHash<const Python::Token*, QTextCharFormat> m_reformats;
    int m_indentCharCount; // as spaces NOTE according to python documentation a tab is 8 spaces

#ifdef BUILD_PYTHON_DEBUGTOOLS
    QString m_textDbg;
#endif

    friend class Python::SyntaxHighlighter; // in order to hide some api
};

// -------------------------------------------------------------------

class TextBlockScanInfo : public Gui::TextEditBlockScanInfo
{
public:
    explicit TextBlockScanInfo();
    virtual ~TextBlockScanInfo();


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

} // namespace Python


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
