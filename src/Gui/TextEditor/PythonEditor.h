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


#ifndef GUI_PYTHONEDITOR_H
#define GUI_PYTHONEDITOR_H


#include <FCConfig.h>

#include "TextEditor.h"
#include "SyntaxHighlighter.h"
#include "PythonSyntaxHighlighter.h"
#include "PythonJedi.h"
#include "Window.h"

#include <QDialog>
#include <QAbstractListModel>
#include <QLabel>

QT_BEGIN_NAMESPACE
class QSpinBox;
class QLineEdit;
class QCheckBox;
QT_END_NAMESPACE

namespace App {
class BreakpointLine;
}

namespace Gui {

class SyntaxHighlighter;
class SyntaxHighlighterP;
class PythonEditorBreakpointDlg;
class PythonEditorCodeAnalyzer;
class PythonDebugger;

/**
 * Python text editor with syntax highlighting.
 * \author Werner Mayer
 */
class GuiExport PythonEditor : public TextEditor
{
    Q_OBJECT

public:
    PythonEditor(QWidget *parent = nullptr);
    ~PythonEditor();
    const QString &fileName() const;
    PythonEditorCodeAnalyzer *codeAnalyzer() const;
    void setCodeAnalyzer(PythonEditorCodeAnalyzer *analyzer);

public Q_SLOTS:
    void toggleBreakpoint();
    void showDebugMarker(int line);
    void hideDebugMarker();
    void clearAllExceptions();
    void clearException(const QString &fn, int line);


    /** Inserts a '#' at the beginning of each selected line or the current line if 
     * nothing is selected
     */
    void onComment();
    /**
     * Removes the leading '#' from each selected line or the current line if
     * nothing is selected. In case a line hasn't a leading '#' then
     * this line is skipped.
     */
    void onUncomment();
    /**
     * @brief onAutoIndent
     * Indents selected codeblock
     */
    void onAutoIndent();

    void setFileName(const QString&);
    int  findText(const QString find);
    void startDebug();

    void OnChange(Base::Subject<const char*> &rCaller,const char* rcReason);

    void cut();
    void paste();

protected:
    /** Pops up the context menu with some extensions */
    void contextMenuEvent ( QContextMenuEvent* e );
    void drawMarker(int line, int x, int y, QPainter*);
    void keyPressEvent(QKeyEvent * e);
    bool editorToolTipEvent(QPoint pos, const QString &textUnderPos);
    bool lineMarkerAreaToolTipEvent(QPoint pos, int line);
    void setUpMarkerAreaContextMenu(int line);
    void handleMarkerAreaContextMenu(QAction *res, int line);

Q_SIGNALS:
    void fileNameChanged(const QString &fn);

private Q_SLOTS:
    void breakpointAdded(const App::BreakpointLine *bpl);
    void breakpointChanged(const App::BreakpointLine *bpl);
    void breakpointRemoved(int idx, const App::BreakpointLine *bpl);
    void exception(Base::PyExceptionInfo *exc);


private:
    void breakpointPasteOrCut(bool doCut);
    QString introspect(QString varName);
    void renderExceptionExtraSelections();
    struct PythonEditorP* d;
};

// ---------------------------------------------------------------

class PythonEditorBreakpointDlg : public QDialog
{
    Q_OBJECT
public:
    PythonEditorBreakpointDlg(QWidget *parent, App::BreakpointLine *bp);
    ~PythonEditorBreakpointDlg();
protected:
    void accept();
 private:
    App::BreakpointLine *m_bpl;

    QSpinBox  *m_ignoreToHits;
    QSpinBox  *m_ignoreFromHits;
    QLineEdit *m_condition;
    QCheckBox *m_enabled;
};

// ---------------------------------------------------------------

/**
 * @brief PythonEditorCodeAnalyzer is a bridge between editor and Jedi* classes
 * handles code completions, suggestions, etc
 */
class PythonEditorCodeAnalyzerP;
class PythonEditorCodeAnalyzer : public QObject, public WindowParameter
{
    Q_OBJECT
public:
    PythonEditorCodeAnalyzer(PythonEditor *editor);
    virtual ~PythonEditorCodeAnalyzer();

    JediScript_ptr_t currentScriptObj() const;

    void OnChange(Base::Subject<const char*> &rCaller,const char* rcReason);

    QIcon getIconForDefinition(JediBaseDefinition_ptr_t def,
                               bool allowMarkers = true,
                               int recursionGuard = 0);

    PythonEditor *editor() const;


public Q_SLOTS:
    // reparses the whole document
    void parseDocument();

    // called after user has pressed a key
    bool keyPressed(QKeyEvent *e);

protected:
    // snatches editors events and possibly filter them out
    bool eventFilter(QObject *obj, QEvent *event);


private Q_SLOTS:
    void popupChoiceSelected(const QModelIndex &idx);
    void popupChoiceHighlighted(const QModelIndex &idx);
    bool afterChoiceInserted(JediBaseDefinitionObj *obj, int recursionGuard = 0);
    void complete();
    void hideCompleter();
    void afterFileNameChanged();

private:
    void createWidgets();
    const QString buildToolTipText(JediBaseDefinition_ptr_t def);
    PythonEditorCodeAnalyzerP *d;
};

// -------------------------------------------------------------

/**
 * @brief The PythonCompleterModel class
 * The model (data) class for QCompleter pop up
 */
class PythonCompleterModelP;
class PythonCompleterModel : public QAbstractListModel
{
    Q_OBJECT

public:
    PythonCompleterModel(PythonEditorCodeAnalyzer *parent);
    virtual ~PythonCompleterModel();


    void setCompletions(JediCompletion_list_t newList);
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    void clear();

    JediBaseDefinition_ptr_t getCompletion(int at);

private:
    PythonCompleterModelP *d;
};

// ------------------------------------------------------------

/**
 * @brief displays arguments to functions
 * and other usages to this function
 */
class PythonCallSignatureWidgetP;
class PythonCallSignatureWidget : public QLabel
{
    Q_OBJECT
public:
    PythonCallSignatureWidget(PythonEditorCodeAnalyzer *analyzer);
    virtual ~PythonCallSignatureWidget();

    void reEvaluate();

    void update();

protected:
    void showEvent(QShowEvent *event);


public Q_SLOTS:
    void afterKeyPressed(QKeyEvent *event);

private:
    // if true a rebuild content is needed
    bool rebuildContent();
    // finds which param its at iede (arg, arg2, #<- .. ) // would set paramIdx to 2
    bool setParamIdx();
    // if multiple callsignatures find the most likely one
    bool setCurrentIdx() const;

    void resetList();


    PythonCallSignatureWidgetP *d;
};

} // namespace Gui


#endif // GUI_PYTHONEDITOR_H
