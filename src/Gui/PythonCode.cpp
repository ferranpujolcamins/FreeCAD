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
        endStateOfLastPara(PythonSyntaxHighlighter::Standard)
    {

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

        // https://docs.python.org/3/reference/lexical_analysis.html#keywords
        keywords << QLatin1String("False")    << QLatin1String("None")
                 << QLatin1String("None")     << QLatin1String("and")
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
    QString importName;
    QString importFrom;
    PythonSyntaxHighlighter::States endStateOfLastPara;
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
  int endPos = text.length() - 1;
  int blockPos = currentBlock().position();

  PythonTextBlockData *blockData = new PythonTextBlockData;
  setCurrentBlockUserData(blockData);

  d->endStateOfLastPara = static_cast<PythonSyntaxHighlighter::States>(previousBlockState());
  if (d->endStateOfLastPara < 0 || d->endStateOfLastPara > maximumUserState())
    d->endStateOfLastPara = Standard;

  while ( i < text.length() )
  {
    ch = text.at( i );

    switch ( d->endStateOfLastPara )
    {
    case Standard:
      {
        switch ( ch.unicode() )
        {
        case '#':
            // begin a comment
            setComment(i, 1);
            break;
        case '"':
          {
            // Begin either string literal or block comment
            if ((i>=2) && text.at(i-1) == QLatin1Char('"') &&
                text.at(i-2) == QLatin1Char('"'))
            {
              setDoubleQuotBlockComment(i-2, 3);
            } else {
              setDoubleQuotString(i, 1);
            }
          } break;
        case '\'':
          {
            // Begin either string literal or block comment
            if ((i>=2) && text.at(i-1) == QLatin1Char('\'') &&
                text.at(i-2) == QLatin1Char('\''))
            {
              setSingleQuotBlockComment(i-2, 3);
            } else {
              setSingleQuotString(i, 1);
            }
          } break;
        case ' ':
        case '\t':
          {
            // ignore whitespaces
          } break;
        case '(': case '[': case '{':
        case '}': case ')': case ']':
          {
            blockData->insert(ch.toLatin1(), blockPos + i);
            setOperator(i, 1);
          } break;
        case '+': case '-': case '*': case '/':
        case ':': case '%': case '^': case '~':
        case '!': case '=': case '<': case '>': // possibly two characters
            setOperator(i, 1);
            break;
        default:
          {
            // Check for normal text
            if ( ch.isLetter() || ch == QLatin1Char('_') )
            {
              QString buffer;
              int j=i;
              while ( ch.isLetterOrNumber() || ch == QLatin1Char('_') ) {
                buffer += ch;
                ++j;
                if (j >= text.length())
                  break; // end of text
                ch = text.at(j);
              }

              if ( d->keywords.contains( buffer ) != 0 ) {
                setKeyword(i, buffer.length());

                if ( buffer == QLatin1String("def"))
                  d->endStateOfLastPara = DefineName;
                else if ( buffer == QLatin1String("class"))
                  d->endStateOfLastPara = ClassName;
                else if ( buffer == QLatin1String("import"))
                  d->endStateOfLastPara = ImportName;
                else if ( buffer == QLatin1String("from"))
                  d->endStateOfLastPara = FromName;

              } else if(d->builtins.contains(buffer)) {
                setBuiltin(i, buffer.length());
              } else {
                setText(i, buffer.size());
              }

              // increment i
              if ( !buffer.isEmpty() )
                i = j-1;
            }
            // this is the beginning of a number
            else if ( ch.isDigit() )
            {
              setNumber(i, 1);
            }
            // probably an operator
            else if ( ch.isSymbol() || ch.isPunct() )
            {
              setOperator(i, 1);
            }
          }
        }
      } break;
    case Comment:
      {
        setFormat( i, 1, this->colorByType(SyntaxHighlighter::Comment));
      } break;
    case Literal1:
      {
        setFormat( i, 1, this->colorByType(SyntaxHighlighter::String));
        if ( ch == QLatin1Char('"') )
          d->endStateOfLastPara = Standard;
      } break;
    case Literal2:
      {
        setFormat( i, 1, this->colorByType(SyntaxHighlighter::String));
        if ( ch == QLatin1Char('\'') )
          d->endStateOfLastPara = Standard;
      } break;
    case Blockcomment1:
      {
        setFormat( i, 1, this->colorByType(SyntaxHighlighter::BlockComment));
        if ( i>=2 && ch == QLatin1Char('"') &&
            text.at(i-1) == QLatin1Char('"') &&
            text.at(i-2) == QLatin1Char('"'))
          d->endStateOfLastPara = Standard;
      } break;
    case Blockcomment2:
      {
        setFormat( i, 1, this->colorByType(SyntaxHighlighter::BlockComment));
        if ( i>=2 && ch == QLatin1Char('\'') &&
            text.at(i-1) == QLatin1Char('\'') &&
            text.at(i-2) == QLatin1Char('\''))
          d->endStateOfLastPara = Standard;
      } break;
    case DefineName:
      {
        if ( ch.isLetterOrNumber() || ch == QLatin1Char(' ') || ch == QLatin1Char('_') )
        {
          setFormat( i, 1, this->colorByType(SyntaxHighlighter::Defname));
        }
        else
        {
          if (MatchingCharInfo::matchChar(ch.toLatin1()))
            blockData->insert(ch.toLatin1(), blockPos + i);

          if ( ch.isSymbol() || ch.isPunct() )
            setFormat(i, 1, this->colorByType(SyntaxHighlighter::Operator));
          d->endStateOfLastPara = Standard;
        }
      } break;
    case ClassName:
      {
        if ( ch.isLetterOrNumber() || ch == QLatin1Char(' ') || ch == QLatin1Char('_') )
        {
          setFormat( i, 1, this->colorByType(SyntaxHighlighter::Classname));
        }
        else
        {
          if (MatchingCharInfo::matchChar(ch.toLatin1()))
            blockData->insert(ch.toLatin1(), blockPos + i);

          if (ch.isSymbol() || ch.isPunct() )
            setFormat( i, 1, this->colorByType(SyntaxHighlighter::Operator));
          d->endStateOfLastPara = Standard;
        }
      } break;
    case Digit:
      {
        if (ch.isDigit() || ch == QLatin1Char('.'))
        {
          setFormat( i, 1, this->colorByType(SyntaxHighlighter::Number));
        }
        else
        {
          if (MatchingCharInfo::matchChar(ch.toLatin1()))
            blockData->insert(ch.toLatin1(), blockPos + i);

          if ( ch.isSymbol() || ch.isPunct() )
            setFormat( i, 1, this->colorByType(SyntaxHighlighter::Operator));
          d->endStateOfLastPara = Standard;
        }
      } break;
      case ImportName:
      {
        if ( ch.isLetterOrNumber() ||
             ch == QLatin1Char('_') || ch == QLatin1Char('*')  )
        {
          QTextCharFormat format;
          format.setForeground(this->colorByType(SyntaxHighlighter::Text));
          format.setFontWeight(QFont::Bold);
          setFormat(i, 1, format);
          d->importName += ch;
        }
        else
        {
          if (ch.isSymbol() || ch.isPunct())
          {
            setFormat(i, 1, this->colorByType(SyntaxHighlighter::Operator));
            if (ch == QLatin1Char('.'))
                d->importName += ch;
          }
          else if (prev != QLatin1Char(' '))
          {
              if (ch == QLatin1Char(' '))
              {
                //d->emitName();
              }
              else if (!d->importFrom.isEmpty()) {
                  d->importFrom.clear();
              }
          }
        }
        if (i == endPos) { // last char in row
            //d->emitName();
            d->importFrom.clear();
        }
      } break;
      case FromName:
      {
        if (prev.isLetterOrNumber() && ch == QLatin1Char(' '))
        {
            // start import statement
            d->endStateOfLastPara = Standard; //ImportName;
        }
        else if ( ch.isLetterOrNumber() || ch == QLatin1Char('_') )
        {
          QTextCharFormat format;
          format.setForeground(this->colorByType(SyntaxHighlighter::Text));
          format.setFontWeight(QFont::Bold);
          setFormat(i, 1, format);
          d->importFrom += ch;
        }
      } break;
    }

    prev = ch;
    i++;
  }

  // only block comments can have several lines
  if ( d->endStateOfLastPara != Blockcomment1 && d->endStateOfLastPara != Blockcomment2 )
  {
    d->endStateOfLastPara = Standard ;
  }

  setCurrentBlockState(static_cast<int>(d->endStateOfLastPara));
}


