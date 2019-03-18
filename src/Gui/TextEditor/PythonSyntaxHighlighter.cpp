
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
#include <PythonSource/PythonSourceDebugTools.h>
#endif

#include "PythonEditor.h"
#include <Gui/PythonConsole.h>

using namespace Gui;


static const int ParamLineShiftPos = 17,
                 ParenCntShiftPos  = 28;
static const int TokensMASK      = 0x0000FFFF,
                 ParamLineMASK   = 0x00010000, // when we are whithin a function parameter line
                 ParenCntMASK    = 0xF0000000, // how many parens we have open from previous block
                 PreventTokenize = 0x00020000; // prevents this call from running tokenize, used to repaint this block


namespace Gui {
class PythonSyntaxHighlighterP
{
public:
    explicit PythonSyntaxHighlighterP():
        endStateOfLastPara(PythonSyntaxHighlighter::T_Undetermined),
        activeBlock(nullptr),
        srcScanBlock(nullptr)
    {

        { // GIL lock code block
            Base::PyGILStateLocker lock;

            PyObject *pyObj = PyEval_GetBuiltins();
            PyObject *key, *value;
            Py_ssize_t pos = 0;

            while (PyDict_Next(pyObj, &pos, &key, &value)) {

#if PY_MAJOR_VERSION >= 3
                wchar_t *name = PyUnicode_AsWideCharString(key, NULL);
                if (name != nullptr)
                    builtins << QString::fromWCharArray(name);
#else
                char *name = PyString_AS_STRING(key);
                if (name != nullptr)
                    builtins << QString(QLatin1String(name));
#endif
            }
        } // end GIL lock code block

        // https://docs.python.org/3/reference/lexical_analysis.html#keywords
        keywords << QLatin1String("False")    << QLatin1String("None")
                 << QLatin1String("True")     << QLatin1String("and")
                 << QLatin1String("as")       << QLatin1String("assert")
                 << QLatin1String("async")    << QLatin1String("await") //2 new kewords from 3.6
                 << QLatin1String("break")    << QLatin1String("class")
                 << QLatin1String("continue") << QLatin1String("def")
                 << QLatin1String("del")      << QLatin1String("elif")
                 << QLatin1String("else")     << QLatin1String("except")
                 << QLatin1String("finally")  << QLatin1String("for")
                 << QLatin1String("from")     << QLatin1String("global")
                 << QLatin1String("if")       << QLatin1String("import")
                 << QLatin1String("in")       << QLatin1String("is")
                 << QLatin1String("lambda")   << QLatin1String("nonlocal")
                 << QLatin1String("not")      << QLatin1String("or")
                 << QLatin1String("pass")     << QLatin1String("raise")
                 << QLatin1String("return")   << QLatin1String("try")
                 << QLatin1String("while")    << QLatin1String("with")
                 << QLatin1String("yield");

        // keywords takes precedence over builtins
        for (QString name : keywords) {
            int pos = builtins.indexOf(name);
            if (pos > -1)
                builtins.removeAt(pos);
        }

        if (!builtins.contains(QLatin1String("print")))
            keywords << QLatin1String("print"); // python 2.7 and below
    }
    ~PythonSyntaxHighlighterP() {}

    QStringList keywords;
    QStringList builtins;
    PythonSyntaxHighlighter::Tokens endStateOfLastPara;
    PythonTextBlockData *activeBlock,
                        *srcScanBlock;
    QString filePath;
    QTimer sourceScanTmr;


};


} // namespace Gui


// ----------------------------------------------------------------------

/**
 * Constructs a Python syntax highlighter.
 */
PythonSyntaxHighlighter::PythonSyntaxHighlighter(QObject* parent)
    : SyntaxHighlighter(parent)
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
void PythonSyntaxHighlighter::highlightBlock (const QString & text)
{
    int prevState = previousBlockState() != -1 ? previousBlockState() : 0; // is -1 when no state is set
    d->endStateOfLastPara = static_cast<PythonSyntaxHighlighter::Tokens>(prevState & TokensMASK);
    if (d->endStateOfLastPara < T_Undetermined)
        d->endStateOfLastPara = T_Undetermined;

    int curState = currentBlockState();
    if (curState != -1 &&  curState & PreventTokenize) {
        // only paint this block, already tokenized
        QTextBlockUserData *userBlock = currentBlockUserData();
        if (userBlock) {
            d->activeBlock = dynamic_cast<PythonTextBlockData*>(userBlock);
            if (!d->activeBlock)
                return;
            // clear prevent tokenize flag
            curState &= ~PreventTokenize;
            setCurrentBlockState(curState);

            // set reformats again
            for ( const PythonToken *tok : d->activeBlock->tokens()) {
                QTextCharFormat format;
                if (d->activeBlock->allReformats().keys().contains(tok)) {
                    format = d->activeBlock->allReformats()[tok];
                } else {
                    format = getFormatToken(tok);
                }

                PythonTextBlockScanInfo *scanInfo = tok->txtBlock()->scanInfo();
                if (scanInfo) {
                    const PythonTextBlockScanInfo::ParseMsg *parseMsg = scanInfo->getParseMessage(tok);
                    if (parseMsg) {
                        switch (parseMsg->type) {
                        case PythonTextBlockScanInfo::IndentError:
                            format.setUnderlineColor(QColor(244, 143, 66));
                            format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
                            break;
                        case PythonTextBlockScanInfo::SyntaxError:
                            format.setUnderlineColor(colorByType(SyntaxError));
                            format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
                            break;
                        case PythonTextBlockScanInfo::Message:
                            format.setUnderlineColor(QColor(210, 247, 64));
                            format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
                            break;
                        default: break; // nothing
                        }
                    }
                }

                // set the format
                setFormat(tok->startPos, tok->endPos - tok->startPos, format);
            }

            d->activeBlock->allReformats().clear();

            /*
            // iterate through all formats
            auto formats = d->activeBlock->allReformats();
            for (const PythonToken *tok : formats.keys()) {
                if (tok->txtBlock()->scanInfo())
                    qDebug() << tok->line() << QLatin1String(" ") <<
                            &formats[tok] <<
                            QLatin1String(" ") << tok->txtBlock()->scanInfo()->parseMessage(tok) <<  endl;
                QTextCharFormat format;
                format.setUnderlineColor(QColor(3,45,240));
                format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
                format.setFontUnderline(true);
                setFormat(tok->startPos, tok->endPos -tok->startPos, format);
                //setFormatToken(tok);
                //setFormat(tok->startPos, tok->endPos - tok->startPos, formats[tok]);
            }
            formats.clear();
            */
        }
    } else {
        // Normally we end up here

        d->sourceScanTmr.stop(); // schedule source scanner for this row

        // create new userData
        d->activeBlock = new PythonTextBlockData(currentBlock());
        setCurrentBlockUserData(d->activeBlock);

        int parenCnt = (prevState & ParenCntMASK) >> ParenCntShiftPos;
        bool isParamLine  = prevState & ParamLineMASK;

        // scans this line
        int i = tokenize(text);

        // Insert new line token
        if (d->activeBlock->block().next().isValid())
            d->activeBlock->setDeterminedToken(T_DelimiterNewLine, i, 0);


        prevState = static_cast<int>(d->endStateOfLastPara);
        prevState |= parenCnt << ParenCntShiftPos;
        prevState |= (int)isParamLine << ParamLineShiftPos;
        setCurrentBlockState(prevState);
        d->srcScanBlock = d->activeBlock;
        d->activeBlock = nullptr;

        d->sourceScanTmr.start();
    }

}

