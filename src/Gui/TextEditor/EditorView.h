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
QT_END_NAMESPACE

namespace Gui {

class EditorViewP;
class EditorViewTopBar;
class EditorViewWrapper;
class TextEditor;
class EditorType;
class AbstractLangPlugin;


/// different types of editors, could have a special editor for it, xml for example
/// or Py for that matter
class EditorTypeP;
class EditorType {
    EditorTypeP *d;
public:
    typedef QPlainTextEdit* (*createT)();
    explicit EditorType(createT factory, const QString &name,
               const QStringList &suffixes, const QStringList &mimetypes,
               const QString &iconStr);
    virtual ~EditorType();
    createT factory() const;
    QPlainTextEdit* create() const;
    const QString name() const;
    const QStringList& suffixes() const;
    const QStringList& mimetypes() const;
    const QString iconStr() const;
};

// ---------------------------------------------------------------------------

/**
 * A special view class which sends the messages from the application to
 * the editor and embeds it in a window.
 * @author Werner Mayer
 */
class GuiExport EditorView : public MDIView, public WindowParameter,
                             public std::enable_shared_from_this<EditorView>
{
    Q_OBJECT
    EditorViewP* d;
    friend class EditorSearchBar;
    friend class EditorViewWrapper;

public:
    enum DisplayName {
        FullName,
        FileName,
        BaseName
    };

    explicit EditorView(QPlainTextEdit* editor,
                        const QString &fn = QString(),
                        QWidget* parent = nullptr);
    ~EditorView();

    /// get reference to currently used editor or nullptr
    QPlainTextEdit *editor() const;

    /// same as editor() but casted to TextEditor
    TextEditor* textEditor() const;


    /// set if should show filenmae filename, fullpath or just basename
    void setDisplayName(DisplayName);

    /// event system from FreeCAD calls this
    void OnChange(Base::Subject<const char*> &rCaller,const char* rcReason);
    /// also FreeCAD internal
    const char *getName(void) const {return "EditorView";}
    void onUpdate(void){}
    /// FreeCAD commands calls these
    bool onMsg(const char* pMsg, const char** ppReturn);
    bool onHasMsg(const char* pMsg) const;

    virtual bool canClose(void);
    virtual bool closeFile(); // close current file

    /** @name Standard actions of the editor */
    //@{
    virtual bool open   (const QString &fn);
    virtual bool openDlg(); /// open a file dialog and open it that way
    virtual bool save   ();
    virtual bool saveAs (const QString filename = QString());
    virtual void cut    ();
    virtual void copy   ();
    virtual void paste  ();
    virtual void undo   ();
    virtual void redo   ();
    virtual void newFile();
    void print  ();
    void printPdf();
    void printPreview();
    virtual void showFindBar();
    virtual void hideFindBar();
    //@}

    QStringList undoActions() const;
    QStringList redoActions() const;
    QString filename() const;
    void setFilename(QString filename);

    const QList<AbstractLangPlugin*> currentPlugins() const;
    const AbstractLangPlugin *currentPluginByName(const char *name);

    // sets a topbar widget
    void setTopbar(EditorViewTopBar* topBar);
    EditorViewTopBar* topBar();


    /// a wrapper allow different types of editors to be attached to view
    /// could be a specialiced xml editor or for that matter a simple QPlainTextEdit
    /// returns the currently used wrapper. Every file opens in a separate editor
    /// wrapped in i EditViewWrapper
    EditorViewWrapper* wrapper() const;

    /**
     * @brief wrapper gets the EditViewWrapper for the file
     * @param fn filename to search for
     * @return the wrapper for this file or nullptr if not found
     */
    EditorViewWrapper* wrapper(const QString &fn) const;

    /**
     * @brief wrappers get a list of all wrappers
     * @param fn filename to search for, if empty get all wrappers
     * @return list af all wrappers that matches fn
     */
    QList<EditorViewWrapper*> wrappers(const QString &fn = QString()) const;


public Q_SLOTS:
    void setWindowModified(bool modified);
    void print(QPrinter*);
    void refreshLangPlugins();

protected:
    void focusInEvent(QFocusEvent* e);
    /**
     * @brief insertWidget into central widget area
     * @param wgt to insert
     * @param index at index, -1 is last
     */
    void insertWidget(QWidget *wgt, int index = -1);
    EditorViewWrapper* editorWrapper() const;

    /// switches to view this wrapper instead
    void setActiveWrapper(EditorViewWrapper *wrp);

    /// remove wrapper, called from  ~EditViewWrapper()
    bool closeWrapper(EditorViewWrapper *wrapper);

private Q_SLOTS:
    void checkTimestamp();
    void contentsChange(int position, int charsRemoved, int charsAdded);
    void undoAvailable(bool);
    void redoAvailable(bool);

Q_SIGNALS:
    /**
     * @brief changeFileName emitted when filename changes, such as save as
     */
    void changeFileName(const QString &filename);

    /**
     * @brief switchedFile emitted when another file is shown in view
     * for exapmle when we debug trace into another file
     */
    void switchedFile(const QString &filename);

    /**
     * @brief closedFile emitted when a file is closed
     */
    void closedFile(const QString &filename);

    /**
     * @brief modifiedChanged emitted when current editor modified status changes
     */
    void modifiedChanged(bool unsaved);

private:
    void showCurrentFileName();
    bool saveFile();
    QPlainTextEdit* swapEditor(QPlainTextEdit* newEditor);

};

// ----------------------------------------------------------------------------------

/**
 * @brief wrapps a editor with view data
 * undo/ redo, timestamp etc
 */
class EditorViewWrapperP;
class GuiExport EditorViewWrapper : public QObject
{
    EditorViewWrapperP *d;
    Q_OBJECT
public:
    /// Constructor
    /// editor = pointer to a editor in heap, wrapper takes ownership
    /// fn = filename
    explicit EditorViewWrapper(QPlainTextEdit* editor, EditorView* view, const QString &fn);
    virtual ~EditorViewWrapper();

