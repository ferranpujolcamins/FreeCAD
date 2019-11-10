#include "PythonSourceModule.h"
#include <TextEditor/PythonCode.h>
#include <TextEditor/PythonSyntaxHighlighter.h>
#include <QDebug>

DBG_TOKEN_FILE

using namespace Gui;


Python::SourceModule::SourceModule(Python::SourceRoot *root,
                                   Python::SyntaxHighlighter *highlighter) :
    Python::SourceListParentBase(this),
    m_root(root),
    m_rootFrame(this, this),
    m_highlighter(highlighter)
{
}

Python::SourceModule::~SourceModule()
{
}

void Python::SourceModule::scanFrame(Python::Token *tok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    Python::SourceIndent indent = currentBlockIndent(tok);
    const Python::SourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const_cast<Python::SourceFrame*>(frm)->scanFrame(tok, indent);
}

void Python::SourceModule::scanLine(Python::Token *tok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    if (!tok)
        return;

    Python::SourceIndent indent = currentBlockIndent(tok);
    const Python::SourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const_cast<Python::SourceFrame*>(frm)->scanLine(tok, indent);
}

Python::SourceIndent Python::SourceModule::currentBlockIndent(const Python::Token *tok) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    Python::SourceIndent ind;
    if (!tok)
        return ind; // is invalid here

    // push the root indentblock
    ind.pushFrameBlock(-1, 0);

    const Python::SourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const Python::Token *beginTok = frm->token() ? frm->token() : tok;

    int frameIndent = 0,
        currentIndent = 0;

    // root frame should always have 0 as indent
    if (frm->parentFrame() == nullptr)
        frameIndent = 0;
    else
        frameIndent = beginTok->txtBlock()->indent();

    //beginTok = tok;
    DBG_TOKEN(beginTok)

    QList<int> traversedBlocks;
    // rationale here is to back up until we find a line with less indent
    // def func(arg1):     <- frameIndent
    //     var1 = None     <- previousBlockIndent
    //     if (arg1):      <- triggers change in currentBlockIndent
    //         var1 = arg1 <- currentBlockIndent
    Python::TextBlockData *txtData = tok->txtBlock();
    int indent = txtData->indent();
    int guard = 1000;
    while (txtData && (guard--) > 0) {
        if (!txtData->isEmpty()){
            if (indent >= txtData->indent()) {
                // look for ':' if not found its a syntax error unless its a comment
                beginTok = txtData->findToken(Python::Token::T_DelimiterColon, -1);
                DBG_TOKEN(beginTok)
                const Python::Token *colonTok = beginTok;
                while (beginTok && (guard--) > 0) {
                    PREV_TOKEN(beginTok)
                    // make sure isn't a comment line
                    if (!beginTok || beginTok->token == Python::Token::T_Comment ||
                        beginTok->txtBlock() != txtData)
                    {
                        break; // goto parent loop, do previous line
                    } else if (beginTok->token == Python::Token::T_BlockStart) {
                        traversedBlocks.push_back(txtData->indent());
                        guard = -1; break;
                    } else if (beginTok->isCode()) {
                        insertBlockStart(colonTok);
                        traversedBlocks.push_back(txtData->indent());
                        guard = -1;
                        break;
                    } else if(beginTok->token == Python::Token::T_DelimiterNewLine) {
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
        if (currentIndent > -1)
            traversedBlocks.push_back(currentIndent);
    } else {
        if (guard == 0)
            qDebug() << QLatin1String("scanFrame loopguard") << endl;
        // we dind't find any ':', must be in root frame
        assert(frm->parentFrame() == nullptr && frameIndent == 0 && "Should be in root frame here!");
    }
    if (frameIndent > 0 || traversedBlocks.size() > 0){
        for(int indnt : traversedBlocks)
            ind.pushFrameBlock(frameIndent, indnt);
    }
    return ind;
}

int Python::SourceModule::_currentBlockIndent(const Python::Token *tok) const
{
    DEFINE_DBG_VARS

    const Python::Token *beginTok = tok;
    DBG_TOKEN(beginTok)

    // get indent of first line after newline at frame start
    int newLines = 0;
    while(beginTok && newLines < 2) {
        switch (beginTok->token){
        case Python::Token::T_DelimiterLineContinue:
            --newLines; // newline escaped
            break;
        case Python::Token::T_DelimiterColon:
            insertBlockStart(beginTok);
            break;
        case Python::Token::T_DelimiterNewLine:
            ++newLines;
            break;
        case Python::Token::T_Comment:
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

void Python::SourceModule::newFormatToken(const Python::Token *tok, QTextCharFormat format) const
{
    if (m_highlighter)
        m_highlighter->newFormatToken(tok, format);
}

void Python::SourceModule::updateFormatToken(const Python::Token *tok) const
{
    if (m_highlighter)
        m_highlighter->updateFormatToken(tok);
}

int Python::SourceModule::compare(const Python::SourceListNodeBase *left,
                                const Python::SourceListNodeBase *right)
{
    const Python::SourceModule *l = dynamic_cast<const Python::SourceModule*>(left),
                             *r = dynamic_cast<const Python::SourceModule*>(right);
    assert(l != nullptr && r != nullptr && "Invalid class stored as Python::SourceModule");
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

const Python::SourceFrame *Python::SourceModule::getFrameForToken(const Python::Token *tok,
                                                        const Python::SourceFrame *parentFrame) const
{
    DEFINE_DBG_VARS

    // find the frame associated with this token
    if (parentFrame->empty())
        return parentFrame; // we have no children, this is it!

    Python::SourceFrame *childFrm = nullptr;
    Python::SourceListNodeBase *itm = nullptr;

    for (itm = parentFrame->begin();
         itm != parentFrame->end();
         itm = itm->next())
    {
        childFrm = dynamic_cast<Python::SourceFrame*>(itm);
        assert(childFrm != nullptr && "Wrong datatype stored in Python::SourceFrame");
        if (!childFrm->token())
            continue;
        if (!childFrm->lastToken()) {
            // end token have been deleted, re-parse end token
            int ind = childFrm->token()->txtBlock()->indent();
            Python::TextBlockData *txtData = childFrm->token()->txtBlock();
            while ((txtData = txtData->next())) {
                if (txtData->isCodeLine() && txtData->indent() <= ind) {
                    while((txtData = txtData->previous())) {
                        if (txtData->isCodeLine()) {
                            childFrm->setLastToken(txtData->tokens().last());
                            break;
                        }
                    }
                    break;
                } else if (!txtData->next()) {
                    // we might be at a row with no tokens
                    while(txtData->tokens().empty())
                        txtData = txtData->previous();
                    if (txtData && txtData->tokens().size())
                        childFrm->setLastToken(txtData->tokens().last());
                    break;
                }
            }
        }
        // we might have not found it
        if (!childFrm->lastToken())
            continue;

        qDebug() << tok->line() << QLatin1String(">=") << childFrm->token()->line()<<endl;
        qDebug() << tok->line() << QLatin1String(">=") << childFrm->lastToken()->line()<<endl;
        qDebug() << "------" <<endl;
        if (*tok >= *childFrm->token() && *tok <= *childFrm->lastToken())
        {
            // find recursivly
            if (!childFrm->empty())
                return getFrameForToken(tok, dynamic_cast<const Python::SourceFrame*>(childFrm));
            // no childs, return ourself
            return childFrm;
        }
    }

    return parentFrame;
}

void Python::SourceModule::insertBlockStart(const Python::Token *colonTok) const
{
    if (colonTok->next() && colonTok->next()->token != Python::Token::T_BlockStart) {
        colonTok->txtBlock()->insertToken(Python::Token::T_BlockStart,
                                          colonTok->startPos, 0);
        colonTok->txtBlock()->incBlockState();
    }
}

Python::Token *Python::SourceModule::insertBlockEnd(const Python::Token *newLineTok) const
{
    if (newLineTok->previous() &&
        newLineTok->previous()->token != Python::Token::T_BlockEnd)
    {
        Python::Token *blkEndTok = newLineTok->txtBlock()->insertToken(Python::Token::T_BlockEnd,
                                                               newLineTok->previous()->startPos, 0);
        newLineTok->txtBlock()->decBlockState();
        return blkEndTok;
    }
    return nullptr;
}


void Python::SourceModule::setSyntaxError(const Python::Token *tok, QString parseMessage) const
{
    DEFINE_DBG_VARS

    Python::TextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new Python::TextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, parseMessage, TextEditBlockScanInfo::SyntaxError);

    // create format with default format for syntax error
    const_cast<Python::Token*>(tok)->token = Python::Token::T_SyntaxError;
    if (m_highlighter)
        m_highlighter->updateFormatToken(tok);

    // set text underline of all text in here
    // move to beginning of text
    const Python::Token *startTok = tok;
    DBG_TOKEN(startTok)

    // first move up to end of code if its a whitespace
    if (!startTok->isCode()) {
        int guard = 100;
        bool hasCode = false;
        while (guard--) {
            NEXT_TOKEN(startTok)
            if (!startTok)
                break;
            if (hasCode) {
                if (!startTok->isCode()) {
                    PREV_TOKEN(startTok)
                    break;
                }
            } else
                hasCode = startTok->isCode();
        }
    }
    // then move down the line til first whitespace
    while(startTok && startTok->isCode()) {
        if (m_highlighter)
            m_highlighter->setSyntaxError(tok);

        // previous token
        if (startTok->previous()) {
            PREV_TOKEN(startTok)
        } else
            break;
    }
}

void Python::SourceModule::setIndentError(const Python::Token *tok) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    Python::TextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new Python::TextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, QLatin1String("Unexpected indent"), TextEditBlockScanInfo::IndentError);

    const_cast<Python::Token*>(tok)->token = Python::Token::T_SyntaxError;
    updateFormatToken(tok);

    if (m_highlighter)
        m_highlighter->setIndentError(tok);
}

void Python::SourceModule::setLookupError(const Python::Token *tok, QString parseMessage) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    Python::TextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new Python::TextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    if (parseMessage.isEmpty())
        parseMessage = QString::fromLatin1("Can't lookup identifier '%1'").arg(tok->text());

    scanInfo->setParseMessage(tok, parseMessage, TextEditBlockScanInfo::LookupError);
}

void Python::SourceModule::setMessage(const Python::Token *tok, QString parseMessage) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    Python::TextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
    if (!scanInfo) {
        scanInfo = new Python::TextBlockScanInfo();
        tok->txtBlock()->setScanInfo(scanInfo);
    }

    scanInfo->setParseMessage(tok, parseMessage, Python::TextBlockScanInfo::Message);

    if (m_highlighter)
        m_highlighter->setMessage(tok);
}



// -----------------------------------------------------------------------

Python::SourceModuleList::SourceModuleList() :
    Python::SourceListParentBase(this),
    m_main(nullptr)
{
}

Python::SourceModuleList::~SourceModuleList()
{
}

void Python::SourceModuleList::setMain(Python::SourceModule *main)
{
    if (!contains(main))
        insert(main);

    m_main = main;
}

bool Python::SourceModuleList::remove(Python::SourceListNodeBase *node, bool deleteNode)
{
    if (node == m_main)
        m_main = nullptr;

    return Python::SourceListParentBase::remove(node, deleteNode);
}
