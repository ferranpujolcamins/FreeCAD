/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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


#ifndef GUI_TEXTEDIT_H
#define GUI_TEXTEDIT_H

#include <FCConfig.h>

#include <Gui/View.h>
#include <Gui/Window.h>

#include <QListWidget>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlockUserData>
#include <QMenu>

QT_BEGIN_NAMESPACE
class QCompleter;
QT_END_NAMESPACE

namespace Gui {
class SyntaxHighlighter;


class LineMarkerArea;
class SyntaxHighlighter;
class TextEditBlockData;
class GuiExport TextEditor : public QPlainTextEdit, public WindowParameter
{
    Q_OBJECT

public:
    TextEditor(QWidget *parent = nullptr);
    virtual ~TextEditor();

    /**
     * @brief Highlights text such as matching parentesis
     * @param pos = position in text
     * @param len = length to highlight, highlights to right
     * @param format = using this format
     */
    virtual void highlightText(int pos, int len, const QTextCharFormat format);


    void setSyntaxHighlighter(SyntaxHighlighter*);
    SyntaxHighlighter *syntaxHighlighter();

    void OnChange(Base::Subject<const char*> &rCaller,const char* rcReason);

    //void lineNumberAreaPaintEvent(QPaintEvent* );
    int lineNumberAreaWidth();
    int findAndHighlight(const QString needle, QTextDocument::FindFlags flags = nullptr);
    // highlights text such as search for
    void setTextMarkers(QString key, QList<QTextEdit::ExtraSelection> selections);

    void setCompleter(QCompleter *completer) const;
    QCompleter *completer() const;

public Q_SLOTS:
    void onSave();

private Q_SLOTS:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &, int);
    void highlightSelections();
    void markerAreaContextMenu(int line, QContextMenuEvent *event);

protected:
    void keyPressEvent (QKeyEvent * e);
    /** Draw a beam in the line where the cursor is. */
    void paintEvent (QPaintEvent * e);
    void resizeEvent(QResizeEvent* e);
    LineMarkerArea* lineMarkerArea() const
    { return lineNumberArea; }
    virtual void drawMarker(int line, int x, int y, QPainter*);
    bool event(QEvent *event);
    virtual bool editorToolTipEvent(QPoint pos, const QString &textUnderPos);
    virtual bool lineMarkerAreaToolTipEvent(QPoint pos, int line);
    /// creates a Menu to use when a contextMenu event occurs
    virtual void setUpMarkerAreaContextMenu(int line);
    /// after contextmenu for marker area has finished
    virtual void handleMarkerAreaContextMenu(QAction *res, int line);

    /// these types are valid contextMenu types
    enum ContextEvtType {
        // linemarerarea specific
        InValid, Bookmark,
        BreakpointAdd, BreakpointDelete, BreakpointEdit,
        BreakpointEnable, BreakpointDisable,
        ExceptionClear,
        // editor specific
        Undo, Redo,
        Comment, UnComment,
        AutoIndent
    };
    QMenu m_markerAreaContextMenu;
    QMenu m_contextMenu;


private:
    SyntaxHighlighter* highlighter;
    LineMarkerArea* lineNumberArea;
    struct TextEditorP* d;

    void paintIndentMarkers(QPaintEvent *e);
    void paintTextWidthMarker(QPaintEvent *e);
    void paintFoldedTextMarker(QPaintEvent *e);

    friend class SyntaxHighlighter;
    friend class LineMarkerArea;
};

// ---------------------------------------------------------------------------------

class LineMarkerAreaP;
class LineMarkerArea : public QWidget
{
    Q_OBJECT

public:
    LineMarkerArea(TextEditor* editor);
    virtual ~LineMarkerArea();

    QSize sizeHint() const;
    void setLineNumbersVisible(bool active);
    bool lineNumbersVisible() const;

    /// called when editor has changed font size
    void fontSizeChanged();

