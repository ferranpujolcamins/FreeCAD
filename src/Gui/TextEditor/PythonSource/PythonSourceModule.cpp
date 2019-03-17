#include "PythonSourceModule.h"
#include <TextEditor/PythonCode.h>

DBG_TOKEN_FILE

using namespace Gui;


PythonSourceModule::PythonSourceModule(PythonSourceRoot *root,
                                       PythonSyntaxHighlighter *highlighter) :
    PythonSourceListParentBase(this),
    m_root(root),
    m_rootFrame(this, this),
    m_highlighter(highlighter),
    m_rehighlight(false)
{
}

PythonSourceModule::~PythonSourceModule()
{
}

void PythonSourceModule::scanFrame(PythonToken *tok)
{
    PythonSourceIndent indent = currentBlockIndent(tok);
    const PythonSourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const_cast<PythonSourceFrame*>(frm)->scanFrame(tok, indent);

   // if (m_rehighlight)
        //m_highlighter->rehighlight();
}

void PythonSourceModule::scanLine(PythonToken *tok)
{
    PythonSourceIndent indent = currentBlockIndent(tok);
    const PythonSourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const_cast<PythonSourceFrame*>(frm)->scanLine(tok, indent);

   // if (m_rehighlight && tok)
   //     m_highlighter->rehighlightBlock(tok->txtBlock()->block());
}

PythonSourceIndent PythonSourceModule::currentBlockIndent(const PythonToken *tok) const
{
    DEFINE_DBG_VARS

    PythonSourceIndent ind;
    if (!tok)
        return ind; // is invalid here

    // push the root indentblock
    ind.pushFrameBlock(0, 0);

    const PythonSourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const PythonToken *beginTok = frm->token() ? frm->token() : tok;

    int frameIndent = 0,
        currentIndent = 0;

    frameIndent = beginTok->txtBlock()->indent();

    //beginTok = tok;
    DBG_TOKEN(beginTok)

    // rationale here is to back up until we find a line with less indent
    // def func(arg1):     <- frameIndent
    //     var1 = None     <- previousBlockIndent
    //     if (arg1):      <- triggers change in currentBlockIndent
    //         var1 = arg1 <- currentBlockIndent
    PythonTextBlockData *txtData = tok->txtBlock();
    int indent = txtData->indent();
    int guard = 1000;
    while (txtData && (guard--) > 0) {
        if (!txtData->isEmpty()){
            if (indent >= txtData->indent()) {
                // look for ':' if not found its a syntax error unless its a comment
                beginTok = txtData->findToken(PythonSyntaxHighlighter::T_DelimiterColon, -1);
                DBG_TOKEN(beginTok)
                const PythonToken *colonTok = beginTok;
                while (beginTok && (guard--) > 0) {
                    PREV_TOKEN(beginTok)
                    // make sure isn't a comment line
                    if (!beginTok || beginTok->token == PythonSyntaxHighlighter::T_Comment ||
                        beginTok->txtBlock() != txtData)
                    {
                        break; // goto parent loop, do previous line
                    } else if (beginTok->token == PythonSyntaxHighlighter::T_BlockStart) {
                        guard = -1; break;
                    } else if (beginTok->isCode()) {
                        insertBlockStart(colonTok);
                        guard = -1;
                        break;
                    } else if(beginTok->token == PythonSyntaxHighlighter::T_DelimiterNewLine) {
                        guard = -2; // exit parent loop
                        break;
                    }
                }

                if (guard < -1)
                    setSyntaxError(beginTok, QString::fromLatin1("Blockstart without ':'"));
            }

        }

        if (guard > 0) // guard is 0 when we exit, no need to look up previous row
            txtData = txtData->previous();
    }

    if (guard < 0) {
        // we found it
        currentIndent = _currentBlockIndent(beginTok);
    } else {
        // we dind't find any ':', must be in root frame
        assert(frameIndent == 0 && "Should be in root frame here!");
    }
    if (frameIndent > 0 || currentIndent > 0)
        ind.pushFrameBlock(frameIndent, currentIndent);
    return ind;
}

int PythonSourceModule::_currentBlockIndent(const PythonToken *tok) const
{
    DEFINE_DBG_VARS

    const PythonToken *beginTok = tok;
    DBG_TOKEN(beginTok)

    // get indent of first line after newline at frame start
    int newLines = 0;
    while(beginTok && newLines < 2) {
        switch (beginTok->token){
        case PythonSyntaxHighlighter::T_DelimiterLineContinue:
            --newLines; // newline escaped
            break;
        case PythonSyntaxHighlighter::T_DelimiterColon:
            insertBlockStart(beginTok);
            break;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            ++newLines;
            break;
        case PythonSyntaxHighlighter::T_Comment:
            if (newLines)
                --newLines; // don't count a comment as a indent
            break;
        default:
            if (newLines > 0 && beginTok->isCode())
                return beginTok->txtBlock()->indent();
        }
        NEXT_TOKEN(beginTok)
    }
    // we didn't find any block
    return 0;
}

void PythonSourceModule::setFormatToken(const PythonToken *tok, QTextCharFormat format)
{
    if (tok->txtBlock()->setReformat(tok, format))
        m_rehighlight = true; // only re-highlight if we succeed
}

void PythonSourceModule::setFormatToken(const PythonToken *tok)
{
    setFormatToken(tok, m_highlighter->getFormatToken(tok));
}

