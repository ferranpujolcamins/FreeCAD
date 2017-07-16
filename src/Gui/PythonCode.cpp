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
#include "PythonDebugger.h"
#include "Application.h"
#include "BitmapFactory.h"
#include "Macro.h"
#include <Base/PyTools.h>

#include <CXX/Objects.hxx>
#include <Base/PyObjectBase.h>
#include <Base/Interpreter.h>
#include <Base/Exception.h>
#include <Base/Parameter.h>
#include <QApplication>
#include <QDebug>

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
    PyFrameObject *current_frame = PythonDebugger::instance()->currentFrame();
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
                char *name;
                name = PyBytes_AS_STRING(key);
                if (name != nullptr)
                    builtins << QString(QLatin1String(name));
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
    int i = 0;
    QChar prev, ch;
    int blockPos = currentBlock().position();

    d->activeBlock = new PythonTextBlockData(currentBlock());
    setCurrentBlockUserData(d->activeBlock);

    d->endStateOfLastPara = static_cast<PythonSyntaxHighlighter::Tokens>(previousBlockState());
    if (d->endStateOfLastPara < T_Undetermined)
        d->endStateOfLastPara = T_Undetermined;

    bool isModuleLine = false;

    while ( i < text.length() )
    {
        ch = text.at( i );

        switch ( d->endStateOfLastPara )
        {
        case T_Undetermined:
        {
            char nextCh = 0, thirdCh =0;
            if (text.length() >= i+1)
                nextCh = text.at(i+1).unicode();
            if (text.length() >= i+2)
                thirdCh = text.at(i+2).unicode();

            switch ( ch.unicode() )
            {
            // all these chars are described at: https://docs.python.org/3/reference/lexical_analysis.html
            case '#':
                // begin a comment
                // it goes on to the end of the row
                setRestOfLine(i, text, Tokens::T_Comment, TColor::Comment); // function advances i
                break;
            case '"':
            {
                // Begin either string literal or block string
                char compare = '"';
                if (nextCh == compare && thirdCh == compare) {
                    // block string
                    int len = lastDblQuoteStringCh(i+3, text);
                    int endPos = text.indexOf(QLatin1String("\"\"\""), len);
                    if (endPos != -1)
                        len = endPos;

                    setLiteral(i, len, T_LiteralBlockDblQuote, StringBlockDoubleQoute);

                } else {
                    // single line string
                    int len = lastDblQuoteStringCh(i + 1, text);
                    if ((text.length() >= i + len +1) && (text.at(i+len) == QLatin1Char(compare)))
                        len += 1;

                    setLiteral(i, len, T_LiteralDblQuote, StringDoubleQoute);
                }

            } break;
            case '\'':
            {
                // Begin either string literal or block comment
                //int startPos = text.indexOf(QLatin1String("'''"), i)
                char compare = '\'';
                if (nextCh == compare && thirdCh == compare) {
                    // block string
                    int len = lastSglQuoteStringCh(i+3, text);
                    int endPos = text.indexOf(QLatin1String("'''"), len);
                    if (endPos != -1)
                        len = endPos;

                    setLiteral(i, len, T_LiteralBlockSglQuote, StringBlockSingleQoute);

                } else {
                    // single line string
                    int len = lastSglQuoteStringCh(i + 1, text);
                    if ((text.length() >= i + len +1) && (text.at(i+len) == QLatin1Char(compare)))
                        len += 1;

                    setLiteral(i, len, T_LiteralSglQuote, StringSingleQoute);
                }

            } break;

            // handle indentation
            case ' ': case '\t':
            {
                if (d->activeBlock->isEmpty()) {
                    int count = 0, j = i;

                    while (j++ < text.length()) {
                        if (text.at(i) == QLatin1Char(' ')) {
                            ++i;
                            ++count;
                        } else if (text.at(i) == QLatin1Char('\t')) {
                            ++i;
                            count += 4; // TODO Better figure out how to handle tabs
                        } else {
                            i = j -1;
                            break;
                        }
                    }
                    setIndentation(i, count);
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
                if (nextCh == ch.unicode()) {
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
                    setOperator(i, 1, T_OperatorAssign);
                    break;
                }
                setOperator(i, 1, T_Operator);
                break;
            case '!': case '=':
                setOperator(i, 1, T_OperatorCompare);
            // only single char
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
            {
                if (text.length() >= i + 1) {
                   if(text.at(i+1).isLetter()) {
                       int len = lastWordCh(i + 1, text);
                       setWord(i, len, T_IdentifierDecorator, TColor::IdentifierDecorator);
                       break;
                   } else if (text.at(i+1) == QLatin1Char('=')) {
                       setOperator(i, 2, T_OperatorAssign);
                       break;
                   }
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
                // Check for normal text
                if ( ch.isLetter() || ch == QLatin1Char('_') )
                {
                    int endCh = lastWordCh(i, text);
                    QString word = text.mid(i, endCh);

                    if ( d->keywords.contains( word ) != 0 ) {
                        if (word == QLatin1String("def")) {
                            setBoldWord(i, endCh, Tokens::T_KeywordDef, SyntaxHighlighter::KeywordDef);
                            d->endStateOfLastPara = T_IdentifierDefUnknown; // step out to handle def name

                        } else if (word == QLatin1String("class")) {
                            setBoldWord(i, endCh, Tokens::T_KeywordClass, SyntaxHighlighter::KeywordClass);
                            d->endStateOfLastPara = T_IdentifierClass; // step out to handle class name

                        } else if (word == QLatin1String("import")) {
                            setBoldWord(i, endCh, Tokens::T_KeywordImport, SyntaxHighlighter::Keyword);
                            d->endStateOfLastPara = T_IdentifierModule; // step out to handle module name
                            isModuleLine = true;

                        } else if (word == QLatin1String("from")) {
                            setBoldWord(i, endCh, Tokens::T_KeywordFrom, SyntaxHighlighter::Keyword);
                            d->endStateOfLastPara = T_IdentifierModule; // step out handle module name
                            isModuleLine = true;

                        } else if (word == QLatin1String("as")) {
                            setBoldWord(i, endCh, T_KeywordAs, SyntaxHighlighter::Keyword);
                            if (isModuleLine)
                                d->endStateOfLastPara = T_IdentifierModule;
                            else
                                d->endStateOfLastPara = T_Undetermined;
                        } else if (word == QLatin1String("in")) {
                            setBoldWord(i, endCh, T_KeywordIn, SyntaxHighlighter::Keyword);
                            d->endStateOfLastPara = T_Undetermined;

                        } else {
                            setBoldWord(i, endCh, Tokens::T_Keyword, SyntaxHighlighter::Keyword);
                        }

                    } else if(d->builtins.contains(word)) {
                        setBoldWord(i, endCh, Tokens::T_IdentifierBuiltin, SyntaxHighlighter::IdentifierBuiltin);
                    } else if (isModuleLine) {
                        i -= 1; // jump to module block
                        d->endStateOfLastPara = T_IdentifierModule;
                    } else {
                        setUndeterminedIdentifier(i, endCh, T_IdentifierUnknown, SyntaxHighlighter::IdentifierUnknown);
                    }

                    break;
                    //i += word.length(); setWord increments
                }

                // is it the beginning of a number?
                int endPos = lastNumberCh(i, text);
                if ( endPos > i) {
                    setNumber(i, endPos, numberType(text));
                    break;
                }
                // its a error if we ever get here
                setSyntaxError(i, 1);


                qDebug() << "unknown state for: " << qPrintable(ch) <<
                          " row:"<< qPrintable(blockPos) <<
                          " col:" << qPrintable(i) << endl;

            } // end default:
            } // end switch

        } break;// end T_Undetermined

        // we should only arrive here when we start on a new line
        case T_LiteralBlockDblQuote:
        {
            // multiline double quote string continued from previous line
            int endPos = text.indexOf(QLatin1String("\"\"\""), i);
            if (endPos != -1) {
                setLiteral(i, endPos, T_LiteralBlockSglQuote, StringBlockSingleQoute);
                d->endStateOfLastPara = T_Undetermined;
            }

        } break;
        case T_LiteralBlockSglQuote:
        {
            // multiline single quote string continued from previous line
            int endPos = text.indexOf(QLatin1String("'''"), i);
            if (endPos != -1) {
                setLiteral(i, endPos, T_LiteralBlockSglQuote, StringBlockSingleQoute);
                d->endStateOfLastPara = T_Undetermined;
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
            setUndeterminedIdentifier(i, len, T_IdentifierDefUnknown, SyntaxHighlighter::IdentifierUnknown);
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
            setUndeterminedIdentifier(i, len, T_IdentifierClass, SyntaxHighlighter::IdentifierClass);
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
            setUndeterminedIdentifier(i, len, T_IdentifierModule, SyntaxHighlighter::IdentifierModule);
            d->endStateOfLastPara = T_Undetermined;

        } break;
        default:
        {
            if (d->endStateOfLastPara > T_Invalid)
                break;  // pythonCosole has some special values

            setSyntaxError(i, lastWordCh(i, text));
            qDebug() << "unknown state for: " << qPrintable(ch) <<
                        " row:"<< qPrintable(blockPos) <<
                        " col:" << qPrintable(i) << endl;
        }
        } // end switch

        prev = ch;
        i++;
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
        if (!ch.isLetterOrNumber() || ch != QLatin1Char('_'))
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
    bool escape = false;
    int maxLen = text.length();

    for (; len < maxLen; ++len) {
        if (text.at(len) == QLatin1Char('\\')) {
            escape = true;
            continue;
        }
        if (escape)
            escape = false;
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
    pos += len;
}

void PythonSyntaxHighlighter::setWord(int &pos, int len,
                                      PythonSyntaxHighlighter::Tokens token,
                                      SyntaxHighlighter::TColor color)
{
    setFormat(pos, len, this->colorByType(color));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len;
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
    pos += len;
}


void PythonSyntaxHighlighter::setIdentifier(int &pos, int len, Tokens token, TColor color)
{
    setFormat(pos, len, this->colorByType(color));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len;
}


void PythonSyntaxHighlighter::setUndeterminedIdentifier(int &pos, int len,
                                                        Tokens token, TColor color)
{
    setFormat(pos, len, this->colorByType(color));

    // let parse tree figure out and color at a later stage
    d->activeBlock->setUndeterminedToken(token, pos, len);
    pos += len;
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
    pos += len;
}

void PythonSyntaxHighlighter::setOperator(int &pos, int len,
                                          PythonSyntaxHighlighter::Tokens token)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::Operator));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len;

}

void PythonSyntaxHighlighter::setDelimiter(int &pos, int len,
                                           PythonSyntaxHighlighter::Tokens token)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::Delimiter));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len;
}

