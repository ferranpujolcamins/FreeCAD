
#include "PreCompiled.h"
#include "PythonToken.h"
#include <TextEditor/PythonSource/PythonSourceListBase.h>
#include "PythonSyntaxHighlighter.h"

using namespace Gui;


Python::Token::Token(Python::Token::Tokens token, int startPos, int endPos,
             Python::TextBlockData *block) :
    token(token), startPos(startPos), endPos(endPos),
    m_txtBlock(block),
    m_next(nullptr), m_previous(nullptr)
{
#ifdef BUILD_PYTHON_DEBUGTOOLS
    m_nameDbg = text();
    m_lineDbg = block->block().blockNumber();
#endif
}

Python::Token::Token(const Python::Token &other) :
    token(other.token), startPos(other.startPos), endPos(other.endPos),
    m_txtBlock(other.m_txtBlock),
    m_next(other.m_next), m_previous(other.m_previous)
{
}

Python::Token::~Token()
{
    // notify our listeners
    for (Python::SourceListNodeBase *n : m_srcLstNodes)
        n->tokenDeleted();
}

/*
friend
bool Token::operator >=(const PythonToken &lhs, const PythonToken &rhs)
{
    return lhs.line() >= rhs.line() &&
           lhs.startPos >= rhs.startPos;
}
*/

Python::Token *Python::Token::next() const
{
    TextBlockData *txt = m_txtBlock;
    while (txt) {
        const TextBlockData::tokens_t tokens = txt->tokens();
        if (txt == m_txtBlock) {
            int i = tokens.indexOf(const_cast<Token*>(this));
            if (i > -1 && i +1 < tokens.size())
                return tokens.at(i+1);
        } else {
            if (tokens.isEmpty())
                return nullptr;
            return tokens[0];
        }

        // we are the last token in this txtBlock or it was empty
        txt = txt->next();
    }
    return nullptr;
}

Python::Token *Python::Token::previous() const
{
    TextBlockData *txt = m_txtBlock;
    while (txt) {
        const TextBlockData::tokens_t tokens = txt->tokens();
        if (txt == m_txtBlock) {
            int i = tokens.indexOf(const_cast<Token*>(this));
            if (i > 0)
                return tokens.at(i-1);
        } else {
            if (tokens.isEmpty())
                return nullptr;
            return tokens[tokens.size() -1];
        }

        // we are the last token in this txtBlock or it was empty
        txt = txt->previous();
    }
    return nullptr;
}

QString Python::Token::text() const
{
    return m_txtBlock->tokenAtAsString(this);
}

int Python::Token::line() const
{
    return m_txtBlock->block().blockNumber();
}

bool Python::Token::isNumber() const { // is a number in src file
    return token > T__NumbersStart &&
           token < T__NumbersEnd;
}

bool Python::Token::isInt() const { // is a integer (dec) in src file
    return token > T__NumbersIntStart &&
           token < T__NumbersIntEnd;
}

bool Python::Token::isFloat() const {
    return token == T_NumberFloat;
}

bool Python::Token::isString() const {
    return token > T__LiteralsStart &&
            token < T__LiteralsEnd;
}

bool Python::Token::isBoolean() const {
    return token == T_IdentifierTrue ||
           token == T_IdentifierFalse;
}

bool Python::Token::isMultilineString() const
{
    return token > T__LiteralsMultilineStart &&
           token < T__LiteralsMultilineEnd;
}

bool Python::Token::isKeyword() const {
    return token > T__KeywordsStart &&
           token <  T__KeywordsEnd;
}

bool Python::Token::isOperator() const {
    return token > T__OperatorStart &&
           token <  T__OperatorEnd;
}

bool Python::Token::isOperatorArithmetic() const {
    return token > T__OperatorArithmeticStart &&
           token < T__OperatorArithmeticEnd;
}

bool Python::Token::isOperatorBitwise() const {
    return token > T__OperatorBitwiseStart &&
           token < T__OperatorBitwiseEnd;
}