void PythonSyntaxHighlighter::setComment(int pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::Comment));
    d->endStateOfLastPara = Comment;
}

void PythonSyntaxHighlighter::setSingleQuotString(int pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::String));
    d->endStateOfLastPara = Literal2;
}

void PythonSyntaxHighlighter::setDoubleQuotString(int pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::String));
    d->endStateOfLastPara = Literal1;

}

void PythonSyntaxHighlighter::setSingleQuotBlockComment(int pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::BlockComment));
    d->endStateOfLastPara = Blockcomment2;
}

void PythonSyntaxHighlighter::setDoubleQuotBlockComment(int pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::BlockComment));
    d->endStateOfLastPara = Blockcomment1;
}

void PythonSyntaxHighlighter::setOperator(int pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::Operator));
    d->endStateOfLastPara = Standard;
}

void PythonSyntaxHighlighter::setKeyword(int pos, int len)
{
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(this->colorByType(SyntaxHighlighter::Keyword));
    keywordFormat.setFontWeight(QFont::Bold);
    setFormat(pos, len, keywordFormat);
    d->endStateOfLastPara = Standard;
}

void PythonSyntaxHighlighter::setText(int pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::Text));
    d->endStateOfLastPara = Standard;
}

void PythonSyntaxHighlighter::setNumber(int pos, int len)
{
    setFormat(pos, len, this->colorByType(SyntaxHighlighter::Number));
    d->endStateOfLastPara = Digit;
}

