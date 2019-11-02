#ifndef PYTHONSOURCEINDENT_H
#define PYTHONSOURCEINDENT_H

#include "PythonSourceRoot.h"

namespace Gui {

// used as a message parsing between functions
class PythonSourceIndent {
    struct Indent {
        int frameIndent,
            currentBlockIndent;
        explicit Indent(int frmInd = -1, int curInd = -1):
            frameIndent(frmInd), currentBlockIndent(curInd)
        {}
        Indent(const Indent &other):
            frameIndent(other.frameIndent),
            currentBlockIndent(other.currentBlockIndent)
        {}
        ~Indent() {}
    };
public:
    explicit PythonSourceIndent();
    ~PythonSourceIndent();
    PythonSourceIndent operator =(const PythonSourceIndent &other);
    bool operator == (const PythonSourceIndent &other);
    /// returns the number of pushed block -1, invalid == -1
    int atIndentBlock() const { return m_indentStack.size() -1; }

    int frameIndent() const;
    int currentBlockIndent() const;
    int previousBlockIndent() const;

    /// true if we have a valid indentBlock stored
    bool isValid() const;

    /// inserts a new frameBlock (ie def func(arg1):)
    /// returns current indentBlock
    void pushFrameBlock(int frmIndent, int currentIndent);
    /// inserts a new block, get frameblock form previous block (indent ie if (arg1):)
    /// returns current indentblock
    void pushBlock(int currentIndent);
    /// pop a indentblock (de-indent)
    void popBlock();
    /// framePopCnt is a counter that stores the number of frames that have popped
    int framePopCnt() const { return m_framePopCnt; }
    int framePopCntDecr();

    /// returns true if line pointed to by tok is valid, ie: no comment string etc
    bool validIndentLine(PythonToken *tok);
private:
    QList<Indent> m_indentStack;
    Indent _current() const;
    int m_framePopCnt;
};

} // namespace Gui

#endif // PYTHONSOURCEINDENT_H
