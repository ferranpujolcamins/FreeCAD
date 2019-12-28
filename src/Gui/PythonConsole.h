/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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


#ifndef GUI_PYTHONCONSOLE_H
#define GUI_PYTHONCONSOLE_H


#include <QListWidget>
#include <QPlainTextEdit>
#include <TextEditor/PythonSyntaxHighlighter.h>
#include "View.h"
#include "Window.h"

#include <Base/PyObjectBase.h>

class QPushButton;

namespace Gui {

/**
 * This class implements an interactive Python interpreter.
 * @author Werner Mayer
 */
class GuiExport InteractiveInterpreter
{
public:
    InteractiveInterpreter();
    ~InteractiveInterpreter();

    bool push(const char*);
    int compileCommand(const char*) const;
    bool hasPendingInput( void ) const;
    void setBuffer(const QStringList&);
    QStringList getBuffer() const;
    void clearBuffer();

private:
    bool runSource(const char*) const;
    PyObject* compile(const char*) const;
    void runCode(PyCodeObject*) const;
    void setPrompt();

private:
    struct InteractiveInterpreterP* d;
};

// ----------------------------------------------------------------------------------

/**
 * This class implements the history for the Python console.
 * @author Werner Mayer
 */
class GuiExport ConsoleHistory
{
public:
    ConsoleHistory();
    ~ConsoleHistory();

    void first();
    bool more();
    bool next();
    bool prev(const QString &prefix = QString());
    bool isEmpty() const;
    const QString& value() const;
    void append(const QString &inputLine);
    const QStringList& values() const;
    void restart();
    void markScratch( void );
    void doScratch( void );

private:
    QStringList                _history;
    QStringList::ConstIterator _it;
    int                        _scratchBegin;
    QString                    _prefix;
};

// -----------------------------------------------------------------------------------

/**
 * Completion is a means by which an editor automatically completes words that the user is typing.
 * For example, in a code editor, a programmer might type "sur", then Tab, and the editor will complete
 * the word the programmer was typing so that "sur" is replaced by "surnameLineEdit". This is very
 * useful for text that contains long words or variable names. The completion mechanism usually works
 * by looking at the existing text to see if any words begin with what the user has typed, and in most
 * editors completion is invoked by a special key sequence.
 *
 * PythonConsoleTextEdit can detect a special key sequence to invoke the completion mechanism, and can handle three
 * different situations:
 * \li There are no possible completions.
 * \li There is a single possible completion.
 * \li There are two or more possible completions.
 *
 * \remark The original sources are taken from Qt Quarterly (Customizing for Completion).
 * @author Werner Mayer
 */
class CompletionList;
class GuiExport PythonConsoleTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    PythonConsoleTextEdit(QWidget *parent = nullptr);
    virtual ~PythonConsoleTextEdit();
    virtual void highlightText(int pos, int len, const QTextCharFormat format);

private Q_SLOTS:
    void complete();

protected:
    void keyPressEvent(QKeyEvent *);

private:
    void createListBox();

private:
    QString wordPrefix;
    int cursorPosition;
    CompletionList *listBox;
};

// ---------------------------------------------------------------------------------

/**
 * The CompletionList class provides a list box that pops up in a text edit if the user has pressed
 * an accelerator to complete the current word he is typing in.
 * @author Werner Mayer
 */
class CompletionList : public QListWidget
{
    Q_OBJECT

public:
    /// Construction
    CompletionList(QPlainTextEdit* parent);
    /// Destruction
    ~CompletionList();

    void findCurrentWord(const QString&);

protected:
    bool eventFilter(QObject *, QEvent *);

private Q_SLOTS:
    void completionItem(QListWidgetItem *item);

private:
    QPlainTextEdit* textEdit;
};

// -----------------------------------------------------------------------------------

/**
 * Python text console with syntax highlighting.
 * @author Werner Mayer
 */
class PythonConsoleHighlighter;
class GuiExport PythonConsole : public PythonConsoleTextEdit, public WindowParameter
{
    Q_OBJECT

public:
    enum Prompt {
        Complete   = 0,
        Incomplete = 1,
        Flush      = 2,
        Special    = 3
    };

    PythonConsole(QWidget *parent = nullptr);
    ~PythonConsole();

    void OnChange( Base::Subject<const char*> &rCaller,const char* rcReason );
    void printStatement( const QString& cmd );
    QString readline( void );

public Q_SLOTS:
    void onSaveHistoryAs();
    void onInsertFileName();
    void onCopyHistory();
    void onCopyCommand();
    void onClearConsole();
    void onFlush();

private Q_SLOTS:
    void visibilityChanged (bool visible);

protected:
    void keyPressEvent  ( QKeyEvent         * e );
    void showEvent      ( QShowEvent        * e );
    void dropEvent      ( QDropEvent        * e );
    void dragEnterEvent ( QDragEnterEvent   * e );
    void dragMoveEvent  ( QDragMoveEvent    * e );
    void changeEvent    ( QEvent            * e );
    void mouseReleaseEvent( QMouseEvent     * e );

    void overrideCursor(const QString& txt);

    /** Pops up the context menu with some extensions */
    void contextMenuEvent ( QContextMenuEvent* e );
    bool canInsertFromMimeData ( const QMimeData * source ) const;
    QMimeData * createMimeDataFromSelection () const;
    void insertFromMimeData ( const QMimeData * source );
    QTextCursor inputBegin( void ) const;

private:
    void runSource(const QString&);
    bool isComment(const QString&) const;
    void printPrompt(Prompt);
    void insertPythonOutput(const QString&);
    void insertPythonError (const QString&);
    void runSourceFromMimeData(const QString&);
    void appendOutput(const QString&, int);
    void loadHistory() const;
    void saveHistory() const;

Q_SIGNALS:
    void pendingSource( void );

private:
    struct PythonConsoleP* d;

    friend class PythonStdout;
    friend class PythonStderr;

private:
    PythonConsoleHighlighter* pythonSyntax;
    QString                 *_sourceDrain;
    QString                  _historyFile;
};

/**
 * Syntax highlighter for Python console.
 * @author Werner Mayer
 */
class GuiExport PythonConsoleHighlighter : public PythonSyntaxHighlighter
{
public:
    PythonConsoleHighlighter(QObject* parent);
    ~PythonConsoleHighlighter() override;

    void highlightBlock (const QString & text) override;

protected:
    void colorChanged(const QString& type, const QColor& col) override;
    Python::Token::Type unhandledState(uint16_t &pos, int state, const std::string &text) override;
};

} // namespace Gui

#endif // GUI_PYTHONCONSOLE_H
