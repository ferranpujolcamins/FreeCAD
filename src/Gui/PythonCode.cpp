/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *   Copyright (c) 2017 Fredrik Johansson github.com/mumme74               *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
# include <QContextMenuEvent>
# include <QMenu>
# include <QPainter>
# include <QShortcut>
# include <QTextCursor>
#endif

#include "PythonCode.h"
#include "PythonEditor.h"
#include "Application.h"
#include "BitmapFactory.h"
#include "Macro.h"
#include <Base/PyTools.h>

#include <CXX/Objects.hxx>
#include <Base/PyObjectBase.h>
#include <Base/Interpreter.h>
#include <Base/Exception.h>
#include <Base/Parameter.h>
#include <App/PythonDebugger.h>
#include <QApplication>
#include <QDebug>
#include <QEventLoop>

namespace Gui {
struct PythonCodeP
{
    PythonCodeP()
    {  }
    ~PythonCodeP()
    {  }
};

} // namespace Gui


using namespace Gui;


PythonCode::PythonCode(QObject *parent) :
    QObject(parent), d(new PythonCodeP)
{
}

PythonCode::~PythonCode()
{
    delete d;
}

/**
 * @brief deepcopys a object, caller takes ownership
 * @param obj to deep copy
 * @return the new obj
 */
PyObject *PythonCode::deepCopy(PyObject *obj)
{
    Base::PyGILStateLocker lock;

    // load copy module
    PyObject *deepcopyPtr = PP_Load_Attribute("copy", "deepcopy");
    if (!deepcopyPtr)
        return nullptr;

    Py_INCREF(deepcopyPtr);

    // create argument tuple
    PyObject *args = PyTuple_New(sizeof obj);
    if (!args) {
        Py_DECREF(deepcopyPtr);
        return nullptr;
    }

    Py_INCREF(args);

    if (PyTuple_SetItem(args, 0, (PyObject*)obj) != 0) {
        Py_DECREF(args);
        Py_DECREF(deepcopyPtr);
    }

    // call pythons copy.deepcopy
    PyObject *result = PyObject_CallObject(deepcopyPtr, args);
    if (!result) {
        PyErr_Clear();
    }

    return result;
}

// get thee root of the parent identifier ie os.path.join
//                                                    ^
// must traverse from os, then os.path before os.path.join
QString PythonCode::findFromCurrentFrame(QString varName)
{
    Base::PyGILStateLocker locker;
    PyFrameObject *current_frame = App::PythonDebugger::instance()->currentFrame();
    if (current_frame == 0)
        return QString();

    QString foundKey;

    PyFrame_FastToLocals(current_frame);
    Py::Object obj = getDeepObject(current_frame->f_locals, varName, foundKey);
    if (obj.isNull())
        obj = getDeepObject(current_frame->f_globals, varName, foundKey);

    if (obj.isNull())
        obj = getDeepObject(current_frame->f_builtins, varName, foundKey);

    if (obj.isNull()) {
        return QString();
    }

    return QString(QLatin1String("%1:%3\nType:%2\n%4"))
            .arg(foundKey)
            .arg(QString::fromStdString(obj.type().as_string()))
            .arg(QString::fromStdString(obj.str().as_string()))
            .arg(QString::fromStdString(obj.repr().as_string()));

//    // found correct object
//    const char *valueStr, *reprStr, *typeStr;
//    PyObject *repr_obj;
//    repr_obj = PyObject_Repr(obj);
//    typeStr = Py_TYPE(obj)->tp_name;
//    obj = PyObject_Str(obj);
//    valueStr = PyBytes_AS_STRING(obj);
//    reprStr = PyBytes_AS_STRING(repr_obj);
//    return QString(QLatin1String("%1:%3\nType:%2\n%4"))
//                .arg(foundKey)
//                .arg(QLatin1String(typeStr))
//                .arg(QLatin1String(valueStr))
//                .arg(QLatin1String(reprStr));
}

  /**
 * @brief PythonCode::findObjFromFrame
 * get thee root of the parent identifier ie os.path.join
 *                                                     ^
 *  must traverse from os, then os.path before os.path.join
 *
 * @param obj: Search in obj as a root
 * @param key: name of var to find
 * @return Obj if found or nullptr
 */
Py::Object PythonCode::getDeepObject(PyObject *obj, QString key, QString &foundKey)
{
    Base::PyGILStateLocker locker;
    PyObject *keyObj = nullptr;
    PyObject *tmp = nullptr;

    QStringList parts = key.split(QLatin1Char('.'));
    if (!parts.size())
        return Py::Object();

    for (int i = 0; i < parts.size(); ++i) {
        keyObj = PyBytes_FromString(parts[i].toLatin1());
        if (keyObj != nullptr) {
            do {
                if (PyObject_HasAttr(obj, keyObj) > 0) {
                    obj = PyObject_GetAttr(obj, keyObj);
                    tmp = obj;
                    Py_XINCREF(tmp);
                } else if (PyDict_Check(obj) &&
                           PyDict_Contains(obj, keyObj))
                {
                    obj = PyDict_GetItem(obj, keyObj);
                    Py_XDECREF(tmp);
                    tmp = nullptr;
                } else
                    break; // bust out as we havent found it

                // if we get here we have found what we want
                if (i == parts.size() -1) {
                    // found the last part
                    foundKey = parts[i];
                    return Py::Object(obj);
                } else {
                    continue;
                }

            } while(false); // bust of 1 time loop
        } else {
            return Py::Object();
        }
    }
    return Py::Object();
}


// -------------------------------------------------------------------------------


namespace Gui {
class PythonSyntaxHighlighterP
{
public:
    PythonSyntaxHighlighterP():
        endStateOfLastPara(PythonSyntaxHighlighter::T_Undetermined),
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

    QStringList keywords;
    QStringList builtins;
    PythonSyntaxHighlighter::Tokens endStateOfLastPara;
    PythonTextBlockData *activeBlock;

};
} // namespace Gui

// ----------------------------------------------------------------------

/**
 * Constructs a Python syntax highlighter.
 */
