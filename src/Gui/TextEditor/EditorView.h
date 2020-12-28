/***************************************************************************
 *   Copyright (c) 2007 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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


#ifndef GUI_EDITORVIEW_H
#define GUI_EDITORVIEW_H

#include <FCConfig.h>
#include <Gui/MDIView.h>
#include <Gui/Window.h>
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
class TextEditor;
class EditorType;

/**
 * A special view class which sends the messages from the application to
 * the editor and embeds it in a window.
 * @author Werner Mayer
 */
class GuiExport EditorView : public MDIView, public WindowParameter,
                             public std::enable_shared_from_this<EditorView>
{
    Q_OBJECT

public:
    enum DisplayName {
        FullName,
        FileName,
        BaseName
    };

    explicit EditorView(QPlainTextEdit* editor, QWidget* parent);
    ~EditorView();

    QPlainTextEdit *editor() const;
    TextEditor* textEditor() const;
    void setDisplayName(DisplayName);
    void OnChange(Base::Subject<const char*> &rCaller,const char* rcReason);

    const char *getName(void) const {return "EditorView";}
    void onUpdate(void){}

    bool onMsg(const char* pMsg, const char** ppReturn);
    bool onHasMsg(const char* pMsg) const;

    virtual bool canClose(void);
    virtual bool closeFile(); // close current file

    /** @name Standard actions of the editor */
    //@{
    virtual bool open   (const QString &f);
    virtual bool save   ();
    virtual bool saveAs (const QString fileName = QString());
    virtual void cut    ();
    virtual void copy   ();
    virtual void paste  ();
    virtual void undo   ();
    virtual void redo   ();
    void print  ();
    void printPdf();
    void printPreview();
    virtual void showFindBar();
    virtual void hideFindBar();
    //@}

    QStringList undoActions() const;
    QStringList redoActions() const;
    QString fileName() const;
    void setFileName(QString fileName);

    // sets a topbar widget
    void setTopbar(EditorViewTopBar* topBar);
    EditorViewTopBar* topBar();

public Q_SLOTS:
    void setWindowModified(bool modified);
    void print(QPrinter*);

protected:
    void focusInEvent(QFocusEvent* e);
    /**
     * @brief insertWidget into central widget area
     * @param wgt to insert
     * @param index at index, -1 is last
     */
    void insertWidget(QWidget *wgt, int index = -1);
    EditorViewWrapper* editorWrapper() const;



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
    QPlainTextEdit* swapEditor(QPlainTextEdit* newEditor);

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
    /// Constructor
    /// editor = pointer to a editor in heap, wrapper takes ownership
    /// fn = filename
    explicit EditorViewWrapper(QPlainTextEdit* editor, const QString &fn);
    virtual ~EditorViewWrapper();

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

    QPlainTextEdit* editor() const;
    TextEditor* textEditor() const;
    EditorView* view() const;
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

    //friend class EditorViewWrapper;
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

    /**
     * @brief openFile opens fn in view
     * @param fn filename to open
     * @param view the view to open fn in,
     *         if null open in activeview, create a editorview if not opened
     * @return the editorView now open
     */
    EditorView* openFile(const QString fn, EditorView* view = nullptr);

    /**
     * @brief getWrapper gets the EditViewWrapper for the file
     * @param fn filename to search for
     * @param ownerView in which view to search
     * @return the wrapper for this file or nullptr if not found
     */
    EditorViewWrapper* getWrapper(const QString &fn, EditorView* ownerView);

    /**
     * @brief getWrappers get a list of all global wrappers
     * @param fn filename to search for
     * @return list af all wrappers that matches fn
     */
    QList<EditorViewWrapper*> getWrappers(const QString &fn);

    /**
     * @brief createWrapper create a new wrapper for fn
     * @param fn filename
     * @param editor (optional) if none it creates a new Editor
     * @return the fresh wrapper
     */
    EditorViewWrapper* createWrapper(const QString &fn, QPlainTextEdit* editor = nullptr);

    /**
     * @brief createView create a new EditorView and attach it to MainWindow
     * @param edit optional QPlainTextEdit
     * @return the created EditorView
     */
    EditorView* createView(QPlainTextEdit *edit = nullptr);

    /**
     * @brief createEditor create a new editor with type based on fn's mimetype.
     * @param fn filename to open
     * @param view parent view
     * @return the new editor, view has ownership
     */
    QPlainTextEdit* createEditor(const QString &fn) const;

    /**
     * @brief removeWrapper removes the wrapper from global store
     * @param ew the EditViewWrapper to remove
     * @return true if successfull (found and removed)
     */
    bool removeWrapper(EditorViewWrapper* ew);

    /**
     * @brief lastAccessed gets the last accessed EditorWrapper
     * @param backSteps negative numer back from current accessed, ie -1 for previous
     * @return the corresponding EditorWrapper
     */
    EditorViewWrapper* lastAccessed(EditorView* view, int backSteps = 0);

    /**
     * @brief openedBySuffix gets wrappers opened differentiated by file suffixes
     * @param types list of file suffixes
     * @return a list of all EditorViewWrappers opened by mimetypes
     */
    QList<const EditorViewWrapper*> openedBySuffix(QStringList types = QStringList());

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

    const EditorType *editorTypeForFile(const QString &fn) const;
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
