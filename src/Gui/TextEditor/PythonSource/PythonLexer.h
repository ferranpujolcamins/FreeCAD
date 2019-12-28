#ifndef PYTHONLEXER_H
#define PYTHONLEXER_H

#include "PythonToken.h"

namespace Python {

/// implements logic to tokenize a string input
class LexerP;
class Lexer {
public:
    explicit Lexer();
    virtual ~Lexer();

    Python::TokenList &list();

    virtual void tokenTypeChanged(const Python::Token *tok) const;


    void setFilePath(const std::string &filePath);
    const std::string &filePath() const;

    /// event callbacks, subclass may override these to get info
    virtual void setMessage(const Python::Token *tok) const { (void)tok; }
    virtual void setIndentError(const Python::Token *tok) const { (void)tok; }
    virtual void setSyntaxError(const Python::Token *tok) const { (void)tok; }

    static Version version();
    static void setVersion(Version::versions value);

    /// returns the nearest previous line that contains code
    static Python::TokenLine *previousCodeLine(TokenLine *line);

    /// tokenizes the string given by tokLine
    uint tokenize(Python::TokenLine *tokLine);

protected:
    Python::LexerP *d_lex;

    /// this method is called when we cant tokenize a char
    /// subclasses should implement this function
    virtual Python::Token::Type unhandledState(uint16_t &pos, int16_t state,
                                               const std::string &text);

    /// call this method when tok type has changed
    virtual void tokenUpdated(const Python::Token *tok);

    Python::Token::Type &endStateOfLastParaRef() const;
    Python::TokenLine *activeLine() const;
    void setActiveLine(Python::TokenLine *line);


private:
    uint16_t lastWordCh(uint16_t startPos, const std::string &text) const;
    uint16_t lastNumberCh(uint16_t startPos, const std::string &text,
                          Token::Type &type, uint32_t &customMask) const;
    uint16_t lastStringCh(uint16_t startAt, const std::string &text,
                          Token::Type &type, uint32_t &stringOptions) const;

    Token *setRestOfLine(uint16_t &pos, const std::string &text,
                         Python::Token::Type tokType);
    void scanIndentation(uint16_t &pos, const std::string &text);
    Token *setToToken(uint16_t &pos, uint16_t len,
                      Python::Token::Type tokType, uint32_t customMask);
    Token *setIdentifier(uint16_t &pos, uint16_t len, Python::Token::Type tokType);
    Token *setUndeterminedIdentifier(uint16_t &pos, uint16_t len,
                                     Python::Token::Type tokType, uint32_t customMask);
    Token *setNumber(uint16_t &pos, uint16_t len, Python::Token::Type tokType, uint32_t customMask);
    Token *setOperator(uint16_t &pos, uint16_t len, Python::Token::Type tokType);
    Token *setDelimiter(uint16_t &pos, uint16_t len, Python::Token::Type tokType);
    Token *setSyntaxError(uint16_t &pos, uint16_t len);
    Token *setLiteral(uint16_t &pos, uint16_t len,
                      Python::Token::Type tokType, uint32_t customMask);
    void setIndentation();

    Python::Token *createIndentError(const std::string &msg);
    Python::Token *insertDedent();
    void checkForDedent();
    void checkLineEnd();

};

/// reads a file into tokens
class LexerReader : public Lexer
{
    std::list<std::string> m_pyPaths;
public:
    LexerReader(const std::string &pyPath = std::string());
    ~LexerReader();
    bool readFile(const std::string &file = std::string());
    std::string pythonPath() const;
    const std::list<std::string> &paths() const;
};

class LexerPersistent
{
    Lexer *m_lexer;
public:
    /// construct a persitent (cache) lexed output
    /// this class does NOT take ownership of lexer
    explicit LexerPersistent(Lexer *lexer);
    virtual ~LexerPersistent();
    const std::string dumpToString() const;
    bool dumpToFile(const std::string &file) const;
    /// returns how many lines read, if negative=error on that line
    /// ie -11 means it bailed out on line 11
    int reconstructFromString(const std::string &dmpStr) const;
    int reconstructFromDmpFile(const std::string &file) const;
};

} // namespace Python

#endif // PYTHONLEXER_H