int PythonSyntaxHighlighter::tokenize(const QString &text)
{
    int prevState = previousBlockState() != -1 ? previousBlockState() : 0; // is -1 when no state is set
    bool isModuleLine = false;
    bool isParamLine  = prevState & ParamLineMASK;
    int prefixLen = 0;
    int parenCnt = (prevState & ParenCntMASK) >> ParenCntShiftPos;

    int i = 0, lastI = 0;
    QChar prev, ch;

    while ( i < text.length() )
    {
        ch = text.at( i );

        switch ( d->endStateOfLastPara )
        {
        case T_Undetermined:
        {
            char thisCh = ch.unicode(),
                 nextCh = 0, thirdCh = 0;
            if (text.length() > i+1)
                nextCh = text.at(i+1).unicode();
            if (text.length() > i+2)
                thirdCh = text.at(i+2).unicode();

            switch (thisCh)
            {
            // all these chars are described at: https://docs.python.org/3/reference/lexical_analysis.html
            case '#':
                // begin a comment
                // it goes on to the end of the row
                setRestOfLine(i, text, Tokens::T_Comment); // function advances i
                break;
            case '"': case '\'':
            {
                // Begin either string literal or block string
                char compare = thisCh;
                QString endMarker(ch);
                int startStrPos;
                Tokens token = T_Undetermined;
                int len;

                if (nextCh == compare && thirdCh == compare) {
                    // block string
                    endMarker += endMarker + endMarker; // makes """ or '''
                    startStrPos = i + 3;
                    if (compare == '"') {
                        token = T_LiteralBlockDblQuote;
                        len = lastDblQuoteStringCh(startStrPos, text);
                    } else {
                        token = T_LiteralBlockSglQuote;
                        len = lastSglQuoteStringCh(startStrPos, text);
                    }
                    d->endStateOfLastPara = token;
                }

                // just a " or ' quoted string
                if (token == T_Undetermined) {
                    startStrPos = i + 1;
                    if (compare == '"') {
                        token = T_LiteralDblQuote;
                        len = lastDblQuoteStringCh(startStrPos, text);
                    } else {
                        token = T_LiteralSglQuote;
                        len = lastSglQuoteStringCh(startStrPos, text);
                    }
                }

                // search for end of string including endMarker
                int endPos = text.indexOf(endMarker, startStrPos + len);
                if (endPos != -1) {
                    len = (endPos - i) + prefixLen;
                    d->endStateOfLastPara = T_Undetermined;
                }

                // a string might have string prefix, such as r"" or u''
                // detection of that was in a previous iteration
                i -= prefixLen;

                setLiteral(i, len + endMarker.length(), token);
                prefixLen = 0;

            } break;

            // handle indentation
            case ' ': case '\t':
            {
                if (d->activeBlock->isEmpty()) {

                    int count = 0, j = i;

                    for (; j < text.length(); ++j) {
                        if (text.at(j) == QLatin1Char(' ')) {
                            ++count;
                        } else if (text.at(j) == QLatin1Char('\t')) {
                            count += 8; // according to python lexical documentaion tab is eight spaces
                        } else {
                            break;
                        }
                    }
                    setIndentation(i, j, count);
                    break;
                }

            } break;

                // operators or delimiters
                // These are the allowed operators
                // arithmetic
                //  +       -       *       **      /       //      %      @

                // bitwise
                //  <<      >>      &       |       ^       ~

                // assign
                // =       +=      -=      *=      /=       %=
                // //=     **=     @=

                // compare
                //  <       >       <=      >=      ==      !=

                // assign bitwise
                // &=      |=      ^=      >>=     <<=      ~=

                // These are the allowed Delimiters
                // (       )       [       ]       {       }
                // ,       :       .       ;       ->     ...


            // handle all with possibly 3 chrs
            case '*':
                if (isParamLine) {
                    // specialcase * as it may be a *arg or **arg
                    if (nextCh == '*') {
                        setWord(i, 2, T_OperatorKeyWordParam);
                        break;
                    }
                    setWord(i, 1, T_OperatorVariableParam);
                    break;
                } else if (isModuleLine) {
                    // specialcase * as it is also used as glob for modules
                    i -= 1;
                    d->endStateOfLastPara = T_IdentifierModuleGlob;
                    break;
                } else if (nextCh == '*') {
                    if (thirdCh == '=')
                        setOperator(i, 3, T_OperatorExpoEqual);
                    else
                        setOperator(i, 2, T_OperatorExponential);
                } else if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorMulEqual);
                } else {
                    setOperator(i, 1, T_OperatorMul);
                }
                 break;
            case '/':
                if (nextCh == '/') {
                    if (thirdCh == '=') {
                        setOperator(i, 3, T_OperatorFloorDivEqual);
                    } else {
                        setOperator(i, 2, T_OperatorFloorDiv);
                    }
                } else if(nextCh == '=') {
                    setOperator(i, 2, T_OperatorDivEqual);
                    break;
                } else {
                    setOperator(i, 1, T_OperatorDiv);
                }
                break;
            case '>':
            {
                if (nextCh == thisCh) {
                    if(thirdCh == '=') {
                        setOperator(i, 3, T_OperatorBitShiftRightEqual);
                        break;
                    }
                    setOperator(i, 2, T_OperatorBitShiftRight);
                    break;
                } else if(nextCh == '=') {
                    setOperator(i, 2, T_OperatorMoreEqual);
                    break;
                }
                // no just single * or <
                setOperator(i, 1, T_OperatorMore);
            } break;
            case '<': // might be 3 chars ie <<=
            {
                if (nextCh == thisCh) {
                    if(thirdCh == '=') {
                        setOperator(i, 3, T_OperatorBitShiftLeftEqual);
                        break;
                    }
                    setOperator(i, 2, T_OperatorBitShiftLeft);
                    break;
                } else if(nextCh == '=') {
                    setOperator(i, 2, T_OperatorLessEqual);
                    break;
                }
                // no just single <
                setOperator(i, 1, T_OperatorLess);
            } break;

            // handle all left with possibly 2 chars only
            case '-':
                if (nextCh == '>') {
                    // -> is allowed function return type documentation
                    setDelimiter(i, 2, T_DelimiterMetaData);
                    break;
                }
                setOperator(i, 1, T_OperatorMinus);
                break;
            case '+':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorPlusEqual);
                    break;
                }
                setOperator(i, 1, T_OperatorPlus);
                break;
            case '%':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorModuloEqual);
                    break;
                }
                setOperator(i, 1, T_OperatorModulo);
                break;
            case '&':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorBitAndEqual);
                    break;
                }
                setOperator(i, 1, T_OperatorBitAnd);
                break;
            case '^':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorBitXorEqual);
                    break;
                }
                setOperator(i, 1, T_OperatorBitXor);
                break;
            case '|':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorBitOrEqual);
                    break;
                }
                setOperator(i, 1, T_OperatorOr);
                break;
            case '=':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorCompareEqual);
                    break;
                }
                setOperator(i, 1, T_OperatorEqual);
                break;
            case '!':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorNotEqual);
                    break;
                }
                setOperator(i, 1, T_OperatorNot);
                break;
            case '~':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorBitNotEqual);
                    break;
                }
                setOperator(i, 1, T_OperatorBitNot);
                break;
            case '(':
                setDelimiter(i, 1, T_DelimiterOpenParen);
                if (isParamLine)
                    ++parenCnt;
                break;
            case '\r':
                if (nextCh == '\n') {
                    setDelimiter(i, 2, T_DelimiterNewLine);
                    break;
                }
                setDelimiter(i, 1, T_DelimiterNewLine);
                break;
            case '\n':
                setDelimiter(i, 1, T_DelimiterNewLine);
                break;
            case '[':
                setDelimiter(i, 1, T_DelimiterOpenBracket);
                break;
            case '{':
                setDelimiter(i, 1, T_DelimiterOpenBrace);
                break;
            case '}':
                setDelimiter(i, 1, T_DelimiterCloseBrace);
                break;
            case ')':
                setDelimiter(i, 1, T_DelimiterCloseParen);
                if (isParamLine) {
                    --parenCnt;
                    if (parenCnt == 0)
                        isParamLine = false;
                }
                break;
            case ']':
                setDelimiter(i, 1, T_DelimiterCloseBracket);
                break;
            case ',':
                setDelimiter(i, 1, T_DelimiterComma);
                break;
            case '.':
                if (nextCh == '.' && thirdCh == '.') {
                    setDelimiter(i, 3, T_DelimiterEllipsis);
                    break;
                }
                setDelimiter(i, 1, T_DelimiterPeriod);
                break;
            case ';':
                isModuleLine = false;
                setDelimiter(i, 1, T_DelimiterSemiColon);
                break;
            case ':':
                setDelimiter(i, 1, T_DelimiterColon);
                break;
            case '@':
            {   // decorator or matrix add
                if(nextCh && text.at(i+1).isLetter()) {
                    int len = lastWordCh(i + 1, text);
                    setWord(i, len +1, T_IdentifierDecorator);
                    break;
                } else if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorMatrixMulEqual);
                    break;
                }

                 setDelimiter(i, 1, T_Delimiter);
            } break;
            // illegal chars
            case '$': case '?': case '`':
                setSyntaxError(i, 1);
                break;
            // text or number?
            default:
            {
                // check for string prefixes
                if (nextCh == '"' || nextCh == '\'') {
                    thisCh = tolower(thisCh);

                    if (thisCh == 'r' || thisCh == 'b' ||
                        thisCh == 'f' || thisCh == 'u')
                    {
                        prefixLen = 1;
                        break; // let string start code handle it
                    }

                } else if (thirdCh == '"' || thirdCh == '\'') {
                    QString twoCh = text.mid(i, 2).toLower();

                    if (twoCh == QLatin1String("fr") || twoCh == QLatin1String("rf") ||
                        twoCh == QLatin1String("br") || twoCh == QLatin1String("rb"))
                    {
                        prefixLen = 2;
                        i += 1;
                        break; // let string start code handle it
                    }
                }

                // Check for normal text
                if ( ch.isLetter() || ch == QLatin1Char('_') )
                {
                    int len = lastWordCh(i, text);
                    QString word = text.mid(i, len);

                    if ( d->keywords.contains( word ) != 0 ) {
                        if (word == QLatin1String("def")) {
                            isParamLine = true;
                            setWord(i, len, Tokens::T_KeywordDef);
                            d->endStateOfLastPara = T_IdentifierDefUnknown; // step out to handle def name

                        } else if (word == QLatin1String("class")) {
                            setWord(i, len, Tokens::T_KeywordClass);
                            d->endStateOfLastPara = T_IdentifierClass; // step out to handle class name

                        } else if (word == QLatin1String("import")) {
                            setWord(i, len, Tokens::T_KeywordImport);
                            d->endStateOfLastPara = T_IdentifierModule; // step out to handle module name
                            isModuleLine = true;

                        } else if (word == QLatin1String("from")) {
                            setWord(i, len, Tokens::T_KeywordFrom);
                            d->endStateOfLastPara = T_IdentifierModulePackage; // step out handle module name
                            isModuleLine = true;

                        } else if (word == QLatin1String("and")) {
                            setWord(i, len, T_OperatorAnd);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("as")) {
                            setWord(i, len, T_KeywordAs);
                            if (isModuleLine)
                                d->endStateOfLastPara = T_IdentifierModuleAlias;
                            else
                                d->endStateOfLastPara = T_Undetermined;
                        } else if (word == QLatin1String("in")) {
                            setWord(i, len, T_OperatorIn);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("is")) {
                            setWord(i, len, T_OperatorIs);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("not")) {
                            setWord(i, len, T_OperatorNot);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("or")) {
                            setWord(i, len, T_OperatorOr);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("yield")) {
                            setWord(i, len, T_KeywordYield);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("return")) {
                            setWord(i, len, T_KeywordReturn);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("True")) {
                            setIdentifier(i, len, T_IdentifierTrue);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("False")) {
                            setIdentifier(i, len, T_IdentifierFalse);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("None")) {
                            setIdentifier(i, len, T_IdentifierNone);
                            d->endStateOfLastPara = T_Undetermined;

                        }  else {
                            setWord(i, len, Tokens::T_Keyword);
                        }

                    } else if(d->builtins.contains(word) &&
                              !d->activeBlock->lastInsertedIsA(T_DelimiterPeriod))
                    {
                        // avoid match against someObj.print
                        setWord(i, len, Tokens::T_IdentifierBuiltin);
                    } else if (isModuleLine) {
                        i -= 1; // jump to module block
                        d->endStateOfLastPara = T_IdentifierModule;
                    } else {
                        setUndeterminedIdentifier(i, len, T_IdentifierUnknown);
                    }

                    break;
                    //i += word.length(); setWord increments
                }

                // is it the beginning of a number?
                int len = lastNumberCh(i, text);
                if ( len > 0) {
                    setNumber(i, len, numberType(text.mid(i, len)));
                    break;
                } else if (thisCh == '\\' && i == text.length() - 1)
                {
                    setDelimiter(i, 1, T_DelimiterLineContinue);
                    break;
                }
                // its a error if we ever get here
                setSyntaxError(i, 1);


                qDebug() << "PythonSyntaxHighlighter error on char: " << ch <<
                          " row:"<< QString::number(currentBlock().blockNumber() + 1) <<
                          " col:" << QString::number(i) << endl;

            } // end default:
            } // end switch

        } break;// end T_Undetermined

        // we should only arrive here when we start on a new line
        case T_LiteralBlockDblQuote:
        {
            // multiline double quote string continued from previous line
            int endPos = text.indexOf(QLatin1String("\"\"\""), i);
            if (endPos != -1) {
                endPos += 3;
                setLiteral(i, endPos -i, T_LiteralBlockDblQuote);
                d->endStateOfLastPara = T_Undetermined;
            } else {
                setRestOfLine(i, text, T_LiteralBlockDblQuote);
            }

        } break;
        case T_LiteralBlockSglQuote:
        {
            // multiline single quote string continued from previous line
            int endPos = text.indexOf(QLatin1String("'''"), i);
            if (endPos != -1) {
                endPos += 3;
                setLiteral(i, endPos -i, T_LiteralBlockSglQuote);
                d->endStateOfLastPara = T_Undetermined;
            }else {
                setRestOfLine(i, text, T_LiteralBlockSglQuote);
            }

        } break;
        case T_IdentifierDefUnknown:
        {   // before we now if its a class member or a function
            for (; i < text.length(); ++i) {
                if (!text.at(i).isSpace())
                    break;
            }
            if (i == text.length()) {
                d->endStateOfLastPara = T_Undetermined;
                break; // not yet finished this line?
            }

            int len = lastWordCh(i, text);
            if (d->activeBlock->indent() == 0)
                // no indent, it can't be a method
                setIdentifier(i, len, T_IdentifierFunction);
            else
                setUndeterminedIdentifier(i, len, T_IdentifierDefUnknown);
            d->endStateOfLastPara = T_Undetermined;

        } break;
        case T_IdentifierClass:
        {
            for (; i < text.length(); ++i) {
                if (!text.at(i).isSpace())
                    break;
            }
            if (i == text.length()) {
                d->endStateOfLastPara = T_Undetermined;
                break; // not yet finished this line?
            }

            int len = lastWordCh(i, text);
            setIdentifier(i, len, T_IdentifierClass);
            d->endStateOfLastPara = T_Undetermined;

        } break;
        case T_IdentifierModuleAlias: case T_IdentifierModuleGlob:
        case T_IdentifierModule: case T_IdentifierModulePackage:
        {
            // can import multiple modules separated  with ',' so we jump here between here and T_Undertermined
            for (; i < text.length(); ++i) {
                if (!text.at(i).isSpace())
                    break;
            }
            if (i == text.length()) {
                d->endStateOfLastPara = T_Undetermined;
                break; // not yet finished this line?
            }

            int len = lastWordCh(i, text);
            if (len < 1) {
                if (ch == QLatin1Char('*')) // globs aren't a word char so they don't get caught
                    len += 1;
                else
                    break;
            }

            setIdentifier(i, len, d->endStateOfLastPara);
            d->endStateOfLastPara = T_Undetermined;

        } break;
        default:
        {
            // pythonCosole has some special values
            if (d->endStateOfLastPara == T_PythonConsoleError ||
                d->endStateOfLastPara == T_PythonConsoleOutput)
            {
                TColor color = d->endStateOfLastPara == T_PythonConsoleOutput ?
                                                            PythonConsoleOutput :
                                                                PythonConsoleError;
                if (i == 0 && text.length() >= 4) {
                    if (text.left(4) == QLatin1String(">>> ") ||
                        text.left(4) == QLatin1String("... "))
                    {
                        setFormat(0, 4, colorByType(color));
                        d->endStateOfLastPara = T_Undetermined;
                        i += 3;
                    }
                }
                break;
            }

            setSyntaxError(i, lastWordCh(i, text));
            qDebug() << "PythonsytaxHighlighter unknown state for: " << qPrintable(ch) <<
                        " row:"<< QString::number(currentBlock().blockNumber() + 1) <<
                        " col:" << QString::number(i) << endl;
        }
        } // end switch

        prev = ch;
        assert(i >= lastI); // else infinite loop
        lastI = i++;

    } // end loop


    // only block comments can have several lines
    if ( d->endStateOfLastPara != T_LiteralBlockDblQuote &&
         d->endStateOfLastPara != T_LiteralBlockSglQuote )
    {
        d->endStateOfLastPara = T_Undetermined ;
    }

    return i;
}