int PythonSourceModule::compare(const PythonSourceListNodeBase *left,
                                const PythonSourceListNodeBase *right)
{
    const PythonSourceModule *l = dynamic_cast<const PythonSourceModule*>(left),
                             *r = dynamic_cast<const PythonSourceModule*>(right);
    assert(l != nullptr && r != nullptr && "Invalid class stored as PythonSourceModule");
    if (l->moduleName() < r->moduleName())
        return +1;
    else if (l->moduleName() > r->moduleName())
        return -1;
    else {
        if (l->filePath() < r->filePath())
            return -1;
        if (l->filePath() > r->filePath())
            return +1;
        assert(false && "Trying to store the same module twice");
    }
}

const PythonSourceFrame *PythonSourceModule::getFrameForToken(const PythonToken *tok,
                                                        const PythonSourceFrame *parentFrame) const
{
    DEFINE_DBG_VARS

    // find the frame associated with this token
    if (parentFrame->empty())
        return parentFrame; // we have no children, this is it!

    const PythonSourceFrame *childFrm = nullptr;
    PythonSourceListNodeBase *itm = nullptr;

    for (itm = parentFrame->begin();
         itm != parentFrame->end();
         itm = itm->next())
    {
        childFrm = dynamic_cast<const PythonSourceFrame*>(itm);
        assert(childFrm != nullptr && "Wrong datatype stored in PythonSourceFrame");
        if (*tok >= *childFrm->token() && *tok <= *childFrm->lastToken)
        {
            // find recursivly
            if (!childFrm->empty())
                return getFrameForToken(tok, dynamic_cast<const PythonSourceFrame*>(childFrm));
            // no childs, return ourself
            return childFrm;
        }
    }

    return parentFrame;
}

void PythonSourceModule::insertBlockStart(const PythonToken *colonTok) const
{
    if (colonTok->next() && colonTok->next()->token != PythonSyntaxHighlighter::T_BlockStart) {
        colonTok->txtBlock()->insertToken(PythonSyntaxHighlighter::T_BlockStart,
                                          colonTok->startPos, 0);
    }
}

void PythonSourceModule::insertBlockEnd(const PythonToken *newLineTok) const
{
    if (newLineTok->previous() &&
        newLineTok->previous()->token != PythonSyntaxHighlighter::T_BlockEnd)
    {
        newLineTok->txtBlock()->insertToken(PythonSyntaxHighlighter::T_BlockEnd,
                                            newLineTok->previous()->startPos, 0);
    }
}


void PythonSourceModule::setSyntaxError(const PythonToken *tok, QString parseMessage) const
{
    DEFINE_DBG_VARS

    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, parseMessage, TextEditBlockScanInfo::SyntaxError);

    // create format with default format for syntax error
    const_cast<PythonToken*>(tok)->token = PythonSyntaxHighlighter::T_SyntaxError;
    QColor lineColor = m_highlighter->colorByType(SyntaxHighlighter::SyntaxError);

    // set text underline of all text in here
    // move to beginning of text
    const PythonToken *startTok = tok;
    DBG_TOKEN(startTok)

    // first move up to end of code if its a whitespace
    if (!startTok->isCode()) {
        int guard = 100;
        bool hasCode = false;
        while (guard--) {
            NEXT_TOKEN(startTok)
            if (hasCode) {
                if (!startTok->isCode())
                    break;
            } else
                hasCode = startTok->isCode();
        }
    }
    // then move down the line til first whitespace
    while(startTok && startTok->isCode()) {
        QTextCharFormat format = m_highlighter->getFormatToken(tok);
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        format.setUnderlineColor(lineColor);
        const_cast<PythonSourceModule*>(this)->setFormatToken(tok, format);

        // previous token
        if (startTok->previous()) {
            PREV_TOKEN(startTok)
        } else
            break;
    }
}

void PythonSourceModule::setIndentError(const PythonToken *tok) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, QLatin1String("Unexpected indent"), TextEditBlockScanInfo::IndentError);

    // create format with default format for syntax error
    const_cast<PythonToken*>(tok)->token = PythonSyntaxHighlighter::T_SyntaxError;
    QColor lineColor = m_highlighter->colorByType(SyntaxHighlighter::SyntaxError);

    QTextCharFormat format = m_highlighter->getFormatToken(tok);
    format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    format.setUnderlineColor(lineColor);
    const_cast<PythonSourceModule*>(this)->setFormatToken(tok, format);
}

void PythonSourceModule::setLookupError(const PythonToken *tok, QString parseMessage) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    if (parseMessage.isEmpty())
        parseMessage = QString::fromLatin1("Can't lookup identifier '%1'").arg(tok->text());

    scanInfo->setParseMessage(tok, parseMessage, TextEditBlockScanInfo::LookupError);
}

void PythonSourceModule::setMessage(const PythonToken *tok, QString parseMessage) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new PythonTextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, parseMessage, PythonTextBlockScanInfo::Message);
}



// -----------------------------------------------------------------------

PythonSourceModuleList::PythonSourceModuleList() :
    PythonSourceListParentBase(this),
    m_main(nullptr)
{
}

PythonSourceModuleList::~PythonSourceModuleList()
{
}

void PythonSourceModuleList::setMain(PythonSourceModule *main)
{
    if (!contains(main))
        insert(main);

    m_main = main;
}

bool PythonSourceModuleList::remove(PythonSourceListNodeBase *node, bool deleteNode)
{
    if (node == m_main)
        m_main = nullptr;

    return PythonSourceListParentBase::remove(node, deleteNode);
}