bool Python::Token::isOperatorAssignment() const { // modifies lvalue
    return token > T__OperatorAssignmentStart &&
           token < T__OperatorAssignmentEnd;
}

bool Python::Token::isOperatorAssignmentBitwise() const { // modifies through bit twiddling
    return token > T__OperatorBitwiseStart &&
           token < T__OperatorBitwiseEnd;
}

bool Python::Token::isOperatorCompare() const {
    return token > T__OperatorAssignBitwiseStart &&
           token < T__OperatorAssignBitwiseEnd;
}

bool Python::Token::isOperatorCompareKeyword() const {
    return token > T__OperatorCompareKeywordStart &&
           token < T__OperatorCompareKeywordEnd;
}

bool Python::Token::isOperatorParam() const {
    return token > T__OperatorParamStart &&
           token <  T__OperatorParamEnd;
}

bool Python::Token::isDelimiter() const {
    return token > T__DelimiterStart &&
           token < T__DelimiterEnd;
}

bool Python::Token::isIdentifier() const {
    return token > T__IdentifierStart &&
           token < T__IdentifierEnd;
}

bool Python::Token::isIdentifierVariable() const {
    return token > T__IdentifierVariableStart &&
           token < T__IdentifierVariableEnd;
}

bool Python::Token::isIdentifierDeclaration() const {
    return token > T__IdentifierDeclarationStart &&
           token < T__IdentifierDeclarationEnd;
}

bool Python::Token::isNewLine() const
{
    if (token != T_DelimiterNewLine)
        return false;

    // else check for escape char
    const Python::Token *prev = previous();
    return prev && prev->token != T_DelimiterLineContinue;
}

bool Python::Token::isInValid() const  {
    return token == T_Invalid;
}

bool Python::Token::isUndetermined() const {
    return token == T_Undetermined;
}

bool Python::Token::isCode() const
{
    if (token < T__NumbersStart)
        return false;
    if (token > T__DelimiterTextLineStart &&
        token < T__DelimiterTextLineEnd)
    {
        return false;
    }
    if (token >= T_BlockStart &&
        token <= T_BlockEnd)
    {
        return false;
    }
    return true;
}

bool Python::Token::isImport() const
{
    return token > T__IdentifierImportStart &&
           token < T__IdentifierImportEnd;
}
bool Python::Token::isText() const
{
    return token == T_Comment || isCode();
}

void Python::Token::attachReference(Python::SourceListNodeBase *srcListNode)
{
    if (!m_srcLstNodes.contains(srcListNode))
        m_srcLstNodes.push_back(srcListNode);
}

void Python::Token::detachReference(Python::SourceListNodeBase *srcListNode)
{
    int idx = m_srcLstNodes.indexOf(srcListNode);
    if (idx > -1)
        m_srcLstNodes.removeAt(idx);
}

// -------------------------------------------------------------------------------------------

Python::TokenList::TokenList() :
    m_first(nullptr), m_last(nullptr),
    m_current(nullptr), m_insertPos(nullptr),
    m_line(0)
{
}

Python::TokenList::TokenList(const Python::TokenList &other) :
    m_first(other.m_first), m_last(other.m_last),
    m_current(other.m_current), m_insertPos(other.m_insertPos),
    m_line(other.m_line)
{
}

Python::TokenList::~TokenList()
{
    clear();
}

Python::Token *Python::TokenList::begin(){
    m_current = m_first;
    return m_first;
}

Python::Token *Python::TokenList::rbegin() {
    m_current = m_last;
    return  m_last;
}

Python::Token *Python::TokenList::next()
{
    if (!m_current)
        return nullptr;

    return m_current = m_current->m_next;
}

Python::Token *Python::TokenList::previous()
{
    if (!m_current)
        return nullptr;

    return m_current = m_current->m_previous;
}