QTextCharFormat PythonSyntaxHighlighter::getFormatToken(const PythonToken *token) const
{
    assert(token != nullptr && "Need a valid pointer");

    QTextCharFormat format;
    SyntaxHighlighter::TColor colorIdx = SyntaxHighlighter::Text ;
    switch (token->token) {
    // all specialcases chaught by switch, rest in default
    case T_Comment:
        colorIdx = SyntaxHighlighter::Comment; break;
    case T_SyntaxError:
        colorIdx = SyntaxHighlighter::SyntaxError; break;
    case T_IndentError:
        colorIdx = SyntaxHighlighter::SyntaxError;
        format.setUnderlineStyle(QTextCharFormat::QTextCharFormat::DotLine);
        format.setUnderlineColor(colorIdx);
        break;

    // numbers
    case T_NumberHex:
        colorIdx = SyntaxHighlighter::NumberHex; break;
    case T_NumberBinary:
        colorIdx = SyntaxHighlighter::NumberBinary; break;
    case T_NumberOctal:
        colorIdx = SyntaxHighlighter::NumberOctal; break;
    case T_NumberFloat:
        colorIdx = SyntaxHighlighter::NumberFloat; break;

    // strings
    case T_LiteralDblQuote:
        colorIdx = SyntaxHighlighter::StringDoubleQoute; break;
    case T_LiteralSglQuote:
        colorIdx = SyntaxHighlighter::StringSingleQoute; break;
    case T_LiteralBlockDblQuote:
        colorIdx = SyntaxHighlighter::StringBlockDoubleQoute; break;
    case T_LiteralBlockSglQuote:
        colorIdx = SyntaxHighlighter::StringBlockSingleQoute; break;

    // keywords
    case T_KeywordClass:
        colorIdx = SyntaxHighlighter::KeywordClass;
        format.setFontWeight(QFont::Bold);
        break;
    case T_KeywordDef:
        colorIdx = SyntaxHighlighter::KeywordDef;
        format.setFontWeight(QFont::Bold);
        break;

    // operators
    case T_OperatorVariableParam: // fallthrough
    case T_OperatorKeyWordParam:
        colorIdx = SyntaxHighlighter::Text; break;

    // delimiters
    case T_DelimiterLineContinue:
        colorIdx = SyntaxHighlighter::Text; break;

    // identifiers
    case T_IdentifierDefined:
        colorIdx = SyntaxHighlighter::IdentifierDefined; break;

    case T_IdentifierModuleAlias:
    case T_IdentifierModulePackage:// fallthrough
    case T_IdentifierModule:
        colorIdx = SyntaxHighlighter::IdentifierModule; break;
    case T_IdentifierModuleGlob:
        colorIdx = SyntaxHighlighter::Text; break;
    case T_IdentifierFunction:
        colorIdx = SyntaxHighlighter::IdentifierFunction;
        format.setFontWeight(QFont::Bold);
        break;
    case T_IdentifierMethod:
        colorIdx = SyntaxHighlighter::IdentifierMethod;
        format.setFontWeight(QFont::Bold);
        break;
    case T_IdentifierClass:
        colorIdx = SyntaxHighlighter::IdentifierClass;
        format.setFontWeight(QFont::Bold);
        break;
    case T_IdentifierSuperMethod:
        colorIdx = SyntaxHighlighter::IdentifierSuperMethod;
        format.setFontWeight(QFont::Bold);
        break;
    case T_IdentifierBuiltin:
        colorIdx = SyntaxHighlighter::IdentifierBuiltin;
        format.setFontWeight(QFont::Bold);
        break;
    case T_IdentifierDecorator:
        colorIdx = SyntaxHighlighter::IdentifierDecorator;
        format.setFontWeight(QFont::Bold);
        break;
    case T_IdentifierDefUnknown:
        colorIdx = SyntaxHighlighter::IdentifierDefUnknown; break;
    case T_IdentifierSelf:
        format.setFontWeight(QFont::Bold);
        colorIdx = SyntaxHighlighter::IdentifierSelf; break;


    case T_MetaData:
        colorIdx = SyntaxHighlighter::MetaData; break;
    default:
        // all operators Whichh arent specialcased above
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

    format.setForeground(this->colorByType(colorIdx));

    return format;
}

void PythonSyntaxHighlighter::setFormatToken(const PythonToken *tok)
{
    int pos = tok->startPos;
    int len = tok->endPos - pos;
    setFormat(pos, len, getFormatToken(tok));
}

void PythonSyntaxHighlighter::sourceScanTmrCallback() {
    if (d->srcScanBlock)
        PythonSourceRoot::instance()->scanSingleRowModule(d->filePath, d->srcScanBlock, this);
}

void PythonSyntaxHighlighter::setFilePath(QString filePath)
{
    d->filePath = filePath;

    PythonSourceRoot::instance()->scanCompleteModule(filePath, this);
#ifdef BUILD_PYTHON_DEBUGTOOLS
        {
            DumpSyntaxTokens dump(document()->begin());
            DumpModule dMod(PythonSourceRoot::instance()->moduleFromPath(d->filePath));
        }
#endif
}

QString PythonSyntaxHighlighter::filePath() const
{
    return d->filePath;
}

int PythonSyntaxHighlighter::lastWordCh(int startPos, const QString &text) const
{
    int end = startPos;
    int maxPos = text.length();
    for (;end < maxPos; ++end) {
        QChar ch = text[end];
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('_'))
            break;
    }
    return end - startPos;
}