PythonSyntaxHighlighter::PythonSyntaxHighlighter(QObject* parent)
    : SyntaxHighlighter(parent)
{
    d = new PythonSyntaxHighlighterP;
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
    int i = 0, lastI = 0;
    QChar prev, ch;

    d->activeBlock = new PythonTextBlockData(currentBlock());
    setCurrentBlockUserData(d->activeBlock);

    d->endStateOfLastPara = static_cast<PythonSyntaxHighlighter::Tokens>(previousBlockState());
    if (d->endStateOfLastPara < T_Undetermined)
        d->endStateOfLastPara = T_Undetermined;

    bool isModuleLine = false;
    int prefixLen = 0;

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
                setRestOfLine(i, text, Tokens::T_Comment, TColor::Comment); // function advances i
                break;
            case '"': case '\'':
            {
                // Begin either string literal or block string
                char compare = thisCh;
                QString endMarker(ch);
                int startStrPos;
                Tokens token = T_Undetermined;
                TColor color;
                int len;

                if (nextCh == compare && thirdCh == compare) {
                    // block string
                    endMarker += endMarker + endMarker; // makes """ or '''
                    startStrPos = i + 3;
                    if (compare == '"') {
                        token = T_LiteralBlockDblQuote;
                        color = StringBlockDoubleQoute;
                        len = lastDblQuoteStringCh(startStrPos, text);
                    } else {
                        token = T_LiteralBlockSglQuote;
                        color = StringBlockSingleQoute;
                        len = lastSglQuoteStringCh(startStrPos, text);
                    }
                    d->endStateOfLastPara = token;
                }

                // just a " or ' quoted string
                if (token == T_Undetermined) {
                    startStrPos = i + 1;
                    if (compare == '"') {
                        token = T_LiteralDblQuote;
                        color = StringDoubleQoute;
                        len = lastDblQuoteStringCh(startStrPos, text);
                    } else {
                        token = T_LiteralSglQuote;
                        color = StringSingleQoute;
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

                setLiteral(i, len + endMarker.length(), token, color);
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
                //  +       -       *       **      /       //      %      @
                //  <<      >>      &       |       ^       ~
                //  <       >       <=      >=      ==      !=

                // These are the allowed Delimiters
                // (       )       [       ]       {       }
                // ,       :       .       ;       @       =       ->
                // +=      -=      *=      /=      //=     %=      @=
                // &=      |=      ^=      >>=     <<=     **=

            // handle all with possibly 3 chrs
            case '*': // specialcase * as it is also used as glob for modules
                if (isModuleLine) {
                    i -= 1;
                    d->endStateOfLastPara = T_IdentifierModule;
                    break;
                } // else fallthrough

            case '/':
                if(nextCh == '=') {
                    setOperator(i, 2, T_OperatorAssign);
                    break;
                } // else fallthrough

            case '>': case '<': // might be 3 chars ie >>=
            {
                if (nextCh == thisCh) {
                    // test for **= or <<=
                    if(thirdCh == '=') {
                        setDelimiter(i, 3, T_Delimiter);
                        break;
                    }
                    // no just ** or <<
                    setOperator(i, 2, T_Operator);
                    break;
                } else if(nextCh == '=') {
                    // ie  <= and not *= as that is stopped in the case '*' above
                    setOperator(i, 2, T_OperatorCompare);
                    break;
                }
                // no just single * or <
                setOperator(i, 1, T_Operator);
            } break;
            // handle all left with possibly 2 chars
            case '-':
                if (nextCh == '>') {
                    // -> is allowed function return type documentation
                    setDelimiter(i, 2, T_DelimiterMetaData);
                    break;
                } // else fallthrough

            case '+': case '%': case '&':
            case '^': case '|':
                if (nextCh == '=') {
                    // possibly two characters
                    setOperator(i, 2, T_OperatorAssign);
                    break;
                }
                setOperator(i, 1, T_Operator);
                break;
            case '=':
                if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorCompare);
                    break;
                }
                setOperator(i, 1, T_OperatorAssign);
                break;
            case '!':
                setOperator(i, 1, T_OperatorCompare);
            // only single char
                break;
            case '~':
                setOperator(i, 1, T_Operator);
                break;
            case '(':
                setDelimiter(i, 1, T_DelimiterOpenParen);
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
                break;
            case ']':
                setDelimiter(i, 1, T_DelimiterCloseBracket);
                break;
            case ',':
                setDelimiter(i, 1, T_DelimiterComma);
                break;
            case '.':
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
                    setWord(i, len +1, T_IdentifierDecorator, TColor::IdentifierDecorator);
                    break;
                } else if (nextCh == '=') {
                    setOperator(i, 2, T_OperatorAssign);
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
                            setBoldWord(i, len, Tokens::T_KeywordDef, SyntaxHighlighter::KeywordDef);
                            d->endStateOfLastPara = T_IdentifierDefUnknown; // step out to handle def name

                        } else if (word == QLatin1String("class")) {
                            setBoldWord(i, len, Tokens::T_KeywordClass, SyntaxHighlighter::KeywordClass);
                            d->endStateOfLastPara = T_IdentifierClass; // step out to handle class name

                        } else if (word == QLatin1String("import")) {
                            setBoldWord(i, len, Tokens::T_KeywordImport, SyntaxHighlighter::Keyword);
                            d->endStateOfLastPara = T_IdentifierModule; // step out to handle module name
                            isModuleLine = true;

                        } else if (word == QLatin1String("from")) {
                            setBoldWord(i, len, Tokens::T_KeywordFrom, SyntaxHighlighter::Keyword);
                            d->endStateOfLastPara = T_IdentifierModule; // step out handle module name
                            isModuleLine = true;

                        } else if (word == QLatin1String("as")) {
                            setBoldWord(i, len, T_KeywordAs, SyntaxHighlighter::Keyword);
                            if (isModuleLine)
                                d->endStateOfLastPara = T_IdentifierModule;
                            else
                                d->endStateOfLastPara = T_Undetermined;
                        } else if (word == QLatin1String("in")) {
                            setBoldWord(i, len, T_KeywordIn, SyntaxHighlighter::Keyword);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("yield")) {
                            setBoldWord(i, len, T_KeywordYield, SyntaxHighlighter::Keyword);
                            d->endStateOfLastPara = T_Undetermined;

                        } else if (word == QLatin1String("return")) {
                            setBoldWord(i, len, T_KeywordReturn, SyntaxHighlighter::Keyword);
                            d->endStateOfLastPara = T_Undetermined;

                        } else {
                            setBoldWord(i, len, Tokens::T_Keyword, SyntaxHighlighter::Keyword);
                        }

                    } else if(d->builtins.contains(word) &&
                              !d->activeBlock->lastInsertedIsA(T_DelimiterPeriod))
                    {
                        // avoid match against someObj.print
                        setBoldWord(i, len, Tokens::T_IdentifierBuiltin, SyntaxHighlighter::IdentifierBuiltin);
                    } else if (isModuleLine) {
                        i -= 1; // jump to module block
                        d->endStateOfLastPara = T_IdentifierModule;
                    } else {
                        setUndeterminedIdentifier(i, len, T_IdentifierUnknown, SyntaxHighlighter::IdentifierUnknown);
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
                setLiteral(i, endPos -i, T_LiteralBlockDblQuote, StringBlockDoubleQoute);
                d->endStateOfLastPara = T_Undetermined;
            } else {
                setRestOfLine(i, text, T_LiteralBlockDblQuote, StringBlockDoubleQoute);
            }

        } break;
        case T_LiteralBlockSglQuote:
        {
            // multiline single quote string continued from previous line
            int endPos = text.indexOf(QLatin1String("'''"), i);
            if (endPos != -1) {
                endPos += 3;
                setLiteral(i, endPos -i, T_LiteralBlockSglQuote, StringBlockSingleQoute);
                d->endStateOfLastPara = T_Undetermined;
            }else {
                setRestOfLine(i, text, T_LiteralBlockSglQuote, StringBlockSingleQoute);
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
                setIdentifierBold(i, len, T_IdentifierFunction, SyntaxHighlighter::IdentifierFunction);
            else
                setUndeterminedIdentifierBold(i, len, T_IdentifierDefUnknown, SyntaxHighlighter::IdentifierUnknown);
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
            setIdentifierBold(i, len, T_IdentifierClass, SyntaxHighlighter::IdentifierClass);
            d->endStateOfLastPara = T_Undetermined;

        } break;
        case T_IdentifierModule:
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

            setIdentifierBold(i, len, T_IdentifierModule, SyntaxHighlighter::IdentifierModule);
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

    setCurrentBlockState(static_cast<int>(d->endStateOfLastPara));
    d->activeBlock = nullptr;
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
                                            PythonSyntaxHighlighter::Tokens token,
                                            SyntaxHighlighter::TColor color)
{
    int len = text.size() -pos;
    setFormat(pos, len, this->colorByType(color));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;
}

void PythonSyntaxHighlighter::setWord(int &pos, int len,
                                      PythonSyntaxHighlighter::Tokens token,
                                      SyntaxHighlighter::TColor color)
{
    setFormat(pos, len, this->colorByType(color));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;
}

void PythonSyntaxHighlighter::setBoldWord(int &pos, int len,
                                          PythonSyntaxHighlighter::Tokens token,
                                          SyntaxHighlighter::TColor color)
{
    QTextCharFormat format;
    format.setForeground(this->colorByType(color));
    format.setFontWeight(QFont::Bold);
    setFormat(pos, len, format);
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;
}


void PythonSyntaxHighlighter::setIdentifier(int &pos, int len, Tokens token, TColor color)
{
    setFormat(pos, len, this->colorByType(color));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;
}


void PythonSyntaxHighlighter::setUndeterminedIdentifier(int &pos, int len,
                                                        Tokens token, TColor color)
{
    setFormat(pos, len, this->colorByType(color));

    // let parse tree figure out and color at a later stage
    d->activeBlock->setUndeterminedToken(token, pos, len);
    pos += len -1;
}

void PythonSyntaxHighlighter::setIdentifierBold(int &pos, int len, PythonSyntaxHighlighter::Tokens token, SyntaxHighlighter::TColor color)
{
    QTextCharFormat format;
    format.setForeground(this->colorByType(color));
    format.setFontWeight(QFont::Bold);
    setFormat(pos, len, format);
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;

}

