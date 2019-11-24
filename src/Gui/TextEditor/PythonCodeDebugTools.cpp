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


#include "PreCompiled.h"

#include "PythonCodeDebugTools.h"
#include "PythonCode.h"
#include <PythonSource/PythonSource.h>
#include <PythonSource/PythonSourceRoot.h>
#include "PythonSource/PythonSourceDebugTools.h"
#include "PythonSyntaxHighlighter.h"
#include "PythonEditor.h"
#include <QTextBlock>
#include <QTextBlockUserData>
#include <QFileInfo>
#include <QTreeView>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <Gui/TextEditor/TextEditor.h>

using namespace Gui;
using namespace Python;

DBG_TOKEN_FILE

static const uint8_t SRC_ROW_SHIFT = 8;
static const std::intptr_t SRC_ROW_FLAG = (1 << 7),
                           SRC_ROW_START = (1 << SRC_ROW_SHIFT),
                           SRC_ROW_MASK = 0xFFFFFFFFFFFFFF00,
                           TOK_ROW_MASK = 0x00000000000000FF;

TokenModel::TokenModel(const PythonEditor *editor, QObject *parent) :
    QAbstractItemModel(parent),
    m_editor(editor)
{
}

TokenModel::~TokenModel()
{
}

QVariant TokenModel::data(const QModelIndex &index, int role) const
{
    DEFINE_DBG_VARS

    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::ForegroundRole))
        return QVariant();

    auto idx = reinterpret_cast<std::intptr_t>(index.internalPointer());
    if (idx & SRC_ROW_FLAG) {
        // its a code line
        const Python::TokenLine *line = getTokenLine(index.row());
        if (line) {
            if (index.column() == 0) {
                if (role == Qt::ForegroundRole)
                    return QBrush(QColor(0, 0, 50));
                return QString::number(line->lineNr() +1);
            } else {
                if (role == Qt::ForegroundRole)
                    return QBrush(QColor(0, 0, 50));
                return QString::fromStdString(line->text());
            }
        }

    } else {
        // its a token line, and we have parent
        int parentline = (idx & SRC_ROW_MASK) >> SRC_ROW_SHIFT;
        uint tokIdx = idx & TOK_ROW_MASK;
        auto line = getTokenLine(parentline - 1);
        if (line) {
            const std::list<Python::Token*> &tokens = line->tokens();
            if (line->tokens().size() > tokIdx) {
                const Python::Token *tok = *std::next(tokens.begin(), tokIdx);
                if (tok) {
                    DBG_TOKEN(tok)
                    if (index.column() == 0) {
                        if (role == Qt::ForegroundRole)
                            return QBrush(QColor(0, 0, 170));
                        return QString::fromLatin1("s:%1 e:%2")
                                .arg(tok->startPos())
                                .arg(tok->endPos());
                    } else if (index.column() == 1) {
                        if (role == Qt::ForegroundRole)
                            return QBrush(QColor(0, 0, 120));
                        return QString::fromLatin1(tokenToCStr(tok->type()));
                    }
                }
            }
        }
    }

    return QVariant();
}

Qt::ItemFlags TokenModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;

    return QAbstractItemModel::flags(index);
}

QVariant TokenModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0)
            return QLatin1String("line");
        else
            return QLatin1String("code");
    }

    return QVariant();
}

QModelIndex TokenModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.isValid()) {
        // its the tokens row
        std::intptr_t idx = reinterpret_cast<std::intptr_t>(parent.internalPointer());
        auto line = getTokenLine((idx >> SRC_ROW_SHIFT) -1);
        if (line && static_cast<uint>(row) < line->tokens().size()){
            idx = (idx & SRC_ROW_MASK) | row;
            return createIndex(row, column, reinterpret_cast<void*>(idx));
        }
    } else {
        // its the code line
        auto line = getTokenLine(row);
        int idx = ((row + 1) << SRC_ROW_SHIFT) | SRC_ROW_FLAG;
        if (line){
            idx |= line->tokens().size();
        }
        return createIndex(row, column, reinterpret_cast<void*>(idx));
    }

    return QModelIndex();
}

