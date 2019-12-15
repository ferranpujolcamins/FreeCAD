
#include "PythonSyntaxHighlighter.h"

#include "PreCompiled.h"

#include <CXX/Objects.hxx>
#include <Base/Interpreter.h>
#include <QDebug>
#include <QTimer>
#include "PythonCode.h"
#include <PythonSource/PythonSourceRoot.h>
#include <PythonSource/PythonSourceListBase.h>

#ifdef BUILD_PYTHON_DEBUGTOOLS
# include <PythonSource/PythonSourceDebugTools.h>
# include <QElapsedTimer>
#endif

#include "PythonEditor.h"
#include <Gui/PythonConsole.h>

using namespace Gui;



namespace Gui {

class PythonSyntaxHighlighterP
{
public:
    explicit PythonSyntaxHighlighterP()
    {
    }
    ~PythonSyntaxHighlighterP()
    {
    }

    QTimer sourceScanTmr;
    QList<int> srcScanBlocks;
};

} // namespace Gui


// ----------------------------------------------------------------------

/**
 * Constructs a Python syntax highlighter.
 */
PythonSyntaxHighlighter::PythonSyntaxHighlighter(QObject* parent):
    Gui::SyntaxHighlighter(parent),
    Python::Lexer()
{
    d = new PythonSyntaxHighlighterP();
    d->sourceScanTmr.setInterval(50); // wait before running PythonSourceRoot rescan
                                      // dont scan on every row during complete rescan
    d->sourceScanTmr.setSingleShot(true);
    connect(&d->sourceScanTmr, SIGNAL(timeout()), this, SLOT(sourceScanTmrCallback()));
}

/** Destroys this object. */
PythonSyntaxHighlighter::~PythonSyntaxHighlighter()
{
    delete d;
}

/**
 * Detects all kinds of text to highlight them in the correct color.
 */
void PythonSyntaxHighlighter::highlightBlock (const QString &text)
{
    Q_UNUSED(text)
    int prevState = previousBlockState();// != -1 ? previousBlockState() : 0; // is -1 when no state is set
    Python::Token::Type endStateOfLastPara = static_cast<Python::Token::Type>(prevState);

    if (endStateOfLastPara < Python::Token::T_Undetermined)
        endStateOfLastPara = Python::Token::T_Undetermined;

    d->sourceScanTmr.blockSignals(true);
    d->sourceScanTmr.stop();

    // create new userData, copy bookmark etc
    PythonTextBlockData *txtBlock = new PythonTextBlockData(currentBlock(), &list()),
                        *curBlock = nullptr;
    if (currentBlock().isValid())
        curBlock = dynamic_cast<PythonTextBlockData*>(currentBlock().userData());

    if (curBlock) {
        txtBlock->copyBlock(*curBlock);
        list().swapLine(curBlock, txtBlock);
    } else {
        // insert into list at correct pos
        Python::TokenLine *prevLine = dynamic_cast<Python::TokenLine*>(
                                                    currentBlock().previous().userData());
        // prevBlock = nullptr means first line
        list().insertLine(prevLine, txtBlock);
    }

    setActiveLine(txtBlock);
    setCurrentBlockUserData(txtBlock); // implicitly deletes curBlock


#ifdef BUILD_PYTHON_DEBUGTOOLS
    txtBlock->m_textDbg = text;
#endif

    // scans this line
    endStateOfLastParaRef() = endStateOfLastPara;
    tokenize(txtBlock);

    prevState = static_cast<int>(endStateOfLastParaRef());
    setCurrentBlockState(prevState);
    d->srcScanBlocks.push_back(txtBlock->block().blockNumber());
    setActiveLine(nullptr);

    d->sourceScanTmr.start();
    d->sourceScanTmr.blockSignals(false);
}

void PythonSyntaxHighlighter::rehighlight()
{
    Gui::SyntaxHighlighter::rehighlight();
    d->srcScanBlocks.clear();
    Python::SourceRoot::instance()->scanCompleteModule(Python::Lexer::filePath(), this);
}