void PythonSyntaxHighlighter::setUndeterminedIdentifierBold(int &pos, int len, PythonSyntaxHighlighter::Tokens token, SyntaxHighlighter::TColor color)
{
    QTextCharFormat format;
    format.setForeground(this->colorByType(color));
    format.setFontWeight(QFont::Bold);
    setFormat(pos, len, format);

    // let parse tree figure out and color at a later stage
    d->activeBlock->setUndeterminedToken(token, pos, len);
    pos += len -1;
}

void PythonSyntaxHighlighter::setNumber(int &pos, int len,
                                        PythonSyntaxHighlighter::Tokens token)
{
    SyntaxHighlighter::TColor color;
    switch (token) {
    case T_NumberHex:
        color = SyntaxHighlighter::NumberHex;
        break;
    case T_NumberBinary:
        color = SyntaxHighlighter::NumberBinary;
        break;
    case T_NumberFloat:
        color = SyntaxHighlighter::NumberFloat;
        break;
    case T_NumberOctal:
        color = SyntaxHighlighter::NumberOctal;
        break;
    default:
        color = SyntaxHighlighter::Number;
    }

    setFormat(pos, len, this->colorByType(color));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;
}

void PythonSyntaxHighlighter::setOperator(int &pos, int len,
                                          PythonSyntaxHighlighter::Tokens token)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::Operator));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;

}

void PythonSyntaxHighlighter::setDelimiter(int &pos, int len,
                                           PythonSyntaxHighlighter::Tokens token)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::Delimiter));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;
}

void PythonSyntaxHighlighter::setSyntaxError(int &pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::SyntaxError));
    d->activeBlock->setDeterminedToken(T_SyntaxError, pos, len);
    pos += len -1;
}

void PythonSyntaxHighlighter::setLiteral(int &pos, int len,
                                         Tokens token, SyntaxHighlighter::TColor color)
{
    setFormat(pos, len, this->colorByType(color));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len -1;
}