    /// calculate line from given pos, handles folded blocks
    /// firstline starts at 1
    /// if not possible to calculate return 0
    int lineFromPos(const QPoint &pos);

Q_SIGNALS:
    void clickedOnLine(int line, QMouseEvent *event);
    void contextMenuOnLine(int line, QContextMenuEvent * event);


protected:
    void paintEvent (QPaintEvent *event);
    void mouseReleaseEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);
    void contextMenuEvent(QContextMenuEvent * event);
    virtual void foldingClicked(int line);


private:
    LineMarkerAreaP *d;
};

// --------------------------------------------------------------------------------
class TextEditBlockScanInfo;

class TextEditBlockData : public QTextBlockUserData
{
    // QTextBlockUserData does not inherit QObject
public:
    explicit TextEditBlockData(QTextBlock block);
    TextEditBlockData(const TextEditBlockData &other);
    virtual ~TextEditBlockData();

    /**
     * @brief bookmark sets if this row has a bookmark
     *         (shows in scrollbar and linemarkerArea)
     */
    bool bookmark() const { return m_bookmarkSet; }
    void setBookmark(bool active) { m_bookmarkSet = active; }

    /**
     * @brief blockState tells if this block starts a new block,
     *         such as '{' in C like languages or ':' in python
     *         +1 = blockstart, -1 = blockend, -2 = 2 blockends ie '}}'
     */
    int blockState() const { return m_blockStateCnt; }
    int incBlockState() { return ++m_blockStateCnt; }
    int decBlockState() { return --m_blockStateCnt; }

    /**
     * @brief foldBlockEvt folds (makes invisible) this block and increments foldCounter
     */
    void foldBlockEvt();
    /**
     * @brief unfoldedEvt, decreases foldCounter nad if it reaches 0 makes this block visible again
     */
    void unfoldedEvt();

    bool isFolded() const { return m_foldCnt > 0; }



    static TextEditBlockData *blockDataFromCursor(const QTextCursor &cursor);

    const QTextBlock &block() const;

    virtual TextEditBlockData *next() const;
    virtual TextEditBlockData *previous() const;

    /**
     * @brief copyBlock copies a textblocks info, such as bookmark, etc
     * @param other TextBlockData to copy
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
     */
    void setScanInfo(TextEditBlockScanInfo *scanInfo) { m_scanInfo = scanInfo; }

protected:
    QTextBlock m_block;
    TextEditBlockScanInfo *m_scanInfo;
    bool m_bookmarkSet;
    int m_blockStateCnt, // +1 = new Block, -1 pop a block
        m_foldCnt;       // indicates if we have folded this textBlock
};

// --------------------------------------------------------------------------------

/**
 * @brief The TextEditBlockScanInfo class stores scaninfo for this row
 *          Such as SyntaxError annotations
 */
class TextEditBlockScanInfo
{
public:
    enum MsgType { Message, LookupError, SyntaxError, IndentError, Warning, AllMsgTypes };
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

// ---------------------------------------------------------------------------------

class AnnotatedScrollBarP;
/**
 * @brief A custom scrollbar that can show markers at different lines
 *      such as markers when searching for a word
 */
class AnnotatedScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    explicit AnnotatedScrollBar(TextEditor *parent = nullptr);
    virtual ~AnnotatedScrollBar();

    /**
     * @brief setMarker at a given line in document
     * @param line: the line
     * @param color: the color to render with
     */
    void setMarker(int line, QColor color);

    /**
     * @brief resetMarkers clears markers och color and sets new ones found in color
     * @param color
     */
    void resetMarkers(QList<int> newMarkers, QColor color);
    /**
     * @brief Clear all markers
     */
    void clearMarkers();

    /**
     * @brief Clear all Markers of the given color
     * @param color
     */
    void clearMarkers(QColor color);

    void clearMarker(int line, QColor color);

protected:
    void paintEvent(QPaintEvent *e);

private:
    AnnotatedScrollBarP *d;
};

} // namespace Gui

#endif // GUI_TEXTEDIT_H
