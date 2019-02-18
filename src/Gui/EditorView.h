/***************************************************************************
 *   Copyright (c) 2007 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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


#ifndef GUI_EDITORVIEW_H
#define GUI_EDITORVIEW_H

#include "MDIView.h"
#include "Window.h"
#include <QTextDocument>
#include <QLineEdit>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QPrinter;
class QLabel;
class QPushButton;
class QComboBox;
QT_END_NAMESPACE

namespace Gui {

class EditorViewP;
class EditorViewTopBar;
class EditorViewWrapper;
class EditorSearchClearEdit;
/**
 * A special view class which sends the messages from the application to
 * the editor and embeds it in a window.
 * @author Werner Mayer
 */
class GuiExport EditorView : public MDIView, public WindowParameter
{
    Q_OBJECT

public:
    enum DisplayName {
        FullName,
        FileName,
        BaseName
    };

    EditorView(QPlainTextEdit *editor, QWidget* parent);
    ~EditorView();

    QPlainTextEdit* getEditor() const;
    void setDisplayName(DisplayName);
    void OnChange(Base::Subject<const char*> &rCaller,const char* rcReason);

    const char *getName(void) const {return "EditorView";}
    void onUpdate(void){}

    bool onMsg(const char* pMsg,const char** ppReturn);
    bool onHasMsg(const char* pMsg) const;

    virtual bool canClose(void);
    virtual bool closeFile(); // close current file

    /** @name Standard actions of the editor */
    //@{
    virtual bool open   (const QString &f);
    virtual bool saveAs ();
    virtual void cut    ();
    virtual void copy   ();
    virtual void paste  ();
    void undo   ();
    void redo   ();
    void print  ();
    void printPdf();
    void printPreview();
    void print(QPrinter*);
    void showFindBar();
    void hideFindBar();
    //@}

    QStringList undoActions() const;
    QStringList redoActions() const;
    QString fileName() const;

    // sets a topbar widget
    void setTopbar(EditorViewTopBar *topBar);
    EditorViewTopBar *topBar();

public Q_SLOTS:
    void setWindowModified(bool modified);

protected:
    void focusInEvent(QFocusEvent* e);
    /**
     * @brief insertWidget into central widget area
     * @param wgt to insert
     * @param index at index, -1 is last
     */
    void insertWidget(QWidget *wgt, int index = -1);
    EditorViewWrapper *editorWrapper() const;



private Q_SLOTS:
    void checkTimestamp();
    void contentsChange(int position, int charsRemoved, int charsAdded);
    void undoAvailable(bool);
    void redoAvailable(bool);

Q_SIGNALS:
    /**
     * @brief changeFileName emitted when filename changes, such as save as
     */
    void changeFileName(const QString &fileName);

    /**
     * @brief switchedFile emitted when another file is shown in view
     * for exapmle when we debug trace into another file
     */
    void switchedFile(const QString &fileName);

    /**
     * @brief closedFile emitted when a file is closed
     */
    void closedFile(const QString &fileName);

    /**
     * @brief modifiedChanged emitted when current editor modified status changes
     */
    void modifiedChanged(bool unsaved);

private:
    void setCurrentFileName(const QString &fileName);
    bool saveFile();
    QPlainTextEdit* swapEditor(QPlainTextEdit *newEditor);

private:
    EditorViewP* d;
    friend class EditorSearchBar;
};


// -----------------------------------------------------------------------------------

class PythonEditor;
class PythonEditorViewP;
class GuiExport PythonEditorView : public EditorView
{
    Q_OBJECT

public:
    PythonEditorView(PythonEditor* editor, QWidget* parent);
    ~PythonEditorView();

    bool onMsg(const char* pMsg,const char** ppReturn);
    bool onHasMsg(const char* pMsg) const;

    static PythonEditorView *setAsActive(QString fileName = QString());

public Q_SLOTS:
    void executeScript();
    void startDebug();
    void toggleBreakpoint();
    void showDebugMarker(const QString &fileName, int line);
    void hideDebugMarker(const QString &fileName, int line);

private:
    PythonEditorViewP *d;
};

// ----------------------------------------------------------------------------------

/**
 * @brief wrapps a editor with view data
 * undo/ redo, timestamp etc
 */
class EditorViewWrapperP;
class GuiExport EditorViewWrapper
{
    EditorViewWrapperP *d;
public:
    EditorViewWrapper(QPlainTextEdit *editor, const QString &fn);
    ~EditorViewWrapper();

    /**
     * @brief attach/detach from EditorView
     * @param sharedOwner, current view
     */
    void attach(EditorView* sharedOwner);
    void detach(EditorView* sharedOwner);
    /**
     * @brief detaches from sharedOwner and if file has no other owners,
     *      editor gets destroyed
     * @param sharedOwner, ownerView
     * @return true if editor gets destoyed (no other shared owners)
     */
    bool close(EditorView* sharedOwner);