void PythonSyntaxHighlighter::setIndentation(int &pos, int len, int count)
{
    d->activeBlock->setDeterminedToken(T_Indent, pos, len);
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
    txtBlock(block), srcLstNode(nullptr)
{
}

PythonToken::~PythonToken()
{
    // remove from AST
    if (srcLstNode)
        delete srcLstNode;
}

bool PythonToken::operator==(const PythonToken &other) const
{
    return token == other.token &&
           startPos == other.startPos &&
            endPos == other.endPos;
}

bool PythonToken::operator >(const PythonToken &other) const
{
    return startPos > other.endPos;
}

PythonToken *PythonToken::next() const
{
    PythonTextBlockData *txt = txtBlock;
    while (txt) {
        const PythonTextBlockData::tokens_t tokens = txtBlock->tokens();
        int i = tokens.indexOf(const_cast<PythonToken*>(this));
        if (i > -1 && i +1 < tokens.size())
            return tokens.at(i+1);

        // we are the last token in this txtBlock or it was empty
        txt = dynamic_cast<PythonTextBlockData*>(txt->block().next().userData());
    }
    return nullptr;
}

PythonToken *PythonToken::previous() const
{
    PythonTextBlockData *txt = txtBlock;
    while (txt) {
        const PythonTextBlockData::tokens_t tokens = txtBlock->tokens();
        int i = tokens.indexOf(const_cast<PythonToken*>(this));
        if (i > 0)
            return tokens.at(i-1);

        // we are the last token in this txtBlock or it was empty
        txt = dynamic_cast<PythonTextBlockData*>(txt->block().next().userData());
    }
    return nullptr;
}

// -------------------------------------------------------------------------------------------

PythonTextBlockData::PythonTextBlockData(QTextBlock block) :
    m_block(block), m_indentCharCount(0)
{
}

PythonTextBlockData::~PythonTextBlockData()
{
    qDeleteAll(m_tokens);
}

PythonTextBlockData *PythonTextBlockData::blockDataFromCursor(const QTextCursor &cursor)
{

    QTextBlock block = cursor.block();
    if (!block.isValid())
        return nullptr;

    return reinterpret_cast<PythonTextBlockData*>(block.userData());
}

const QTextBlock &PythonTextBlockData::block() const
{
    return m_block;
}

const PythonTextBlockData::tokens_t &PythonTextBlockData::tokens() const
{
    return m_tokens;
}

void PythonTextBlockData::setDeterminedToken(PythonSyntaxHighlighter::Tokens token,
                                             int startPos, int len)
{
    PythonToken *tokenObj = new PythonToken(token, startPos, startPos + len, this);
    m_tokens.push_back(tokenObj);
}

void PythonTextBlockData::setUndeterminedToken(PythonSyntaxHighlighter::Tokens token,
                                               int startPos, int len)
{
    // TODO should call parse tree here to notify that we need to determine this
    setDeterminedToken(token, startPos, len);

    // store this index so context parser can look it up at a later stage
    m_undeterminedIndexes.append(m_tokens.size() - 1);
}

void PythonTextBlockData::setIndentCount(int count)
{
    m_indentCharCount = count;
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

const PythonToken *PythonTextBlockData::tokenAt(int pos) const
{
    for (const PythonToken *tok : m_tokens) {
        if (tok->startPos <= pos && tok->endPos > pos)
            return tok;
    }

    return nullptr;
}

// static
const PythonToken *PythonTextBlockData::tokenAt(const QTextCursor &cursor)
{
    PythonTextBlockData *textData = blockDataFromCursor(cursor);
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
    if (!tok)
        return QString();

    QTextCursor cursor(m_block);
    cursor.setPosition(m_block.position());
    cursor.setPosition(cursor.position() + tok->startPos, QTextCursor::KeepAnchor);
    return cursor.selectedText();
}

// static
QString PythonTextBlockData::tokenAtAsString(QTextCursor &cursor)
{
    PythonTextBlockData *textData = blockDataFromCursor(cursor);
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
        if (tok->token != PythonSyntaxHighlighter::T_Comment)
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



PythonMatchingChars::PythonMatchingChars(TextEdit *parent):
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
    m_editor->highlightText(pos1, 1, format);
    m_editor->highlightText(pos2, 1, format);
}



// Begin Python Code AST methods (allthough no tree as we constantly keep changing src in editor)
// ------------------------------------------------------------------------

PythonSourceRoot::PythonSourceRoot() :
    m_uniqueCustomTypeNames(-1)
{
}

PythonSourceRoot::~PythonSourceRoot()
{
}

// static
PythonSourceRoot *PythonSourceRoot::instance()
{
    if (m_instance == nullptr)
        m_instance = new PythonSourceRoot;
    return m_instance;

}
PythonSourceRoot *PythonSourceRoot::m_instance = nullptr;

PythonSourceRoot::DataTypes PythonSourceRoot::mapDataType(const QString typeAnnotation) const
{
    // try to determine type form type annotaion
    // more info at:
    // https://www.python.org/dev/peps/pep-0484/
    // https://www.python.org/dev/peps/pep-0526/
    // https://mypy.readthedocs.io/en/latest/cheat_sheet_py3.html
    if (typeAnnotation == QLatin1String("none"))
        return NoneType;
    else if (typeAnnotation == QLatin1String("bool"))
        return BoolType;
    else if (typeAnnotation == QLatin1String("int"))
        return IntType;
    else if (typeAnnotation == QLatin1String("float"))
        return FloatType;
    else if (typeAnnotation == QLatin1String("str"))
        return StringType;
    else if (typeAnnotation == QLatin1String("bytes"))
        return BytesType;
    else if (typeAnnotation == QLatin1String("List"))
        return ListType;
    else if (typeAnnotation == QLatin1String("Tuple"))
        return TupleType;
    else if (typeAnnotation == QLatin1String("Dict"))
        return DictType;
    else if (typeAnnotation == QLatin1String("Set"))
        return SetType;
    return CustomType;
}

QString PythonSourceRoot::customTypeNameFor(PythonSourceRoot::CustomNameIdx_t customIdx)
{
    return m_customTypeNames[customIdx];
}

PythonSourceRoot::CustomNameIdx_t PythonSourceRoot::addCustomTypeName(const QString name)
{
    m_customTypeNames[++m_uniqueCustomTypeNames] = name;
    return m_uniqueCustomTypeNames;
}

PythonSourceRoot::CustomNameIdx_t PythonSourceRoot::indexOfCustomTypeName(const QString name)
{
    if (m_customTypeNames.values().contains(QString(name)))
        return m_customTypeNames.key(QString(name));
    return -1;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// this struct is contained by PythonSourceRoot
PythonSourceRoot::TypeInfo::TypeInfo() :
    type(PythonSourceRoot::InValidType),
    customNameIdx(-1)
{
}

PythonSourceRoot::TypeInfo::~TypeInfo()
{
}

QString PythonSourceRoot::TypeInfo::typeAsStr() const
{
    const char *typeAsStr;
    switch (type) {
    case PythonSourceRoot::InValidType:  typeAsStr = "InValidType";  break;
    case PythonSourceRoot::NoneType:     typeAsStr = "NoneType";     break;
    case PythonSourceRoot::BoolType:     typeAsStr = "BoolType";     break;
    case PythonSourceRoot::IntType:      typeAsStr = "IntType";      break;
    case PythonSourceRoot::FloatType:    typeAsStr = "FloatType";    break;
    case PythonSourceRoot::StringType:   typeAsStr = "StringType";   break;
    case PythonSourceRoot::BytesType:    typeAsStr = "BytesType";    break;
    case PythonSourceRoot::ListType:     typeAsStr = "ListType";     break;
    case PythonSourceRoot::TupleType:    typeAsStr = "TupleType";    break;
    case PythonSourceRoot::SetType:      typeAsStr = "SetType";      break;
    case PythonSourceRoot::RangeType:    typeAsStr = "RangeType";    break;
    case PythonSourceRoot::DictType:     typeAsStr = "DictType";     break;
    case PythonSourceRoot::FunctionType: typeAsStr = "FunctionType"; break;
    case PythonSourceRoot::ClassType:    typeAsStr = "ClassType";    break;
    case PythonSourceRoot::MethodType:   typeAsStr = "MethodType";   break;
    case PythonSourceRoot::CustomType:   typeAsStr = "CustomType";   break;
    default:
        typeAsStr = "InValidType";
    }

    return QLatin1String(typeAsStr);
}

QString PythonSourceRoot::TypeInfo::customName() const
{
    if (customNameIdx > -1) {

    }
    return QString();
}

// ------------------------------------------------------------------------


PythonSourceListNodeBase::PythonSourceListNodeBase(PythonSourceListParentBase *owner) :
    m_previous(nullptr), m_next(nullptr),
    m_owner(owner), m_token(nullptr)
{
    assert(owner != nullptr && "Must have valid owner");
}

PythonSourceListNodeBase::~PythonSourceListNodeBase()
{
    if (m_token)
        m_token->srcLstNode = nullptr;
    if (m_owner)
        m_owner->remove(this);
}

// this should only be called from PythonToken destructor when it gets destroyed
void PythonSourceListNodeBase::tokenDeleted()
{
    m_token = nullptr;
    if (m_owner)
        m_owner->remove(this, true); // remove me from owner and let him delete me
}

// -------------------------------------------------------------------------


PythonSourceListParentBase::PythonSourceListParentBase(PythonSourceListParentBase *owner) :
    PythonSourceListNodeBase(owner),
    m_first(nullptr), m_last(nullptr)
{
}

PythonSourceListParentBase::~PythonSourceListParentBase()
{
    // delete all children
    PythonSourceListNodeBase *n = m_first,
                             *tmp = nullptr;
    while(n) {
        n->setOwner(nullptr);
        tmp = n;
        n = n->next();
        delete tmp;
    }
}

void PythonSourceListParentBase::add(PythonSourceListNodeBase *node)
{
    if (m_last) {
        // look up the place for this
        PythonSourceListNodeBase *n = m_last,
                                 *tmp = nullptr;
        while (n != nullptr) {
            int res = compare(node, n);
            if (res < 0) {
                // n is less than node, inert after n
                tmp = n->next();
                n->setNext(node);
                node->setPrevious(n);
                if (tmp) {
                    tmp->setPrevious(node);
                    node->setNext(tmp);
                } else {
                    node->setNext(nullptr);
                    m_last = node;
                    break;
                }
            } else if (n->previous() == nullptr) {
                // n is more than node and we are at the beginning (won't iterate further)
                node->setNext(n);
                n->setPrevious(node);
                m_first = node;
                node->setPrevious(nullptr);
                break;
            }
            n = n->previous();
        }
    } else {
        m_first = node;
        m_last = node;
    }
}

bool PythonSourceListParentBase::remove(PythonSourceListNodeBase *node, bool deleteNode)
{
    if (contains(node)) {
        // remove from list
        if (node->previous())
            node->previous()->setNext(node->next());
        if (node->next())
            node->next()->setPrevious(node->previous());
        if (m_first == node)
            m_first = node->next();
        if (m_last == node)
            m_last = node->previous();

        node->setNext(nullptr);
        node->setPrevious(nullptr);
        if (deleteNode)
            delete node;

        // if we are empty we should remove ourselfs
        if (m_first == nullptr && m_last == nullptr) {
            m_owner->remove(this, true);
        }
        return true;

    }

    return false;
}

bool PythonSourceListParentBase::contains(PythonSourceListNodeBase *node) const
{
    PythonSourceListNodeBase *f = m_first,
                             *l = m_last;
    // we search front and back to be 1/2 n at worst lookup time
    while(f && l) {
        if (f == node || l == node)
            return true;
        f = f->next();
        l = l->previous();
    }

    return false;
}

bool PythonSourceListParentBase::empty() const
{
    return m_first == nullptr && m_last == nullptr;
}

std::size_t PythonSourceListParentBase::size() const
{
    PythonSourceListNodeBase *f = m_first;
    std::size_t cnt = -1;
    while(f) {
        ++cnt;
        f = f->next();
    }

    return cnt;
}

std::size_t PythonSourceListParentBase::indexOf(PythonSourceListNodeBase *node) const
{
    std::size_t idx = -1;
    PythonSourceListNodeBase *f = m_first;
    while (f) {
        ++idx;
        if (f == node)
            break;
        f = f->next();
    }

    return idx;
}

PythonSourceListNodeBase *PythonSourceListParentBase::operator [](std::size_t idx) const
{
    PythonSourceListNodeBase *n = m_first;
    for (std::size_t i = 0; i < idx; ++i) {
        if (n == nullptr)
            return nullptr;
        n = n->next();
    }
    return n;
}

int PythonSourceListParentBase::compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const
{
    if (!left || ! right)
        return 0;
    if (left->token() > right->token())
        return -1;
    else if (left->token() < right->token())
        return +1;
    return 0;
}

// ------------------------------------------------------------------------


PythonSourceIdentifierAssignment::PythonSourceIdentifierAssignment(PythonSourceIdentifier *owner,
                                                                   PythonToken *token,
                                                                   PythonSourceRoot::DataTypes type) :
    PythonSourceListNodeBase(owner),
    m_linenr(-1)
{
    m_type.type = type;
    assert(token != nullptr && "Must have valid token");
    assert(token->srcLstNode == nullptr && "token is already taken");
    m_token = token;
    m_token->srcLstNode = this;
}

PythonSourceIdentifierAssignment::~PythonSourceIdentifierAssignment()
{
}

int PythonSourceIdentifierAssignment::linenr()
{
    if (m_linenr < 0) {
        m_linenr = m_token->txtBlock->block().blockNumber();
    }
    return m_linenr;
}

// ------------------------------------------------------------------------

PythonSourceIdentifier::PythonSourceIdentifier(PythonSourceListParentBase *owner, PythonSourceFrame *frame) :
    PythonSourceListParentBase(owner),
    m_frame(frame)
{
    assert(m_frame != nullptr && "Must have a valid frame");
}

PythonSourceIdentifier::~PythonSourceIdentifier()
{
}

int PythonSourceIdentifier::compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const
{
    // should sort by linenr and (if same line also token startpos)
    PythonSourceIdentifierAssignment *l = dynamic_cast<PythonSourceIdentifierAssignment*>(left),
                                     *r = dynamic_cast<PythonSourceIdentifierAssignment*>(right);
    assert(l != nullptr && r != nullptr && "PythonSourceIdentifier stored non compatible node");
    if (l->linenr() > r->linenr()) {
        return -1;
    } else if (l->linenr() < r->linenr()) {
        return +1;
    } else {
        // line nr equal
        if (l->token()->startPos > r->token()->startPos)
            return -1;
        else
            return +1; // can't be at same source position
    }
}

PythonSourceIdentifierAssignment *PythonSourceIdentifier::getFromPos(int line, int pos) const
{
    PythonSourceIdentifierAssignment *l = dynamic_cast<PythonSourceIdentifierAssignment*>(m_last);
    while(l) {
        if (l->linenr() == line && l->token()->endPos < pos) {
            return l;
        } else if (l->linenr() < line) {
            return l;
        }
        l = dynamic_cast<PythonSourceIdentifierAssignment*>(l->previous());
    }
    // not found
    return nullptr;
}

// -------------------------------------------------------------------------------


PythonSourceIdentifiers::PythonSourceIdentifiers(PythonSourceListParentBase *owner, PythonSourceFrame *frame):
    PythonSourceListParentBase(owner),
    m_frame(frame)
{
    assert(frame != nullptr && "Must have a valid frame");
}

PythonSourceIdentifiers::~PythonSourceIdentifiers()
{
}

const PythonSourceIdentifier *PythonSourceIdentifiers::getIdentifier(const QString name) const
{
    PythonSourceIdentifier *i = dynamic_cast<PythonSourceIdentifier*>(m_first);
    while (i) {
        if (i->name() == name)
            return i;
        i = dynamic_cast<PythonSourceIdentifier*>(i->next());
    }
    return nullptr;
}

int PythonSourceIdentifiers::compare(PythonSourceListNodeBase *left, PythonSourceListNodeBase *right) const
{
    PythonSourceIdentifier *l = dynamic_cast<PythonSourceIdentifier*>(left),
                           *r = dynamic_cast<PythonSourceIdentifier*>(right);
    assert(l != nullptr && r != nullptr && "PythonSourceIdentifiers contained invalid nodes");
    if (l->name() > r->name())
        return -1;
    else if (r->name() > l->name())
        return +1;
    return 0;
}


// -----------------------------------------------------------------------------

PythonSourceFrame::PythonSourceFrame(PythonSourceFrame *owner):
    PythonSourceListParentBase(owner),
    m_identifiers(nullptr),
    m_arguments(nullptr),
    m_returns(nullptr),
    lastToken(nullptr)
{
    m_identifiers = new PythonSourceIdentifiers(this, this);
}

PythonSourceFrame::PythonSourceFrame(PythonSourceModule *owner):
    PythonSourceListParentBase(owner),
    m_identifiers(nullptr),
    m_arguments(nullptr),
    m_returns(nullptr),
    lastToken(nullptr)
{
    m_identifiers = new PythonSourceIdentifiers(this, this);
}

PythonSourceFrame::~PythonSourceFrame()
{
    delete m_identifiers;
    delete m_arguments;
    delete m_returns;
}

QString PythonSourceFrame::docstring() const
{
    // retrive docstring for this function
    QStringList docStrs;
    if (m_token) {
        PythonToken *token = endOfArgumentsList(m_token);
        int guard = 20;
        while(token && token->token != PythonSyntaxHighlighter::T_DelimiterSemiColon) {
            if ((--guard) <= 0)
                return QString();
            token = token->next();
        }

        if (!token)
            return QString();

        // now we are at semicolon (even with typehint)
        //  def func(arg1,arg2) -> str:
        //                            ^
        // find first string, before other stuff
        while (token) {
            if (!token->txtBlock)
                continue;
            switch (token->token) {
            case PythonSyntaxHighlighter::T_LiteralBlockDblQuote:// fallthrough
            case PythonSyntaxHighlighter::T_LiteralBlockSglQuote: {
                // multiline
                QString tmp = token->txtBlock->tokenAtAsString(token);
                docStrs << tmp.mid(3, tmp.size() - 6); // clip """
            }   break;
            case PythonSyntaxHighlighter::T_LiteralDblQuote: // fallthrough
            case PythonSyntaxHighlighter::T_LiteralSglQuote: {
                // single line
                QString tmp = token->txtBlock->tokenAtAsString(token);
                docStrs << tmp.mid(1, tmp.size() - 2); // clip "
            }   break;
            case PythonSyntaxHighlighter::T_Indent: // fallthrough
            case PythonSyntaxHighlighter::T_Comment:
                token = token->next();
                break;
            default:
                // some other token, probably some code
                token = nullptr; // bust out of while
            }
        }

        // return what we have collected
        return docStrs.join(QLatin1String("\n"));
    }

    return QString(); // no token
}

PythonSourceRoot::TypeInfo PythonSourceFrame::returnTypeHint() const
{
    PythonSourceRoot::TypeInfo tpInfo;
    if (m_token) {
        PythonToken *token = endOfArgumentsList(m_token),
                    *commentToken = nullptr;
        int guard = 10;
        while(token && token->token != PythonSyntaxHighlighter::T_DelimiterMetaData) {
            if ((--guard) <= 0)
                return tpInfo;

            if (token->token == PythonSyntaxHighlighter::T_DelimiterSemiColon) {
                // no metadata
                // we may still have type hint in a comment in row below
                while (token && token->token == PythonSyntaxHighlighter::T_Indent) {
                    token = token->next();
                    if (token && token->token == PythonSyntaxHighlighter::T_Comment) {
                        commentToken = token;
                        token = nullptr; // bust out of loops
                    }
                }

                if (!commentToken)
                    return tpInfo; // have no metadata, might have commentdata
            } else
                token = token->next();
        }

        QString annotation;

        if (token) {
            // now we are at metadata token
            //  def func(arg1: int, arg2: bool) -> MyType:
            //                                  ^
            // step one more further
            token = token->next();
            if (token)
                annotation = token->txtBlock->tokenAtAsString(token);

        } else if (commentToken) {
            // extract from comment
            // type: def func(int, bool) -> MyType
            QRegExp re(QLatin1String("type\\s*:\\s*def\\s+[_a-zA-Z]+\\w*\\(.*\\)\\s*->\\s*(\\w+)"));
            if (re.indexIn(token->txtBlock->tokenAtAsString(token)) > -1)
                annotation = re.cap(1);
        }

        if (!annotation.isEmpty()) {
            // set type and customname
            PythonSourceRoot *sr = PythonSourceRoot::instance();
            tpInfo.type = sr->mapDataType(annotation);
            if (tpInfo.type == PythonSourceRoot::CustomType) {
                tpInfo.customNameIdx = sr->indexOfCustomTypeName(annotation);
                if (tpInfo.customNameIdx < 0)
                    tpInfo.customNameIdx = sr->addCustomTypeName(annotation);
            }
        }
    }

    return tpInfo;
}

// may return nullptr on error
PythonToken *PythonSourceFrame::endOfArgumentsList(PythonToken *token) const
{
    // safely goes to closingParen of arguments list
    int parenCount = 0,
        safeGuard = 50;
    while(token && token->token != PythonSyntaxHighlighter::T_DelimiterMetaData)
    {
        if ((--safeGuard) <= 0)
            return nullptr;
        // first we must make sure we clear all parens
        if (token->token == PythonSyntaxHighlighter::T_DelimiterOpenParen)
            ++parenCount;
        else if (token->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            --parenCount;

        if (parenCount == 0 && token->token == PythonSyntaxHighlighter::T_DelimiterCloseParen)
            return token;

        token = token->next();
    }

    return nullptr;
}

// --------------------------------------------------------------------------

PythonSourceModule::PythonSourceModule(PythonSourceRoot *root) :
    PythonSourceListParentBase(this),
    m_root(root),
    m_rootFrame(nullptr)
{
    m_rootFrame = new PythonSourceFrame(this);
}

PythonSourceModule::~PythonSourceModule()
{
    delete m_rootFrame;
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

// -------------------------------------------------------------------------

namespace Gui {
class JediCommonP
{
public:
    static void printErr();

    // helper functions that GIL locks and swaps interpreter

    // these must be specialized
    static QString fetchStr(Py::Object self, const char *name,
                            Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);

    static bool fetchBool(Py::Object self, const char *name,
                          Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);

    static int fetchInt(Py::Object self, const char *name,
                        Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);


    // templated functions, more generalized, reduce risk for bugs
    template <typename retT, typename objT>
    static retT fetchObj(Py::Object self, const char *name,
                         Py::Tuple *args = nullptr, Py::Dict *kw = nullptr)
    {
        JediInterpreter::SwapIn jedi;
        try {
            Py::Object o;
            o = getProperty(self, name);
            if (o.isCallable())
                o = callProperty(o, args, kw);

            if (!o.isNull())
                return retT(new objT(o));

        } catch (Py::Exception) {
            printErr();
        }
        return nullptr;
    }

    template <typename retT, typename objT, typename ptrT>
    static retT fetchList(Py::Object self, const char *name,
                          Py::Tuple *args = nullptr, Py::Dict *kw = nullptr)
    {
        JediInterpreter::SwapIn jedi;
        try {
            Py::Object o;
            o = getProperty(self, name);
            if (o.isCallable())
                o = callProperty(o, args, kw);

            if (o.isList())
                return createList<retT, objT, ptrT>(o);

        } catch (Py::Exception) {
            printErr();
        }
        return retT();
    }


    // state should be swapped and GIL lock held before entering
    template<typename retT, typename objT, typename ptrT>
    static retT createList(const Py::List lst)
    {
        retT ret;

        for (const Py::Object &o : lst) {
            objT *defObj = new objT(o);
            if (defObj->isValid())
                ret.append(ptrT(defObj));
            else
                delete defObj;
        }

        return ret;
    }

    // these methods should be called while jedi is swaped in
    static Py::Object getProperty(Py::Object self, const char *method);
    static Py::Object callProperty(Py::Object prop, Py::Tuple *args = nullptr,
                                   Py::Dict *kw = nullptr);

    static bool squelshError;
};

} // namespace Gui
bool JediCommonP::squelshError = false;
void JediCommonP::printErr()
{
    JediInterpreter::SwapIn jedi;
    if (!squelshError) {
        //PyErr_Print(); // don't use python io
        PyObject *tp, *vl, *tr;
        PyErr_Fetch(&tp, &vl, &tr);
        if (!tp)
            return;
        PyErr_NormalizeException(&tp, &vl, &tr);
        PyObject *msgo = PyObject_Str(vl);
        if (!msgo)
            return;
        const char *traceback = nullptr;
#if PY_MAJOR_VERSION >=3
        const char *msg = PyUnicode_AsUTF8(msgo);
        if (tr)
            traceback = PyUnicode_AsUTF8(tr);
#else
        const char *msg = PyBytes_AsString(msgo);
        if (tr)
            traceback = PyBytes_AsString(tr);
#endif
        if (tr)
            Base::Console().Error("Code analyser: %s\n%s", msg, traceback);
        else
            Base::Console().Error("Code analyser: %s", msg);

    }

    PyErr_Clear();
}

QString JediCommonP::fetchStr(Py::Object self, const char *name,
                              Py::Tuple *args, Py::Dict *kw)
{
    JediInterpreter::SwapIn jedi;
    try {
        Py::Object o;
        o = getProperty(self, name);
        if (o.isCallable())
            o = callProperty(o, args, kw);

        if (o.isString())
            return QString::fromStdString(Py::String(o.as_string()).as_std_string());

    } catch (Py::Exception) {
        printErr();
    }
    return QString();
}

bool JediCommonP::fetchBool(Py::Object self, const char *name,
               Py::Tuple *args, Py::Dict *kw)
{
    JediInterpreter::SwapIn jedi;
    try {
        Py::Object o;
        o = getProperty(self, name);
        if (o.isCallable())
            o = callProperty(o, args, kw);

        if (o.isBoolean())
            return Py::Boolean(o);

    } catch (Py::Exception) {
        printErr();
    }
    return false;
}

int JediCommonP::fetchInt(Py::Object self, const char *name,
             Py::Tuple *args, Py::Dict *kw)
{
    JediInterpreter::SwapIn jedi;
    try {
        Py::Object o;
        o = getProperty(self, name);
        if (o.isCallable())
            o = callProperty(o, args, kw);

        if (o.isNumeric())
            return static_cast<int>(Py::Long(o).as_long());

    } catch (Py::Exception) {
        printErr();
    }
    return -1;
}

Py::Object JediCommonP::getProperty(Py::Object self, const char *method)
{
    try {
        return self.getAttr(method);

    } catch(Py::Exception e) {
        printErr();
    }

    return Py::Object();
}

Py::Object JediCommonP::callProperty(Py::Object prop,
                                     Py::Tuple *args, Py::Dict *kw)
{
    Py::Object res;
    try {
        PyObject *a = args ? args->ptr() : Py::Tuple().ptr();
        PyObject *k = kw ? kw->ptr() : nullptr;
        res = PyObject_Call(prop.ptr(), a, k);
        if (res.isNull())
            throw Py::Exception(); // error msg is already set

    } catch(Py::Exception e) {
        printErr();
    }

    return res;
}




// -------------------------------------------------------------------------

JediBaseDefinitionObj::JediBaseDefinitionObj(Py::Object obj) :
    m_obj(obj), m_type(JediBaseDefinitionObj::Base)
{
}

JediBaseDefinitionObj::~JediBaseDefinitionObj()
{
}

QString JediBaseDefinitionObj::name() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "name");
}

QString JediBaseDefinitionObj::type() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "type");
}

QString JediBaseDefinitionObj::module_name() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "module_name");
}