QTextCharFormat PythonSyntaxHighlighter::getFormatToken(const Python::Token *token) const
{
    assert(token != nullptr && "Need a valid pointer");

    QTextCharFormat format;
    SyntaxHighlighter::TColor colorIdx = SyntaxHighlighter::Text ;
    switch (token->type()) {
    // all specialcases chaught by switch, rest in default
    case Python::Token::T_Comment:
        colorIdx = SyntaxHighlighter::Comment; break;
    case Python::Token::T_SyntaxError:
        colorIdx = SyntaxHighlighter::SyntaxError; break;
    case Python::Token::T_IndentError:
        colorIdx = SyntaxHighlighter::SyntaxError;
        format.setUnderlineStyle(QTextCharFormat::QTextCharFormat::DotLine);
        format.setUnderlineColor(colorIdx);
        break;

    // numbers
    case Python::Token::T_NumberHex:
        colorIdx = SyntaxHighlighter::NumberHex; break;
    case Python::Token::T_NumberBinary:
        colorIdx = SyntaxHighlighter::NumberBinary; break;
    case Python::Token::T_NumberOctal:
        colorIdx = SyntaxHighlighter::NumberOctal; break;
    case Python::Token::T_NumberFloat:
        colorIdx = SyntaxHighlighter::NumberFloat; break;

    // strings
    case Python::Token::T_LiteralDblQuote:
        colorIdx = SyntaxHighlighter::StringDoubleQoute; break;
    case Python::Token::T_LiteralSglQuote:
        colorIdx = SyntaxHighlighter::StringSingleQoute; break;
    case Python::Token::T_LiteralBlockDblQuote:
        colorIdx = SyntaxHighlighter::StringBlockDoubleQoute; break;
    case Python::Token::T_LiteralBlockSglQuote:
        colorIdx = SyntaxHighlighter::StringBlockSingleQoute; break;

    // keywords
    case Python::Token::T_KeywordClass:
        colorIdx = SyntaxHighlighter::KeywordClass;
        format.setFontWeight(QFont::Bold);
        break;
    case Python::Token::T_KeywordDef:
        colorIdx = SyntaxHighlighter::KeywordDef;
        format.setFontWeight(QFont::Bold);
        break;

    // operators
    case Python::Token::T_OperatorVariableParam: // fallthrough
    case Python::Token::T_OperatorKeyWordParam:
        colorIdx = SyntaxHighlighter::Text; break;

    // delimiters
    case Python::Token::T_DelimiterBackSlash:
        colorIdx = SyntaxHighlighter::Text; break;

    // identifiers
    case Python::Token::T_IdentifierDefined:
        colorIdx = SyntaxHighlighter::IdentifierDefined; break;

    case Python::Token::T_IdentifierModuleAlias:
    case Python::Token::T_IdentifierModulePackage:// fallthrough
    case Python::Token::T_IdentifierModule:
        colorIdx = SyntaxHighlighter::IdentifierModule; break;
    case Python::Token::T_IdentifierModuleGlob:
        colorIdx = SyntaxHighlighter::Text; break;
    case Python::Token::T_IdentifierFunction:
        colorIdx = SyntaxHighlighter::IdentifierFunction;
        format.setFontWeight(QFont::Bold);
        break;
    case Python::Token::T_IdentifierMethod:
        colorIdx = SyntaxHighlighter::IdentifierMethod;
        format.setFontWeight(QFont::Bold);
        break;
    case Python::Token::T_IdentifierClass:
        colorIdx = SyntaxHighlighter::IdentifierClass;
        format.setFontWeight(QFont::Bold);
        break;
    case Python::Token::T_IdentifierSuperMethod:
        colorIdx = SyntaxHighlighter::IdentifierSuperMethod;
        format.setFontWeight(QFont::Bold);
        break;
    case Python::Token::T_IdentifierBuiltin:
        colorIdx = SyntaxHighlighter::IdentifierBuiltin;
        format.setFontWeight(QFont::Bold);
        break;
    case Python::Token::T_IdentifierDecorator:
        colorIdx = SyntaxHighlighter::IdentifierDecorator;
        format.setFontWeight(QFont::Bold);
        break;
    case Python::Token::T_IdentifierDefUnknown:
        colorIdx = SyntaxHighlighter::IdentifierDefUnknown; break;
    case Python::Token::T_IdentifierSelf:
        format.setFontWeight(QFont::Bold);
        colorIdx = SyntaxHighlighter::IdentifierSelf; break;
    case Python::Token::T_IdentifierFalse:
    case Python::Token::T_IdentifierNone: // fallthrough
    case Python::Token::T_IdentifierTrue:
        format.setFontWeight(QFont::Bold);
        colorIdx = SyntaxHighlighter::Keyword;
        break;
    case Python::Token::T_IdentifierInvalid:
        format.setFontItalic(true);
        colorIdx = SyntaxHighlighter::IdentifierUnknown;
        break;
    case Python::Token::T_MetaData:
        colorIdx = SyntaxHighlighter::MetaData; break;
    default:
        // all operators Which arent specialcased above
        if (token->isKeyword() || token->isOperatorCompareKeyword()) {
            colorIdx = SyntaxHighlighter::Keyword;
            format.setFontWeight(QFont::Bold);
        } else if (token->isOperator()) {
            colorIdx = SyntaxHighlighter::Operator;
        } else if (token->isDelimiter()) {
            colorIdx = SyntaxHighlighter::Delimiter;
        } else if (token->isIdentifier()) {
            colorIdx = SyntaxHighlighter::IdentifierUnknown;
        }
        break;
    }

    QColor col = colorByType(colorIdx);
    format.setForeground(col);

    return format;
}

