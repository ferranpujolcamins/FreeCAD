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
class TextEdit;
class PythonTextBlockData;
}; // namespace Gui

namespace Python {
class CodeP;
class Token;


/**
 * @brief Handles code inspection from the python engine internals
 */
class Code : QObject
{
    Q_OBJECT
public:
    explicit Code(QObject *parent = nullptr);
    virtual ~Code();


    // copies object and all subobjects
    //PyObject* deepCopy(PyObject *obj) const;

    QString findFromCurrentFrame(const QString lineText, int pos, const QString word) const;

    //QString findFromCurrentFrame(const Python::Token *tok) const;

    // get the root of the parent identifier ie os.path.join
    //                                                    ^
    // must traverse from os, then os.path before os.path.join
    //PyObject *getDeepObject(PyObject *obj, const Python::Token *needleTok, QString &foundKey) const;

private:
    Python::CodeP *d;
};

// ----------------------------------------------------------------------

} // namespace Python

#endif // PYTHONCODE_H
