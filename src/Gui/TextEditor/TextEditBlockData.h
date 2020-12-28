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
#ifndef TEXTEDITBLOCKDATA_H
#define TEXTEDITBLOCKDATA_H

#include <FCConfig.h>
#include <QTextBlockUserData>
#include <QTextBlock>

namespace Gui {

// --------------------------------------------------------------------------------
class TextEditBlockScanInfo;
class GuiExport TextEditBlockData : public QTextBlockUserData
{
    // QTextBlockUserData does not inherit QObject
public:
    enum FoldingState { Unchanged, Begin, End};
    explicit TextEditBlockData(QTextBlock &block);
    TextEditBlockData(const TextEditBlockData &other);
    virtual ~TextEditBlockData();

    /**
     * @brief bookmark sets if this row has a bookmark
     *         (shows in scrollbar and linemarkerArea)
     */
    bool bookmark() const { return m_bookmarkSet; }
    void setBookmark(bool active) { m_bookmarkSet = active; }

//    /**
//     * @brief blockState tells if this block starts a new block,
//     *         such as '{' in C like languages or ':' in python
//     *         +1 = blockstart, -1 = blockend, -2 = 2 blockends ie '}}'
//     */
//    virtual int blockState() const { return m_blockStateCnt; }
//    virtual int incBlockState() { return ++m_blockStateCnt; }
//    virtual int decBlockState() { return --m_blockStateCnt; }

//    /**
//     * @brief foldBlockEvt folds (makes invisible) this block and increments foldCounter
//     */
//    void foldBlockEvt();
//    /**
//     * @brief unfoldedEvt, decreases foldCounter nad if it reaches 0 makes this block visible again
//     */
//    void unfoldedEvt();

//    bool isFolded() const { return m_foldCnt > 0; }

    /**
     * @brief addFoldingInfo pushes a folding infoi on this row
     * @param id, unique id for this document for folding
     * @param state, what kind of folding: Begin or End
     */
    void addFoldingInfo(uint id, FoldingState state);
    int  removeFoldingInfo(uint id);
    FoldingState foldingInfo(uint id) const;
    QList<uint> foldingIDs() const;
    /// returns first folding id that starts a block (where the block ends on a forward line)
    uint foldingBeginID() const;
    /// returns true if folding ID ends in this line
    bool foldingEndID(uint ID) const;
    /// returns block that ends folding started by startBlock
    QTextBlock foldingEndBlock(const TextEditBlockData* startBlock) const;

    static TextEditBlockData *blockDataFromCursor(const QTextCursor &cursor);

    const QTextBlock &block() const;

    virtual TextEditBlockData *nextBlock() const;
    virtual TextEditBlockData *previousBlock() const;

    /**
     * @brief copyBlock copies a textblocks info, such as bookmark, etc
     * @param other TextBlockData to copy
     *        Note!! does not copy the textblock and scanInfo
     */
    void copyBlock(const TextEditBlockData &other);

    /**
     * @brief scanInfo contains messages for a specific code line/col
     * @return nullptr if no parsemsgs  or PythonTextBlockScanInfo*
     */
    TextEditBlockScanInfo *scanInfo() const { return m_scanInfo; }

    /**
     * @brief setScanInfo set class with parsemessages
     * @param scanInfo instance of PythonTextBlockScanInfo
     *                 this takes ownership of scanInfo
     */
    void setScanInfo(TextEditBlockScanInfo *scanInfo) { m_scanInfo = scanInfo; }

    /**
     * @brief customData make it possible to stor a custom data pointer to any class
     *        this class takes ownership of m_customdata and frees it
     * @return return the pointer
     */
    QTextBlockUserData* customData() const { return m_customData; }
    void setCustomData(QTextBlockUserData* data) { m_customData = data; }

protected:
    QTextBlock m_block;
    TextEditBlockScanInfo *m_scanInfo;
    /// indicates if we have a new folding block start for block id
    QHash<uint, FoldingState> m_folding;
    bool m_bookmarkSet;
    QTextBlockUserData *m_customData;
    //int m_blockStateCnt, // +1 = new Block, -1 pop a block
    //    m_foldCnt;       // indicates if we have folded this textBlock
};

// --------------------------------------------------------------------------------

/**
 * @brief The TextEditBlockScanInfo class stores scaninfo for this row
 *          Such as SyntaxError annotations
 */
class TextEditBlockScanInfo
{
public:
    /// types of messages, sorted in priority, higher idx == higher prio
    enum MsgType { Invalid, AllMsgTypes, Message, Warning, LookupError, IndentError, SyntaxError };
    struct ParseMsg {
    public:
        explicit ParseMsg(QString message, int start, int end, MsgType type);
        ~ParseMsg();
        QString msgTypeAsString() const;
        QString message() const;
        int startPos() const;
        int endPos() const;
        MsgType type() const;

    private:
        QString m_message;
        int m_startPos, m_endPos;
        MsgType m_type;
    };
    typedef QList<ParseMsg> parsemsgs_t;
    explicit TextEditBlockScanInfo();
    virtual ~TextEditBlockScanInfo();

    /// set message at line with startPos - endPos boundaries
    void setParseMessage(int startPos, int endPos, QString message, MsgType type = Message);
    /// get the ParseMsg that is contained within startPos, endPos,, filter by type
    /// nullptr if not found
    const ParseMsg *getParseMessage(int startPos, int endPos, MsgType type = AllMsgTypes) const;
    /// get parseMessage for line contained by col, filter by type
    QString parseMessage(int col, MsgType type = AllMsgTypes) const;
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
#endif // TEXTEDITBLOCKDATA_H