int32_t Python::TokenList::count() const
{
    int siz = -1;
    for (Python::Token *iter = m_first;
         iter != nullptr;
         iter = iter->m_next)
    {
        ++siz;
    }
    return siz;
}

void Python::TokenList::clear()
{
    Python::Token *iter = m_first,
                  *last = nullptr;

    for(;iter != nullptr;
        iter = iter->m_next)
    {
        if (last) {
            delete last;
            last = nullptr;
        }
        last = iter;
    }

    // delete the last item
    if (last)
        delete last;

    m_insertPos = m_current = m_first = m_last = nullptr;
}

void Python::TokenList::push_back(Python::Token *tok)
{
    if (m_last) {
        m_last->m_next = tok;
        tok->m_previous = m_last;
    }

    m_last = tok;

    if (!m_first)
        m_first = m_last;
}

void Python::TokenList::push_front(Python::Token *tok)
{
    if (m_first) {
        m_first->m_previous = tok;
        tok->m_next = m_first;
    }

    m_first = tok;

    if (!m_last)
        m_last = m_first;
}

Python::Token *Python::TokenList::pop_back()
{
    Python::Token *tok = m_last;
    if (tok){
        m_last = tok->m_previous;
        tok->m_next = tok->m_previous = nullptr;
    }
    return nullptr;
}

Python::Token *Python::TokenList::pop_front()
{
    Python::Token *tok = m_first;
    if (tok) {
        m_first = tok->m_next;
        tok->m_next = tok->m_previous = nullptr;
    }
    return nullptr;
}

bool Python::TokenList::insert(Python::Token *tok)
{
    // move to correct pos up
    while (m_insertPos && m_insertPos->startPos < tok->startPos)
        m_insertPos = m_insertPos->m_next;
    // move to correct pos down
    while (m_insertPos && m_insertPos->startPos > tok->startPos)
        m_insertPos = m_insertPos->m_previous;

    if (m_insertPos) {
        // move to last token with the same startPos
        while (m_insertPos && m_insertPos->m_next &&
               m_insertPos->m_next->startPos == tok->startPos)
        {
            m_insertPos = m_insertPos->m_next;
        }

        // insert token into list
        if (m_insertPos) {
            tok->m_next = m_insertPos->m_next;
            tok->m_previous = m_insertPos;
            m_insertPos->m_next = tok;
            if (m_insertPos->m_next)
                m_insertPos->m_next->m_previous = tok;
        }
    }
    if (m_first && m_first->startPos > tok->startPos) {
        // at beginning
        push_front(tok);
        return true;
    } else if (m_last && m_last->startPos < tok->startPos) {
        // at end
        push_back(tok);
        return true;
    }
    return false;
}

void Python::TokenList::remove(Python::Token *tok, bool deleteTok)
{
    if (!tok)
        return;

    if (m_current == tok)
        m_current = tok->m_next;
    if (m_insertPos == tok)
        m_insertPos = tok->m_next;

    if (tok->m_previous)
        tok->m_previous->m_next = tok->m_next;
    if (tok->m_next)
        tok->m_next->m_previous = tok->m_previous;

    if (deleteTok)
        delete tok;
}

void Python::TokenList::remove(Python::Token *tok, Python::Token *endTok, bool deleteTok)
{
    if (!tok || !endTok)
        return;

    if (tok->m_previous)
        tok->m_previous->m_next = endTok;
    if (endTok->m_previous) {
        endTok->m_previous->m_next = nullptr;
        endTok->m_previous = tok->m_previous;
    }
    tok->m_previous = nullptr;

    Python::Token *last = nullptr;
    for (Python::Token *iter = tok;
         iter != endTok && iter != nullptr;
         iter = iter->m_next)
    {
        if (m_current == iter)
            m_current = endTok;
        if (m_insertPos == iter)
            m_insertPos = endTok;

        if (deleteTok) {
            if (last)
                delete last;
            last = iter;
        }
    }

    // delete the very last element
    if (last)
        delete last;
}
