
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



namespace Gui {
namespace Python {


class SyntaxHighlighterP
{
public:
    explicit SyntaxHighlighterP():
        endStateOfLastPara(Python::Token::T_Undetermined),
        activeBlock(nullptr)
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
    ~SyntaxHighlighterP() {}

    QStringList keywords;
    QStringList builtins;
    QString filePath;
    QTimer sourceScanTmr;
    Python::Token::Tokens endStateOfLastPara;
    Python::TextBlockData *activeBlock;
    QList<int> srcScanBlocks;
};

} // namespace Python

} // namespace Gui


// ----------------------------------------------------------------------

/**
 * Constructs a Python syntax highlighter.
 */
Python::SyntaxHighlighter::SyntaxHighlighter(QObject* parent)
    : Gui::SyntaxHighlighter(parent)
{
    d = new Python::SyntaxHighlighterP();
    d->sourceScanTmr.setInterval(50); // wait before running PythonSourceRoot rescan
                                      // dont scan on every row during complete rescan
    d->sourceScanTmr.setSingleShot(true);
    connect(&d->sourceScanTmr, SIGNAL(timeout()), this, SLOT(sourceScanTmrCallback()));
}

/** Destroys this object. */
Python::SyntaxHighlighter::~SyntaxHighlighter()
{
    delete d;
}

/**
 * Detects all kinds of text to highlight them in the correct color.
 */
void Python::SyntaxHighlighter::highlightBlock (const QString & text)
{
    int prevState = previousBlockState() != -1 ? previousBlockState() : 0; // is -1 when no state is set
    d->endStateOfLastPara = static_cast<Python::Token::Tokens>(prevState & TokensMASK);
    if (d->endStateOfLastPara < Python::Token::T_Undetermined)
        d->endStateOfLastPara = Python::Token::T_Undetermined;

    d->sourceScanTmr.blockSignals(true);
    d->sourceScanTmr.stop();

    // create new userData, copy bookmark etc
    Python::TextBlockData *txtBlock = new Python::TextBlockData(currentBlock());
    d->activeBlock = dynamic_cast<Python::TextBlockData*>(currentBlock().userData());
    if (d->activeBlock)
        txtBlock->copyBlock(*d->activeBlock);
    d->activeBlock = txtBlock;
    setCurrentBlockUserData(d->activeBlock);

#ifdef BUILD_PYTHON_DEBUGTOOLS
    txtBlock->m_textDbg = text;
#endif

    int parenCnt = (prevState & ParenCntMASK) >> ParenCntShiftPos;
    bool isParamLine  = prevState & ParamLineMASK;

    // scans this line
    int i = tokenize(text, parenCnt, isParamLine);

    // Insert new line token
    if (d->activeBlock->block().next().isValid())
        d->activeBlock->setDeterminedToken(Python::Token::T_DelimiterNewLine, i, 0);

    prevState = static_cast<int>(d->endStateOfLastPara);
    prevState |= parenCnt << ParenCntShiftPos;
    prevState |= static_cast<int>(isParamLine) << ParamLineShiftPos;
    setCurrentBlockState(prevState);
    d->srcScanBlocks.push_back(d->activeBlock->block().blockNumber());
    d->activeBlock = nullptr;

    d->sourceScanTmr.start();
    d->sourceScanTmr.blockSignals(false);
}

