#include "PythonSourceIndent.h"
#include "PythonSource.h"


DBG_TOKEN_FILE

using namespace Gui;

Python::SourceIndent::SourceIndent():
    m_framePopCnt(0)
{
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

int Python::SourceIndent::frameIndent() const
{
    return _current().frameIndent;
}

int Python::SourceIndent::currentBlockIndent() const
{
    return _current().currentBlockIndent;
}

int Python::SourceIndent::previousBlockIndent() const
{
    if (m_indentStack.empty())
        return 0;
    if (m_indentStack.size() == 1)
        return m_indentStack.front().currentBlockIndent;
    return m_indentStack.back().currentBlockIndent;
}

bool Python::SourceIndent::isValid() const {
    if (m_indentStack.size() < 1)
        return false;
    const Indent ind = m_indentStack.back();
    return ind.currentBlockIndent > -1 || ind.frameIndent > -1;
}

void Python::SourceIndent::pushFrameBlock(int frmIndent, int currentIndent)
{
    // insert a framestarter
    assert(frmIndent <= currentIndent && "frmIndent must be less or equal to currentIndent");
    Indent ind(frmIndent, currentIndent);
    m_indentStack.push_back(ind);
}

void Python::SourceIndent::pushBlock(int currentIndent)
{
    Indent ind(_current().frameIndent, currentIndent);
    m_indentStack.push_back(ind);
}

void Python::SourceIndent::popBlock()
{
    if (m_indentStack.size() < 2)
        return;
    m_indentStack.pop_back();
    Indent ind = m_indentStack.back();
    if (ind.frameIndent >= ind.currentBlockIndent) {
        // pop a framestarter
        m_indentStack.pop_back();
        ++m_framePopCnt; // store that we have popped a frame
    }
}

int Python::SourceIndent::framePopCntDecr()
{
    if (framePopCnt() > 0)
        --m_framePopCnt;
    return m_framePopCnt;
}


bool Python::SourceIndent::validIndentLine(Python::Token *tok)
{
    DEFINE_DBG_VARS

    // make sure we don't dedent on a comment or a string
    for(Python::Token *tmpTok : tok->ownerLine()->tokens()) {
        DBG_TOKEN(tmpTok)
        switch (tmpTok->type()) {
        case Python::Token::T_LiteralBlockDblQuote:
        case Python::Token::T_LiteralBlockSglQuote:
        case Python::Token::T_LiteralDblQuote:
        case Python::Token::T_LiteralSglQuote: FALLTHROUGH
        case Python::Token::T_Comment:
            return false;// ignore indents if we are at a multiline string
        default:
            if (tmpTok->isCode())
                return true;
        }
    }

    return true;
}

Python::SourceIndent::Indent Python::SourceIndent::_current() const
{
    if (m_indentStack.size() < 1)
        return Indent();
    return m_indentStack.back();
}
