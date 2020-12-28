/***************************************************************************
 *   Copyright (c) 2020 Fredrik Johansson github.com/mumme74               *
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

#include "TextEditBlockData.h"

using namespace Gui;

// ----------------------------------------------------------------------------

TextEditBlockData::TextEditBlockData(QTextBlock &block)
    : m_block(block)
    , m_scanInfo(nullptr)
    , m_bookmarkSet(false)
    , m_customData(nullptr)
//    , m_blockStateCnt(0)
//    , m_foldCnt(0)
{
}

TextEditBlockData::TextEditBlockData(const TextEditBlockData &other)
    : m_block(other.m_block)
    , m_scanInfo(other.m_scanInfo)
    , m_bookmarkSet(other.m_bookmarkSet)
    , m_customData(other.m_customData)
//    , m_blockStateCnt(other.m_blockStateCnt)
//    , m_foldCnt(other.m_foldCnt)
{
}

TextEditBlockData::~TextEditBlockData()
{
    delete m_scanInfo;
    if (m_customData)
        free(m_customData);
}

void TextEditBlockData::addFoldingInfo(uint id, TextEditBlockData::FoldingState state)
{
    m_folding.insert(id, state);
}

int TextEditBlockData::removeFoldingInfo(uint id)
{
    return m_folding.remove(id);
}

TextEditBlockData::FoldingState TextEditBlockData::foldingInfo(uint id) const
{
    return m_folding[id];
}

QList<uint> TextEditBlockData::foldingIDs() const
{
    return m_folding.keys();
}

uint TextEditBlockData::foldingBeginID() const
{
    uint startID = 0;
    for (auto id : m_folding.keys()) {
        if (m_folding[id] == Begin)
            startID = id;
        else if (startID == id && m_folding[id] == End)
            startID = 0;
    }
    return startID;
}

bool TextEditBlockData::foldingEndID(uint ID) const
{
    for (auto id : m_folding.keys()) {
        if (id == ID && m_folding[id] == End)
            return true;
    }
    return false;
}

QTextBlock TextEditBlockData::foldingEndBlock(const TextEditBlockData *startBlock) const
{
    auto foldId = startBlock->foldingBeginID();
    uint cnt = foldId ? 1 : 0;
    auto block = startBlock->block();
    while(cnt && block.isValid() && block.next().isValid()) {
        block = block.next();
        auto userData = dynamic_cast<TextEditBlockData*>(block.userData());
        if (userData) {
            if (userData->foldingBeginID() == foldId)
                ++cnt;
            else if (userData->foldingEndID(foldId))
                --cnt;
        }
    }
    return block;
}

TextEditBlockData *TextEditBlockData::blockDataFromCursor(const QTextCursor &cursor)
{
    QTextBlock block = cursor.block();
    if (!block.isValid())
        return nullptr;

    return dynamic_cast<TextEditBlockData*>(block.userData());
}

const QTextBlock &TextEditBlockData::block() const
{
    return m_block;
}

TextEditBlockData *TextEditBlockData::nextBlock() const
{
    QTextBlock nextBlock = block().next();
    if (!nextBlock.isValid())
        return nullptr;
    return dynamic_cast<TextEditBlockData*>(nextBlock.userData());
}

TextEditBlockData *TextEditBlockData::previousBlock() const
{
    QTextBlock nextBlock = block().previous();
    if (!nextBlock.isValid())
        return nullptr;
    return dynamic_cast<TextEditBlockData*>(nextBlock.userData());
}

void TextEditBlockData::copyBlock(const TextEditBlockData &other)
{
    m_bookmarkSet = other.m_bookmarkSet;
    m_folding = other.m_folding;
    m_customData = other.m_customData;
//    m_blockStateCnt = other.m_blockStateCnt;
}


//void TextEditBlockData::foldBlockEvt()
//{
//    ++m_foldCnt;
////    m_block.setVisible(false);
//}

//void TextEditBlockData::unfoldedEvt()
//{
//    if (m_foldCnt > 0)
//        --m_foldCnt;

//    m_block.setVisible(m_foldCnt < 1);
//}

// ------------------------------------------------------------------------------------

TextEditBlockScanInfo::ParseMsg::ParseMsg(QString message, int start, int end, TextEditBlockScanInfo::MsgType type) :
    m_message(message), m_startPos(start),
    m_endPos(end), m_type(type)
{}

TextEditBlockScanInfo::ParseMsg::~ParseMsg()
{
}

QString TextEditBlockScanInfo::ParseMsg::msgTypeAsString() const
{
    switch (m_type) {
    case Message: return QLatin1String("Message");
    case LookupError: return QLatin1String("LookupError");
    case SyntaxError: return QLatin1String("SyntaxError");
    case IndentError: return QLatin1String("IndentError");
    case Warning: return QLatin1String("Warning");
    //case AllMsgTypes:
    default: return QLatin1String("UnknownError");
    }
}

QString TextEditBlockScanInfo::ParseMsg::message() const
{
    return m_message;
}

int TextEditBlockScanInfo::ParseMsg::startPos() const
{
    return m_startPos;
}

int TextEditBlockScanInfo::ParseMsg::endPos() const
{
    return m_endPos;
}

TextEditBlockScanInfo::MsgType TextEditBlockScanInfo::ParseMsg::type() const
{
    return m_type;
}

// ------------------------------------------------------------------------------------

TextEditBlockScanInfo::TextEditBlockScanInfo()
{
}

TextEditBlockScanInfo::~TextEditBlockScanInfo()
{
}

void TextEditBlockScanInfo::setParseMessage(int startPos, int endPos, QString message,
                                              MsgType type)
{
    ParseMsg msg(message, startPos, endPos, type);
    m_parseMessages.push_back(msg);
}

const TextEditBlockScanInfo::ParseMsg
*TextEditBlockScanInfo::getParseMessage(int startPos, int endPos,
                                          TextEditBlockScanInfo::MsgType type) const
{
    for (const ParseMsg &msg : m_parseMessages) {
        if (msg.startPos() <= startPos && msg.endPos() >= endPos) {
            if (type == AllMsgTypes)
                return &msg;
            else if (msg.type() == type)
                return &msg;
            break;
        }
    }
    return nullptr;
}

QString TextEditBlockScanInfo::parseMessage(int col, MsgType type) const
{
    const ParseMsg *parseMsg = getParseMessage(col, col, type);
    if (parseMsg)
        return parseMsg->message();
    return QString();
}

void TextEditBlockScanInfo::clearParseMessage(int col)
{
    int idx = -1;
    for (ParseMsg &msg : m_parseMessages) {
        ++idx;
        if (msg.startPos() <= col && msg.endPos() >= col)
            m_parseMessages.removeAt(idx);
    }
}

void TextEditBlockScanInfo::clearParseMessages()
{
    m_parseMessages.clear();
}

#include "moc_TextEditBlockData.cpp"