int Python::SyntaxHighlighter::tokenize(const QString &text, int &parenCnt, bool &isParamLine)
{
    //int prevState = previousBlockState() != -1 ? previousBlockState() : 0; // is -1 when no state is set
    bool isModuleLine = false;
    int prefixLen = 0;

    int i = 0, lastI = 0;
    QChar prev, ch;

    while ( i < text.length() )
    {
        ch = text.at( i );

        switch ( d->endStateOfLastPara )
        {
        case Python::Token::T_Undetermined:
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
                setRestOfLine(i, text, Python::Token::T_Comment); // function advances i
                break;
            case '"': case '\'':
            {
                // Begin either string literal or block string
                char compare = thisCh;
                QString endMarker(ch);
                int startStrPos = 0;
                Python::Token::Tokens token = Python::Token::T_Undetermined;
                int len = 0;

                if (nextCh == compare && thirdCh == compare) {
                    // block string
                    endMarker += endMarker + endMarker; // makes """ or '''
                    startStrPos = i + 3;
                    if (compare == '"') {
                        token = Python::Token::T_LiteralBlockDblQuote;
                        len = lastDblQuoteStringCh(startStrPos, text);
                    } else {
                        token = Python::Token::T_LiteralBlockSglQuote;
                        len = lastSglQuoteStringCh(startStrPos, text);
                    }
                    d->endStateOfLastPara = token;
                }

                // just a " or ' quoted string
                if (token == Python::Token::T_Undetermined) {
                    startStrPos = i + 1;
                    if (compare == '"') {
                        token = Python::Token::T_LiteralDblQuote;
                        len = lastDblQuoteStringCh(startStrPos, text);
                    } else {
                        token = Python::Token::T_LiteralSglQuote;
                        len = lastSglQuoteStringCh(startStrPos, text);
                    }
                }

                // search for end of string including endMarker
                int endPos = text.indexOf(endMarker, startStrPos + len);
                if (endPos != -1) {
                    len = (endPos - i) + prefixLen;
                    d->endStateOfLastPara = Python::Token::T_Undetermined;
                }

                // a string might have string prefix, such as r"" or u''
                // detection of that was in a previous iteration
                i -= prefixLen;

                setLiteral(i, len + endMarker.length() + prefixLen, token);
                prefixLen = 0;

            } break;

            // handle indentation
            case ' ': case '\t':
            {
                scanIndentation(i, text);
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
                        setWord(i, 2, Python::Token::T_OperatorKeyWordParam);
                        break;
                    }
                    setWord(i, 1, Python::Token::T_OperatorVariableParam);
                    break;
                } else if (isModuleLine) {
                    // specialcase * as it is also used as glob for modules
                    i -= 1;
                    d->endStateOfLastPara = Python::Token::T_IdentifierModuleGlob;
                    break;
                } else if (nextCh == '*') {
                    if (thirdCh == '=')
                        setOperator(i, 3, Python::Token::T_OperatorExpoEqual);
                    else
                        setOperator(i, 2, Python::Token::T_OperatorExponential);
                } else if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorMulEqual);
                } else {
                    setOperator(i, 1, Python::Token::T_OperatorMul);
                }
                 break;
            case '/':
                if (nextCh == '/') {
                    if (thirdCh == '=') {
                        setOperator(i, 3, Python::Token::T_OperatorFloorDivEqual);
                    } else {
                        setOperator(i, 2, Python::Token::T_OperatorFloorDiv);
                    }
                } else if(nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorDivEqual);
                    break;
                } else {
                    setOperator(i, 1, Python::Token::T_OperatorDiv);
                }
                break;
            case '>':
            {
                if (nextCh == thisCh) {
                    if(thirdCh == '=') {
                        setOperator(i, 3, Python::Token::T_OperatorBitShiftRightEqual);
                        break;
                    }
                    setOperator(i, 2, Python::Token::T_OperatorBitShiftRight);
                    break;
                } else if(nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorMoreEqual);
                    break;
                }
                // no just single * or <
                setOperator(i, 1, Python::Token::T_OperatorMore);
            } break;
            case '<': // might be 3 chars ie <<=
            {
                if (nextCh == thisCh) {
                    if(thirdCh == '=') {
                        setOperator(i, 3, Python::Token::T_OperatorBitShiftLeftEqual);
                        break;
                    }
                    setOperator(i, 2, Python::Token::T_OperatorBitShiftLeft);
                    break;
                } else if(nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorLessEqual);
                    break;
                }
                // no just single <
                setOperator(i, 1, Python::Token::T_OperatorLess);
            } break;

            // handle all left with possibly 2 chars only
            case '-':
                if (nextCh == '>') {
                    // -> is allowed function return type documentation
                    setDelimiter(i, 2, Python::Token::T_DelimiterMetaData);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorMinus);
                break;
            case '+':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorPlusEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorPlus);
                break;
            case '%':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorModuloEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorModulo);
                break;
            case '&':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorBitAndEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorBitAnd);
                break;
            case '^':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorBitXorEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorBitXor);
                break;
            case '|':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorBitOrEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorOr);
                break;
            case '=':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorCompareEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorEqual);
                break;
            case '!':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorNotEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorNot);
                break;
            case '~':
                if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorBitNotEqual);
                    break;
                }
                setOperator(i, 1, Python::Token::T_OperatorBitNot);
                break;
            case '(':
                setDelimiter(i, 1, Python::Token::T_DelimiterOpenParen);
                //if (isParamLine)
                    ++parenCnt;
                break;
            case '\r':
                if (nextCh == '\n') {
                    setDelimiter(i, 2, Python::Token::T_DelimiterNewLine);
                    break;
                }
                setDelimiter(i, 1, Python::Token::T_DelimiterNewLine);
                break;
            case '\n':
                setDelimiter(i, 1, Python::Token::T_DelimiterNewLine);
                break;
            case '[':
                setDelimiter(i, 1, Python::Token::T_DelimiterOpenBracket);
                break;
            case '{':
                setDelimiter(i, 1, Python::Token::T_DelimiterOpenBrace);
                break;
            case '}':
                setDelimiter(i, 1, Python::Token::T_DelimiterCloseBrace);
                break;
            case ')':
                setDelimiter(i, 1, Python::Token::T_DelimiterCloseParen);
                //if (isParamLine) {
                    --parenCnt;
                    if (parenCnt == 0)
                        isParamLine = false;
                //}
                break;
            case ']':
                setDelimiter(i, 1, Python::Token::T_DelimiterCloseBracket);
                break;
            case ',':
                setDelimiter(i, 1, Python::Token::T_DelimiterComma);
                break;
            case '.':
                if (nextCh == '.' && thirdCh == '.') {
                    setDelimiter(i, 3, Python::Token::T_DelimiterEllipsis);
                    break;
                }
                setDelimiter(i, 1, Python::Token::T_DelimiterPeriod);
                break;
            case ';':
                isModuleLine = false;
                setDelimiter(i, 1, Python::Token::T_DelimiterSemiColon);
                break;
            case ':':
                setDelimiter(i, 1, Python::Token::T_DelimiterColon);
                break;
            case '@':
            {   // decorator or matrix add
                if(nextCh && text.at(i+1).isLetter()) {
                    int len = lastWordCh(i + 1, text);
                    setWord(i, len +1, Python::Token::T_IdentifierDecorator);
                    break;
                } else if (nextCh == '=') {
                    setOperator(i, 2, Python::Token::T_OperatorMatrixMulEqual);
                    break;
                }

                 setDelimiter(i, 1, Python::Token::T_Delimiter);
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
                            setWord(i, len, Python::Token::T_KeywordDef);
                            d->endStateOfLastPara = Python::Token::T_IdentifierDefUnknown; // step out to handle def name

                        } else if (word == QLatin1String("class")) {
                            setWord(i, len, Python::Token::T_KeywordClass);
                            d->endStateOfLastPara = Python::Token::T_IdentifierClass; // step out to handle class name

                        } else if (word == QLatin1String("import")) {
                            setWord(i, len, Python::Token::T_KeywordImport);
                            d->endStateOfLastPara = Python::Token::T_IdentifierModule; // step out to handle module name
                            isModuleLine = true;

                        } else if (word == QLatin1String("from")) {
                            setWord(i, len, Python::Token::T_KeywordFrom);
                            d->endStateOfLastPara = Python::Token::T_IdentifierModulePackage; // step out handle module name
                            isModuleLine = true;

                        } else if (word == QLatin1String("and")) {
                            setWord(i, len, Python::Token::T_OperatorAnd);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;

                        } else if (word == QLatin1String("as")) {
                            setWord(i, len, Python::Token::T_KeywordAs);
                            if (isModuleLine)
                                d->endStateOfLastPara = Python::Token::T_IdentifierModuleAlias;
                            else
                                d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("in")) {
                            setWord(i, len, Python::Token::T_OperatorIn);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("is")) {
                            setWord(i, len, Python::Token::T_OperatorIs);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("not")) {
                            setWord(i, len, Python::Token::T_OperatorNot);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("or")) {
                            setWord(i, len, Python::Token::T_OperatorOr);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("yield")) {
                            setWord(i, len, Python::Token::T_KeywordYield);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("return")) {
                            setWord(i, len, Python::Token::T_KeywordReturn);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("True")) {
                            setIdentifier(i, len, Python::Token::T_IdentifierTrue);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("False")) {
                            setIdentifier(i, len, Python::Token::T_IdentifierFalse);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("None")) {
                            setIdentifier(i, len, Python::Token::T_IdentifierNone);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("if")) {
                            setIdentifier(i, len, Python::Token::T_KeywordIf);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        } else if (word == QLatin1String("elif")) {
                            setIdentifier(i, len, Python::Token::T_KeywordElIf);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        }  else if (word == QLatin1String("else")) {
                            setIdentifier(i, len, Python::Token::T_KeywordElse);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        }  else if (word == QLatin1String("for")) {
                            setIdentifier(i, len, Python::Token::T_KeywordFor);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        }   else if (word == QLatin1String("while")) {
                            setIdentifier(i, len, Python::Token::T_KeywordWhile);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        }   else if (word == QLatin1String("break")) {
                            setIdentifier(i, len, Python::Token::T_KeywordBreak);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        }   else if (word == QLatin1String("continue")) {
                            setIdentifier(i, len, Python::Token::T_KeywordContinue);
                            d->endStateOfLastPara = Python::Token::T_Undetermined;
                        }   else {
                            setWord(i, len, Python::Token::T_Keyword);
                        }

                    } else if(d->builtins.contains(word) &&
                              !d->activeBlock->lastInsertedIsA(Python::Token::T_DelimiterPeriod))
                    {
                        // avoid match against someObj.print
                        setWord(i, len, Python::Token::T_IdentifierBuiltin);
                    } else if (isModuleLine) {
                        i -= 1; // jump to module block
                        d->endStateOfLastPara = Python::Token::T_IdentifierModule;
                    } else {
                        setUndeterminedIdentifier(i, len, Python::Token::T_IdentifierUnknown);
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
                    setDelimiter(i, 1, Python::Token::T_DelimiterLineContinue);
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
        case Python::Token::T_LiteralBlockDblQuote:
        {
            // multiline double quote string continued from previous line
            scanIndentation(i, text);

            int endPos = text.indexOf(QLatin1String("\"\"\""), i);
            if (endPos != -1) {
                endPos += 3;
                setLiteral(i, endPos -i, Python::Token::T_LiteralBlockDblQuote);
                d->endStateOfLastPara = Python::Token::T_Undetermined;
            } else {
                setRestOfLine(i, text, Python::Token::T_LiteralBlockDblQuote);
            }

        } break;
        case Python::Token::T_LiteralBlockSglQuote:
        {
            // multiline single quote string continued from previous line
            scanIndentation(i, text);

            int endPos = text.indexOf(QLatin1String("'''"), i);
            if (endPos != -1) {
                endPos += 3;
                setLiteral(i, endPos -i, Python::Token::T_LiteralBlockSglQuote);
                d->endStateOfLastPara = Python::Token::T_Undetermined;
            }else {
                setRestOfLine(i, text, Python::Token::T_LiteralBlockSglQuote);
            }

        } break;
        case Python::Token::T_IdentifierDefUnknown:
        {   // before we now if its a class member or a function
            for (; i < text.length(); ++i) {
                if (!text.at(i).isSpace())
                    break;
            }
            if (i == text.length()) {
                d->endStateOfLastPara = Python::Token::T_Undetermined;
                break; // not yet finished this line?
            }

            int len = lastWordCh(i, text);
            if (d->activeBlock->indent() == 0)
                // no indent, it can't be a method
                setIdentifier(i, len, Python::Token::T_IdentifierFunction);
            else
                setUndeterminedIdentifier(i, len, Python::Token::T_IdentifierDefUnknown);
            d->endStateOfLastPara = Python::Token::T_Undetermined;

        } break;
        case Python::Token::T_IdentifierClass:
        {
            for (; i < text.length(); ++i) {
                if (!text.at(i).isSpace())
                    break;
            }
            if (i == text.length()) {
                d->endStateOfLastPara = Python::Token::T_Undetermined;
                break; // not yet finished this line?
            }

            int len = lastWordCh(i, text);
            setIdentifier(i, len, Python::Token::T_IdentifierClass);
            d->endStateOfLastPara = Python::Token::T_Undetermined;

        } break;
        case Python::Token::T_IdentifierModuleAlias:
        case Python::Token::T_IdentifierModuleGlob:
        case Python::Token::T_IdentifierModule:  // fallthrough
        case Python::Token::T_IdentifierModulePackage:
        {
            // can import multiple modules separated  with ',' so we jump here between here and T_Undertermined
            for (; i < text.length(); ++i) {
                if (!text.at(i).isSpace())
                    break;
            }
            if (i == text.length()) {
                d->endStateOfLastPara = Python::Token::T_Undetermined;
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
            d->endStateOfLastPara = Python::Token::T_Undetermined;

        } break;
        default:
        {
            // pythonCosole has some special values
            if (d->endStateOfLastPara == Python::Token::T_PythonConsoleError ||
                d->endStateOfLastPara == Python::Token::T_PythonConsoleOutput)
            {
                TColor color = d->endStateOfLastPara == Python::Token::T_PythonConsoleOutput ?
                                                            PythonConsoleOutput :
                                                                PythonConsoleError;
                if (i == 0 && text.length() >= 4) {
                    if (text.left(4) == QLatin1String(">>> ") ||
                        text.left(4) == QLatin1String("... "))
                    {
                        setFormat(0, 4, colorByType(color));
                        d->endStateOfLastPara = Python::Token::T_Undetermined;
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
    if ( d->endStateOfLastPara != Python::Token::T_LiteralBlockDblQuote &&
         d->endStateOfLastPara != Python::Token::T_LiteralBlockSglQuote )
    {
        d->endStateOfLastPara = Python::Token::T_Undetermined ;
    }

    return i;
}


QTextCharFormat Python::SyntaxHighlighter::getFormatToken(const Python::Token *token) const
{
    assert(token != nullptr && "Need a valid pointer");

    QTextCharFormat format;
    SyntaxHighlighter::TColor colorIdx = SyntaxHighlighter::Text ;
    switch (token->token) {
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
    case Python::Token::T_DelimiterLineContinue:
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

void Python::SyntaxHighlighter::updateFormatToken(const Python::Token *tok) const
{
    Python::TextBlockData *txtBlock = tok->txtBlock();
    if (!txtBlock || !txtBlock->block().isValid())
        return;

    updateFormat(txtBlock->block(), tok->startPos, tok->textLength(), getFormatToken(tok));
}

void Python::SyntaxHighlighter::setMessage(const Python::Token *tok) const
{
    Python::TextBlockData *txtData = tok->txtBlock();
    if (!txtData || !txtData->block().isValid())
        return;

    QTextCharFormat format;
    format.setUnderlineColor(QColor(210, 247, 64));
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    setMessageFormat(txtData->block(), tok->startPos, tok->textLength(), format);
}

void Python::SyntaxHighlighter::setIndentError(const Python::Token *tok) const
{
    Python::TextBlockData *txtData = tok->txtBlock();
    if (!txtData || !txtData->block().isValid())
        return;

    QTextCharFormat format;
    format.setUnderlineColor(QColor(244, 143, 66));
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    setMessageFormat(txtData->block(), tok->startPos, tok->textLength(), format);
}

void Python::SyntaxHighlighter::setSyntaxError(const Python::Token *tok) const
{
    Python::TextBlockData *txtData = tok->txtBlock();
    if (!txtData || !txtData->block().isValid())
        return;

    QTextCharFormat format;
    format.setUnderlineColor(colorByType(SyntaxError));
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    setMessageFormat(txtData->block(), tok->startPos, tok->textLength(), format);
}

void Python::SyntaxHighlighter::newFormatToken(const Python::Token *tok, QTextCharFormat format) const
{
    Python::TextBlockData *txtBlock = tok->txtBlock();
    if (!txtBlock || !txtBlock->block().isValid())
        return;

    addFormat(txtBlock->block(), tok->startPos, tok->textLength(), format);
}

void Python::SyntaxHighlighter::setFormatToken(const Python::Token *tok)
{
    int pos = tok->startPos;
    int len = tok->endPos - pos;
    setFormat(pos, len, getFormatToken(tok));
}

void Python::SyntaxHighlighter::sourceScanTmrCallback() {
    for(int row : d->srcScanBlocks) {
        QTextBlock block = document()->findBlockByNumber(row);
        if (block.isValid()) {
            Python::TextBlockData *txtData = dynamic_cast<Python::TextBlockData*>(block.userData());
            if (txtData)
                Python::SourceRoot::instance()->scanSingleRowModule(d->filePath, txtData, this);
        }
    }
    d->srcScanBlocks.clear();
}

void Python::SyntaxHighlighter::setFilePath(QString filePath)
{
    d->filePath = filePath;

    Python::SourceRoot::instance()->scanCompleteModule(filePath, this);
    d->srcScanBlocks.clear();
}

QString Python::SyntaxHighlighter::filePath() const
{
    return d->filePath;
}

int Python::SyntaxHighlighter::lastWordCh(int startPos, const QString &text) const
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

int Python::SyntaxHighlighter::lastNumberCh(int startPos, const QString &text) const
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

int Python::SyntaxHighlighter::lastDblQuoteStringCh(int startAt, const QString &text) const
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

int Python::SyntaxHighlighter::lastSglQuoteStringCh(int startAt, const QString &text) const
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

Python::Token::Tokens Python::SyntaxHighlighter::numberType(const QString &text) const
{
    if (text.isEmpty())
        return Python::Token::T_Invalid;
    if (text.indexOf(QLatin1String(".")) != -1)
        return Python::Token::T_NumberFloat;
    if (text.length() >= 2){
        QString two = text.toLower().left(2);
        if (two[0] == QLatin1Char('0')) {
            if (two[1] == QLatin1Char('x'))
                return Python::Token::T_NumberHex;
            if (two[1] == QLatin1Char('b'))
                return Python::Token::T_NumberBinary;
            if (two[1] == QLatin1Char('o') || two[0] == QLatin1Char('0'))
                return Python::Token::T_NumberOctal;
            return Python::Token::T_NumberOctal; // python 2 octal ie. 01234 != 1234
        }
    }
    return Python::Token::T_NumberDecimal;
}

void Python::SyntaxHighlighter::setRestOfLine(int &pos, const QString &text,
                                            Python::Token::Tokens token)
{
    int len = text.size() -pos;
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void Python::SyntaxHighlighter::scanIndentation(int &pos, const QString &text)
{
    if (d->activeBlock->isEmpty()) {

        int count = 0, j = pos;

        for (; j < text.length(); ++j) {
            if (text.at(j) == QLatin1Char(' ')) {
                ++count;
            } else if (text.at(j) == QLatin1Char('\t')) {
                count += 8; // according to python lexical documentaion tab is eight spaces
            } else {
                break;
            }
        }

        if (j - pos > 0)
            setIndentation(pos, j, count);
    }
}

void Python::SyntaxHighlighter::setWord(int &pos, int len,
                                      Python::Token::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}


void Python::SyntaxHighlighter::setIdentifier(int &pos, int len, Python::Token::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}


void Python::SyntaxHighlighter::setUndeterminedIdentifier(int &pos, int len,
                                                        Python::Token::Tokens token)
{
    // let parse tree figure out and color at a later stage
    setFormatToken(d->activeBlock->setUndeterminedToken(token, pos, len));
    pos += len -1;
}

void Python::SyntaxHighlighter::setNumber(int &pos, int len,
                                        Python::Token::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void Python::SyntaxHighlighter::setOperator(int &pos, int len,
                                          Python::Token::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void Python::SyntaxHighlighter::setDelimiter(int &pos, int len,
                                           Python::Token::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void Python::SyntaxHighlighter::setSyntaxError(int &pos, int len)
{
    setFormatToken(d->activeBlock->setDeterminedToken(Python::Token::T_SyntaxError, pos, len));
    pos += len -1;
}

void Python::SyntaxHighlighter::setLiteral(int &pos, int len,
                                         Python::Token::Tokens token)
{
    setFormatToken(d->activeBlock->setDeterminedToken(token, pos, len));
    pos += len -1;
}

void Python::SyntaxHighlighter::setIndentation(int &pos, int len, int count)
{
    setFormatToken(d->activeBlock->setDeterminedToken(Python::Token::T_Indent, pos, len));
    d->activeBlock->setIndentCount(count);
    pos += len -1;
}


// --------------------------------------------------------------------------------------------


Python::MatchingCharInfo::MatchingCharInfo():
    character(0), position(0)
{
}

Python::MatchingCharInfo::MatchingCharInfo(const MatchingCharInfo &other)
{
    character = other.character;
    position = other.position;
}

Python::MatchingCharInfo::MatchingCharInfo(char chr, int pos):
    character(chr), position(pos)
{
}

Python::MatchingCharInfo::~MatchingCharInfo()
{
}

//static
char Python::MatchingCharInfo::matchChar(char match)
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

char Python::MatchingCharInfo::matchingChar() const
{
    return MatchingCharInfo::matchChar(character);
}

// -------------------------------------------------------------------------------------------

Python::TextBlockData::TextBlockData(QTextBlock block) :
    Gui::TextEditBlockData(block),
    m_indentCharCount(0)
{
}

Python::TextBlockData::TextBlockData(const Python::TextBlockData &other) :
    Gui::TextEditBlockData(other),
    m_tokens(other.m_tokens),
    m_undeterminedIndexes(other.m_undeterminedIndexes),
    m_indentCharCount(other.m_indentCharCount)
{
    // FIXME ! handle tokens copying mer gracefully
    // Should probably make a deep copy here
}

Python::TextBlockData::~TextBlockData()
{
    qDeleteAll(m_tokens);
}

Python::TextBlockData *Python::TextBlockData::pyBlockDataFromCursor(const QTextCursor &cursor)
{
    return dynamic_cast<Python::TextBlockData*>(blockDataFromCursor(cursor));
}

const Python::TextBlockData::tokens_t &Python::TextBlockData::tokens() const
{
    return m_tokens;
}

const Python::TextBlockData::tokens_t Python::TextBlockData::undeterminedTokens() const
{
    tokens_t uTokens;
    for(int idx : m_undeterminedIndexes)
        uTokens.push_back(m_tokens[idx]);

    return uTokens;
}

Python::TextBlockData *Python::TextBlockData::next() const
{
    return dynamic_cast<Python::TextBlockData*>(TextEditBlockData::next());
}

Python::TextBlockData *Python::TextBlockData::previous() const
{
    return dynamic_cast<Python::TextBlockData*>(TextEditBlockData::previous());
}

const Python::Token *Python::TextBlockData::setDeterminedToken(Python::Token::Tokens token,
                                             int startPos, int len)
{
    Python::Token *tokenObj = new Python::Token(token, startPos, startPos + len, this);
    m_tokens.push_back(tokenObj);
    return tokenObj;
}

const Python::Token *Python::TextBlockData::setUndeterminedToken(Python::Token::Tokens token,
                                               int startPos, int len)
{
    const Python::Token *tokenObj = setDeterminedToken(token, startPos, len);

    // store this index so context parser can look it up at a later stage
    m_undeterminedIndexes.append(m_tokens.size() - 1);
    return tokenObj;
}

void Python::TextBlockData::setIndentCount(int count)
{
    m_indentCharCount = count;
}

const Python::Token *Python::TextBlockData::findToken(Python::Token::Tokens token,
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
            Python::Token *tok = m_tokens[i];
            if (tok->token == token &&
                searchFrom <= tok->startPos &&
                tok->endPos > searchFrom)
            {
                return tok;
            }
        }
    } else {
        // search from front (linestart)
        for (const Python::Token *tok : m_tokens) {
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

const Python::Token *Python::TextBlockData::firstTextToken() const
{
    for (Python::Token *tok : m_tokens) {
        if (tok->isText())
            return tok;
    }
    return nullptr;
}

const Python::Token *Python::TextBlockData::firstCodeToken() const
{
    for (Python::Token *tok : m_tokens) {
        if (tok->isCode())
            return tok;
    }
    return nullptr;
}

int Python::TextBlockData::tokensBetweenOfType(int startPos, int endPos,
                                             Python::Token::Tokens match) const
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

    for (Python::Token *tok : m_tokens) {
        if (start <= tok->startPos &&
            end > tok->endPos &&
            match == tok->token)
        {
            ++cnt;
        }
    }

    return cnt;
}

bool Python::TextBlockData::lastInsertedIsA(Python::Token::Tokens match, bool lookInPreviousRows) const
{
    if (m_tokens.isEmpty() ||
        m_tokens[m_tokens.size() -1]->token == Python::Token::T_Indent)
    {
        if (lookInPreviousRows) {
            Python::Token *tok = lastInserted(true);
            if (tok)
                return tok->token == match;
        }
        return false;
    }
    //
    Python::Token *tok = lastInserted(lookInPreviousRows);
    if (tok)
        return tok->token == match;
    return false;
}

Python::Token *Python::TextBlockData::lastInserted(bool lookInPreviousRows) const
{
    if (m_tokens.isEmpty() ||
        m_tokens[m_tokens.size() -1]->token == Python::Token::T_Indent)
    {
        if (!lookInPreviousRows)
            return nullptr;

        QTextBlock block = m_block.previous();
        while (block.isValid()) {
            Python::TextBlockData *textData = reinterpret_cast<Python::TextBlockData*>(block.userData());
            if (textData)
                return textData->lastInserted(true);
            block = block.previous();
        }
        // not found
        return nullptr;
    }

    return m_tokens[m_tokens.size() -1];
}

Python::Token *Python::TextBlockData::tokenAt(int pos) const
{
    for (Python::Token *tok : m_tokens) {
        if (tok->startPos <= pos && tok->endPos > pos)
            return tok;
    }

    return nullptr;
}

// static
Python::Token *Python::TextBlockData::tokenAt(const QTextCursor &cursor)
{
    Python::TextBlockData *textData = pyBlockDataFromCursor(cursor);
    if (!textData)
        return nullptr;

    return textData->tokenAt(cursor.position() - cursor.block().position());
}

QString Python::TextBlockData::tokenAtAsString(int pos) const
{
    const Python::Token *tok = tokenAt(pos);
    return tokenAtAsString(tok);
}

QString Python::TextBlockData::tokenAtAsString(const Python::Token *tok) const
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
QString Python::TextBlockData::tokenAtAsString(QTextCursor &cursor)
{
    Python::TextBlockData *textData = pyBlockDataFromCursor(cursor);
    if (!textData)
        return QString();

    return textData->tokenAtAsString(cursor.position() - cursor.block().position());
}

bool Python::TextBlockData::isMatchAt(int pos, Python::Token::Tokens token) const
{
    const Python::Token *tok = tokenAt(pos);
    if (!tok)
        return false;
    return token == tok->token;
}

bool Python::TextBlockData::isMatchAt(int pos, const QList<Python::Token::Tokens> tokens) const
{
    for (const Python::Token::Tokens token : tokens) {
        if (isMatchAt(pos, token))
            return true;
    }
    return false;
}

bool Python::TextBlockData::isCodeLine() const
{
    for (const Python::Token *tok : m_tokens) {
        if (tok->isCode())
            return true;
    }
    return false;
}

bool Python::TextBlockData::isEmpty() const
{
    return m_tokens.isEmpty();
}

int Python::TextBlockData::indent() const
{
    return m_indentCharCount; // note tab=8
}

Python::Token *Python::TextBlockData::insertToken(Python::Token::Tokens token, int pos, int len)
{
    int idx = -1;
    for (Python::Token *tok : m_tokens) {
        ++idx;
        if (tok->startPos > pos)
            break;
    }

    if (idx > -1) {
        Python::Token *tokenObj = new Python::Token(token, pos, pos + len, this);
        m_tokens.insert(idx, tokenObj);
        return tokenObj;
    }

    return nullptr;
}

Python::TextBlockScanInfo *Python::TextBlockData::scanInfo() const
{
    return dynamic_cast<Python::TextBlockScanInfo*>(Gui::TextEditBlockData::scanInfo());
}

void Python::TextBlockData::setScanInfo(Python::TextBlockScanInfo *scanInfo)
{
    m_scanInfo = scanInfo;
}

QString Python::TextBlockData::indentString() const
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


Python::TextBlockScanInfo::TextBlockScanInfo() :
    Gui::TextEditBlockScanInfo()
{
}

Python::TextBlockScanInfo::~TextBlockScanInfo()
{
}

void Python::TextBlockScanInfo::setParseMessage(const Python::Token *tok, QString message,
                                              MsgType type)
{
    Gui::TextEditBlockScanInfo::setParseMessage(tok->startPos, tok->endPos, message, type);
}

const Python::TextBlockScanInfo::ParseMsg
*Python::TextBlockScanInfo::getParseMessage(const Python::Token *tok,
                                          Python::TextBlockScanInfo::MsgType type) const
{
    return Gui::TextEditBlockScanInfo::getParseMessage(tok->startPos, tok->endPos, type);
}

QString Python::TextBlockScanInfo::parseMessage(const Python::Token *tok, MsgType type) const
{
    return Gui::TextEditBlockScanInfo::parseMessage(tok->startPos, type);
}


void Python::TextBlockScanInfo::clearParseMessage(const Python::Token *tok)
{
    Gui::TextEditBlockScanInfo::clearParseMessage(tok->startPos);
}

// -------------------------------------------------------------------------------------

Python::MatchingChars::MatchingChars(PythonEditor *parent):
    QObject(parent),
    m_editor(parent)
{
    // for matching chars such as (, [, { etc.
    connect(parent, SIGNAL(cursorPositionChanged()),
            this, SLOT(cursorPositionChange()));
}

Python::MatchingChars::MatchingChars(PythonConsoleTextEdit *parent):
    QObject(parent),
    m_editor(parent)
{
    // for matching chars such as (, [, { etc.
    connect(parent, SIGNAL(cursorPositionChanged()),
            this, SLOT(cursorPositionChange()));
}

Python::MatchingChars::~MatchingChars()
{
}


void Python::MatchingChars::cursorPositionChange()
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
    Python::TextBlockData *textData = reinterpret_cast<Python::TextBlockData*>(rawTextData);
    if (!textData)
        return;

    // query tokens
    int linePos = cursor.position() - currentBlock.position();
    const Python::Token *tokenObj = nullptr,
                      *triggerObj = nullptr;

    static const QList<Python::Token::Tokens> openTokens = {
            Python::Token::T_DelimiterOpenBrace,
            Python::Token::T_DelimiterOpenBracket,
            Python::Token::Tokens::T_DelimiterOpenParen
    };

    static const QList<Python::Token::Tokens> closeTokens = {
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

    Python::Token::Tokens token1 = triggerObj->token;
    Python::Token::Tokens token2 = Python::Token::T_Invalid;

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
    tokenObj = triggerObj;

    // search row by row
    int innerCount = 0;
    bool startLook = false;
    while (currentBlock.isValid()) {
        const QVector<Python::Token*> tokens = textData->tokens();
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
        textData = static_cast<Python::TextBlockData*>(currentBlock.userData());
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
