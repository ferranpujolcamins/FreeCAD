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

#ifdef BUILD_PYTHON_DEBUGTOOLS

#include "PythonSyntaxHighlighter.h"
#include <stdio.h>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QTextDocument;
class QTabWidget;
class QTreeView;
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Gui {
class TextEditor;
class PythonEditor;
namespace Python {



// -----------------------------------------------------------------------------

class TokenModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit TokenModel(const PythonEditor *editor, QObject *parent = nullptr);
    virtual ~TokenModel() override;
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
    const Python::TokenLine *getTokenLine(long line) const;
    const PythonEditor *m_editor;
};

// -------------------------------------------------------------

class DebugWindow : public QWidget
{
    Q_OBJECT
public:
    explicit DebugWindow(PythonEditor *editor);
    virtual ~DebugWindow();

private Q_SLOTS:
    void dumpFrames();

private:
    PythonEditor *m_editor;
    QTabWidget *m_tabWgt;
    TokenModel *m_tokModel;
    QTreeView  *m_tokTree;
    QPlainTextEdit *m_frameDumpEdit;
};

} // namespace Python


// -----------------------------------------------------------------


} // namespace Gui

#endif // BUILD_PYTHON_DEBUG_TOOLS

#endif // PYTHONCODEDEBUGTOOLS_H