void PythonSyntaxHighlighter::tokenTypeChanged(const Python::Token *tok) const
{
    PythonTextBlockData *txtBlock = dynamic_cast<PythonTextBlockData*>(tok->ownerLine());
    if (!txtBlock || !txtBlock->block().isValid())
        return;

    updateFormat(txtBlock->block(), tok->startPosInt(),
                 static_cast<int>(tok->textLength()), getFormatToken(tok));
}

void PythonSyntaxHighlighter::setMessage(const Python::Token *tok) const
{
    PythonTextBlockData *txtData = dynamic_cast<PythonTextBlockData*>(tok->ownerLine());
    if (!txtData || !txtData->block().isValid())
        return;

    QTextCharFormat format;
    format.setUnderlineColor(QColor(210, 247, 64));
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    setMessageFormat(txtData->block(), tok->startPosInt(),
                     static_cast<int>(tok->textLength()), format);
}

void PythonSyntaxHighlighter::setIndentError(const Python::Token *tok) const
{
    PythonTextBlockData *txtData = dynamic_cast<PythonTextBlockData*>(tok->ownerLine());
    if (!txtData || !txtData->block().isValid())
        return;

    QTextCharFormat format;
    format.setUnderlineColor(QColor(244, 143, 66));
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    setMessageFormat(txtData->block(), tok->startPosInt(),
                     static_cast<int>(tok->textLength()), format);
}

void PythonSyntaxHighlighter::setSyntaxError(const Python::Token *tok) const
{
    PythonTextBlockData *txtData = dynamic_cast<PythonTextBlockData*>(tok->ownerLine());
    if (!txtData || !txtData->block().isValid())
        return;

    QTextCharFormat format;
    format.setUnderlineColor(colorByType(SyntaxError));
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    setMessageFormat(txtData->block(), tok->startPosInt(),
                     static_cast<int>(tok->textLength()), format);
}

void PythonSyntaxHighlighter::newFormatToken(const Python::Token *tok, QTextCharFormat format) const
{
    PythonTextBlockData *txtBlock = dynamic_cast<PythonTextBlockData*>(tok->ownerLine());
    if (!txtBlock || !txtBlock->block().isValid())
        return;

    addFormat(txtBlock->block(), tok->startPosInt(),
              static_cast<int>(tok->textLength()), format);
}

void PythonSyntaxHighlighter::tokenUpdated(const Python::Token *tok)
{
    int pos = tok->startPosInt();
    int len = tok->endPosInt() - pos;
    setFormat(pos, len, getFormatToken(tok));
}