void PythonSyntaxHighlighter::setBuiltin(int pos, int len)
{
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(this->colorByType(SyntaxHighlighter::Builtin));
    keywordFormat.setFontWeight(QFont::Bold);
    setFormat(pos, len, keywordFormat);
    d->endStateOfLastPara = Standard;
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



PythonTextBlockData::PythonTextBlockData()
{
}

PythonTextBlockData::~PythonTextBlockData()
{
    qDeleteAll(m_matchingChrs);
}

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

// -------------------------------------------------------------------------------------



PythonMatchingChars::PythonMatchingChars(TextEdit *parent):
    QObject(parent),
    m_editor(parent),
    m_lastPos1(-1),
    m_lastPos2(-1)
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
    char leftChr = 0,
         rightChr = 0;
    PythonTextBlockData *data = nullptr;


    QTextCharFormat format;
    format.setForeground(QColor(QLatin1String("#f43218")));
    format.setBackground(QColor(QLatin1String("#f9c7f6")));

    // clear old highlights
    QList<QTextEdit::ExtraSelection> selections = m_editor->extraSelections();
    for (int i = 0; i < selections.size(); ++i) {
        if ((selections[i].cursor.position() == m_lastPos1 ||
             selections[i].cursor.position() == m_lastPos2) &&
            selections[i].format == format)
        {
            selections.removeAt(i);
            --i;
        }
    }
    m_editor->setExtraSelections(selections);

    QTextCursor cursor = m_editor->textCursor();
    int startPos = cursor.position(),
        matchSkip = 0;

    // grab right char from cursor
    if (cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor)) {
        leftChr = cursor.selectedText()[0].toLatin1();
    }

    if (leftChr == '(' || leftChr == '[' || leftChr == '{') {
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
                if (match->character == leftChr) {
                    ++matchSkip;
                } else if (match->matchingChar() == leftChr) {
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

    // if we get here we didnt find any mathing char on right side
    // grab left char from cursor
    cursor.setPosition(startPos);
    if (cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor)) {
        rightChr = cursor.selectedText()[0].toLatin1();
    }

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
}