QString JediBaseDefinitionObj::module_path() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "module_name");
}

bool JediBaseDefinitionObj::in_builtin_module() const
{
    if (!isValid())
        return false;
    return JediCommonP::fetchBool(m_obj, "in_builtin_module");
}

int JediBaseDefinitionObj::line() const
{
    if (!isValid())
        return -1;
    return JediCommonP::fetchInt(m_obj, "line");
}

int JediBaseDefinitionObj::column() const
{
    if (!isValid())
        return -1;
    return JediCommonP::fetchInt(m_obj, "column");
}

QString JediBaseDefinitionObj::docstring(bool raw, bool fast) const
{
    if (!isValid())
        return QString();

    JediInterpreter::SwapIn me;
    try {
        Py::Dict kw;
        Py::Tuple args;
        kw["raw"] = Py::Boolean(raw);
        kw["fast"] = Py::Boolean(fast);
        return JediCommonP::fetchStr(m_obj,"docstring", &args, &kw);

    } catch (Py::Exception) {
        JediCommonP::printErr();
    }

    return QString();
}

QString JediBaseDefinitionObj::description() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "description");
}

QString JediBaseDefinitionObj::full_name() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "full_name");
}

JediDefinition_list_t JediBaseDefinitionObj::goto_assignments() const
{
    if (!isValid())
        return JediDefinition_list_t();
    return JediCommonP::fetchList
                <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                (m_obj, "goto_assignments", nullptr, nullptr);
}

