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

protected:
    Python::LexerP *d_lex;

    uint tokenize(Python::TokenLine *tokLine);

    /// this method is called when we cant tokenize a char
    /// subclasses should implement this function
    virtual Python::Token::Type unhandledState(uint &pos, int state, const std::string &text);

    /// call this method when tok type has changed
    virtual void tokenUpdated(const Python::Token *tok);

    Python::Token::Type &endStateOfLastParaRef() const;
    Python::TokenLine *activeLine() const;
    void setActiveLine(Python::TokenLine *line);


private:
    uint lastWordCh(uint startPos, const std::string &text) const;
    uint lastNumberCh(uint startPos, const std::string &text) const;
    uint lastDblQuoteStringCh(uint startAt, const std::string &text) const;
    uint lastSglQuoteStringCh(uint startAt, const std::string &text) const;
    Python::Token::Type numberType(const std::string &text) const;

    void setRestOfLine(uint &pos, const std::string &text, Python::Token::Type tokType);
    void scanIndentation(uint &pos, const std::string &text);
    void setWord(uint &pos, uint len, Python::Token::Type tokType);
    void setIdentifier(uint &pos, uint len, Python::Token::Type tokType);
    void setUndeterminedIdentifier(uint &pos, uint len, Python::Token::Type tokType);
    void setNumber(uint &pos, uint len, Python::Token::Type tokType);
    void setOperator(uint &pos, uint len, Python::Token::Type tokType);
    void setDelimiter(uint &pos, uint len, Python::Token::Type tokType);
    void setSyntaxError(uint &pos, uint len);
    void setLiteral(uint &pos, uint len, Python::Token::Type tokType);
    void setIndentation();

    Python::Token *createIndentError(const std::string &msg);
    Python::Token *insertDedent();
    void checkForDedent();
    void checkLineEnd();

};

}; // namespace Python

#endif // PYTHONLEXER_H