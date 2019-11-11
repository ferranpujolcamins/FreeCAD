
#include "PreCompiled.h"
#include "PythonToken.h"
#include <TextEditor/PythonSource/PythonSourceListBase.h>
#include "PythonSyntaxHighlighter.h"

using namespace Gui;


Python::Token::Token(Python::Token::Tokens token, int startPos, int endPos, TokenLine *line) :
    token(token), startPos(startPos), endPos(endPos),
    m_next(nullptr), m_previous(nullptr),
    m_ownerLine(line)
{
#ifdef BUILD_PYTHON_DEBUGTOOLS
    m_nameDbg = text();
    if (m_ownerLine)
        m_lineDbg = m_ownerLine->lineNr();
#endif
}

Python::Token::Token(const Python::Token &other) :
    token(other.token), startPos(other.startPos), endPos(other.endPos),
    m_next(other.m_next), m_previous(other.m_previous),
    m_ownerLine(other.m_ownerLine)
{
#ifdef BUILD_PYTHON_DEBUGTOOLS
    m_nameDbg = text();
    if (m_ownerLine)
        m_lineDbg = m_ownerLine->lineNr();
#endif
}

Python::Token::~Token()
{
    // notify our listeners
    for (Python::SourceListNodeBase *n : m_srcLstNodes)
        n->tokenDeleted();

    if (m_ownerLine)
        m_ownerLine->remove(this, false);
}

Python::TokenList *Python::Token::ownerList() const {
    return m_ownerLine->ownerList();
}

QString Python::Token::text() const
{
    if (m_ownerLine)
        return m_ownerLine->text().mid(startPos, endPos - startPos);
    return QString();
}