void PythonSyntaxHighlighter::setSyntaxError(int &pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::SyntaxError));
    d->activeBlock->setDeterminedToken(T_SyntaxError, pos, len);
    pos += len;
}

void PythonSyntaxHighlighter::setLiteral(int &pos, int len,
                                         Tokens token, SyntaxHighlighter::TColor color)
{
    setFormat(pos, len, this->colorByType(color));
    d->activeBlock->setDeterminedToken(token, pos, len);
    pos += len;
}

void PythonSyntaxHighlighter::setIndentation(int &pos, int len)
{
    d->activeBlock->setDeterminedToken(T_Indent, pos, len);
    pos += len;
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

PythonToken::PythonToken(PythonSyntaxHighlighter::Tokens token, int startPos, int endPos) :
    token(token), startPos(startPos), endPos(endPos)
{
}

PythonToken::~PythonToken()
{
}

// -------------------------------------------------------------------------------------------

PythonTextBlockData::PythonTextBlockData(QTextBlock block) :
    m_block(block)
{
}

PythonTextBlockData::~PythonTextBlockData()
{
    //qDeleteAll(m_matchingChrs);
    qDeleteAll(m_tokens);
}

const QTextBlock &PythonTextBlockData::block() const
{
    return m_block;
}

const QVector<PythonToken *> &PythonTextBlockData::tokens() const
{
    return m_tokens;
}

void PythonTextBlockData::setDeterminedToken(PythonSyntaxHighlighter::Tokens token,
                                             int startPos, int len)
{
    PythonToken *tokenObj = new PythonToken(token, startPos, startPos + len);
    m_tokens.push_back(tokenObj);
}

void PythonTextBlockData::setUndeterminedToken(PythonSyntaxHighlighter::Tokens token,
                                               int startPos, int len)
{
    // TODO should call parse tree here to notify that we need to determine this
    setDeterminedToken(token, startPos, startPos + len);
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

const PythonToken *PythonTextBlockData::tokenAt(int pos) const
{
    for (const PythonToken *tok : m_tokens) {
        if (tok->startPos <= pos && tok->endPos >= pos)
            return tok;
    }

    return nullptr;
}

QString PythonTextBlockData::tokenAtAsString(int pos) const
{
    const PythonToken *tok = tokenAt(pos);
    if (!tok)
        return QString();

    QTextCursor cursor(m_block);
    cursor.setPosition(m_block.position());
    cursor.setPosition(cursor.position() + tok->startPos, QTextCursor::KeepAnchor);
    return cursor.selectedText();
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

QString PythonTextBlockData::indention() const
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

/*
QVector<MatchingCharInfo *> PythonTextBlockData::matchingChars()
{
    return m_matchingChrs;
}


void PythonTextBlockData::insert(MatchingCharInfo *info)
{
    int i = 0;
    while (i < m_matchingChrs.size() &&
           info->position > m_matchingChrs.at(i)->position)
        ++i;

    m_matchingChrs.insert(i, info);
}

void PythonTextBlockData::insert(char chr, int pos)
{
    MatchingCharInfo *info = new MatchingCharInfo(chr, pos);
    insert(info);
}
*/

// -------------------------------------------------------------------------------------



PythonMatchingChars::PythonMatchingChars(TextEdit *parent):
    QObject(parent),
    m_editor(parent)
   // m_lastPos1(-1),
   // m_lastPos2(-1)
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

    PythonTextBlockData *textData = static_cast<PythonTextBlockData*>(currentBlock.userData());
    if (!textData)
        return;

    // query tokens
    int linePos = cursor.position() - currentBlock.position();
    PythonSyntaxHighlighter::Tokens token1 = textData->tokenAt(linePos)->token;
    PythonSyntaxHighlighter::Tokens token2 = PythonSyntaxHighlighter::T_Invalid;

    // no token here, nothing to do
    if (token1 == PythonSyntaxHighlighter::T_Invalid)
        return;

    // which token?
    int pos1 = -1, pos2 = -1;
    bool forward = true;
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
        forward = false;
        break;
    case PythonSyntaxHighlighter::T_DelimiterCloseBracket:
        token2 = PythonSyntaxHighlighter::T_DelimiterOpenBracket;
        forward = false;
        break;
    case PythonSyntaxHighlighter::T_DelimiterCloseParen:
        token2 = PythonSyntaxHighlighter::T_DelimiterOpenParen;
        forward = false;
        break;
    default:
        return;
    }

    // if we got here we should find a matching char
    pos1 = cursor.position();
    int startPos = pos1;

    while (currentBlock.isValid()) {
        const PythonToken *tok = textData->findToken(token2, startPos);
        if (!tok) {
            pos2 = tok->startPos;
            break; // found it!
        }

        // look in next block
        currentBlock = forward ? currentBlock.next() : currentBlock.previous();
        textData = static_cast<PythonTextBlockData*>(currentBlock.userData());
        if (!textData)
            return;

        startPos = forward ? 0 : (-currentBlock.length());
    }

    // highlight both
    m_editor->highlightText(pos1, 1, format);
    m_editor->highlightText(pos2, 1, format);


/*
    PythonTextBlockData *data = nullptr;
    int startPos = cursor.position(),
        matchSkip = 0;

    // opening chars tests for opening chars (right side of cursor)
    char openingChr = cursor.document()->characterAt(cursor.position()).toLatin1();

    if (openingChr == '(' || openingChr == '[' || openingChr == '{') {
        for (QTextBlock block = cursor.block();
             block.isValid(); block = block.next())
        {
            data = static_cast<PythonTextBlockData *>(block.userData());
            if (!data)
                continue;
            QVector<MatchingCharInfo *> matchingChars = data->matchingChars();
            for (const MatchingCharInfo *match : matchingChars) {
                if (match->position <= startPos)
                    continue;
                if (match->character == openingChr) {
                    ++matchSkip;
                } else if (match->matchingChar() == openingChr) {
                    if (matchSkip) {
                        --matchSkip;
                    } else {
                        m_editor->highlightText(startPos, 1, format);
                        m_editor->highlightText(match->position, 1, format);
                        m_lastPos1 = startPos;
                        m_lastPos2 = match->position;
                        return;
                    }
                }

            }
        }
    }

    // if we get here we didnt find any mathing opening chars (char on right side of cursor)
    // test for closing char (left side of cursor)
    char rightChr = cursor.document()->characterAt(startPos -1).toLatin1();

    if (rightChr == ')' || rightChr == ']' || rightChr == '}') {
        for (QTextBlock block = cursor.block();
             block.isValid(); block = block.previous())
        {
            data = static_cast<PythonTextBlockData *>(block.userData());
            if (!data)
                continue;
            QVector<MatchingCharInfo *> matchingChars = data->matchingChars();
            QVector<const MatchingCharInfo *>::const_iterator match = matchingChars.end();
            while (match != matchingChars.begin()) {
                --match;
                if ((*match)->position >= startPos -1)
                    continue;
                if ((*match)->character == rightChr) {
                    ++matchSkip;
                } else if ((*match)->matchingChar() == rightChr) {
                    if (matchSkip) {
                        --matchSkip;
                    } else {
                        m_editor->highlightText(startPos - 1, 1, format);
                        m_editor->highlightText((*match)->position, 1, format);
                        m_lastPos1 = startPos;
                        m_lastPos2 = (*match)->position;
                        return;
                    }
                }

            }
        }
    }
    */
}

// -------------------------------------------------------------------------

namespace Gui {
struct JediCommonP
{
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
    if (!squelshError)
        PyErr_Print();

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

    // questionable workaround, this function isnt public yet according to comment in code it seems be soon
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
            // need to lock here if our recievers uses anything python in recieving slots
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
    m_instance = const_cast<JediInterpreter*>(this);

    // reconstruct command line args passed to application on startup
    QStringList args = QApplication::arguments();
    char **argv = new char*[args.size()];
    int argc = 0;

    for (const QString str : args) {
        char *src = str.toLatin1().data();
        char *dest = new char[sizeof(char) * (strlen(src))];
        strcpy(dest, src);
        argv[argc++] = dest;
    }

    Base::PyGILStateLocker lock;

    // need to redirect in new interpreter
    PyObject *stdout = PySys_GetObject("stdout");
    PyObject *stderr = PySys_GetObject("stderr");

    // create subinterpreter and set up its threadstate
    PyThreadState *oldState = PyThreadState_Swap(NULL);
    m_interpState = Py_NewInterpreter();// new sub interpreter
    m_threadState = PyThreadState_New(m_interpState->interp);
    int sr = PySys_SetObject("stdout", stdout);
    int er = PySys_SetObject("stderr", stderr);
    PySys_SetArgvEx(argc, argv, 0);

    // restore to main interpreter and release GIL lock
    PyThreadState_Swap(oldState);
    Base::PyGILStateRelease release;


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
// it doesnt import them to local env. if they are not C modules
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
    // ensure we dont re-swap and aquire lock (leads to deadlock)
    if (!static_GILHeld) {
        static_GILHeld = true;
        // aquire for global thread
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
