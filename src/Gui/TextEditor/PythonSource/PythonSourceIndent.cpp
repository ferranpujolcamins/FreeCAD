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
    for (int i = 0; i < m_indentStack.size(); ++i){
        const Indent lhs = m_indentStack[i],
                     rhs = other.m_indentStack[i];
        if (lhs.frameIndent != rhs.frameIndent ||
            lhs.currentBlockIndent != rhs.currentBlockIndent)
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
        return m_indentStack.first().currentBlockIndent;
    return m_indentStack[m_indentStack.size()-1].currentBlockIndent;
}

bool Python::SourceIndent::isValid() const {
    if (m_indentStack.size() < 1)
        return false;
    const Indent ind = m_indentStack[m_indentStack.size()-1];
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
    Indent ind = m_indentStack.last();
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
    for(Python::Token *tmpTok : tok->txtBlock()->tokens()) {
        DBG_TOKEN(tmpTok)
        switch (tmpTok->token) {
        case Python::Token::T_LiteralBlockDblQuote:
        case Python::Token::T_LiteralBlockSglQuote:
        case Python::Token::T_LiteralDblQuote:
        case Python::Token::T_LiteralSglQuote:
        case Python::Token::T_Comment: // fallthrough
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
    return m_indentStack.last();
}