int PythonSyntaxHighlighter::lastNumberCh(int startPos, const QString &text) const
{
    int len = startPos;
    int maxPos = text.length();
    ushort first = text[len].toLower().unicode();
    for (;len < maxPos; ++len) {
        ushort ch = text[len].toLower().unicode();
        if (ch >= '0' && ch <= '9')
            continue;
        if (ch >= 'a' && ch <= 'f')
            continue;
        if (ch == '.')
            continue;
        // hex, binary, octal
        if ((ch == 'x' || ch == 'b' || ch == 'o') &&  first == '0')
            continue;
        break;
    }
    // long integer or imaginary?
    if (len == maxPos -1) {
        QChar ch = text[len].toUpper();
        if (ch == QLatin1Char('L') || ch == QLatin1Char('J'))
            len += 1;

    }

    return len - startPos;
}

int PythonSyntaxHighlighter::lastDblQuoteStringCh(int startAt, const QString &text) const
{
    if (text.length() <= startAt)
        return text.length() -1;

    int len = startAt;
    int maxLen = text.length();

    for (; len < maxLen; ++len) {
        if (text.at(len) == QLatin1Char('\\')) {
            ++len;
            continue;
        }
        if (text.at(len) == QLatin1Char('"'))
            break;
    }
    return len - startAt;
}