void PythonSyntaxHighlighter::sourceScanTmrCallback() {
    for(int row : d->srcScanBlocks) {
        QTextBlock block = document()->findBlockByNumber(row);
        if (block.isValid()) {
            PythonTextBlockData *txtData = dynamic_cast<PythonTextBlockData*>(block.userData());
            if (txtData)
                Python::SourceRoot::instance()->scanSingleRowModule(
                            Python::Lexer::filePath(), txtData, this);
        }
    }
    d->srcScanBlocks.clear();
}

void PythonSyntaxHighlighter::setFilePath(QString filePath)
{
    Python::Lexer::setFilePath(filePath.toStdString());
#ifdef BUILD_PYTHON_DEBUGTOOLS
    QElapsedTimer timer;
    timer.start();
#endif

    Python::SourceRoot::instance()->scanCompleteModule(filePath.toStdString(), this);

#ifdef BUILD_PYTHON_DEBUGTOOLS
    qDebug() << QString::fromLatin1("scanCompleteModule took %1ms %2ns").arg(timer.elapsed()).arg(timer.nsecsElapsed()) << endl;
#endif
    d->srcScanBlocks.clear();
}

QString PythonSyntaxHighlighter::filePath() const
{
    return QString::fromStdString(Python::Lexer::filePath());
}

// --------------------------------------------------------------------------------------------


PythonMatchingCharInfo::PythonMatchingCharInfo():
    character(0), position(0)
{
}

PythonMatchingCharInfo::PythonMatchingCharInfo(const PythonMatchingCharInfo &other)
{
    character = other.character;
    position = other.position;
}

PythonMatchingCharInfo::PythonMatchingCharInfo(char chr, int pos):
    character(chr), position(pos)
{
}

PythonMatchingCharInfo::~PythonMatchingCharInfo()
{
}

//static
char PythonMatchingCharInfo::matchChar(char match)
{
    switch (match) {
    case '(':
       return ')';
    case ')':
       return '(';
    case '[':
       return ']';
    case ']':
       return '[';
    case '{':
       return '}';
    case '}':
       return '{';
    default:
       return 0;
    }
}

char PythonMatchingCharInfo::matchingChar() const
{
    return PythonMatchingCharInfo::matchChar(character);
}

// -------------------------------------------------------------------------------------------

PythonTextBlockData::PythonTextBlockData(QTextBlock block, Python::TokenList *tokenList,
                                     Python::Token *startTok) :
    Gui::TextEditBlockData(block),
    Python::TokenLine(tokenList, startTok, block.text().toStdString())
{
}

PythonTextBlockData::PythonTextBlockData(const PythonTextBlockData &other) :
    Gui::TextEditBlockData(other),
    Python::TokenLine(other)
{
}

PythonTextBlockData::~PythonTextBlockData()
{
}

PythonTextBlockData *PythonTextBlockData::pyBlockDataFromCursor(const QTextCursor &cursor)
{
    return dynamic_cast<PythonTextBlockData*>(blockDataFromCursor(cursor));
}

PythonTextBlockData *PythonTextBlockData::nextBlock() const
{
    return dynamic_cast<PythonTextBlockData*>(TextEditBlockData::nextBlock());
}

PythonTextBlockData *PythonTextBlockData::previousBlock() const
{
    return dynamic_cast<PythonTextBlockData*>(TextEditBlockData::previousBlock());
}

// static
Python::Token *PythonTextBlockData::tokenAt(const QTextCursor &cursor)
{
    PythonTextBlockData *textData = pyBlockDataFromCursor(cursor);
    if (!textData)
        return nullptr;

    return textData->tokenAt(cursor.position() - cursor.block().position());
}

Python::Token *PythonTextBlockData::tokenAt(int pos) const
{
    return Python::TokenLine::tokenAt(pos);
}

bool PythonTextBlockData::isMatchAt(int pos, Python::Token::Type tokType) const
{
    const Python::Token *tok = tokenAt(pos);
    if (!tok)
        return false;
    return tokType == tok->type();
}

bool PythonTextBlockData::isMatchAt(int pos, const QList<Python::Token::Type> tokTypes) const
{
    Python::Token *tok = Python::TokenLine::tokenAt(pos);
    if (tok) {
        for (const Python::Token::Type tokType : tokTypes) {
            if (tok->type() == tokType)
                return true;
        }
    }
    return false;
}