// -------------------------------------------------------------------------

namespace Gui {
struct JediCommonP
{
    // helper functions that GIL locks and swaps interpreter
    static QString callStr(Py::Callable self, const char *method,
                           Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);

    static int callInt(Py::Callable self, const char *method,
                       Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);

    static bool callBool(Py::Callable self, const char *method,
                        Py::Tuple *args = nullptr, Py::Dict *kw = nullptr);

    static JediDefinition_ptr_t callDefinition(Py::Callable self, const char *method,
                                               Py::Tuple *args = nullptr,
                                               Py::Dict *kw = nullptr);

    static JediDefinition_list_t callDefinitionsList(Py::Callable self, const char *method,
                                                         Py::Tuple *args = nullptr,
                                                         Py::Dict *kw = nullptr);

    static JediCompletion_list_t callCompletionsList(Py::Callable self, const char *method,
                                                     Py::Tuple *args = nullptr,
                                                     Py::Dict *kw = nullptr);

    static JediCallSignature_list_t callCallSignaturesList(Py::Callable self, const char *method,
                                                           Py::Tuple *args = nullptr,
                                                           Py::Dict *kw = nullptr);

    // state should be swapped and GIL lock held before entering these
    static JediDefinition_list_t createDefinitionsList(const Py::List lst);
    static JediCompletion_list_t createCompletionsList(const Py::List lst);
    static JediCallSignature_list_t createCallSignaturesList(const Py::List lst);
};
}