JediDefinition_list_t JediBaseDefinitionObj::goto_definitions() const
{
    if (!isValid())
        return JediDefinition_list_t();

    bool printErr = JediCommonP::squelshError;

    JediCommonP::squelshError = true;
    JediDefinition_list_t res = JediCommonP::fetchList
                                      <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                                (m_obj, "goto_definitions", nullptr, nullptr);

    // questionable workaround, this function isn't public yet according to comment in code it seems be soon
    if (!res.size())
        res = JediCommonP::fetchList
            <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                      (m_obj, "_goto_definitions", nullptr, nullptr);

    JediCommonP::squelshError = printErr;

    return res;
}

JediDefinition_list_t JediBaseDefinitionObj::params() const
{
    if (!isValid())
        return JediDefinition_list_t();
    return JediCommonP::fetchList
                <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                            (m_obj, "params", nullptr, nullptr);

}

JediDefinition_ptr_t JediBaseDefinitionObj::parent() const
{
    if (!isValid())
        return nullptr;
    return JediCommonP::fetchObj<JediDefinition_ptr_t, JediDefinitionObj>
                                    (m_obj, "parent", nullptr, nullptr);
}

int JediBaseDefinitionObj::get_line_code(int before, int after) const
{
    if (!isValid())
        return -1;

    JediInterpreter::SwapIn me;
    try {
        Py::Dict kw;
        Py::Tuple args;
        kw["before"] = Py::Long(before);
        kw["after"] = Py::Long(after);

        return JediCommonP::fetchInt(m_obj, "get_line_code", &args, &kw);

    } catch (Py::Exception) {
        JediCommonP::printErr();
    }
    return -1;
}

JediDefinitionObj *JediBaseDefinitionObj::toDefinitionObj()
{
    if (!isValid())
        return nullptr;

    return dynamic_cast<JediDefinitionObj*>(this);
}