QModelIndex TokenModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    auto idx = reinterpret_cast<std::intptr_t>(index.internalPointer());
    if (!(idx & SRC_ROW_FLAG)) {
        // its a token line, and we have parent
        int parentline = (idx & SRC_ROW_MASK) >> SRC_ROW_SHIFT;
        idx |= (idx & SRC_ROW_MASK) | SRC_ROW_FLAG;
        auto txtData = getTokenLine(parentline);
        if (txtData)
            idx |= txtData->tokens().size();
        return createIndex(parentline -1, 0 , reinterpret_cast<void*>(idx));
    }

    return QModelIndex();
}

int TokenModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0 || m_editor->document()->isEmpty())
        return 0; // we only have children to col0

    if (parent.isValid()) {
        std::intptr_t idx = reinterpret_cast<std::intptr_t>(parent.internalPointer());
        int line = idx >> SRC_ROW_SHIFT;
        if (idx & SRC_ROW_FLAG) {
            // its a src row
            auto txtData = getTokenLine(line -1);
            if (txtData)
                return static_cast<int>(txtData->tokens().size());
        }
        // its a tokens row or fail txtData
        return 0;
    }
    // root element, number of rows
    if (parent.row() < 0)
        return m_editor->document()->lineCount();
    return 0;
}

int TokenModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 2; // token
    return 2; // line
}

void TokenModel::refreshAll()
{
    beginResetModel();
    endResetModel();
}

const Python::TokenLine *TokenModel::getTokenLine(long line) const
{
    Python::SyntaxHighlighter *hl = dynamic_cast<Python::SyntaxHighlighter*>(m_editor->syntaxHighlighter());
    if (hl)
        return hl->list().lineAt(static_cast<int>(line));
    return nullptr;
}

// -----------------------------------------------------------------------

DebugWindow::DebugWindow(PythonEditor *editor) :
    QWidget(),
    m_editor(editor),
    m_tabWgt(nullptr), m_tokModel(nullptr), m_tokTree(nullptr)
{
    setWindowTitle(QString::fromLatin1("Debug view"));
    resize(800, 680);

    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);
    m_tabWgt = new QTabWidget(this);
    layout->addWidget(m_tabWgt);

    m_tokModel = new TokenModel(m_editor);
    m_tokTree  = new QTreeView();
    m_tokTree->setModel(m_tokModel);

    QWidget *tokPage = new QWidget(this);
    QVBoxLayout *tokLayout = new QVBoxLayout();
    tokPage->setLayout(tokLayout);
    QPushButton *tokRefresh = new QPushButton(QLatin1String("refresh"));
    connect(tokRefresh, SIGNAL(clicked()), m_tokModel, SLOT(refreshAll()));

    tokLayout->addWidget(tokRefresh);
    tokLayout->addWidget(m_tokTree);

    m_tabWgt->addTab(tokPage, QLatin1String("Tokens"));


    // framedump
    QWidget *frameDump = new QWidget(this);
    QVBoxLayout *frameDumpLayout = new QVBoxLayout();
    frameDump->setLayout(frameDumpLayout);

    m_frameDumpEdit = new QPlainTextEdit(this);
    m_frameDumpEdit->setReadOnly(true);

    QPushButton *frameDumpRefresh = new QPushButton(QLatin1String("refresh"));
    connect(frameDumpRefresh, SIGNAL(clicked()), this, SLOT(dumpFrames()));

    frameDumpLayout->addWidget(frameDumpRefresh);
    frameDumpLayout->addWidget(m_frameDumpEdit);

    m_tabWgt->addTab(frameDump, QLatin1String("frames"));

    dumpFrames();
}

DebugWindow::~DebugWindow()
{
    delete m_tokTree;
    delete m_tokModel;
    delete m_tabWgt;
}

void DebugWindow::dumpFrames()
{
    FILE *tmpFilePtr;

    tmpFilePtr = tmpfile();

    //DumpSyntaxTokens dump(document()->begin());
    QString fileName = m_editor->fileName();
    Python::SourceModule *module =
            Python::SourceRoot::instance()->moduleFromPath(fileName.toStdString());
    Python::DumpModule dMod(module, tmpFilePtr);

    rewind(tmpFilePtr);

    QByteArray textBytes;
    while(!feof(tmpFilePtr)) {
        char c = fgetc(tmpFilePtr);
        textBytes.append(c);
    }

    m_frameDumpEdit->setPlainText(QString::fromLatin1(textBytes));

}



#include "moc_PythonCodeDebugTools.cpp"

