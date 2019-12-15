#include "PythonSourceIndent.h"
#include "PythonSource.h"


DBG_TOKEN_FILE

Python::SourceIndent::SourceIndent():
    m_framePopCnt(0)
{
    // default to a root frame
    m_indentStack.push_back(Indent(-1, 0));
}

Python::SourceIndent::SourceIndent(const SourceIndent &other) :
    m_indentStack(other.m_indentStack),
    m_framePopCnt(other.m_framePopCnt)
{
}


Python::SourceIndent::~SourceIndent()
{
}

Python::SourceIndent &Python::SourceIndent::operator =(const SourceIndent &other) {
    m_indentStack = other.m_indentStack;
    return *this;
}

bool Python::SourceIndent::operator ==(const Python::SourceIndent &other) {
    if (m_indentStack.size() != other.m_indentStack.size())
        return false;

    for (std::list<Indent>::const_iterator thisStack = m_indentStack.begin(),
                                           otherStack = other.m_indentStack.begin();
         thisStack != m_indentStack.end() && otherStack != other.m_indentStack.end();
         ++thisStack, ++otherStack)
    {
        if (thisStack->frameIndent != otherStack->frameIndent ||
            thisStack->currentBlockIndent != otherStack->currentBlockIndent)
        {
            return false;
        }
    }

    return true;
}

uint Python::SourceIndent::frameIndent() const
{
    return static_cast<uint>(_current().frameIndent);
}

uint Python::SourceIndent::currentBlockIndent() const
{
    return static_cast<uint>(_current().currentBlockIndent);
}

uint Python::SourceIndent::previousBlockIndent() const
{
    if (m_indentStack.empty())
        return 0;
    if (m_indentStack.size() == 1)
        return static_cast<uint>(m_indentStack.front().currentBlockIndent);
    return static_cast<uint>(m_indentStack.back().currentBlockIndent);
}

bool Python::SourceIndent::isValid() const {
    if (m_indentStack.size() < 1)
        return false;
    const Indent ind = m_indentStack.back();
    return ind.currentBlockIndent > -1 || ind.frameIndent > -1;
}

void Python::SourceIndent::pushFrameBlock(uint frmIndent, uint currentIndent)
{
    // insert a framestarter
    assert(frmIndent <= currentIndent && "frmIndent must be less or equal to currentIndent");
    // we store as ints internally because we need a framestarter=-1 to indicate rootframe
    Indent ind(static_cast<int>(frmIndent), static_cast<int>(currentIndent));
    m_indentStack.push_back(ind);
}

void Python::SourceIndent::pushBlock(uint currentIndent)
{
    // we store as ints internally because we need a framestarter=-1 to indicate rootframe
    Indent ind(_current().frameIndent, static_cast<int>(currentIndent));
    m_indentStack.push_back(ind);
}

void Python::SourceIndent::popBlock()
{
    // keep rootframe
    if (m_indentStack.size() <= 1)
        return;

    // might have a frame block
    Indent ind = m_indentStack.back();
    m_indentStack.pop_back();
    if (ind.frameIndent > m_indentStack.back().frameIndent) {
        // pop a framestarter
        ++m_framePopCnt; // store that we have popped a frame
    }
}

int Python::SourceIndent::framePopCntDecr()
{
    if (framePopCnt() > 0)
        --m_framePopCnt;
    return m_framePopCnt;
}


bool Python::SourceIndent::validIndentLine(const Python::Token *tok)
{
    DEFINE_DBG_VARS

    // make sure we don't dedent on a comment or a string
    Python::Token *itTok = tok->ownerLine()->front();
    while(itTok && itTok->ownerLine() == tok->ownerLine()) {
        DBG_TOKEN(itTok)
        switch (itTok->type()) {
        case Python::Token::T_LiteralBlockDblQuote:
        case Python::Token::T_LiteralBlockSglQuote:
        case Python::Token::T_LiteralDblQuote:
        case Python::Token::T_LiteralSglQuote: FALLTHROUGH
        case Python::Token::T_Comment:
            return false;// ignore indents if we are at a multiline string
        default:
            if (itTok->isCode())
                return true;
        }
        itTok = itTok->next();
    }

    return true;
}

Python::SourceIndent::Indent Python::SourceIndent::_current() const
{
    if (m_indentStack.size() < 1)
        return Indent();
    return m_indentStack.back();
}
