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

#ifndef PYTHONCODE_H
#define PYTHONCODE_H


#include <FCConfig.h>
#include "Base/Interpreter.h"

#include "Window.h"
#include "SyntaxHighlighter.h"



class QEventLoop;

namespace Gui {

class PythonSyntaxHighlighter;
class PythonSyntaxHighlighterP;
class PythonEditorBreakpointDlg;
class PythonCodeP;
class PythonTextBlockData;
class PythonToken;
class TextEdit;


/**
 * @brief Handles code inspection from the python engine internals
 */
class PythonCode : QObject
{
    Q_OBJECT
public:
    PythonCode(QObject *parent = 0);
    virtual ~PythonCode();


    // copies object and all subobjects
    PyObject* deepCopy(PyObject *obj);

    QString findFromCurrentFrame(QString varName);

    // get the root of the parent identifier ie os.path.join
    //                                                    ^
    // must traverse from os, then os.path before os.path.join
    Py::Object getDeepObject(PyObject *obj, QString key, QString &foundKey);

private:
    PythonCodeP *d;
};



// ----------------------------------------------------------------------

/**
 * @brief The PythonTextBlockScanInfo class stores scaninfo for this row
 *          Such as SyntaxError annotations
 */
class PythonTextBlockScanInfo
{
public:
    enum MsgType { Message, LookupError, SyntaxError, IndentError, AllMsgTypes };
    struct ParseMsg {
        explicit ParseMsg(QString message, int start, int end, MsgType type) :
                    message(message), startPos(start),
                    endPos(end), type(type)
        {}
        ~ParseMsg() {}
        QString message;
        int startPos;
        int endPos;
        MsgType type;
    };
    typedef QList<ParseMsg> parsemsgs_t;
    explicit PythonTextBlockScanInfo();
    ~PythonTextBlockScanInfo();

    /// set message for token
    void setParseMessage(const PythonToken *tok, QString message, MsgType type = Message);
    /// set message at line with startPos - endPos boundaries
    void setParseMessage(int startPos, int endPos, QString message, MsgType type = Message);
    /// get the ParseMsg for tok, filter by type
    /// nullptr if not found
    const ParseMsg *getParseMessage(const PythonToken *tok, MsgType type = AllMsgTypes) const;
    /// get the ParseMsg that is contained within startPos, endPos,, filter by type
    /// nullptr if not found
    const ParseMsg *getParseMessage(int startPos, int endPos, MsgType type = AllMsgTypes) const;
    /// get parseMessage for token, filter by type
    QString parseMessage(const PythonToken *tok, MsgType type = AllMsgTypes) const;
    /// get parseMessage for line contained by col, filter by type
    QString parseMessage(int col, MsgType type = AllMsgTypes) const;
    /// clear message
    void clearParseMessage(const PythonToken *tok);
    /// clears message that is contained by col
    void clearParseMessage(int col);
    /// clears all messages on this line
    void clearParseMessages();

    /// get all parseMessages for this module
    const parsemsgs_t allMessages() const { return m_parseMessages; }

private:
    parsemsgs_t m_parseMessages;
};

} // namespace Gui



#endif // PYTHONCODE_H