PythonTextBlockScanInfo *PythonTextBlockData::scanInfo() const
{
    return dynamic_cast<PythonTextBlockScanInfo*>(Gui::TextEditBlockData::scanInfo());
}

void PythonTextBlockData::setScanInfo(PythonTextBlockScanInfo *scanInfo)
{
    m_scanInfo = scanInfo;
}

int PythonTextBlockData::blockState() const
{
    return Python::TokenLine::blockState();
}

int PythonTextBlockData::incBlockState()
{
    return Python::TokenLine::incBlockState();
}

int PythonTextBlockData::decBlockState()
{
    return Python::TokenLine::decBlockState();
}

// -------------------------------------------------------------------------------------


PythonTextBlockScanInfo::PythonTextBlockScanInfo() :
    Gui::TextEditBlockScanInfo()
{
}

PythonTextBlockScanInfo::~PythonTextBlockScanInfo()
{
}

void PythonTextBlockScanInfo::setParseMessage(const Python::Token *tok, QString message,
                                              MsgType type)
{
    Gui::TextEditBlockScanInfo::setParseMessage(tok->startPosInt(), tok->endPosInt(), message, type);
}

const PythonTextBlockScanInfo::ParseMsg
*PythonTextBlockScanInfo::getParseMessage(const Python::Token *tok,
                                          PythonTextBlockScanInfo::MsgType type) const
{
    return Gui::TextEditBlockScanInfo::getParseMessage(tok->startPosInt(), tok->endPosInt(), type);
}

QString PythonTextBlockScanInfo::parseMessage(const Python::Token *tok, MsgType type) const
{
    return Gui::TextEditBlockScanInfo::parseMessage(tok->startPosInt(), type);
}


void PythonTextBlockScanInfo::clearParseMessage(const Python::Token *tok)
{
    Gui::TextEditBlockScanInfo::clearParseMessage(tok->startPosInt());
}

// -------------------------------------------------------------------------------------

PythonMatchingChars::PythonMatchingChars(PythonEditor *parent):
    QObject(parent),
    m_editor(parent)
{
    // for matching chars such as (, [, { etc.
    connect(parent, SIGNAL(cursorPositionChanged()),
            this, SLOT(cursorPositionChange()));
}

PythonMatchingChars::PythonMatchingChars(PythonConsoleTextEdit *parent):
    QObject(parent),
    m_editor(parent)
{
    // for matching chars such as (, [, { etc.
    connect(parent, SIGNAL(cursorPositionChanged()),
            this, SLOT(cursorPositionChange()));
}

PythonMatchingChars::~PythonMatchingChars()
{
}