int Python::Token::line() const
{
    if (m_ownerLine)
        return static_cast<int>(m_ownerLine->lineNr());
    return -1;
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
    m_firstLine(nullptr), m_lastLine(nullptr),
    m_size(0)
{
}

Python::TokenList::TokenList(const Python::TokenList &other) :
    m_first(other.m_first), m_last(other.m_last),
    m_firstLine(other.m_firstLine), m_lastLine(other.m_lastLine),
    m_size(other.m_size)
{
}

Python::TokenList::~TokenList()
{
    clear();
}



Python::Token *Python::TokenList::operator[](int32_t idx)
{
    int32_t cnt = static_cast<int32_t>(count());

    while (idx < 0)
        idx = cnt - idx;

    if (cnt / 2 > idx) {
        // lookup from beginning
        Python::Token *iter = m_first;
        for (int32_t i = 0;
             iter && i < cnt;
             iter = iter->m_next, ++i)
        { /* intentionally empty*/ }
        return iter;
    }
    // lookup from end (reverse)
    Python::Token *iter = m_last;
    for (int32_t i = cnt -1;
         iter && i >= cnt;
         iter = iter->m_previous,--i)
    { /* intentionally empty*/ }
    return iter;
}

uint32_t Python::TokenList::count() const
{
    return m_size;
    /*uint32_t siz = 0;
    uint32_t guard = max_size();
    Python::Token *iter = m_first;
    while (iter && guard--) {
        iter = iter->m_next;
        ++siz;
    }
    return siz; */
}

void Python::TokenList::clear()
{
    remove(m_first, m_last, true);
    m_first = m_last = nullptr;
}

void Python::TokenList::push_back(Python::Token *tok)
{
    assert(tok->m_ownerLine && "Token has no line attached");
    if (m_last) {
        m_last->m_next = tok;
        tok->m_previous = m_last;
    }

    m_last = tok;
    //tok->m_ownerLine = this;
    ++m_size;

    if (!m_first)
        m_first = m_last;
}

void Python::TokenList::push_front(Python::Token *tok)
{
    assert(tok->m_ownerLine && "Token has no line attached");
    if (m_first) {
        m_first->m_previous = tok;
        tok->m_next = m_first;
    }

    m_first = tok;
    //tok->m_ownerLine = this;
    ++m_size;

    if (!m_last)
        m_last = m_first;
}

Python::Token *Python::TokenList::pop_back()
{
    Python::Token *tok = m_last;
    if (tok){
        m_last = tok->m_previous;
        tok->m_next = tok->m_previous = nullptr;
        tok->m_ownerLine = nullptr;
        assert(m_size > 0 && "Size cache out of order");
        --m_size;
    }
    return tok;
}

Python::Token *Python::TokenList::pop_front()
{
    Python::Token *tok = m_first;
    if (tok) {
        m_first = tok->m_next;
        tok->m_next = tok->m_previous = nullptr;
        tok->m_ownerLine = nullptr;
        assert(m_size > 0 && "Size cache out of order");
        --m_size;
    }
    return tok;
}

void Python::TokenList::insert(Python::Token *previousTok, Python::Token *insertTok)
{
    assert(insertTok->m_ownerLine && "insertTok has no line attached");
    assert(!insertTok->m_previous && !insertTok->m_next && "insertTok is already inserted");

    if (previousTok) {
        assert(previousTok->m_ownerLine->m_ownerList == this && "previousTok not in this list");
        insertTok->m_next = previousTok->m_next;
        if (previousTok->m_next)
            previousTok->m_next->m_previous = insertTok;
        previousTok->m_next = insertTok;
        insertTok->m_previous = previousTok;
        ++m_size;
    }
}

bool Python::TokenList::remove(Python::Token *tok, bool deleteTok)
{
    if (!tok)
        return false;

    if (tok->m_previous)
        tok->m_previous->m_next = tok->m_next;
    if (tok->m_next)
        tok->m_next->m_previous = tok->m_previous;

    assert(m_size > 0 && "Size cache out of order");
    --m_size;

    if (deleteTok)
        delete tok;
    else
        tok->m_ownerLine = nullptr;
    return true;
}

bool Python::TokenList::remove(Python::Token *tok, Python::Token *endTok, bool deleteTok)
{
    if (!tok || !endTok ||
        tok->m_ownerLine != endTok->m_ownerLine)
    {
        return false;
    }

    if (tok->m_previous)
        tok->m_previous->m_next = endTok;
    if (endTok->m_previous) {
        endTok->m_previous->m_next = nullptr;
        endTok->m_previous = tok->m_previous;
    }
    tok->m_previous = nullptr;

    uint32_t guard = 20000000; // 100k rows with 20 tokens each row is max allowed

    Python::Token *iter = tok;
    while (iter && guard--) {

        iter->m_ownerLine = nullptr;

        Python::Token *tmp = iter;
        iter = iter->m_next;

        assert(m_size > 0 && "Size cache out of order");
        --m_size;

        if (deleteTok)
            delete tmp;
        else
            tmp->m_ownerLine = nullptr;
    }
    return true;
}

Python::TokenLine *Python::TokenList::lineAt(int32_t lineNr) const
{
    if (!m_firstLine || !m_lastLine)
        return nullptr;

    if (lineNr < 0) {
        // lookup from the back
        Python::TokenLine *line = m_lastLine;
        for (int32_t idx = lineNr; idx < 0 && line; ++idx) {
            line = line->m_previousLine;
        }
        return line;
    }

    // lookup from start
    Python::TokenLine *line = m_firstLine;
    for (int32_t idx = 0; idx < lineNr && line; ++idx) {
        line = line->m_nextLine;
    }
    return line;
}

uint32_t Python::TokenList::lineCount() const
{
    uint32_t cnt = 0;
    uint32_t guard = max_size();
    for (Python::TokenLine *line = m_firstLine;
         line && guard;
         line = line->m_nextLine, --guard)
    {
        ++cnt;
    }
    return cnt;
}

void Python::TokenList::swapLine(int32_t lineNr, Python::TokenLine *swapIn)
{
    swapLine(lineAt(lineNr), swapIn);
}

void Python::TokenList::swapLine(Python::TokenLine *swapOut, Python::TokenLine *swapIn)
{
    assert(swapIn == nullptr);
    assert(swapOut == nullptr);
    assert(swapOut->m_ownerList == this && "swapOut not in this list");
    assert(!swapIn->m_ownerList && "swapIn already attached to a list");

    swapIn->m_ownerList = this;

    if (m_firstLine == swapOut)
        m_firstLine = swapIn;
    if (m_lastLine == swapOut)
        m_lastLine = swapIn;

    delete swapOut;
}

void Python::TokenList::insertLine(int32_t lineNr, Python::TokenLine *insertLine)
{
    Python::TokenLine *lineObj = lineAt(lineNr);
    if (lineObj)
        lineObj = lineObj->m_previousLine;
    this->insertLine(lineObj, insertLine);
}

void Python::TokenList::insertLine(Python::TokenLine *previousLine,
                                   Python::TokenLine *insertLine)
{
    assert(insertLine != nullptr);
    assert(insertLine->m_ownerList == nullptr && "insertLine contained in another list");

    if (previousLine) {
        assert(previousLine->m_ownerList == this && "previousLine not stored in this list");
        insertLine->m_previousLine = previousLine;
        insertLine->m_nextLine = previousLine->m_nextLine;
        if (previousLine->m_nextLine) {
            // insert in the middle
            assert(m_lastLine != previousLine && "*m_lastLine out of sync");
            insertLine->m_nextLine->m_previousLine = insertLine;
        } else {
            // last line
            assert(m_lastLine == previousLine);
            m_lastLine = insertLine;
        }
        previousLine->m_nextLine = insertLine;
    } else {
        // at beginning (ie. first row)
        insertLine->m_nextLine = m_firstLine;
        m_firstLine->m_previousLine = insertLine;
        m_firstLine = insertLine;
    }

    insertLine->m_ownerList = this;
}

void Python::TokenList::appendLine(Python::TokenLine *lineToPush)
{
    assert(lineToPush);
    assert(lineToPush->m_ownerList == nullptr && "lineToPush is contained in another list");
    if (m_lastLine)
        lineToPush->m_previousLine = m_lastLine;

    m_lastLine = lineToPush;
    if (!m_firstLine)
        m_firstLine = lineToPush;

    lineToPush->m_ownerList = this;
}

void Python::TokenList::removeLine(int32_t lineNr)
{
    removeLine(lineAt(lineNr));
}

void Python::TokenList::removeLine(Python::TokenLine *lineToRemove)
{
    assert(lineToRemove);
    assert(lineToRemove->m_ownerList == this && "lineToRemove not contained in this list");

    if (lineToRemove == m_firstLine)
        m_firstLine = lineToRemove->m_nextLine;
    if (lineToRemove == m_lastLine)
        m_lastLine = lineToRemove->m_previousLine;
    if (lineToRemove->m_previousLine)
        lineToRemove->m_previousLine->m_nextLine = lineToRemove->m_nextLine;
    if (lineToRemove->m_nextLine)
        lineToRemove->m_nextLine->m_previousLine = lineToRemove->m_previousLine;

    delete lineToRemove;
}

// ----------------------------------------------------------------------------------------

Python::TokenLine::TokenLine(Python::TokenList *ownerList,
                             Python::Token *startTok,
                             const QString &text,
                             Python::TextBlockData *txtBlock) :
    m_ownerList(ownerList), m_startTok(startTok),
    m_txtBlock(txtBlock)
{
    // strip newline chars
    QRegExp re(QLatin1String("(\\r\\n|\\n\\r|\\n\\r)$"));
    int lineEndingPos = re.indexIn(text);
    if (lineEndingPos > -1)
        m_text = text.left(lineEndingPos);
    else
        m_text = text;
}

uint32_t Python::TokenLine::lineNr() const
{
    uint32_t lineCnt = 0;
    Python::TokenLine *iter = m_previousLine;
    uint guard = m_ownerList->max_size();
    while(iter && (--guard)) {
        ++lineCnt;
        iter = iter->m_previousLine;
    }
    return lineCnt;
}

uint Python::TokenLine::count() const
{
    uint cnt = 0;
    Python::Token *iter = m_startTok;
    uint guard = m_ownerList->max_size();
    while(iter && (--guard)) {
        if (iter->m_ownerLine != this)
            break;
        ++cnt;
        iter = iter->m_next;
    }

    return cnt;
}

Python::Token *Python::TokenLine::back() const {
    Python::Token *iter = m_startTok;
    uint32_t guard = m_ownerList->max_size();
    while (iter && iter->m_ownerLine == this && (--guard))
        iter = iter->next();

    return iter;
}

Python::Token *Python::TokenLine::end() const {
    Python::Token *tok = back();
    return tok ? tok->m_next : nullptr;
}

Python::Token *Python::TokenLine::rend() const {
    return m_startTok ? m_startTok->m_previous : nullptr;
}

Python::Token *Python::TokenLine::operator[](int idx)
{
    // allow lookup from the end
    while (idx < 0)
        idx = static_cast<int>(count()) - idx;

    int cnt = 0;
    Python::Token *iter = m_startTok;
    uint guard = m_ownerList->max_size();
    while(iter && (--guard)) {
        if (iter->m_ownerLine != this)
            break;
        if (idx == cnt++)
            return iter;
        iter = iter->m_next;
    }

    return nullptr;
}

void Python::TokenLine::push_back(Python::Token *tok)
{
    assert(!tok->m_next && !tok->m_previous && "tok already attached to container");
    assert((!tok->m_ownerLine || tok->m_ownerLine != this) && "tok already added to a Line");
    assert(m_ownerList != nullptr && "Line must be inserted to a list to add tokens");
    tok->m_ownerLine = this;
    m_ownerList->push_back(tok);
}

void Python::TokenLine::push_front(Python::Token *tok)
{
    assert(!tok->m_next && !tok->m_previous && "tok already attached to container");
    assert((!tok->m_ownerLine || tok->m_ownerLine != this) && "tok already added to a Line");
    assert(m_ownerList != nullptr && "Line must be inserted to a list to add tokens");
    tok->m_ownerLine = this;

    Python::Token *prevTok = nullptr;
    if (m_startTok)
        prevTok = m_startTok->m_previous;
    else if (m_previousLine)
        prevTok = m_previousLine->back();
    m_ownerList->insert(prevTok, tok);
}

Python::Token *Python::TokenLine::pop_back()
{
    assert(m_ownerList != nullptr && "Line must be inserted to a list to remove tokens");
    Python::Token *tok = back();
    if (tok)
        m_ownerList->remove(tok, false);
    return tok;
}

Python::Token *Python::TokenLine::pop_front()
{
    assert(m_ownerList != nullptr && "Line must be inserted to a list to remove tokens");
    if (m_startTok)
        m_ownerList->remove(m_startTok, false);
    return m_startTok;
}

void Python::TokenLine::insert(Python::Token *tok)
{
    assert(!tok->m_next && !tok->m_previous && "tok already attached to container");
    assert((!tok->m_ownerLine || tok->m_ownerLine != this) && "tok already added to a Line");
    assert(m_ownerList != nullptr && "Line must be inserted to a list to add tokens");
    tok->m_ownerLine = this;

    Python::Token *prevTok = m_startTok;
    if (!prevTok && m_previousLine)
        prevTok = m_previousLine->back();
    // move up to correct place within line
    while (prevTok && prevTok->m_ownerLine == this && *prevTok > *tok) {
        if (!prevTok->m_next)
            break; // at last token in document, must keep this token as ref.
        prevTok = prevTok->m_next;
    }
    // insert into list, if prevTok is nullptr its inserted at beginning of list
    m_ownerList->insert(prevTok, tok);
}

bool Python::TokenLine::remove(Python::Token *tok, bool deleteTok)
{
    assert(m_ownerList != nullptr && "Line must be inserted to a list to remove tokens");
    if (tok->m_ownerLine != this)
        return false;

    if (tok == m_startTok && count() > 1)
            m_startTok = m_startTok->m_next;

    return m_ownerList->remove(tok, deleteTok);
}

bool Python::TokenLine::remove(Python::Token *tok, Python::Token *endTok, bool deleteTok)
{
    assert(m_ownerList != nullptr && "Line must be inserted to a list to remove tokens");
    if (tok->m_ownerLine != this || endTok->m_ownerLine != this)
        return false;

    if (tok == m_startTok && count() > 1)
            m_startTok = m_startTok->m_next;

    return m_ownerList->remove(tok, deleteTok);
}
