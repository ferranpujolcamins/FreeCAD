#include "PythonSourceIndent.h"
#include "PythonSource.h"


DBG_TOKEN_FILE

using namespace Gui;

PythonSourceIndent::PythonSourceIndent():
    m_framePopCnt(0)
{
}

PythonSourceIndent::~PythonSourceIndent()
{
}

PythonSourceIndent PythonSourceIndent::operator =(const PythonSourceIndent &other) {
    m_indentStack = other.m_indentStack;
    return *this;
}

bool PythonSourceIndent::operator ==(const PythonSourceIndent &other) {
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

int PythonSourceIndent::frameIndent() const
{
    return _current().frameIndent;
}

int PythonSourceIndent::currentBlockIndent() const
{
    return _current().currentBlockIndent;
}

int PythonSourceIndent::previousBlockIndent() const
{
    if (m_indentStack.empty())
        return 0;
    if (m_indentStack.size() == 1)
        return m_indentStack.first().currentBlockIndent;
    return m_indentStack[m_indentStack.size()-1].currentBlockIndent;
}

bool PythonSourceIndent::isValid() const {
    if (m_indentStack.size() < 1)
        return false;
    const Indent ind = m_indentStack[m_indentStack.size()-1];
    return ind.currentBlockIndent > -1 && ind.frameIndent > -1;
}

void PythonSourceIndent::pushFrameBlock(int frmIndent, int currentIndent)
{
    Indent ind(frmIndent, currentIndent);
    m_indentStack.push_back(ind);
}

void PythonSourceIndent::pushBlock(int currentIndent)
{
    Indent ind(_current().frameIndent, currentIndent);
    m_indentStack.push_back(ind);
}

void PythonSourceIndent::popBlock()
{
    if (m_indentStack.isEmpty())
        return;
    Indent ind = m_indentStack.takeLast();
    if (ind.frameIndent >= m_indentStack.last().currentBlockIndent)
        ++m_framePopCnt; // store that we have popped a frame
}

int PythonSourceIndent::framePopCntDecr()
{
    if (framePopCnt() > 0)
        --m_framePopCnt;
    return m_framePopCnt;
}

PythonSourceIndent::Indent PythonSourceIndent::_current() const
{
    if (m_indentStack.size() < 1)
        return Indent();
    return m_indentStack.last();
}