void PythonMatchingChars::cursorPositionChange()
{

    QTextCharFormat format;
    format.setForeground(QColor(QLatin1String("#f43218")));
    format.setBackground(QColor(QLatin1String("#f9c7f6")));

    // clear old highlights
    QList<QTextEdit::ExtraSelection> selections = m_editor->extraSelections();
    for (int i = 0; i < selections.size(); ++i) {
        if (selections[i].format == format) {
            selections.removeAt(i);
            --i;
        }
    }
    m_editor->setExtraSelections(selections);


    // start to search
    QTextCursor cursor = m_editor->textCursor();

    // init current blok userdata
    QTextBlock currentBlock = cursor.block();
    if (!currentBlock.isValid())
        return;

    QTextBlockUserData *rawTextData = currentBlock.userData();
    if (!rawTextData)
        return;
    PythonTextBlockData *textData = dynamic_cast<PythonTextBlockData*>(rawTextData);
    if (!textData)
        return;

    // query tokens
    int linePos = cursor.position() - currentBlock.position();
    const Python::Token *tokenObj = nullptr,
                      *triggerObj = nullptr;

    static const QList<Python::Token::Type> openTokens = {
            Python::Token::T_DelimiterOpenBrace,
            Python::Token::T_DelimiterOpenBracket,
            Python::Token::Type::T_DelimiterOpenParen
    };

    static const QList<Python::Token::Type> closeTokens = {
            Python::Token::T_DelimiterCloseBrace,
            Python::Token::T_DelimiterCloseBracket,
            Python::Token::T_DelimiterCloseParen
    };

    bool forward = false;
    if (textData->isMatchAt(linePos, openTokens)) {
        // look in front as in cursor here ->#(blah)
        triggerObj = textData->tokenAt(linePos);
        forward = true;
    } else if (textData->isMatchAt(linePos -1, closeTokens)) {
        // search the character before as in (blah)#<- cursor here should look behind
        linePos -= 1;
        triggerObj = textData->tokenAt(linePos);
        forward = false;
    }

    if (!triggerObj)
        return;

    Python::Token::Type token1 = triggerObj->type();
    Python::Token::Type token2 = Python::Token::T_Invalid;

    // no token here, nothing to do
    if (token1 == Python::Token::T_Invalid)
        return;

    // current pos in doc and in line
    int pos1 = cursor.block().position() + linePos;
    //int startPos = forward ? linePos +1 : -(linePos -1);// search backwards

    // which token?
    int pos2 = -1;
    switch (token1) {
    case Python::Token::T_DelimiterOpenParen:
        token2 = Python::Token::T_DelimiterCloseParen;
        break;
    case Python::Token::T_DelimiterOpenBracket:
        token2 = Python::Token::T_DelimiterCloseBracket;
        break;
    case Python::Token::T_DelimiterOpenBrace:
        token2 = Python::Token::T_DelimiterCloseBrace;
        break;
    case Python::Token::T_DelimiterCloseBrace:
        token2 = Python::Token::T_DelimiterOpenBrace;
        break;
    case Python::Token::T_DelimiterCloseBracket:
        token2 = Python::Token::T_DelimiterOpenBracket;
        break;
    case Python::Token::T_DelimiterCloseParen:
        token2 = Python::Token::T_DelimiterOpenParen;
        break;
    default:
        return;
    }

    // if we got here we should find a matching char
    // search row by row
    int innerCount = 0;
    bool startLook = false;

    tokenObj = triggerObj;
    uint guard = tokenObj->ownerList()->max_size();

    while (tokenObj && (--guard)) {
        if (tokenObj == triggerObj)
            startLook = true;
        else if (startLook) {
            // same type as trigger
            if (tokenObj->type() == token1)
                ++innerCount;
            // the mirror of trigger
            else if (tokenObj->type() == token2)
                --innerCount;
            if (innerCount < 0 && tokenObj->type() == token2) {
                // found it!
                PythonTextBlockData *pyBlock =
                        dynamic_cast<PythonTextBlockData*>(tokenObj->ownerLine());
                if (!pyBlock)
                    return;
                pos2 = tokenObj->startPosInt() + pyBlock->block().position();
                break; // finsihed! set extraformat
            }
        }

        if (forward)
            tokenObj = tokenObj->next();
        else
            tokenObj = tokenObj->previous();
    }

    // highlight both
    PythonEditor *pyEdit = qobject_cast<PythonEditor*>(m_editor);
    if (pyEdit) {
        pyEdit->highlightText(pos1, 1, format);
        pyEdit->highlightText(pos2, 1, format);
        return;
    }

    // its a console
    PythonConsoleTextEdit *consoleEdit = qobject_cast<PythonConsoleTextEdit*>(m_editor);
    if (consoleEdit) {
        consoleEdit->highlightText(pos1, 1, format);
        consoleEdit->highlightText(pos2, 1, format);
    }
}

// ------------------------------------------------------------------------

PyExceptionInfoGui::PyExceptionInfoGui(Base::PyExceptionInfo *exc) :
    m_exc(exc)
{
}

PyExceptionInfoGui::~PyExceptionInfoGui()
{
}

const char *PyExceptionInfoGui::iconName() const
{
    const char *iconStr = "exception";

    if (m_exc->isIndentationError())
        iconStr = "indentation-error";
    else if (m_exc->isSyntaxError())
        iconStr = "syntax-error";
    else if (m_exc->isWarning())
        iconStr = "warning";
    return iconStr;
}


#include "moc_PythonSyntaxHighlighter.cpp"