JediCompletionObj *JediBaseDefinitionObj::toCompletionObj()
{
    if (!isValid())
        return nullptr;

    return dynamic_cast<JediCompletionObj*>(this);
}

JediCallSignatureObj *JediBaseDefinitionObj::toCallSignatureObj()
{
    if (!isValid())
        return nullptr;

    return dynamic_cast<JediCallSignatureObj*>(this);
}

bool JediBaseDefinitionObj::isDefinitionObj()
{
    return m_type == Definition;
}

bool JediBaseDefinitionObj::isCompletionObj()
{
    return m_type == Completion;
}

bool JediBaseDefinitionObj::isCallSignatureObj()
{
    return m_type == CallSignature;
}


// ------------------------------------------------------------------------

JediCompletionObj::JediCompletionObj(Py::Object obj) :
    JediBaseDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::Completion;
}

JediCompletionObj::~JediCompletionObj()
{
}

QString JediCompletionObj::complete() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "complete");
}

QString JediCompletionObj::name_with_symbols() const
{
    if (!isValid())
        return QString();
    return JediCommonP::fetchStr(m_obj, "name_with_symbols");
}


// ------------------------------------------------------------------------

JediDefinitionObj::JediDefinitionObj(Py::Object obj) :
    JediBaseDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::Definition;
}

JediDefinitionObj::~JediDefinitionObj()
{
}

JediDefinition_list_t JediDefinitionObj::defined_names() const
{
    if (!isValid())
        return JediDefinition_list_t();

    return JediCommonP::fetchList
            <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                            (m_obj, "defined_names", nullptr, nullptr);
}

bool JediDefinitionObj::is_definition() const
{
    if (!isValid())
        return false;
    return JediCommonP::fetchBool(m_obj, "is_definition", nullptr, nullptr);
}

// -----------------------------------------------------------------------

JediCallSignatureObj::JediCallSignatureObj(Py::Object obj) :
    JediDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::CallSignature;
}

JediCallSignatureObj::~JediCallSignatureObj()
{
}

JediDefinition_ptr_t JediCallSignatureObj::index() const
{
    if (!isValid())
        return nullptr;
    return JediCommonP::fetchObj<JediDefinition_ptr_t, JediDefinitionObj>
                            (m_obj, "index", nullptr, nullptr);
}

int JediCallSignatureObj::bracket_start() const
{
    if (!isValid())
        return -1;
    return JediCommonP::fetchInt(m_obj, "bracket_start");
}



// -------------------------------------------------------------------------

JediScriptObj::JediScriptObj(const QString source, int line, int column,
                             const QString path, const QString encoding)
{
    if (!JediInterpreter::instance()->isValid())
        return;

    //Base::PyGILStateRelease release;
    JediInterpreter::SwapIn jedi;

    try {
        Py::Dict kw;
        Py::Tuple args;
        kw["source"] = Py::String(source.toUtf8().constData());//source.toStdString());
        kw["line"] = Py::Long(line);
        kw["column"] = Py::Long(column);
        kw["path"] = Py::String(path.toStdString());
        kw["encoding"] = Py::String(encoding.toStdString());

        // construct a new object
        PyObject *module = JediInterpreter::instance()->api().ptr();
        PyObject *dict = PyModule_GetDict(module);
        if(!dict)
            throw Py::AttributeError("Module error in jedi.api.*");

        PyObject *clsType = PyObject_GetItem(dict, Py::String("Script").ptr());
        if (!clsType)
            throw Py::AttributeError("jedi.api.String was not found");

        PyObject *newCls(PyObject_Call(clsType, args.ptr(), kw.ptr()));
        if (!newCls)
            throw Py::Exception(); // error message already set by PyObject_Call

        m_obj = newCls;

    } catch (Py::Exception e) {
        JediCommonP::printErr();
    }
}

JediScriptObj::~JediScriptObj()
{
}

JediCompletion_list_t JediScriptObj::completions() const
{
    if (!isValid())
        return JediCompletion_list_t();

    return JediCommonP::fetchList
                <JediCompletion_list_t, JediCompletionObj, JediCompletion_ptr_t>
                            (m_obj, "completions", nullptr, nullptr);
}

JediDefinition_list_t JediScriptObj::goto_definitions() const
{
    if (!isValid())
        return JediDefinition_list_t();

    return JediCommonP::fetchList
                <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                            (m_obj, "goto_definitions", nullptr, nullptr);
}

JediDefinition_list_t JediScriptObj::goto_assignments(bool follow_imports) const
{
    if (!isValid())
        return JediDefinition_list_t();

    JediInterpreter::SwapIn jedi;

    try {
        Py::Tuple args(1);
        args[0] = Py::Boolean(follow_imports);

        return JediCommonP::fetchList
                    <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                (m_obj, "goto_assignments", &args, nullptr);

    } catch (Py::Exception e) {
        JediCommonP::printErr();
    }

    return JediDefinition_list_t();
}

JediDefinition_list_t JediScriptObj::usages(QStringList additional_module_paths) const
{
    if (!isValid())
        return JediDefinition_list_t();

    JediInterpreter::SwapIn jedi;

    try {
        Py::Tuple args(additional_module_paths.size());
        for (int i = 0; i < additional_module_paths.size(); ++i)
            args[i] = Py::String(additional_module_paths[i].toStdString());

        return JediCommonP::fetchList
                    <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                (m_obj, "usages", &args, nullptr);

    } catch (Py::Exception e) {
        JediCommonP::printErr();
    }

    return JediDefinition_list_t();
}

JediCallSignature_list_t JediScriptObj::call_signatures() const
{
    if (!isValid())
        return JediCallSignature_list_t();

    return JediCommonP::fetchList
                    <JediCallSignature_list_t, JediCallSignatureObj, JediCallSignature_ptr_t>
                            (m_obj, "call_signatures", nullptr, nullptr);
}

bool JediScriptObj::isValid() const
{
    return JediInterpreter::instance()->isValid() &&
           m_obj.ptr() != nullptr;
}

//--------------------------------------------------------------------------

JediDebugProxy::JediDebugProxy()
    : Py::ExtensionModule<JediDebugProxy>( "JediDebugProxy" )
{
    add_varargs_method("callback", &JediDebugProxy::proxy, "callback(color, str)");
    initialize( "Module for debug callback to C++" );
}

JediDebugProxy::~JediDebugProxy()
{
}

Py::Object JediDebugProxy::proxy(const Py::Tuple &args)
{
    try {
        Py::Tuple argsTuple(args);
        if (argsTuple.length() >= 2) {
            std::string color = Py::String(argsTuple[0]).as_std_string();
            std::string str = Py::String(argsTuple[1]).as_std_string();
            // need to lock here if our receivers uses anything python in receiving slots
            Base::PyGILStateLocker lock;
            Q_EMIT JediInterpreter::instance()->onDebugInfo(QString::fromStdString(color),
                                                            QString::fromStdString(str));
        }

    } catch (Py::Exception e) {
        e.clear();
    }
    return Py::None();
}
const char *JediDebugProxy::ModuleName = "_jedi_debug_proxy";

//-------------------------------------------------------------------------