QString JediCommonP::callStr(Py::Callable self, const char *method, Py::Tuple *args, Py::Dict *kw)
{
    if (!JediInterpreter::instance()->isValid())
        return QString();

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn jedi;

    try {
        if (args == nullptr) {
            Py::Tuple t;
            args = &t;
        }
        Py::Object res;
        if (kw == nullptr)
            res = self.callMemberFunction(method, *args);
        else
            res = self.callMemberFunction(method, *args, *kw);

        if (res.isString())
            return QString::fromStdString(Py::String(res.as_string()).as_std_string());

    } catch(Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return QString();
}

int JediCommonP::callInt(Py::Callable self, const char *method, Py::Tuple *args, Py::Dict *kw)
{
    if (!JediInterpreter::instance()->isValid())
        return -1;

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn me;

    try {
        if (args == nullptr) {
            Py::Tuple t;
            args = &t;
        }
        Py::Object res;
        if (kw == nullptr)
            res = self.callMemberFunction(method, *args);
        else
            res = self.callMemberFunction(method, *args, *kw);

        if (res.isNumeric())
            return static_cast<int>(Py::Long(res).as_long());

    } catch(Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return -1;
}

bool JediCommonP::callBool(Py::Callable self, const char *method, Py::Tuple *args, Py::Dict *kw)
{
    if (!JediInterpreter::instance()->isValid())
        return false;

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn me;

    try {
        if (args == nullptr) {
            Py::Tuple t;
            args = &t;
        }
        Py::Object res;
        if (kw == nullptr)
            res = self.callMemberFunction(method, *args);
        else
            res = self.callMemberFunction(method, *args, *kw);

        if (res.isBoolean())
            return Py::Boolean(res);

    } catch(Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return false;
}

JediDefinition_ptr_t JediCommonP::callDefinition(Py::Callable self, const char *method,
                                                 Py::Tuple *args, Py::Dict *kw)
{
    if (!JediInterpreter::instance()->isValid())
        return nullptr;

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn me;

    try {
        if (args == nullptr) {
            Py::Tuple t;
            args = &t;
        }
        Py::Object res;
        if (kw == nullptr)
            res = self.callMemberFunction(method, *args);
        else
            res = self.callMemberFunction(method, *args, *kw);

        if (res.isCallable())
            return JediDefinition_ptr_t(new JediDefinitionObj(res));

    } catch(Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return nullptr;
}

JediDefinition_list_t JediCommonP::callDefinitionsList(Py::Callable self, const char *method,
                                                           Py::Tuple *args, Py::Dict *kw)
{
    if (!JediInterpreter::instance()->isValid())
        return JediDefinition_list_t();

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn jedi;

    try {
        if (args == nullptr) {
            Py::Tuple t;
            args = &t;
        }
        Py::Object res;
        if (kw == nullptr)
            res = self.callMemberFunction(method, *args);
        else
            res = self.callMemberFunction(method, *args, *kw);

        if (res.isList())
            return JediCommonP::createDefinitionsList(res);

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return JediDefinition_list_t();
}

JediCompletion_list_t JediCommonP::callCompletionsList(Py::Callable self, const char *method,
                                                           Py::Tuple *args, Py::Dict *kw)
{
    if (!JediInterpreter::instance()->isValid())
        return JediCompletion_list_t();

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn jedi;

    try {
        if (args == nullptr) {
            Py::Tuple t;
            args = &t;
        }
        Py::Object res;
        if (kw == nullptr)
            res = self.callMemberFunction(method, *args);
        else
            res = self.callMemberFunction(method, *args, *kw);

        if (res.isList())
            return JediCommonP::createCompletionsList(res);

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return JediCompletion_list_t();
}

JediCallSignature_list_t JediCommonP::callCallSignaturesList(Py::Callable self, const char *method,
                                                                 Py::Tuple *args, Py::Dict *kw)
{
    if (!JediInterpreter::instance()->isValid())
        return  JediCallSignature_list_t();

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn jedi;

    try {
        if (args == nullptr) {
            Py::Tuple t;
            args = &t;
        }
        Py::Object res;
        if (kw == nullptr)
            res = self.callMemberFunction(method, *args);
        else
            res = self.callMemberFunction(method, *args, *kw);

        if (res.isList())
            return JediCommonP::createCallSignaturesList(res);

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return  JediCallSignature_list_t();
}


// state should be swapped and GIL lock held before entering
JediDefinition_list_t JediCommonP::createDefinitionsList(const Py::List lst)
{
    JediDefinition_list_t ret;

    for (const Py::Object &o : lst) {
        JediDefinitionObj *defObj = new JediDefinitionObj(o);
        if (defObj->isValid())
            ret.append(JediDefinition_ptr_t(defObj));
        else
            delete defObj;
    }

    return ret;
}


// state should be swapped and GIL lock held before entering
JediCompletion_list_t JediCommonP::createCompletionsList(const Py::List lst)
{
    JediCompletion_list_t ret;

    for (const Py::Object &o : lst) {
        JediCompletionObj *defObj = new JediCompletionObj(o);
        if (defObj->isValid())
            ret.append(JediCompletion_ptr_t(defObj));
        else
            delete defObj;
    }

    return ret;
}


// state should be swapped and GIL lock held before entering
JediCallSignature_list_t JediCommonP::createCallSignaturesList(const Py::List lst)
{
    JediCallSignature_list_t ret;

    for (const Py::Object &o : lst) {
        JediCallSignatureObj *defObj = new JediCallSignatureObj(o);
        if (defObj->isValid())
            ret.append(JediCallSignature_ptr_t(defObj));
        else
            delete defObj;
    }

    return ret;
}


// -------------------------------------------------------------------------

JediBaseDefinitionObj::JediBaseDefinitionObj(Py::Callable obj) :
    m_obj(obj), m_type(JediBaseDefinitionObj::Base)
{
}

JediBaseDefinitionObj::~JediBaseDefinitionObj()
{
}

QString JediBaseDefinitionObj::name() const
{
    return JediCommonP::callStr(this->m_obj, "name");
}

QString JediBaseDefinitionObj::type() const
{
    return JediCommonP::callStr(this->m_obj, "type");
}

QString JediBaseDefinitionObj::module_name() const
{
    return JediCommonP::callStr(this->m_obj, "module_name");
}

bool JediBaseDefinitionObj::in_builtin_module() const
{
    return JediCommonP::callBool(this->m_obj, "in_builtin_module");
}

int JediBaseDefinitionObj::line() const
{
    return JediCommonP::callInt(this->m_obj, "line");
}

int JediBaseDefinitionObj::column() const
{
    return JediCommonP::callInt(this->m_obj, "column");
}

QString JediBaseDefinitionObj::docstring(bool raw, bool fast) const
{
    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn me;
    Py::Dict kw;
    Py::Tuple args;
    kw["raw"] = Py::Long(0);
    args[0] = Py::Boolean(raw);

    kw["fast"] = Py::Long(1);
    args[1] = Py::Boolean(fast);
    return JediCommonP::callStr(this->m_obj, "docstring", &args, &kw);
}

QString JediBaseDefinitionObj::description() const
{
    return JediCommonP::callStr(this->m_obj, "description");
}

QString JediBaseDefinitionObj::full_name() const
{
    return JediCommonP::callStr(this->m_obj, "full_name");
}

JediDefinition_list_t JediBaseDefinitionObj::goto_assignments() const
{
    return JediCommonP::callDefinitionsList(this->m_obj, "goto_assignments");
}

JediDefinition_list_t JediBaseDefinitionObj::goto_definitions() const
{
    return JediCommonP::callDefinitionsList(this->m_obj, "goto_definitions");
}

JediDefinition_list_t JediBaseDefinitionObj::params() const
{
    return JediCommonP::callDefinitionsList(this->m_obj, "params");

}

int JediBaseDefinitionObj::get_line_code(int before, int after) const
{
    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn me;

    Py::Dict kw;
    Py::Tuple args(2);
    kw["before"] = Py::Long(0);
    args[0] = Py::Long(before);

    kw["after"] = Py::Long(1);
    args[1] = Py::Long(after);

    return JediCommonP::callInt(this->m_obj, "get_line_code", &args, &kw);
}

JediDefinitionObj *JediBaseDefinitionObj::toDefinition(bool &ok)
{
    JediDefinitionObj *def = dynamic_cast<JediDefinitionObj*>(this);
    ok = def != nullptr;
    return def;
}

JediCompletionObj *JediBaseDefinitionObj::toCompletion(bool &ok)
{
    JediCompletionObj *def = dynamic_cast<JediCompletionObj*>(this);
    ok = def != nullptr;
    return def;
}

JediCallSignatureObj *JediBaseDefinitionObj::toCallSignature(bool &ok)
{
    JediCallSignatureObj *def = dynamic_cast<JediCallSignatureObj*>(this);
    ok = def != nullptr;
    return def;
}


// ------------------------------------------------------------------------

JediCompletionObj::JediCompletionObj(Py::Callable obj) :
    JediBaseDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::Completion;
}

JediCompletionObj::~JediCompletionObj()
{
}

QString JediCompletionObj::complete() const
{
    return JediCommonP::callStr(this->m_obj, "complete");
}

QString JediCompletionObj::name_with_symbols() const
{
    return JediCommonP::callStr(this->m_obj, "mane_with_symbols");
}


// ------------------------------------------------------------------------

JediDefinitionObj::JediDefinitionObj(Py::Callable obj) :
    JediBaseDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::Definition;
}

JediDefinitionObj::~JediDefinitionObj()
{
}

JediDefinition_list_t JediDefinitionObj::defined_names() const
{
    return JediCommonP::callDefinitionsList(this->m_obj, "defined_names");
}

bool JediDefinitionObj::is_definition() const
{
    return JediCommonP::callBool(this->m_obj, "is_definition");
}

// -----------------------------------------------------------------------

JediCallSignatureObj::JediCallSignatureObj(Py::Callable obj) :
    JediBaseDefinitionObj(obj)
{
    m_type = JediBaseDefinitionObj::CallSignature;
}

JediCallSignatureObj::~JediCallSignatureObj()
{
}

JediDefinition_ptr_t JediCallSignatureObj::index() const
{
    return JediCommonP::callDefinition(m_obj, "index");
}

int JediCallSignatureObj::bracket_start() const
{
    return JediCommonP::callInt(m_obj, "brcket_start");
}



// -------------------------------------------------------------------------

JediScriptObj::JediScriptObj(const QString source, int line, int column,
                             const QString path, const QString encoding)
{
    if (!JediInterpreter::instance()->isValid())
        return;

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn jedi;

    try {
        Py::Dict kw;
        Py::Tuple args(5);

        kw["source"] = Py::Long(0);
        args[0] = Py::String(source.toStdString());

        kw["line"] = Py::Long(1);
        args[1] = Py::Long(line);

        kw["column"] = Py::Long(2);
        args[2] = Py::Long(column);

        kw["path"] = Py::Long(3);
        args[3] = Py::String(path.toStdString());

        kw["encoding"] = Py::Long(4);
        args[4] = Py::String(encoding.toStdString());

        m_obj = JediInterpreter::instance()->api().
                                             callMemberFunction("String", args, kw);

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
    }
}

JediScriptObj::~JediScriptObj()
{
}

JediCompletion_list_t JediScriptObj::completions() const
{
    return JediCommonP::callCompletionsList(m_obj, "completions");
}

JediDefinition_list_t JediScriptObj::goto_definitions() const
{
    return JediCommonP::callDefinitionsList(m_obj, "goto_definitions");
}

JediDefinition_list_t JediScriptObj::goto_assignments(bool follow_imports) const
{
    if (!isValid())
        return JediDefinition_list_t();

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn jedi;

    try {
        Py::Tuple args(1);
        args[0] = Py::Boolean(follow_imports);
        return JediCommonP::callDefinitionsList(m_obj, "goto_assignments");

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return JediDefinition_list_t();
}

JediDefinition_list_t JediScriptObj::usages(QStringList additional_module_paths) const
{
    if (!isValid())
        return JediDefinition_list_t();

    Base::PyGILStateLocker lock;
    JediInterpreter::SwapIn jedi;

    try {
        Py::Tuple args(additional_module_paths.size());
        for (int i = 0; i < additional_module_paths.size(); ++i)
            args[i] = Py::String(additional_module_paths[i].toStdString());

        return JediCommonP::callDefinitionsList(m_obj, "usages", &args);

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
    }

    return JediDefinition_list_t();
}

JediCallSignature_list_t JediScriptObj::call_signatures() const
{
    return JediCommonP::callCallSignaturesList(m_obj, "call_signatures");
}

bool JediScriptObj::isValid() const
{
    return JediInterpreter::instance()->isValid() &&
           m_obj.ptr() != nullptr;
}

//--------------------------------------------------------------------------

JediDebugProxy::JediDebugProxy()
    : Py::ExtensionModule<JediDebugProxy>( ModuleName )
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

//-------------------------------------------------------------------------

JediInterpreter::JediInterpreter() :
    m_jedi(nullptr), m_api(nullptr)
{
    PyThreadState *oldState = PyThreadState_Get();
    m_threadState = Py_NewInterpreter();// new sub interpreter
    PyThreadState_Swap(oldState); // need restore here

    if (m_threadState) {
        SwapIn jedi; // scopeguarded to handle uncaught exceptions

        // init with default modules
        //Base::Interpreter().runString(Base::ScriptFactory().ProduceScript("CMakeVariables"));
        //Base::Interpreter().runString(Base::ScriptFactory().ProduceScript("FreeCADInit"));

        try {
            // init jedi
            m_jedi = Py::Module("jedi");
            m_api = m_jedi.getAttr("api");
            QStringList preLoads = {
                QLatin1String("FreeCAD"), QLatin1String("FreeCAD.App"),
                QLatin1String("FreeCAD.Gui"), QLatin1String("Part")
            };

            preload_module(preLoads);

            // TODO jedi.settings

        } catch(Py::Exception e) {
            PyErr_Print();
            e.clear();
        }
    }
}

JediInterpreter::~JediInterpreter()
{
    if (m_threadState) {
        SwapIn swap;
        Py_EndInterpreter(m_threadState);
    }
}

JediInterpreter *JediInterpreter::instance()
{
    static JediInterpreter *instance = new JediInterpreter;
    return instance;
}

bool JediInterpreter::isValid() const
{
    return m_threadState != nullptr &&
           Py_IsInitialized() &&
           !m_jedi.isNull();
}

Py::Object JediInterpreter::runString(const QString &src)
{
    Base::PyGILStateLocker lock;
    SwapIn myself;
    return Base::Interpreter().runStringObject(src.toLatin1());
}

const Py::Module &JediInterpreter::jedi() const
{
    return m_jedi;
}

const Py::Module &JediInterpreter::api() const
{
    return m_api;
}

PyThreadState *JediInterpreter::interp() const
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
        Base::PyGILStateLocker lock;
        SwapIn jedi;

        Py::Dict kw;
        Py::Tuple args(6);

        kw["source"] = Py::Long(0);
        args[0] = Py::String(source.toStdString());

        kw["path"] = Py::Long(1);
        args[1] = Py::String(path.toStdString());

        kw["encoding"] = Py::Long(2);
        args[2] = Py::String(encoding.toStdString());

        kw["all_scopes"] = Py::Long(3);
        args[3] = Py::Boolean(all_scopes);

        kw["definitions"] = Py::Long(4);
        args[4] = Py::Boolean(definitions);

        kw["references"] = Py::Long(5);
        args[5] = Py::Boolean(references);

        return JediCommonP::callDefinitionsList(m_api, "names", &args, &kw);

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
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
            m_api.callMemberFunction("preload_module", arg);
            return true;

        } catch (Py::Exception e) {
            PyErr_Print();
            e.clear();
            return false;
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
            kw["debug_func"] = Py::Long(0);
            kw["warnings"] = Py::Long(1);
            kw["notices"] = Py::Long(2);
            kw["speed"] = Py::Long(3);

            Py::Tuple arg(kw.length());
            if (on)
                arg[0] = Py::Callable(proxy->module());
            else
                arg[0] = Py::None();
            arg[1] = Py::Boolean(warnings);
            arg[2] = Py::Boolean(notices);
            arg[3] = Py::Boolean(speed);

            m_jedi.callMemberFunction("set_debug_func", arg, kw);
            return true;

        } catch (Py::Exception e) {
            e.clear();
            return false;
        }
    }

    return false;
}


// -----------------------------------------------------------------------


JediInterpreter::SwapIn::SwapIn()
{
    Base::PyGILStateLocker lock;
    m_oldState = PyThreadState_Get();
    PyThreadState_Swap(JediInterpreter::instance()->interp());
}

JediInterpreter::SwapIn::~SwapIn()
{
    Base::PyGILStateLocker lock;
    PyThreadState_Swap(m_oldState);
}

#include "moc_PythonCode.cpp"