    QPlainTextEdit *editor() const;
    QString fileName() const;
    void setFileName(const QString &fn);
    uint timestamp() const;
    void setTimestamp(uint timestamp);
    void setLocked(bool locked);
    bool isLocked() const;

    QStringList &undos();
    QStringList &redos();
};

// ------------------------------------------------------------------------------------

/**
 * @brief The EditorViewSingleton a singleton which owns all
 * opened editors so that they can be opened in different views.
 * For example split views, different tabs, etc
 * Creates the correct editor based on mimetype, filesuffix and
 * wraps it in a EditorViewWrapper
 */
class EditorViewSingletonP;
class GuiExport EditorViewSingleton : public QObject
{
    Q_OBJECT

    EditorViewSingletonP *d;
public:
    typedef QPlainTextEdit* (*createT)();

    EditorViewSingleton();
    ~EditorViewSingleton();
    static bool registerTextEditorType(createT factory, const QString &name,
                                       const QStringList &suffixes, const QStringList &mimetypes,
                                       const QString &icon = QLatin1String("accessories-text-editor"));

    /**
     * @brief instance, gets the singleton
     * @return sigleton instance
     */
    static EditorViewSingleton* instance();

    EditorViewWrapper *getWrapper(const QString &fn);
    EditorViewWrapper *createWrapper(const QString &fn, QPlainTextEdit *editor = nullptr);
    /**
     * @brief lastAccessed gets the last accessed EditorWrapper
     * @param backSteps negative numer back from current accessed, ie -1 for previous
     * @return the corresponding EditorWrapper
     */
    EditorViewWrapper *lastAccessed(int backSteps = 0);
    QList<const EditorViewWrapper*> openedByType(QStringList types = QStringList());

Q_SIGNALS:
    /**
     * @brief emitted when number of opened files has changed
     */
    void openFilesChanged();

    /**
     * @brief emitted when editor document isModfied signal is emitted
     * @param fn = filename
     * @param changed = true if changed, false when not (such as undo)
     */
    void modifiedChanged(const QString &fn, bool changed);

    /**
     * @brief emitted when all instances of file is closed
     * @param fn = filename
     */
    void fileClosed(const QString &fn);
    /**
     * @brief fileOpened emitted when first editor instance opened file
     * @param fn = filename
     */
    void fileOpened(const QString &fn);

private Q_SLOTS:
    void docModifiedChanged(bool changed);
    void connectToDebugger(void);

private:
    friend class EditorViewWrapper;
};

// ---------------------------------------------------------------------------

/**
 * @brief holds the topbar (opened files, class browser etc)
 */
class EditorViewTopBarP;
class EditorViewTopBar : public QWidget
{
   Q_OBJECT
public:
    EditorViewTopBar(EditorView *parent = nullptr);
    ~EditorViewTopBar();

    void setParent(QWidget *parent);

private Q_SLOTS:
    void rebuildOpenedFiles();
    void setCurrentIdx(const QString fileName);
    void closeCurrentFile();
    void switchFile(int index) const;
    void modifiedChanged(const QString &fn, bool changed);

private:
    QString createViewName(const QString &fn, bool changed) const;
    void init();
    EditorViewTopBarP *d;
};

//-----------------------------------------------------------------------------------

/**
 * @brief The EditorSearchBar class
 * holds the widgets in the search/find in document
 */
class EditorSearchBar : public QFrame
{
    Q_OBJECT
public:
    EditorSearchBar(EditorView *parent, EditorViewP *editorViewP);
    ~EditorSearchBar();

public Q_SLOTS:
    void show();

private Q_SLOTS:
    void upSearch(bool cycle = true);
    void downSearch(bool cycle = true);
    void foundCount(int foundOcurrences);
    void searchChanged(const QString &str);
    void replace();
    void replaceAndFind();
    void replaceAll();
    void showSettings();

private:
    EditorViewP *d;
    QLabel                  *m_foundCountLabel;
    EditorSearchClearEdit   *m_searchEdit;
    QPushButton             *m_searchButton;
    QPushButton             *m_upButton;
    QPushButton             *m_downButton;
    QPushButton             *m_hideButton;
    EditorSearchClearEdit   *m_replaceEdit;
    QPushButton             *m_replaceButton;
    QPushButton             *m_replaceAndNextButton;
    QPushButton             *m_replaceAllButton;
    QTextDocument::FindFlags m_findFlags;
};

// ---------------------------------------------------------------------------------

/**
 * @brief a lineedit class that shows a clear button and a settings button
 */
class EditorSearchClearEdit : public QLineEdit
{
    Q_OBJECT
public:
    EditorSearchClearEdit(QWidget *parent, bool showSettingButton);
    ~EditorSearchClearEdit();

Q_SIGNALS:
    void showSettings();
};

} // namespace Gui

#endif // GUI_EDITORVIEW_H
