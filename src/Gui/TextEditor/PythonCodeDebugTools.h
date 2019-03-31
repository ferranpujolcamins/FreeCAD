/***************************************************************************
 *   Copyright (c) 2019 Fredrik Johansson github.com/mumme74               *
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
#ifndef PYTHONCODEDEBUGTOOLS_H
#define PYTHONCODEDEBUGTOOLS_H

#include "PythonSyntaxHighlighter.h"
#include <stdio.h>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QTextDocument;
class QTabWidget;
class QTreeView;
QT_END_NAMESPACE

namespace Gui {
class TextEditor;
namespace Syntax {

const char* tokenToCStr(PythonSyntaxHighlighter::Tokens tok);


// -----------------------------------------------------------------------------

class TokenModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit TokenModel(const TextEditor *editor, QObject *parent = 0);
    virtual ~TokenModel();
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

public Q_SLOTS:
    void refreshAll();

private:
    const PythonTextBlockData *getTextBlock(long line) const;
    const TextEditor *m_editor;
};

// -------------------------------------------------------------

class DebugWindow : public QWidget
{
    Q_OBJECT
public:
    explicit DebugWindow(TextEditor *editor);
    virtual ~DebugWindow();

private:
    TextEditor *m_editor;
    QTabWidget *m_tabWgt;
    TokenModel *m_tokModel;
    QTreeView  *m_tokTree;
};

} // namespace Syntax

// --------------------------------------------------------------------

class DumpToFileBaseClass
{
public:
    explicit DumpToFileBaseClass(const char *outfile = "stdout");
    virtual ~DumpToFileBaseClass();

    const char *datetimeStr();
protected:
    FILE *m_file;
};

// -----------------------------------------------------------------

// dumps all tokens from pythonsyntaxhighlighter
class DumpSyntaxTokens : public DumpToFileBaseClass
{
public:
    explicit DumpSyntaxTokens(QTextBlock firstBlock, const char *outfile = "stdout");
    ~DumpSyntaxTokens();
};

// -----------------------------------------------------------------




} // namespace Gui

#endif // PYTHONCODEDEBUGTOOLS_H