int PythonSyntaxHighlighter::lastSglQuoteStringCh(int startAt, const QString &text) const
{
    // no escapes in this type
    if (text.length() <= startAt)
        return text.length() -1;

    int len = startAt;
    int maxLen = text.length();

    for (; len < maxLen; ++len) {
        if (text.at(len) == QLatin1Char('\''))
            break;
    }
    return len - startAt;

}

PythonSyntaxHighlighter::Tokens PythonSyntaxHighlighter::numberType(const QString &text) const
{
    if (text.isEmpty())
        return T_Invalid;
    if (text.indexOf(QLatin1String(".")) != -1)
        return T_NumberFloat;
    if (text.length() >= 2){
        QString two = text.toLower().left(2);
        if (two[0] == QLatin1Char('0')) {
            if (two[1] == QLatin1Char('x'))
                return T_NumberHex;
            if (two[1] == QLatin1Char('b'))
                return T_NumberBinary;
            if (two[1] == QLatin1Char('o') || two[0] == QLatin1Char('0'))
                return T_NumberOctal;
            return T_NumberOctal; // python 2 octal ie. 01234 != 1234
        }
    }
    return T_NumberDecimal;
}

void PythonSyntaxHighlighter::setRestOfLine(int &pos, const QString &text,
                                            PythonSyntaxHighlighter::Tokens token)
{
    int len = text.size() -pos;
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void PythonSyntaxHighlighter::setWord(int &pos, int len,
                                      PythonSyntaxHighlighter::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}


void PythonSyntaxHighlighter::setIdentifier(int &pos, int len, Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}


void PythonSyntaxHighlighter::setUndeterminedIdentifier(int &pos, int len,
                                                        Tokens token)
{
    // let parse tree figure out and color at a later stage
    setFormatToken(d->activeBlock->setUndeterminedToken(token, pos, len));
    pos += len -1;
}

void PythonSyntaxHighlighter::setNumber(int &pos, int len,
                                        PythonSyntaxHighlighter::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void PythonSyntaxHighlighter::setOperator(int &pos, int len,
                                          PythonSyntaxHighlighter::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void PythonSyntaxHighlighter::setDelimiter(int &pos, int len,
                                           PythonSyntaxHighlighter::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void PythonSyntaxHighlighter::setSyntaxError(int &pos, int len)
{
    setFormatToken(d->activeBlock->setDeterminedToken(T_SyntaxError, pos, len));
    pos += len -1;
}

void PythonSyntaxHighlighter::setLiteral(int &pos, int len,
                                         Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void PythonSyntaxHighlighter::setIndentation(int &pos, int len, int count)
{
    setFormatToken(d->activeBlock->setDeterminedToken(T_Indent, pos, len));
    d->activeBlock->setIndentCount(count);
    pos += len -1;
}


// --------------------------------------------------------------------------------------------


MatchingCharInfo::MatchingCharInfo():
    character(0), position(0)
{
}

MatchingCharInfo::MatchingCharInfo(const MatchingCharInfo &other)
{
    character = other.character;
    position = other.position;
}

MatchingCharInfo::MatchingCharInfo(char chr, int pos):
    character(chr), position(pos)
{
}

MatchingCharInfo::~MatchingCharInfo()
{
}

//static
char MatchingCharInfo::matchChar(char match)
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

char MatchingCharInfo::matchingChar() const
{
    return MatchingCharInfo::matchChar(character);
}

// -------------------------------------------------------------------------------------------

PythonToken::PythonToken(PythonSyntaxHighlighter::Tokens token, int startPos, int endPos,
                         Gui::PythonTextBlockData *block) :
    token(token), startPos(startPos), endPos(endPos),
    m_txtBlock(block)
{
}

PythonToken::~PythonToken()
{
    // notify our listeners
    for (PythonSourceListNodeBase *n : m_srcLstNodes)
        n->tokenDeleted();
}

/*
friend
bool PythonToken::operator >=(const PythonToken &lhs, const PythonToken &rhs)
{
    return lhs.line() >= rhs.line() &&
           lhs.startPos >= rhs.startPos;
}
*/

PythonToken *PythonToken::next() const
{
    PythonTextBlockData *txt = m_txtBlock;
    while (txt) {
        const PythonTextBlockData::tokens_t tokens = txt->tokens();
        if (txt == m_txtBlock) {
            int i = tokens.indexOf(const_cast<PythonToken*>(this));
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

PythonToken *PythonToken::previous() const
{
    PythonTextBlockData *txt = m_txtBlock;
    while (txt) {
        const PythonTextBlockData::tokens_t tokens = txt->tokens();
        if (txt == m_txtBlock) {
            int i = tokens.indexOf(const_cast<PythonToken*>(this));
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

QString PythonToken::text() const
{
    return m_txtBlock->tokenAtAsString(this);
}

int PythonToken::line() const
{
    return m_txtBlock->block().blockNumber();
}

bool PythonToken::isNumber() const { // is a number in src file
    return token > PythonSyntaxHighlighter::T__NumbersStart &&
           token < PythonSyntaxHighlighter::T__NumbersEnd;
}

bool PythonToken::isInt() const { // is a integer (dec) in src file
    return token > PythonSyntaxHighlighter::T__NumbersIntStart &&
           token <  PythonSyntaxHighlighter::T__NumbersIntEnd;
}

bool PythonToken::isFloat() const {
    return token == PythonSyntaxHighlighter::T_NumberFloat;
}

bool PythonToken::isString() const {
    return token > PythonSyntaxHighlighter::T__LiteralsStart &&
            token < PythonSyntaxHighlighter::T__LiteralsEnd;
}

bool PythonToken::isMultilineString() const
{
    return token > PythonSyntaxHighlighter::T__LiteralsMultilineStart &&
           token < PythonSyntaxHighlighter::T__LiteralsMultilineEnd;
}

bool PythonToken::isKeyword() const {
    return token > PythonSyntaxHighlighter::T__KeywordsStart &&
           token <  PythonSyntaxHighlighter::T__KeywordsEnd;
}

bool PythonToken::isOperator() const {
    return token > PythonSyntaxHighlighter::T__OperatorStart &&
           token <  PythonSyntaxHighlighter::T__OperatorEnd;
}

bool PythonToken::isOperatorArithmetic() const {
    return token > PythonSyntaxHighlighter::T__OperatorArithmeticStart &&
           token < PythonSyntaxHighlighter::T__OperatorArithmeticEnd;
}

bool PythonToken::isOperatorBitwise() const {
    return token > PythonSyntaxHighlighter::T__OperatorBitwiseStart &&
           token < PythonSyntaxHighlighter::T__OperatorBitwiseEnd;
}

bool PythonToken::isOperatorAssignment() const { // modifies lvalue
    return token > PythonSyntaxHighlighter::T__OperatorAssignmentStart &&
           token < PythonSyntaxHighlighter::T__OperatorAssignmentEnd;
}

bool PythonToken::isOperatorAssignmentBitwise() const { // modifies through bit twiddling
    return token > PythonSyntaxHighlighter::T__OperatorBitwiseStart &&
           token < PythonSyntaxHighlighter::T__OperatorBitwiseEnd;
}

bool PythonToken::isOperatorCompare() const {
    return token > PythonSyntaxHighlighter::T__OperatorAssignBitwiseStart &&
           token < PythonSyntaxHighlighter::T__OperatorAssignBitwiseEnd;
}

bool PythonToken::isOperatorCompareKeyword() const {
    return token > PythonSyntaxHighlighter::T__OperatorCompareKeywordStart &&
           token < PythonSyntaxHighlighter::T__OperatorCompareKeywordEnd;
}

bool PythonToken::isOperatorParam() const {
    return token > PythonSyntaxHighlighter::T__OperatorParamStart &&
           token <  PythonSyntaxHighlighter::T__OperatorParamEnd;
}

bool PythonToken::isDelimiter() const {
    return token > PythonSyntaxHighlighter::T__DelimiterStart &&
           token < PythonSyntaxHighlighter::T__DelimiterEnd;
}

bool PythonToken::isIdentifier() const {
    return token > PythonSyntaxHighlighter::T__IdentifierStart &&
           token < PythonSyntaxHighlighter::T__IdentifierEnd;
}

bool PythonToken::isIdentifierVariable() const {
    return token > PythonSyntaxHighlighter::T__IdentifierVariableStart &&
           token < PythonSyntaxHighlighter::T__IdentifierVariableEnd;
}

bool PythonToken::isIdentifierDeclaration() const {
    return token > PythonSyntaxHighlighter::T__IdentifierDeclarationStart &&
           token < PythonSyntaxHighlighter::T__IdentifierDeclarationEnd;
}

bool PythonToken::isNewLine() const
{
    if (token != PythonSyntaxHighlighter::T_DelimiterNewLine)
        return false;

    // else check for escape char
    const PythonToken *prev = previous();
    return prev && prev->token != PythonSyntaxHighlighter::T_DelimiterLineContinue;
}

bool PythonToken::isInValid() const  {
    return token == PythonSyntaxHighlighter::T_Invalid;
}

bool PythonToken::isUndetermined() const {
    return token == PythonSyntaxHighlighter::T_Undetermined;
}

bool PythonToken::isCode() const
{
    if (token < PythonSyntaxHighlighter::T__NumbersStart)
        return false;
    if (token > PythonSyntaxHighlighter::T__DelimiterTextLineStart &&
        token < PythonSyntaxHighlighter::T__DelimiterTextLineEnd)
    {
        return false;
    }
    if (token >= PythonSyntaxHighlighter::T_BlockStart &&
        token <= PythonSyntaxHighlighter::T_BlockEnd)
    {
        return false;
    }
    return true;
}

bool PythonToken::isImport() const
{
    return token > PythonSyntaxHighlighter::T__IdentifierImportStart &&
           token < PythonSyntaxHighlighter::T__IdentifierImportEnd;
}
bool PythonToken::isText() const
{
    return token == PythonSyntaxHighlighter::T_Comment || isCode();
}

void PythonToken::attachReference(PythonSourceListNodeBase *srcListNode)
{
    if (!m_srcLstNodes.contains(srcListNode))
        m_srcLstNodes.push_back(srcListNode);
}

void PythonToken::detachReference(PythonSourceListNodeBase *srcListNode)
{
    int idx = m_srcLstNodes.indexOf(srcListNode);
    if (idx > -1)
        m_srcLstNodes.removeAt(idx);
}

// -------------------------------------------------------------------------------------------

PythonTextBlockData::PythonTextBlockData(QTextBlock block) :
    TextEditBlockData(block),
    m_indentCharCount(0)
{
}

PythonTextBlockData::PythonTextBlockData(const PythonTextBlockData &other) :
    TextEditBlockData(other),
    m_tokens(other.m_tokens),
    m_undeterminedIndexes(other.m_undeterminedIndexes),
    m_reformats(other.m_reformats),
    m_indentCharCount(other.m_indentCharCount)
{
    // FIXME ! handle tokens cpoying mer gracefully
    // Should probably make a deep copy here
}

PythonTextBlockData::~PythonTextBlockData()
{
    qDeleteAll(m_tokens);
}

PythonTextBlockData *PythonTextBlockData::pyBlockDataFromCursor(const QTextCursor &cursor)
{
    return dynamic_cast<PythonTextBlockData*>(blockDataFromCursor(cursor));
}

const PythonTextBlockData::tokens_t &PythonTextBlockData::tokens() const
{
    return m_tokens;
}

PythonTextBlockData *PythonTextBlockData::next() const
{
    return dynamic_cast<PythonTextBlockData*>(TextEditBlockData::next());
}

PythonTextBlockData *PythonTextBlockData::previous() const
{
    return dynamic_cast<PythonTextBlockData*>(TextEditBlockData::previous());
}

const PythonToken *PythonTextBlockData::setDeterminedToken(PythonSyntaxHighlighter::Tokens token,
                                             int startPos, int len)
{
    PythonToken *tokenObj = new PythonToken(token, startPos, startPos + len, this);
    m_tokens.push_back(tokenObj);
    return tokenObj;
}

const PythonToken *PythonTextBlockData::setUndeterminedToken(PythonSyntaxHighlighter::Tokens token,
                                               int startPos, int len)
{
    const PythonToken *tokenObj = setDeterminedToken(token, startPos, len);

    // store this index so context parser can look it up at a later stage
    m_undeterminedIndexes.append(m_tokens.size() - 1);
    return tokenObj;
}

void PythonTextBlockData::setIndentCount(int count)
{
    m_indentCharCount = count;
}

bool PythonTextBlockData::setReformat(const PythonToken *tok, QTextCharFormat format)
{
    if (!m_tokens.contains(const_cast<PythonToken*>(tok)))
        return false;
    int state = m_block.userState();
    state |= PreventTokenize;
    m_block.setUserState(state);
    m_reformats.insertMulti(tok, format);
    return true;
}

const PythonToken *PythonTextBlockData::findToken(PythonSyntaxHighlighter::Tokens token,
                                                  int searchFrom) const
{
    int lineEnd = m_block.length();

    if (searchFrom > lineEnd)
        searchFrom = lineEnd;
    else if (searchFrom < -lineEnd)
        searchFrom = -lineEnd;

    if (searchFrom < 0) {
        // search reversed (from lineend)
        // bah reverse loop is a pain with Qt iterators...
        for (int i = m_tokens.size() -1; i >= 0; --i)
        {
            PythonToken *tok = m_tokens[i];
            if (tok->token == token &&
                searchFrom <= tok->startPos &&
                tok->endPos > searchFrom)
            {
                return tok;
            }
        }
    } else {
        // search from front (linestart)
        for (const PythonToken *tok : m_tokens) {
            if (tok->token == token &&
                searchFrom <= tok->startPos &&
                tok->endPos > searchFrom)
            {
                return tok;
            }
        }
    }

    return nullptr;
}

const PythonToken *PythonTextBlockData::firstTextToken() const
{
    for (PythonToken *tok : m_tokens) {
        if (tok->isText())
            return tok;
    }
    return nullptr;
}

const PythonToken *PythonTextBlockData::firstCodeToken() const
{
    for (PythonToken *tok : m_tokens) {
        if (tok->isCode())
            return tok;
    }
    return nullptr;
}

int PythonTextBlockData::tokensBetweenOfType(int startPos, int endPos,
                                             PythonSyntaxHighlighter::Tokens match) const
{
    int cnt = 0;
    if (startPos == endPos || m_tokens.isEmpty())
        return 0;

    int start, end;
    if (startPos < endPos) {
        start = startPos;
        end = endPos;
    } else {
        // endPos before startPos, reverse lookup
        start = endPos;
        end = startPos;
    }

    for (PythonToken *tok : m_tokens) {
        if (start <= tok->startPos &&
            end > tok->endPos &&
            match == tok->token)
        {
            ++cnt;
        }
    }

    return cnt;
}

bool PythonTextBlockData::lastInsertedIsA(PythonSyntaxHighlighter::Tokens match, bool lookInPreviousRows) const
{
    if (m_tokens.isEmpty() ||
        m_tokens[m_tokens.size() -1]->token == PythonSyntaxHighlighter::T_Indent)
    {
        if (lookInPreviousRows) {
            PythonToken *tok = lastInserted(true);
            if (tok)
                return tok->token == match;
        }
        return false;
    }
    //
    PythonToken *tok = lastInserted(lookInPreviousRows);
    if (tok)
        return tok->token == match;
    return false;
}

PythonToken *PythonTextBlockData::lastInserted(bool lookInPreviousRows) const
{
    if (m_tokens.isEmpty() ||
        m_tokens[m_tokens.size() -1]->token == PythonSyntaxHighlighter::T_Indent)
    {
        if (!lookInPreviousRows)
            return nullptr;

        QTextBlock block = m_block.previous();
        while (block.isValid()) {
            PythonTextBlockData *textData = reinterpret_cast<PythonTextBlockData*>(block.userData());
            if (textData)
                return textData->lastInserted(true);
            block = block.previous();
        }
        // not found
        return nullptr;
    }

    return m_tokens[m_tokens.size() -1];
}

PythonToken *PythonTextBlockData::tokenAt(int pos) const
{
    for (PythonToken *tok : m_tokens) {
        if (tok->startPos <= pos && tok->endPos > pos)
            return tok;
    }

    return nullptr;
}

// static
PythonToken *PythonTextBlockData::tokenAt(const QTextCursor &cursor)
{
    PythonTextBlockData *textData = pyBlockDataFromCursor(cursor);
    if (!textData)
        return nullptr;

    return textData->tokenAt(cursor.position() - cursor.block().position());
}

QString PythonTextBlockData::tokenAtAsString(int pos) const
{
    const PythonToken *tok = tokenAt(pos);
    return tokenAtAsString(tok);
}

QString PythonTextBlockData::tokenAtAsString(const PythonToken *tok) const
{
    QString ret;
    if (!tok)
        return ret;

    ret = m_block.text();
    if (ret.isEmpty())
        return ret;

    return ret.mid(tok->startPos, tok->endPos - tok->startPos);

//    QTextCursor cursor(m_block);
//    cursor.setPosition(m_block.position());
//    cursor.setPosition(cursor.position() + tok->startPos, QTextCursor::KeepAnchor);
//    return cursor.selectedText();
}

// static
QString PythonTextBlockData::tokenAtAsString(QTextCursor &cursor)
{
    PythonTextBlockData *textData = pyBlockDataFromCursor(cursor);
    if (!textData)
        return QString();

    return textData->tokenAtAsString(cursor.position() - cursor.block().position());
}

bool PythonTextBlockData::isMatchAt(int pos, PythonSyntaxHighlighter::Tokens token) const
{
    const PythonToken *tok = tokenAt(pos);
    if (!tok)
        return false;
    return token == tok->token;
}

bool PythonTextBlockData::isMatchAt(int pos, const QList<PythonSyntaxHighlighter::Tokens> tokens) const
{
    for (const PythonSyntaxHighlighter::Tokens token : tokens) {
        if (isMatchAt(pos, token))
            return true;
    }
    return false;
}

bool PythonTextBlockData::isCodeLine() const
{
    for (const PythonToken *tok : m_tokens) {
        if (tok->isCode())
            return true;
    }
    return false;
}

bool PythonTextBlockData::isEmpty() const
{
    return m_tokens.isEmpty();
}

int PythonTextBlockData::indent() const
{
    return m_indentCharCount; // note tab=8
}

void PythonTextBlockData::insertToken(PythonSyntaxHighlighter::Tokens token, int pos, int len)
{
    PythonToken *tokenObj = new PythonToken(token, pos, pos + len, this);
    int idx = -1;
    for (PythonToken *tok : m_tokens) {
        ++idx;
        if (tok->startPos > pos)
            break;
    }

    if (idx > -1)
        m_tokens.insert(idx, tokenObj);
}

PythonTextBlockScanInfo *PythonTextBlockData::scanInfo() const
{
    return dynamic_cast<PythonTextBlockScanInfo*>(TextEditBlockData::scanInfo());
}

void PythonTextBlockData::setScanInfo(PythonTextBlockScanInfo *scanInfo)
{
    m_scanInfo = scanInfo;
}

QString PythonTextBlockData::indentString() const
{
    if (m_tokens.size() > 0) {
        QTextCursor cursor(m_block);
        cursor.setPosition(m_block.position());
        cursor.setPosition(cursor.position() + m_tokens[0]->startPos,
                           QTextCursor::KeepAnchor);
        return cursor.selectedText();
    }

    return QString();
}

// -------------------------------------------------------------------------------------


PythonTextBlockScanInfo::PythonTextBlockScanInfo() :
    TextEditBlockScanInfo()
{
}

PythonTextBlockScanInfo::~PythonTextBlockScanInfo()
{
}

void PythonTextBlockScanInfo::setParseMessage(const PythonToken *tok, QString message,
                                              MsgType type)
{
    TextEditBlockScanInfo::setParseMessage(tok->startPos, tok->endPos, message, type);
}

const PythonTextBlockScanInfo::ParseMsg
*PythonTextBlockScanInfo::getParseMessage(const PythonToken *tok,
                                          PythonTextBlockScanInfo::MsgType type) const
{
    return TextEditBlockScanInfo::getParseMessage(tok->startPos, tok->endPos, type);
}

QString PythonTextBlockScanInfo::parseMessage(const PythonToken *tok, MsgType type) const
{
    return TextEditBlockScanInfo::parseMessage(tok->startPos, type);
}


void PythonTextBlockScanInfo::clearParseMessage(const PythonToken *tok)
{
    TextEditBlockScanInfo::clearParseMessage(tok->startPos);
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
    PythonTextBlockData *textData = reinterpret_cast<PythonTextBlockData*>(rawTextData);
    if (!textData)
        return;

    // query tokens
    int linePos = cursor.position() - currentBlock.position();
    const PythonToken *tokenObj = nullptr,
                      *triggerObj = nullptr;

    static const QList<PythonSyntaxHighlighter::Tokens> openTokens = {
            PythonSyntaxHighlighter::T_DelimiterOpenBrace,
            PythonSyntaxHighlighter::T_DelimiterOpenBracket,
            PythonSyntaxHighlighter::T_DelimiterOpenParen
    };

    static const QList<PythonSyntaxHighlighter::Tokens> closeTokens = {
            PythonSyntaxHighlighter::T_DelimiterCloseBrace,
            PythonSyntaxHighlighter::T_DelimiterCloseBracket,
            PythonSyntaxHighlighter::T_DelimiterCloseParen
    };

    bool forward;
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

    PythonSyntaxHighlighter::Tokens token1 = triggerObj->token;
    PythonSyntaxHighlighter::Tokens token2 = PythonSyntaxHighlighter::T_Invalid;

    // no token here, nothing to do
    if (token1 == PythonSyntaxHighlighter::T_Invalid)
        return;

    // current pos in doc and in line
    int pos1 = cursor.block().position() + linePos;
    //int startPos = forward ? linePos +1 : -(linePos -1);// search backwards

    // which token?
    int pos2 = -1;
    switch (token1) {
    case PythonSyntaxHighlighter::T_DelimiterOpenParen:
        token2 = PythonSyntaxHighlighter::T_DelimiterCloseParen;
        break;
    case PythonSyntaxHighlighter::T_DelimiterOpenBracket:
        token2 = PythonSyntaxHighlighter::T_DelimiterCloseBracket;
        break;
    case PythonSyntaxHighlighter::T_DelimiterOpenBrace:
        token2 = PythonSyntaxHighlighter::T_DelimiterCloseBrace;
        break;
    case PythonSyntaxHighlighter::T_DelimiterCloseBrace:
        token2 = PythonSyntaxHighlighter::T_DelimiterOpenBrace;
        break;
    case PythonSyntaxHighlighter::T_DelimiterCloseBracket:
        token2 = PythonSyntaxHighlighter::T_DelimiterOpenBracket;
        break;
    case PythonSyntaxHighlighter::T_DelimiterCloseParen:
        token2 = PythonSyntaxHighlighter::T_DelimiterOpenParen;
        break;
    default:
        return;
    }

    // if we got here we should find a matching char
    tokenObj = triggerObj;

    // search row by row
    int innerCount = 0;
    bool startLook = false;
    while (currentBlock.isValid()) {
        const QVector<PythonToken*> tokens = textData->tokens();
        if (!tokens.isEmpty()) {
            // iterate this textblocks tokens
            int i = forward ? 0: tokens.size() -1;
            while (tokenObj && i < tokens.size() && i > -1) {
                tokenObj = tokens[i];
                i += forward ? 1 : -1;

                // a stack push-pop for matching tokens
                if (tokenObj) {
                    if (tokenObj == triggerObj) {
                        startLook = true;
                        continue;
                    }
                    if (!startLook)
                        continue;
                    // same type as trigger
                    if (tokenObj->token == token1)
                        ++innerCount;
                    // the mirror of trigger
                    else if (tokenObj->token == token2)
                        --innerCount;
                    if (innerCount < 0 && tokenObj->token == token2) {
                        // found it!
                        pos2 = tokenObj->startPos + currentBlock.position();
                        goto setformat; // used to break out of both loops
                    }
                }
            }
        }

        // look in next block
        currentBlock = forward ? currentBlock.next() : currentBlock.previous();
        textData = static_cast<PythonTextBlockData*>(currentBlock.userData());
        if (!textData)
            return;
    }

setformat:

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