    /**
     * @brief attach/detach from EditorView
     */
    void attach();
    void detach();
    /**
     * @brief detaches from sharedOwner and if file has no other owners,
     *      editor gets destroyed
     */
    void close();

    QPlainTextEdit* editor() const;
    TextEditor* textEditor() const;
    EditorView* view() const;
    QString filename() const;
    void setFilename(const QString &fn);

    const QString& mimetype() const;
    void setMimetype(QString mime);
    int64_t timestamp() const;
    void setTimestamp(int64_t timestamp);
    void setLocked(bool locked);
    bool isLocked() const;

    /**
     * @brief setMirrorDoc, mirror changes in doc in this editor
     * @param doc
     */
    void setMirrorDoc(EditorViewWrapper *senderWrp);
public Q_SLOT:
    void unsetMirrorDoc(const QTextDocument *senderDoc);

    QStringList &undos();
    QStringList &redos();

private Q_SLOTS:
    void mirrorDocChanged(int from, int charsRemoved, int charsAdded);
    void disconnectDoc(QObject *obj);
    void editorDestroyed();
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

    EditorViewSingleton();
    ~EditorViewSingleton();
    bool registerTextEditorType(EditorType::createT factory,
                                       const QString &name,
                                       const QStringList &suffixes,
                                       const QStringList &mimetypes,
                                       const QString &icon =
                                                QLatin1String("accessories-text-editor"));

    // register new plugin. takes ownership
    void registerLangPlugin(AbstractLangPlugin *plugin);

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
    EditorView* openFile(const QString fn, EditorView* view = nullptr) const;


    /**
     * @brief EditorViewSingleton::wrappers get all wrappers with fn open
     *          Might be open in many different views
     * @param fn the filename sto search for
     * @return List of wrappers
     */
    QList<EditorViewWrapper*> wrappers(const QString &fn) const;

    /**
     * @brief createView create a new EditorView and attach it to MainWindow
     * @param edit optional QPlainTextEdit
     * @param addToMainWindow add it to MainWindow
     * @return the created EditorView
     */
    EditorView* createView(QPlainTextEdit *edit = nullptr, bool addToMainWindow = false) const;

    /**
     * @brief createEditor create a new editor with type based on fn's mimetype.
     * @param fn filename to open
     * @param view parent view
     * @return the new editor, view has ownership
     */
    QPlainTextEdit* createEditor(const QString &fn) const;


    /**
     * @brief lastAccessed gets the last accessed EditorWrapper
     * @param backSteps negative numer back from current accessed, ie -1 for previous
     * @return the corresponding EditorWrapper
     */
    EditorViewWrapper* lastAccessedEditor(EditorView* view, int backSteps = 0) const;

    /**
     * @brief openedBySuffix gets wrappers opened differentiated by file suffixes
     * @param types list of file suffixes
     * @return a list of all EditorViewWrappers opened by mimetypes
     */
    QList<const EditorViewWrapper*> openedBySuffix(QStringList types = QStringList()) const;

    /**
     * @brief editorViews find all EditorViews in application
     * @name  filter by objectname, ie all windows which has setObjectName set
     * @return list of all EditorViews
     */
    QList<EditorView*> editorViews(const QString& name = QString()) const;

    /**
     * @brief activeView get current active view
     * @return
     */
    EditorView* activeView() const;

    /**
     * @brief editorTypeForFile finds the editorType that should open this file
     * @param fn, filename to search for
     * @return the editorType registered for this filesuffix
     */
    const EditorType *editorTypeForFile(const QString &fn) const;

Q_SIGNALS:
    /**
     * @brief emitted when number of opened files has changed
     */
    void openFilesChanged() const;

    /**
     * @brief emitted when editor document isModfied signal is emitted
     * @param fn = filename
     * @param changed = true if changed, false when not (such as undo)
     */
    void modifiedChanged(const QString &fn, bool changed) const;

    /**
     * @brief emitted when all instances of file is closed
     * @param fn = filename
     */
    void fileClosed(const QString &fn) const;
    /**
     * @brief fileOpened emitted when first editor instance opened file
     * @param fn = filename
     */
    void fileOpened(const QString &fn) const;

public Q_SLOTS:
    void docModifiedChanged(bool changed) const;
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
class EditorSearchBarP;
class EditorSearchBar : public QFrame
{
    Q_OBJECT
    EditorSearchBarP *d;
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
    EditorViewP *viewD;
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