JediInterpreter::JediInterpreter() :
    m_jedi(nullptr), m_api(nullptr)
{
    assert(m_instance == nullptr && "JediInterpreter must be singleton");
    m_instance = const_cast<JediInterpreter*>(this);
    m_evtLoop = new QEventLoop(this);

    // reconstruct command line args passed to application on startup
    QStringList args = QApplication::arguments();
#if PY_MAJOR_VERSION >= 3
    wchar_t **argv = new wchar_t*[args.size()];
#else
    char **argv = new char*[args.size()];
#endif
    size_t argc = 0;

    for (const QString str : args) {
#if PY_MAJOR_VERSION >= 3
        wchar_t *dest = new wchar_t[sizeof(wchar_t) * (str.size())];
        str.toWCharArray(dest);
#else
        char *src = str.toLatin1().data();
        char *dest = new char[sizeof(char) * (strlen(src))];
        strcpy(dest, src);
#endif
        argv[argc++] = dest;
    }

    //Base::PyGILStateLocker lock;
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();


    // need to redirect in new interpreter
    PyObject *stdout = PySys_GetObject("stdout");
    PyObject *stderr = PySys_GetObject("stderr");
    auto *pyhome = Py_GetPythonHome();
    // halt possible current tracing/debugging
    PyEval_SetTrace(haltMainInterpreter, NULL);

    // create subinterpreter and set up its threadstate
    PyThreadState *mainInterp = PyThreadState_Swap(NULL); // take reference to main python


    if (!Py_IsInitialized())
        Py_InitializeEx(0);

    m_interpState = Py_NewInterpreter();// new sub interpreter
    m_threadState = m_interpState;// PyThreadState_New(m_interpState->interp);
    PyThreadState_Swap(m_threadState);
    PySys_SetArgvEx(argc, argv, 0);
    int sr = PySys_SetObject("stdout", stdout);
    int er = PySys_SetObject("stderr", stderr);
    Py_SetPythonHome(pyhome);

    // restore to main interpreter and release GIL lock
    PyThreadState_Swap(mainInterp);

    // reset halt for main tread
    PyEval_SetTrace(NULL, NULL);
    m_evtLoop->quit();
    PyGILState_Release(gstate);


    // delete argc again
    while(argc > 0)
        delete [] argv[--argc];
    delete [] argv;


    // is py interpreter is setup now?
    if (sr < 0 || er < 0 || !m_threadState) {
        // error handling
        destroy();

    } else {
        // we have a working interpreter

        SwapIn jedi; // scopeguarded to handle uncaught exceptions
                     // from here on its the new interpreter due to scope guard above

        Py_XINCREF(Py::Module("__main__").ptr()); // adds main module to new interpreter

        // init with default modules
        //Base::Interpreter().runString(Base::ScriptFactory().ProduceScript("CMakeVariables"));
        //Base::Interpreter().runString(Base::ScriptFactory().ProduceScript("FreeCADInit"));

        try {
            // init jedi
            // must use pointers here as Py::Module explicitly has no empty constructor
            PyObject *m = PyImport_ImportModule("jedi");
            if (m && PyModule_Check(m)) {
                m_jedi = new Py::Module(m);
            } else {
                PyErr_SetString(PyExc_ImportError, "jedi not found");
                throw Py::Exception();
            }

            PyObject *api_m = PyImport_ImportModule("jedi.api");
            if (api_m && PyModule_Check(api_m)) {
                m_api = new Py::Module(api_m);
            } else {
                PyErr_SetString(PyExc_ImportError, "jedi.api not found");
                throw Py::Exception();
            }

            QStringList preLoads = {
                QLatin1String("FreeCAD"), QLatin1String("FreeCAD.App"),
                QLatin1String("FreeCAD.Gui"), QLatin1String("Part")
            };

            preload_module(preLoads);


            // TODO jedi.settings

        } catch(Py::Exception e) {
            JediCommonP::printErr();
        }
    }
}

JediInterpreter::~JediInterpreter()
{
    destroy();
}

// used to halt execution on main interpreter while we setup our subinterpreter
QEventLoop *JediInterpreter::m_evtLoop = nullptr;
int JediInterpreter::haltMainInterpreter(PyObject *obj, PyFrameObject *frame,
                                         int what, PyObject *arg)
{
    Q_UNUSED(obj);
    Q_UNUSED(frame);
    Q_UNUSED(what);
    Q_UNUSED(arg);
    m_evtLoop->exec(); // halt main interpreter
    return 0;
}

JediInterpreter *JediInterpreter::m_instance = nullptr;
JediInterpreter *JediInterpreter::instance()
{
    if (!m_instance) {
        new JediInterpreter; // instance gets set in Constructor
    }
    return m_instance;
}

bool JediInterpreter::isValid() const
{
    return m_threadState != nullptr &&
           Py_IsInitialized() &&
           (m_jedi && !m_jedi->isNull()) &&
           (m_api && !m_api->isNull());
}

Py::Object JediInterpreter::runString(const QString &src)
{
    SwapIn myself;
    return Base::Interpreter().runStringObject(src.toLatin1());
}

Py::Module &JediInterpreter::jedi() const
{
    return *m_jedi;
}

Py::Module &JediInterpreter::api() const
{
    return *m_api;
}

PyThreadState *JediInterpreter::threadState() const
{
    return m_threadState;
}

JediDefinition_list_t JediInterpreter::names(const QString source, const QString path,
                                             const QString encoding, bool all_scopes,
                                             bool definitions, bool references) const
{
    if (!isValid())
        return JediDefinition_list_t();

    JediDefinition_list_t ret;
    try {
        SwapIn jedi;

        Py::Dict kw;
        Py::Tuple args;
        kw["source"] = Py::String(source.toStdString());
        kw["path"] = Py::String(path.toStdString());
        kw["encoding"] = Py::String(encoding.toStdString());
        kw["all_scopes"] = Py::Boolean(all_scopes);
        kw["definitions"] = Py::Boolean(definitions);
        kw["references"] = Py::Boolean(references);

        return JediCommonP::fetchList
                    <JediDefinition_list_t, JediDefinitionObj, JediDefinition_ptr_t>
                                (*m_api, "names", &args, &kw);

    } catch (Py::Exception e) {
        JediCommonP::printErr();
    }

    return ret;
}

// preload means that Jedi parsers them now instead of when detected in code
// it doesn't import them to local env. if they are not C modules
bool JediInterpreter::preload_module(const QStringList modules) const
{
    if (isValid()) {
        SwapIn jedi;

        try {
            Py::Tuple arg(modules.size());
            int i = 0;
            for (const QString &module : modules) {
                arg[i++] = Py::String(module.toStdString());
            }
            Py::Object prop = JediCommonP::getProperty(*m_jedi, "preload_module");
            JediCommonP::callProperty(prop, &arg);
            return true;

        } catch (Py::Exception e) {
            JediCommonP::printErr();
        }
    }

    return false;
}

bool JediInterpreter::setDebugging(bool on = true, bool warnings,
                                   bool notices, bool speed) const
{
    if (isValid()) {
        SwapIn jedi;
        try {
            // create and load the module
            static JediDebugProxy *proxy = nullptr;
            if (!proxy) {
                proxy = new JediDebugProxy;
                new Py::Module(JediDebugProxy::ModuleName);
            }

            Py::Dict kw;
            Py::Tuple arg;
            if (on)
                kw["debug_func"] = Py::Callable(proxy->module());
            else
                kw["debug_func"] = Py::None();
            kw["warnings"] = Py::Boolean(warnings);
            kw["notices"] = Py::Boolean(notices);
            kw["speed"] = Py::Boolean(speed);

            Py::Object prop = JediCommonP::getProperty(*m_jedi, "set_debug_func");
            JediCommonP::callProperty(prop, &arg, &kw);
            return true;

        } catch (Py::Exception e) {
            e.clear();
            return false;
        }
    }

    return false;
}

void JediInterpreter::destroy()
{
    if (m_interpState) {

        if (m_jedi) {
            SwapIn swap;
            delete m_jedi;
            m_jedi = nullptr;
        }

        if (m_api) {
            SwapIn swap;
            delete m_api;
            m_api = nullptr;
        }

        if (m_threadState) {
            PyThreadState_Clear(m_threadState);
            PyThreadState_Delete(m_threadState);
            m_threadState = nullptr;
        }

        Py_EndInterpreter(m_interpState);
        m_interpState = nullptr;
    }
}


// -----------------------------------------------------------------------


JediInterpreter::SwapIn::SwapIn() :
    m_oldState(nullptr)
{
    // ensure we don't re-swap and acquire lock (leads to deadlock)
    if (!static_GILHeld) {
        static_GILHeld = true;
        // acquire for global thread
        m_GILState = PyGILState_Ensure();
        m_oldState = PyThreadState_Get();

        // swap to and re-aquire lock for jedi thread
        PyGILState_Release(m_GILState);
        PyThreadState_Swap(JediInterpreter::instance()->threadState());
        m_GILState = PyGILState_Ensure();
    }
}

JediInterpreter::SwapIn::~SwapIn()
{
    // ensure we only swap back if this isntance was the swapping one
    if (m_oldState) {
        // release from jedi thread and swap back to old thread
        PyGILState_Release(m_GILState);
        PyThreadState_Swap(m_oldState);

        if (static_GILHeld)
            static_GILHeld = false;
    }
}
bool JediInterpreter::SwapIn::static_GILHeld = false;


#include "moc_PythonCode.cpp"
