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


#include "PreCompiled.h"
#ifndef _PreComp_
# include <QAbstractTextDocumentLayout>
# include <QApplication>
# include <QClipboard>
# include <QDateTime>
# include <QHBoxLayout>
# include <QMessageBox>
# include <QPainter>
# include <QPrinter>
# include <QPrintDialog>
# include <QScrollBar>
# include <QPlainTextEdit>
# include <QPrintPreviewDialog>
# include <QTextBlock>
# include <QTextCodec>
# include <QTextStream>
# include <QTimer>
#endif

#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QMenu>
#include <QAction>
#include <QComboBox>
#include <QMetaObject>
#include <QMetaMethod>
#include <QDebug>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QMimeDatabase>
#include <QMimeType>
#endif

#include "EditorView.h"
#include "Application.h"
#include "MainWindow.h"
#include "BitmapFactory.h"
#include "FileDialog.h"
#include "Macro.h"
#include "PythonEditor.h"
#include "TextEditor.h"
#include "LangPluginBase.h"

#ifdef BUILD_PYTHON_DEBUGTOOLS
# include <PythonSource/PythonSourceDebugTools.h>
# include <PythonSource/PythonSourceRoot.h>
# include <QElapsedTimer>
#endif


#include <Base/Interpreter.h>
#include <Base/Parameter.h>
#include <Base/Exception.h>
#include <App/PythonDebugger.h>

using namespace Gui;
namespace Gui {


class EditorSearchClearEdit;


/// list of plugins for language, file scope on this one so all classes can access it
static QList<AbstractLangPlugin*> langPlugins; // deleted in EditorViewSingletonP

/// factory methods to create build in editortypes
static QPlainTextEdit* createQPlainTextEdit() { return new QPlainTextEdit(); }
static QPlainTextEdit* createTextEditor() { return new TextEditor(); }
static QPlainTextEdit* createPythonEditor() { return new PythonEditor(); }

// -----------------------------------------------------------------------------------

class EditorTypeP {
public:
    EditorTypeP(EditorType::createT factory, const QString name,
                QStringList suffixes, QStringList mimetypes, const QString icon) :
        name(name),
        suffixes(suffixes),
        mimetypes(mimetypes),
        iconStr(icon),
        factory(factory)
    {}
    ~EditorTypeP() {}
    const QString name;
    const QStringList suffixes,
                      mimetypes;
    const QString iconStr;
    EditorType::createT factory;
};

// --------------------------------------------------------------------------------

class EditorViewP {
public:
    EditorViewP(EditorView *view, QPlainTextEdit* editor) :
        view(view),
        searchBar(nullptr),
        activityTimer(new QTimer()),
        centralLayout(nullptr),
        topBar(nullptr)
    {
    }
    ~EditorViewP()
    {
        if (activityTimer) {
            activityTimer->stop();
            delete activityTimer;
        }
        qDeleteAll(wrappers);
    }

    /// gets the curently viewed wrapper
    EditorViewWrapper* wrapper() {
        if (wrappers.isEmpty())
            return nullptr;
        return wrappers.last();
    }

    EditorViewWrapper* wrapperForFile(const QString &fn) {
        EditorViewWrapper* wrapper = nullptr;
        for(auto wrp : wrappers) {
            if (wrp->filename() == fn) {
                wrapper = wrp;
                // update access order
                wrappers.removeAll(wrp);
                wrappers.append(wrp);
            }
        }
        return wrapper;
    }

    EditorViewWrapper* createWrapper(const QString &fn, QPlainTextEdit *editor) {
        EditorViewWrapper* ew = nullptr;
        QString icon(QLatin1String("accessories-text-editor"));

        auto et = EditorViewSingleton::instance()->editorTypeForFile(fn);
        if (!et && !editor)
            return nullptr;

        if (!editor)
            editor = et->create();

        ew = new EditorViewWrapper(editor, view, fn);

        wrappers.push_back(ew);

        // set icon for this type
        if (editor->windowIcon().isNull())
            editor->setWindowIcon(Gui::BitmapFactory().iconFromTheme(
                                        et->iconStr().toLatin1()));
        auto edit = ew->textEditor();
        if (edit)
            edit->setFilename(fn);

        auto ews = EditorViewSingleton::instance();
        view->connect(editor, &QPlainTextEdit::modificationChanged,
                      ews, &EditorViewSingleton::docModifiedChanged);
        Q_EMIT ews->openFilesChanged();
        Q_EMIT ews->fileOpened(fn);
        return ew;
    }
    bool closeWrapper(EditorViewWrapper* wrap) {
        int idx = wrappers.indexOf(wrap);
        if (idx > -1) {
            wrappers.takeAt(idx);
            auto ews = EditorViewSingleton::instance();
            view->disconnect(wrap->editor(), &QPlainTextEdit::modificationChanged,
                             ews, &EditorViewSingleton::docModifiedChanged);
        }

        return idx > -1;
    }

    QList<AbstractLangPlugin*> currentPlugins;
    QList<EditorViewWrapper*> wrappers;
    EditorView *view;
    EditorSearchBar* searchBar;
    QTimer*  activityTimer;
    QVBoxLayout *centralLayout;
    EditorViewTopBar* topBar;
    EditorView::DisplayName displayName;
};

/**
 * @brief The EditorViewDocP stores the view specific things for each doc
 *          makes switching doc live easier
 */
class EditorViewWrapperP {
public:
    EditorViewWrapperP(QPlainTextEdit* editor, EditorView *view)
        : view(view)
        , plainEdit(editor)
        , timestamp(0)
        , lock(false)
    {}
    ~EditorViewWrapperP()
    {
        if (plainEdit)
            plainEdit->deleteLater();
        plainEdit = nullptr;

    }
    EditorView* view;
    QString filename,
            mime;
    QStringList undos;
    QStringList redos;
    QPlainTextEdit* plainEdit;
    int64_t timestamp;
    bool lock;
};

/**
 * @brief The EditorViewSingletonP a singleton which owns all
 * opened editors so that they can be opened in different views.
 * For example split views, different tabs, etc
 *  saves memory
 */

class EditorViewSingletonP {
public:
    QList<EditorType*> editorTypes;

    EditorViewSingletonP() { }
    ~EditorViewSingletonP()
    {
        qDeleteAll(editorTypes);
        qDeleteAll(langPlugins);
    }
};


class EditorViewTopBarP
{
public:
    explicit EditorViewTopBarP() :
        editorView(nullptr)
    { }
    ~EditorViewTopBarP()
    { }

    EditorView *editorView;
    QComboBox    *openFiles;
    QPushButton  *closeFile;
};

class EditorSearchBarP
{
public:
    explicit EditorSearchBarP() : findFlags(nullptr) {}
    ~EditorSearchBarP() {}
    QLabel                  *foundCountLabel;
    EditorSearchClearEdit   *searchEdit;
    QPushButton             *searchButton;
    QPushButton             *upButton;
    QPushButton             *downButton;
    QPushButton             *hideButton;
    EditorSearchClearEdit   *replaceEdit;
    QPushButton             *replaceButton;
    QPushButton             *replaceAndNextButton;
    QPushButton             *replaceAllButton;
    QTextDocument::FindFlags findFlags;
};

// ------------------------------------------------------------

// ** register default editors, other editor types should register like this
// in there own cpp files
/*static struct _PyEditorRegister
{

    _PyEditorRegister() {
    }
}_pyEditorRegisterSingleton;
*/
}; // namespace Gui

// -------------------------------------------------------

/* TRANSLATOR Gui::EditorView */


EditorType::EditorType(createT factory, const QString &name,
                       const QStringList &suffixes,
                       const QStringList &mimetypes,
                       const QString &iconStr)
    : d(new EditorTypeP(factory, name, suffixes, mimetypes, iconStr))
{
}

EditorType::~EditorType()
{
}

EditorType::createT EditorType::factory() const
{
    return d->factory;
}

QPlainTextEdit *EditorType::create() const
{
    return d->factory();
}

const QString EditorType::name() const
{
    return d->name;
}

const QStringList &EditorType::suffixes() const
{
    return d->suffixes;
}

const QStringList &EditorType::mimetypes() const
{
    return d->mimetypes;
}

const QString EditorType::iconStr() const
{
    return d->iconStr;
}

// ---------------------------------------------------------

/**
 *  Constructs a EditorView which is a child of 'parent', with the
 *  name 'name'.
 */
EditorView::EditorView(QPlainTextEdit *editor,
                       const QString &fn,
                       QWidget* parent)
    : MDIView(nullptr, parent, nullptr)
    , WindowParameter( "Editor" )
    , d(new EditorViewP(this, editor))
{
    // name in tabwidget
    d->displayName = EditorView::FullName;

    // create a wrapper for this editor
    if (editor) {
        auto  editWrapper = d->createWrapper(fn, editor);
        d->wrappers.push_back(editWrapper);
    }

    // create searchbar
    d->searchBar = new EditorSearchBar(this, d);

    // Create the layout containing the workspace and a tab bar
    d->centralLayout = new QVBoxLayout;
    QFrame*     vbox = new QFrame(this);
    d->centralLayout->setMargin(0);

    if (d->wrapper())
        d->wrapper()->attach();

    if (editor) {
        d->centralLayout->addWidget(editor);
        editor->setParent(this);
    }

    d->centralLayout->addWidget(d->searchBar);
    vbox->setLayout(d->centralLayout);

    editor->setParent(vbox);
    editor->document()->setModified(false);
    //setCurrentFileName(fn);
    editor->setFocus();

    setWindowIcon(editor->windowIcon());


    // file selection drop down at top of widget
    setTopbar(new EditorViewTopBar(this));

    QFrame* hbox = new QFrame(this);
    hbox->setFrameStyle(QFrame::NoFrame);
    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->setMargin(1);
    hLayout->addWidget(vbox);
    hbox->setLayout(hLayout);
    setCentralWidget(hbox);


    ParameterGrp::handle hPrefGrp = getWindowParameter();
    hPrefGrp->Attach( this );
    hPrefGrp->NotifyAll();

    connect(d->activityTimer, SIGNAL(timeout()),
            this, SLOT(checkTimestamp()) );

    QShortcut* find = new QShortcut(this);
    find->setKey(Qt::CTRL + Qt::Key_F );
    connect(find, SIGNAL(activated()), d->searchBar, SLOT(show()));
}

/** Destroys the object and frees any allocated resources */
EditorView::~EditorView()
{
    getWindowParameter()->Detach( this );
    delete d;
}

QPlainTextEdit* EditorView::editor() const
{
    if (d->wrappers.isEmpty())
        return nullptr;
    return d->wrappers.last()->editor();
}

TextEditor *EditorView::textEditor() const
{
    return dynamic_cast<TextEditor*>(editor());
}


void EditorView::OnChange(Base::Subject<const char*> &rCaller,const char* rcReason)
{
    Q_UNUSED(rCaller)
    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (strcmp(rcReason, "EnableLineNumber") == 0) {
        //bool show = hPrefGrp->GetBool( "EnableLineNumber", true );
    }
}

void EditorView::checkTimestamp()
{
    auto wrapper = d->wrapper();
    if (!wrapper) return;
    QFileInfo fi(wrapper->filename());
    uint64_t timeStamp = fi.lastModified().toLocalTime().secsTo(QDateTime());
    if (fi.exists() && timeStamp != wrapper->timestamp()) {
        switch( QMessageBox::question( this, tr("Modified file"), 
                tr("%1.\n\nThis has been modified outside of the source editor. Do you want to reload it?")
                    .arg(wrapper->filename()),
                QMessageBox::Yes|QMessageBox::Default, QMessageBox::No|QMessageBox::Escape) )
        {
            case QMessageBox::Yes:
                // updates time stamp and timer
                open(wrapper->filename());
                return;
            case QMessageBox::No:
                wrapper->setTimestamp(timeStamp);
                break;
        }
    }

    d->activityTimer->setSingleShot(true);
    d->activityTimer->start(3000);
}

/**
 * Runs the action specified by \a pMsg.
 */
bool EditorView::onMsg(const char* pMsg,const char** ppReturn)
{
    if (strcmp(pMsg,"Save")==0){
        saveFile();
        return true;
    } else if (strcmp(pMsg,"SaveAs")==0){
        saveAs();
        return true;
    } else if (strcmp(pMsg,"Cut")==0){
        cut();
        return true;
    } else if (strcmp(pMsg,"Copy")==0){
        copy();
        return true;
    } else if (strcmp(pMsg,"Paste")==0){
        paste();
        return true;
    } else if (strcmp(pMsg,"Undo")==0){
        undo();
        return true;
    } else if (strcmp(pMsg,"Redo")==0){
        redo();
        return true;
    } else if (strcmp(pMsg,"ViewFit")==0){
        // just ignore this
        return true;
    } else if (strcmp(pMsg,"ShowFindBar")==0){
        showFindBar();
        return true;
    } else if (strcmp(pMsg,"HideFindBar")==0){
        hideFindBar();
        return true;
    }

    // let our current plugins do its things
    for (auto plug : d->currentPlugins) {
        if (plug->onMsg(this, pMsg, ppReturn))
            return true;
    }

    return false;
}

/**
 * Checks if the action \a pMsg is available. This is for enabling/disabling
 * the corresponding buttons or menu items for this action.
 */
bool EditorView::onHasMsg(const char* pMsg) const
{
    auto wrp = d->wrapper();
    if (!wrp) return false;

    if (strcmp(pMsg,"Run")==0)  return true;
    if (strcmp(pMsg,"DebugStart")==0)  return true;
    if (strcmp(pMsg,"DebugStop")==0)  return true;
    if (strcmp(pMsg,"SaveAs")==0)  return true;
    if (strcmp(pMsg,"Print")==0) return true;
    if (strcmp(pMsg,"PrintPreview")==0) return true;
    if (strcmp(pMsg,"PrintPdf")==0) return true;
    if (strcmp(pMsg,"Save")==0) { 

        return wrp->editor()->document()->isModified();
    } else if (strcmp(pMsg,"Cut")==0) {
        bool canWrite = !wrp->editor()->isReadOnly();
        return (canWrite && (wrp->editor()->textCursor().hasSelection()));
    } else if (strcmp(pMsg,"Copy")==0) {
        if (wrp->editor() &&
            wrp->editor()->document())
            return wrp->editor()->textCursor().hasSelection();
        return false;
    } else if (strcmp(pMsg,"Paste")==0) {
        QClipboard *cb = QApplication::clipboard();
        QString text;

        // Copy text from the clipboard (paste)
        text = cb->text();

        bool canWrite = !wrp->editor()->isReadOnly();
        return ( !text.isEmpty() && canWrite );
    } else if (strcmp(pMsg,"Undo")==0) {
        return wrp->editor()->document()->isUndoAvailable ();
    } else if (strcmp(pMsg,"Redo")==0) {
        return wrp->editor()->document()->isRedoAvailable ();
    } else if (strcmp(pMsg,"ShowFindBar")==0) {
        return d->searchBar->isHidden();
    } else if (strcmp(pMsg,"HideFindBar")==0) {
        return !d->searchBar->isHidden();
    }

    // let our current plugins answer to
    for (auto plug : d->currentPlugins) {
        if (plug->onHasMsg(this, pMsg))
            return true;
    }

    return false;
}

/** Checking on close state. */
bool EditorView::canClose(void)
{
    auto wrp = d->wrapper();
    if (!wrp ||
        (!wrp->editor() && wrp->editor()->document() &&
         wrp->editor()->document()->isModified()))
        return true;
    this->setFocus(); // raises the view to front
    switch(QMessageBox::question(this, tr("Unsaved document"),
                                    tr("The document has been modified.\n"
                                       "Do you want to save your changes?"),
                                     QMessageBox::Yes|QMessageBox::Default, QMessageBox::No, 
                                     QMessageBox::Cancel|QMessageBox::Escape))
    {
        case QMessageBox::Yes:
            return saveFile();
        case QMessageBox::No:
            return true;
        case QMessageBox::Cancel:
            return false;
        default:
            return false;
    }
}

bool EditorView::closeFile()
{
    auto wrp = d->wrapper();
    if (!wrp) return true;

    if (canClose()) {
        // let plugins know
        for (auto plug : currentPlugins())
            plug->onFileClose(this, wrp->filename());

        Q_EMIT closedFile(wrp->filename());

        d->centralLayout->removeWidget(wrp->editor());

        EditorViewWrapper *nextWrapper = nullptr;
        if (!d->wrappers.isEmpty())
            nextWrapper = d->wrappers.back();

        if (!nextWrapper)
            return true;

        setActiveWrapper(nextWrapper);

        delete wrp;
        return true;
    }
    return false;
}

void EditorView::setDisplayName(EditorView::DisplayName type)
{
    d->displayName = type;
}

/**
 * Saves the content of the editor to a file specified by the appearing file dialog.
 */
bool EditorView::saveAs(QString fn)
{
    auto ew = d->wrapper();
    if (fn.isEmpty() && ew) {
        QFileInfo currentName(ew->filename());
        QStringList types;
        types << QLatin1String("all files (*.*)");

        auto textEditor = ew->textEditor();
        if (textEditor && textEditor->syntax() == QLatin1String("Python")) {
            types << QLatin1String("FreeCAD macro (*.FCMacro)")
                  << QLatin1String("Python (*.py)");
        } else if(currentName.exists()) {
            types << QString::fromLatin1("(*.%1)").arg(currentName.suffix());
        }

        fn = FileDialog::getSaveFileName(this, QObject::tr("Save file"),
                                    currentName.filePath(),
                                    types.join(QLatin1String(";;")));
        if (fn.isEmpty())
            return false;

        ew->setFilename(fn);
        return saveFile();
    }

    if (ew && ew->editor() && ew->editor()->document()) {
        ew->editor()->document()->setModified(false);
        return saveFile();
    }
    return false;
}

/**
 * Switches to the editor that has filename open, or create a new editor with this filename
 */
bool EditorView::open(const QString& fn)
{
    QString absPath = fn;
    if (!fn.isEmpty()) {
        absPath = QFileInfo(fn).absoluteFilePath();
    }

    auto wrp = wrapper(fn);
    if (fn.isEmpty() || !wrp) {
        // create a new empty editor or
        // has filename but not yet opened in this view
        auto editor = EditorViewSingleton::instance()->createEditor(fn);
        wrp = d->createWrapper(absPath, editor);

        if (!fn.isEmpty()) {
            QFileInfo fi(fn);
            int64_t timestamp = fi.lastModified().toLocalTime().secsTo(QDateTime());
            wrp->setTimestamp(timestamp);
        }
    }


    // untitled files should be writable
    bool writable = fn.isEmpty();
    if (writable && !QFileInfo(fn).isWritable())
        writable = false;
    wrp->editor()->setReadOnly(writable);

    d->activityTimer->setSingleShot(true);
    d->activityTimer->start(3000);

    setActiveWrapper(wrp);

    Q_EMIT switchedFile(absPath);
    showCurrentFileName();

    // let plugins do whitespace trim etc
    for (auto plug : currentPlugins())
        plug->onFileOpen(this, d->wrapper()->filename());

    return true;
}

bool EditorView::openDlg()
{
    if (editor()->document()->isModified())
        save();

    WindowParameter param("PythonDebuggerView");
    QString path = QString::fromStdString(
                        param.getWindowParameter()->GetASCII("MacroPath",
                               App::Application::getUserMacroDir().c_str()));

    QString fn;
    if (filename().isEmpty())
        fn = QFileDialog::getOpenFileName(getMainWindow(), tr("Open file"),
                                               path, QLatin1String("*.py;;*.FCMacro;;*"));
    if (!fn.isEmpty()) {
        EditorViewSingleton::instance()->openFile(fn, this);
        return true;
    }
    return false;
}

bool EditorView::save()
{
    if (filename().isEmpty()) {
        if (editor()->document()->isModified())
            return saveAs();
    } else
        return saveFile();

    return false;
}

/**
 * Copies the selected text to the clipboard and deletes it from the text edit.
 * If there is no selected text nothing happens.
 */
void EditorView::cut(void)
{
    if (d->wrapper() && d->wrapper()->editor())
        d->wrapper()->editor()->cut();
}

/**
 * Copies any selected text to the clipboard.
 */
void EditorView::copy(void)
{
    if (d->wrapper() && d->wrapper()->editor())
        d->wrapper()->editor()->copy();
}

/**
 * Pastes the text from the clipboard into the text edit at the current cursor position. 
 * If there is no text in the clipboard nothing happens.
 */
void EditorView::paste(void)
{
    if (d->wrapper() && d->wrapper()->editor())
        d->wrapper()->editor()->paste();
}

/**
 * Undoes the last operation.
 * If there is no operation to undo, i.e. there is no undo step in the undo/redo history, nothing happens.
 */
void EditorView::undo(void)
{
    auto wrp = d->wrapper();
    if (wrp && wrp->editor()) {
        wrp->setLocked(true);
        if (!wrp->undos().isEmpty()) {
            wrp->redos() << wrp->undos().back();
            wrp->undos().pop_back();
        }
        wrp->editor()->document()->undo();
        wrp->setLocked(false);
    }
}

/**
 * Redoes the last operation.
 * If there is no operation to undo, i.e. there is no undo step in the undo/redo history, nothing happens.
 */
void EditorView::redo(void)
{
    auto wrp = d->wrapper();
    if (wrp && wrp->editor()) {
        wrp->setLocked(true);
        if (!wrp->redos().isEmpty()) {
            wrp->undos() << wrp->redos().back();
            wrp->redos().pop_back();
        }
        wrp->editor()->document()->redo();
        wrp->setLocked(false);
    }
}

void EditorView::newFile()
{
    auto ews = Gui::EditorViewSingleton::instance();
    auto editor = ews->createEditor(QString());
    auto wrp = d->createWrapper(QString(), editor);
    setActiveWrapper(wrp);
}

/**
 * Shows the printer dialog.
 */
void EditorView::print()
{
    if (!editor() || !editor()->document())
        throw Base::FileException("No wrapper attached, can't print EditorView::print()");

    QPrinter printer(QPrinter::ScreenResolution);
    printer.setFullPage(true);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() == QDialog::Accepted)
        d->wrapper()->editor()->document()->print(&printer);
}

void EditorView::printPreview()
{
    QPrinter printer(QPrinter::ScreenResolution);
    QPrintPreviewDialog dlg(&printer, this);
    connect(&dlg, SIGNAL(paintRequested (QPrinter *)),
            this, SLOT(print(QPrinter *)));
    dlg.exec();
}

void EditorView::print(QPrinter* printer)
{
    if (!editor() || !editor()->document())
        throw Base::FileException("No wrapper attached, can't print EditorView::print(printer)");

    d->wrapper()->editor()->document()->print(printer);
}

void EditorView::refreshLangPlugins()
{
    if (!d->wrapper())
        return;

    auto editor = dynamic_cast<TextEditor*>(d->wrapper()->editor());
    if (!editor)
        return;

    d->currentPlugins.clear();
    for(auto plugin : langPlugins) {
        if (plugin->matchesMimeType(editor->filename(), editor->mimetype()))
            d->currentPlugins.push_back(plugin);
    }
}

void EditorView::showFindBar()
{
    d->searchBar->show();
}

void EditorView::hideFindBar()
{
    d->searchBar->hide();
}

/**
 * Prints the document into a Pdf file.
 */
void EditorView::printPdf()
{
    if (!editor() || !editor()->document())
        throw Base::FileException("No wrapper attached, can't print EditorView::printPdf()");

    QString fileName = FileDialog::getSaveFileName(this, tr("Export PDF"), QString(),
        QString::fromLatin1("%1 (*.pdf)").arg(tr("PDF file")));
    if (!fileName.isEmpty()) {
        QPrinter printer(QPrinter::ScreenResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        d->wrapper()->editor()->document()->print(&printer);
    }
}

void EditorView::showCurrentFileName()
{
    if (!editor() || !editor()->document())
        return;

    QString name;
    QFileInfo fi(d->wrapper()->filename());
    switch (d->displayName) {
    case FullName:
        name = fi.absoluteFilePath();
        break;
    case FileName:
        name = fi.fileName();
        break;
    case BaseName:
        name = fi.baseName();
        break;
    }

    QString shownName;
    if (name.isEmpty())
        shownName = tr("untitled[*]");
    else
        shownName = QString::fromLatin1("%1[*]").arg(name);

    if (d->wrapper()->editor()->isReadOnly())
        shownName += tr(" - Read-only");
    else
        shownName += tr(" - Editor");

    setWindowTitle(shownName);
    editor()->setDocument(new QTextDocument(editor()));
    setWindowModified(d->wrapper()->editor()->document()->isModified());

    refreshLangPlugins();
}

QString EditorView::filename() const
{
    if (d->wrapper())
        return d->wrapper()->filename();
    return QString();
}

void EditorView::setFilename(QString fileName)
{
    if (d->wrapper())
        d->wrapper()->setFilename(fileName);
}

const QList<AbstractLangPlugin *> EditorView::currentPlugins() const
{
    return d->currentPlugins;
}

const AbstractLangPlugin* EditorView::currentPluginByName(const char* name)
{
    for (auto plugin : d->currentPlugins) {
        if (strncmp(plugin->name(), name, 1024) == 0)
            return plugin;
    }
    return nullptr;
}

void EditorView::setTopbar(EditorViewTopBar* topBar)
{
    d->topBar = topBar;
    d->topBar->setParent(this);
    insertWidget(d->topBar, 0);
}

EditorViewTopBar* EditorView::topBar()
{
    return d->topBar;
}

EditorViewWrapper *EditorView::wrapper() const
{
    return d->wrapper();
}

EditorViewWrapper *EditorView::wrapper(const QString &fn) const
{
    for (auto editWrapper : d->wrappers) {
        if (editWrapper->filename() == fn)
            return editWrapper;
    }
    return nullptr;
}

QList<EditorViewWrapper *> EditorView::wrappers(const QString &fn) const
{
    QList<EditorViewWrapper*> ret;
    for (auto wrp : d->wrappers)
        if (fn.isEmpty() || fn == wrp->filename())
            ret.push_back(wrp);
    return ret;
}

void EditorView::setWindowModified(bool modified)
{
    MDIView::setWindowModified(modified);
    Q_EMIT modifiedChanged(modified);
}

/**
 * Saves the contents to a file.
 */
bool EditorView::saveFile()
{
    if (!d->wrapper() || !d->wrapper()->editor())
        return false;
    if (d->wrapper()->filename().isEmpty())
        return saveAs();

    // let plugins do whitespace trim etc
    for (auto plug : currentPlugins())
        plug->onFileSave(this, d->wrapper()->filename());

    QFile file(d->wrapper()->filename());
    if (!file.open(QFile::WriteOnly))
        return false;

    auto editor = d->wrapper()->editor();

    QTextStream ts(&file);
    ts.setCodec(QTextCodec::codecForName("UTF-8"));
    ts << editor->document()->toPlainText();
    file.close();

    editor->document()->setModified(false);
    auto textEditor = dynamic_cast<TextEditor*>(editor);
    if (textEditor)
        textEditor->onSave();

    // notify all other possibly open editors with the same file, that this has changed
    QFileInfo fi(d->wrapper()->filename());
    for (auto wrap : EditorViewSingleton::instance()->wrappers(
                                            d->wrapper()->filename()))
    {
        wrap->setTimestamp(fi.lastModified().toLocalTime().secsTo(QDateTime()));
    }
#ifdef BUILD_PYTHON_DEBUGTOOLS
        {
            //DumpSyntaxTokens tok(editor->document()->begin());
            Python::DumpModule dMod(Python::SourceRoot::instance()->moduleFromPath(
                                        d->wrapper()->filename().toStdString()));
        }
#endif
    return true;
}

void EditorView::setActiveWrapper(EditorViewWrapper *wrp)
{
    if (!wrp) return;
    auto old = d->wrapper();
    if (old)
        old->detach();
    // change access order
    d->wrappers.removeOne(wrp);
    d->wrappers.push_back(wrp);

    swapEditor(wrp->editor());
    wrp->attach();

    Q_EMIT switchedFile(wrp->filename());
}

bool EditorView::closeWrapper(EditorViewWrapper *wrapper)
{
    return d->closeWrapper(wrapper);
}

QPlainTextEdit *EditorView::swapEditor(QPlainTextEdit* newEditor)
{
    QPlainTextEdit *old = nullptr;
    int posEdit = -100, posTop = -100, posSearch = -100;
    for (int i = 0; i < d->centralLayout->count(); ++i) {
        if (dynamic_cast<QPlainTextEdit*>(d->centralLayout->itemAt(i)->widget()))
            posEdit = i;
        if (dynamic_cast<EditorSearchBar*>(d->centralLayout->itemAt(i)->widget()))
            posSearch = i;
        if (dynamic_cast<EditorViewTopBar*>(d->centralLayout->itemAt(i)->widget()))
            posTop = i;
    }

    if (posEdit > -100) {
        auto oldItem = d->centralLayout->takeAt(posEdit);
        d->centralLayout->insertWidget(posEdit, newEditor);
        old = dynamic_cast<QPlainTextEdit*>(oldItem->widget());
        old->setVisible(false);
    } else if (posTop > -100) {
        d->centralLayout->insertWidget(posTop+1, newEditor);
    } else if (posEdit > -100) {
        d->centralLayout->insertWidget(posSearch, newEditor);
    } else {
        d->centralLayout->addWidget(newEditor);
    }

    newEditor->setVisible(true);
    newEditor->setParent(static_cast<QWidget*>(d->centralLayout->parent()));
    newEditor->setFocus();
    setWindowIcon(newEditor->windowIcon());
    newEditor->show();

    return old;
}

void EditorView::undoAvailable(bool undo)
{
    Q_UNUSED(undo)
    if (d->wrapper())
        d->wrapper()->undos().clear();
}

void EditorView::redoAvailable(bool redo)
{
    if (!redo && d->wrapper())
        d->wrapper()->redos().clear();
}

void EditorView::contentsChange(int position, int charsRemoved, int charsAdded)
{
    Q_UNUSED(position)
    auto wrp = d->wrapper();
    if (!wrp) return;

    if (wrp->isLocked())
        return;
    if (charsRemoved > 0 && charsAdded > 0)
        return; // syntax highlighting
    else if (charsRemoved > 0)
        wrp->undos() << tr("%1 chars removed").arg(charsRemoved);
    else if (charsAdded > 0)
        wrp->undos() << tr("%1 chars added").arg(charsAdded);
    else
        wrp->undos() << tr("Formatted");
    wrp->redos().clear();
}

/**
 * Get the undo history.
 */
QStringList EditorView::undoActions() const
{
    auto wrp = d->wrapper();
    if (wrp)
        return wrp->undos();
    return QStringList();
}

/**
 * Get the redo history.
 */
QStringList EditorView::redoActions() const
{
    auto wrp = d->wrapper();
    if (wrp && wrp->editor())
        return wrp->redos();
    return QStringList();
}

void EditorView::focusInEvent (QFocusEvent *)
{
    auto wrp = d->wrapper();
    if (wrp && wrp->editor())
        return wrp->editor()->setFocus();
    return;
}

void EditorView::insertWidget(QWidget *wgt, int index)
{

    if (index > -1)
        d->centralLayout->insertWidget(index, wgt);
    else
        d->centralLayout->addWidget(wgt);
}

EditorViewWrapper* EditorView::editorWrapper() const
{
    return d->wrapper();
}

// -------------------------------------------------------------------------------

EditorViewWrapper::EditorViewWrapper(QPlainTextEdit *editor,
                                     EditorView *view,
                                     const QString &fn)
    : QObject()
    , d(new EditorViewWrapperP(editor, view))
{
    d->plainEdit = editor;
    d->timestamp = 0;
    d->filename = fn;

    auto textEdit = textEditor();
    if (textEdit)
        textEdit->setWrapper(this);

    if (QFile::exists(d->filename)) {
        QFile file(d->filename);
        if (file.open(QFile::ReadOnly)) {
            d->lock = true;

#ifdef BUILD_PYTHON_DEBUGTOOLS
            QElapsedTimer timer;
            timer.start();
#endif
            d->plainEdit->setPlainText(QString::fromUtf8(file.readAll()));

#ifdef BUILD_PYTHON_DEBUGTOOLS
            qDebug() << QString::fromLatin1("setPlainText/Lexer took %1ms %2ns").arg(timer.elapsed()).arg(timer.nsecsElapsed()) << endl;
#endif
            d->lock = false;
            file.close();
        }
    }
}

EditorViewWrapper::~EditorViewWrapper()
{
    auto txtEdit = textEditor();
    if (txtEdit) {
        txtEdit->setWrapper(nullptr);
    }
    close();
    if (d->view)
        d->view->closeWrapper(this);
    d->view = nullptr;
    delete d;
}

void EditorViewWrapper::attach()
{
    if (d->view) {
        // hook up signals with view
        QObject::connect(d->plainEdit->document(), SIGNAL(modificationChanged(bool)),
                d->view, SLOT(setWindowModified(bool)));
        QObject::connect(d->plainEdit->document(), SIGNAL(undoAvailable(bool)),
                d->view, SLOT(undoAvailable(bool)));
        QObject::connect(d->plainEdit->document(), SIGNAL(redoAvailable(bool)),
                d->view, SLOT(redoAvailable(bool)));
        QObject::connect(d->plainEdit->document(), SIGNAL(contentsChange(int, int, int)),
                d->view, SLOT(contentsChange(int, int, int)));

        if (d->plainEdit && d->plainEdit->document())
            d->view->setWindowModified(d->plainEdit->document()->isModified());
    }
}

void EditorViewWrapper::detach()
{
    if (d->view) {
        // disconnect signals from view
        QObject::disconnect(d->plainEdit->document(), SIGNAL(modificationChanged(bool)),
                d->view, SLOT(setWindowModified(bool)));
        QObject::disconnect(d->plainEdit->document(), SIGNAL(undoAvailable(bool)),
                d->view, SLOT(undoAvailable(bool)));
        QObject::disconnect(d->plainEdit->document(), SIGNAL(redoAvailable(bool)),
                d->view, SLOT(redoAvailable(bool)));
        QObject::disconnect(d->plainEdit->document(), SIGNAL(contentsChange(int, int, int)),
                d->view, SLOT(contentsChange(int, int, int)));
    }
}

void EditorViewWrapper::close()
{
    detach();

    // cleanup and memory release
    if (d->view)
        d->view->closeWrapper(this);
    if (d->plainEdit)
        d->plainEdit->deleteLater();

    d->plainEdit = nullptr;

    // emit changes
    Q_EMIT EditorViewSingleton::instance()->openFilesChanged();
    Q_EMIT EditorViewSingleton::instance()->fileClosed(d->filename);
}

QPlainTextEdit* EditorViewWrapper::editor() const
{
    return d->plainEdit;
}

TextEditor* EditorViewWrapper::textEditor() const
{
    return dynamic_cast<TextEditor*>(d->plainEdit);
}

EditorView* EditorViewWrapper::view() const
{
    return d->view;
}

QString EditorViewWrapper::filename() const
{
    return d->filename;
}

void EditorViewWrapper::setFilename(const QString &fn)
{
    QFileInfo oldFi(d->filename);

    d->filename = QFileInfo(fn).absoluteFilePath();

    QFileInfo fi(fn);
    if (!fn.isEmpty() && fi.suffix() != fi.baseName() && fi.suffix() != oldFi.suffix()) {
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(fn);
        if (type.isValid())
            d->mime = type.name();
    }

    // some editors need filename, for example PythonEditor
    auto textEdit = textEditor();
    if (textEdit)
        textEdit->setFilename(fn);

    if (d->view) {
        d->view->showCurrentFileName();
        Q_EMIT d->view->changeFileName(fn);
    }
}

const QString &EditorViewWrapper::mimetype() const
{
    return d->mime;
}

void EditorViewWrapper::setMimetype(QString mime)
{
    d->mime = mime;
    auto edit = dynamic_cast<TextEditor*>(d->plainEdit);
    if (edit)
        edit->setMimetype(mime);
}

int64_t EditorViewWrapper::timestamp() const
{
    return d->timestamp;
}

void EditorViewWrapper::setTimestamp(int64_t timestamp)
{
    d->timestamp = timestamp;
}

void EditorViewWrapper::setLocked(bool locked)
{
    d->lock = locked;
}

bool EditorViewWrapper::isLocked() const
{
    return d->lock;
}

void EditorViewWrapper::setMirrorDoc(EditorViewWrapper *senderWrp)
{
    auto edit = editor(); auto senderDoc = senderWrp->editor()->document();
    if (edit->document() != senderDoc && senderDoc != nullptr) {
        qDebug() << QLatin1String("setMirrorDoc sender:") << senderDoc
                 << QLatin1String(" reciever doc:") << d->plainEdit->document()
                 << QLatin1String(" this: ") << this
                 << QLatin1String(" senderWrp: ") << senderWrp << endl;
        connect(senderDoc, &QTextDocument::contentsChange,
                this, &EditorViewWrapper::mirrorDocChanged);

        auto self = this;
        connect(edit->document(), &QTextDocument::destroyed, [=](QObject*){
            if (!connect(senderDoc, &QTextDocument::contentsChange,
                         self, &EditorViewWrapper::mirrorDocChanged))
                qDebug() << QLatin1String("failed to disconnect from senderDoc");
            else
                qDebug() << QLatin1String("disconnected from senderDoc ") << senderDoc
                         << QLatin1String(" this:") << this
                         << QLatin1String(" senderWrp: ") << senderWrp << endl;
        });

        //connect(senderDoc, &QTextDocument::destroyed,
        //        this, &EditorViewWrapper::disconnectDoc);
        //senderWrp->connect(edit->document(), &QTextDocument::destroyed,
        //        senderWrp, &EditorViewWrapper::disconnectDoc);
    }
}

void EditorViewWrapper::unsetMirrorDoc(const QTextDocument *senderDoc)
{
    qDebug() << QLatin1String("unsetMirrorDoc sender:") << senderDoc
             << QLatin1String(" reciever doc:") << ( d->plainEdit ? d->plainEdit->document() : nullptr);
    if (!disconnect(senderDoc, &QTextDocument::contentsChange,
               this, &EditorViewWrapper::mirrorDocChanged))
        qDebug() << QLatin1String("failed disconnect senderDoc contentschange");
    if (!disconnect(senderDoc, &QTextDocument::destroyed,
               this, &EditorViewWrapper::disconnectDoc))
        qDebug() << QLatin1String("failed to disconnet senderDoc from destryed");
    //if (d->plainEdit)
    //    disconnect(d->plainEdit->document(), &QTextDocument::destroyed,
    //               this, &EditorViewWrapper::disconnectDoc);
    qDebug() << endl;
}

QStringList &EditorViewWrapper::undos()
{
    return d->undos;
}

QStringList &EditorViewWrapper::redos()
{
    return d->redos;
}

void EditorViewWrapper::mirrorDocChanged(int from, int charsRemoved, int charsAdded)
{
    auto sendDoc = qobject_cast<QTextDocument*>(sender());
    qDebug() << QLatin1String("mirrorDocChanged sender:") << sendDoc
             << QLatin1String(" this: ") << this;
    if (sendDoc && d->plainEdit && d->plainEdit->document()) {
        auto thisDoc = d->plainEdit->document();
        qDebug() << QLatin1String(" reciever:") << thisDoc << endl;

        QTextCursor thisCursor(thisDoc),
                    sendCursor(sendDoc);
        // prevent this document form sending signals to sendDoc (prevent endless cycles)
        bool previousBlock = thisDoc->blockSignals(true);

        thisCursor.setPosition(from);
        sendCursor.setPosition(from);

        // text remove´
        thisCursor.setPosition(from + charsRemoved, QTextCursor::KeepAnchor);
        thisCursor.removeSelectedText();

        // text insertion
        sendCursor.setPosition(from - charsRemoved);
        sendCursor.setPosition(from - charsRemoved + charsAdded,
                               QTextCursor::KeepAnchor);
        auto insertText = sendCursor.selectedText();
        thisCursor.insertText(insertText);

        thisDoc->blockSignals(previousBlock);
    }
}

void EditorViewWrapper::disconnectDoc(QObject *obj)
{
    auto doc = reinterpret_cast<QTextDocument*>(obj);
    if (doc)
        unsetMirrorDoc(doc);
}


// -------------------------------------------------------------------
// sort of private, outside of class to prevent deadlock when code
// in constructor ends up calling constructor again
static EditorViewSingleton *_EditorViewInstance = nullptr;

EditorViewSingleton::EditorViewSingleton() :
    d(new EditorViewSingletonP())
{
    _EditorViewInstance = this;

    //_PyEditorRegister();

    // register default build in types
    QStringList plainSuffix { QLatin1String("*") };
    QStringList plainMime { QLatin1String("empty") };

    QStringList txtSuffix { QLatin1String("*") };
    QStringList txtMime {
        QLatin1String("text/text"),
        QLatin1String("text/*")
    };

    QStringList pySuffix { QLatin1String("py"), QLatin1String("fcmacro") };
    QStringList pyMime {
        QLatin1String("text/x-script.python"),
        QLatin1String("text/x-script.phyton"),
        QLatin1String("text/x-python")
    };
// disable for now, source parser is broken
//    registerTextEditorType(
//         reinterpret_cast<EditorType::createT>(&createPythonEditor),
//                QLatin1String("PythonEditor"),
//                pySuffix,
//                pyMime,
//                QLatin1String("document-python")
//    );
    registerTextEditorType(
         reinterpret_cast<EditorType::createT>(&createTextEditor),
                QLatin1String("TextEditor"),
                txtSuffix,
                txtMime,
                QLatin1String("TextDocument")
    );
    registerTextEditorType(
         reinterpret_cast<EditorType::createT>(&createQPlainTextEdit),
                QLatin1String("QPlainTextEdit"),
                plainSuffix,
                plainMime,
                QLatin1String("applications-new")
    );

    // our language plugins, must doi these after eventloop is started
    QTimer::singleShot(0, []() {
        AbstractLangPlugin::registerBuildInPlugins();
    });
}

EditorViewSingleton::~EditorViewSingleton()
{
    delete d;
}

bool EditorViewSingleton::registerTextEditorType(EditorType::createT factory,
                                                 const QString &name,
                                                 const QStringList &suffixes,
                                                 const QStringList &mimetypes,
                                                 const QString &icon)
{
    for (auto et : d->editorTypes) {
        if (et->name() == name)
            return false; // already registered
    }

    // new editor type, register
    auto editType = new EditorType(factory, name, suffixes, mimetypes, icon);
    d->editorTypes.append(editType);
    return true;
}

void EditorViewSingleton::registerLangPlugin(AbstractLangPlugin *plugin)
{
    for (auto plug : langPlugins) {
        if (plug->name() == plugin->name()) {
            delete plugin;
            return;
        }
    }

    langPlugins.push_back(plugin);

    if (qApp->startingUp())
        return; // cant refresh a non created view

    for (auto view : editorViews()) {
        for(const auto &wrp : view->wrappers()) {
            if (wrp && wrp->editor() == wrp->view()->editor()) {
                if (plugin->matchesMimeType(wrp->filename(), wrp->mimetype()))
                    wrp->view()->refreshLangPlugins();
            }
        }
    }
}

/* static */
EditorViewSingleton* EditorViewSingleton::instance()
{
    return _EditorViewInstance ? _EditorViewInstance : new EditorViewSingleton();
}

EditorView* EditorViewSingleton::openFile(const QString fn, EditorView* view) const
{
    if (!view) {
        // find a edit view among windows
        auto views = editorViews();

        if (!views.isEmpty())
            view = views.first();
        else
            // create a new view
            view = createView(createEditor(fn));
    }

    Q_ASSERT(view != nullptr);
    MainWindow::getInstance()->setActiveWindow(view);
    view->open(fn);

    return view;
}

QList<EditorViewWrapper*>
EditorViewSingleton::wrappers(const QString &fn) const
{
    QList<EditorViewWrapper*> wrappers;
    for (auto view : editorViews()) {
        for (auto editWrapper : view->wrappers()) {
            if (editWrapper->filename() == fn || fn.isEmpty())
                wrappers.append(editWrapper);
        }
    }

    return wrappers;
}

EditorView *EditorViewSingleton::createView(QPlainTextEdit *edit, bool addToMainWindow) const
{
    auto mainWindow = addToMainWindow ? MainWindow::getInstance() : nullptr;
    QString fn;
    auto textEdit = dynamic_cast<TextEditor*>(edit);
    if (textEdit)
        fn = textEdit->filename();
    auto editorView = new EditorView(edit, fn, mainWindow);
    editorView->setDisplayName(EditorView::FileName);
    if (addToMainWindow)
        MainWindow::getInstance()->addWindow(editorView);
    return editorView;
}

QPlainTextEdit *EditorViewSingleton::createEditor(const QString &fn) const
{
    auto et = editorTypeForFile(fn);
    if (et)
        return et->create();
    return new TextEditor();
}

EditorViewWrapper*
EditorViewSingleton::lastAccessedEditor(EditorView* view, int backSteps) const
{
    for (auto v : editorViews()) {
        if (view == v) {
            auto wrappers = view->wrappers();
            int idx = wrappers.size() + backSteps -1;
            if (idx >= 0 && idx < wrappers.size())
                return wrappers.at(idx);
        }
    }
    return nullptr;
}

QList<const EditorViewWrapper*>
EditorViewSingleton::openedBySuffix(QStringList types) const
{
    QList<const EditorViewWrapper*> res;
    for (auto view : editorViews()) {
        for (auto editWrapper : view->wrappers()) {
            QFileInfo fi(editWrapper->filename());
            if (types.size() == 0 || types.contains(fi.suffix(), Qt::CaseInsensitive))
                res.append(editWrapper);
        }
    }
    return res;
}

QList<EditorView *> EditorViewSingleton::editorViews(const QString &name) const
{
    QList<EditorView*> views;
    for (auto wdgt : QApplication::topLevelWidgets()) {
        auto v = wdgt->findChildren<EditorView*>(name);
        if (!v.isEmpty())
            views.append(v);
    }

    return views;
}

EditorView *EditorViewSingleton::activeView() const
{
    auto views = editorViews();
    for (auto view : editorViews()) {
        if (view->editor()->hasFocus())
            return view;
    }

    if (views.isEmpty())
        return createView();
    return views.last();
}

void EditorViewSingleton::docModifiedChanged(bool changed) const
{
    QObject *obj = sender();
    for (auto view : editorViews()) {
        for (auto ew : view->wrappers()) {
            if (ew->editor() == obj)
                Q_EMIT modifiedChanged(ew->filename(), changed);
        }
    }
}

const EditorType *EditorViewSingleton::editorTypeForFile(const QString &fn) const
{
    QString mime = QLatin1String("text/plain"); // default ;
    QFileInfo fi(fn);
    QString suffix = fi.suffix().toLower();
    if (suffix == QLatin1String("fcmacro")) {
        mime = QLatin1String("text/x-script.python");
    } else if (fi.isFile()) {
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(fn);
        if (type.isValid())
            mime = type.name();
    }

    for (const auto et : d->editorTypes) {
        for (auto suf : et->suffixes()) {
            // first find via file suffix, ie .py or wildcard ht* matches html and htm and hta etc
            auto fiSuf = fi.suffix().toLower();
            if ((suf == fiSuf) ||
                (suf.right(1) == QLatin1String("*") &&
                suf.left(suf.length()-1) == fiSuf.left(suf.length()-1)))
            {
                return et;
            }
        }

        // find the registered editor for this mimetype also match wildcard at the end
        for (auto mi : et->mimetypes()) {
            if (mi == mime ||
                (mi.right(1) == QLatin1String("*") &&
                 mi.left(mi.length()-1) == mime.left(mi.length()-1)))
            {
                return et;
            }
        }
    }
    return nullptr;
}

// -------------------------------------------------------------------------------------

EditorViewTopBar::EditorViewTopBar(EditorView *parent)
    : QWidget(parent)
    , d(new EditorViewTopBarP)
{
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setContentsMargins(0,0,0,0);
    d->openFiles = new QComboBox(this);
    d->openFiles->setMinimumContentsLength(40);
    d->openFiles->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    d->closeFile = new QPushButton(this);
    d->closeFile->setIcon(BitmapFactory().iconFromTheme("delete"));
    hLayout->addWidget(d->openFiles);
    hLayout->addWidget(d->closeFile);
    hLayout->addStretch();
    setLayout(hLayout);


    if (parent)
        setParent(parent);
}

EditorViewTopBar::~EditorViewTopBar()
{
    delete d;
}

void EditorViewTopBar::setParent(QWidget *parent)
{
    d->editorView = qobject_cast<EditorView*>(parent);
    QWidget::setParent(parent);

    if (!d->editorView)
        return;

    EditorViewSingleton *evs = EditorViewSingleton::instance();

    EditorView *editView = qobject_cast<EditorView*>(parent);
    if (editView) {
        if (d->editorView) {
           disconnect(d->closeFile, SIGNAL(clicked()),
                      this, SLOT(closeCurrentFile()));
           disconnect(d->openFiles, SIGNAL(currentIndexChanged(int)),
                      this, SLOT(switchFile(int)));
           disconnect(d->editorView, SIGNAL(changeFileName(QString)),
                           this, SLOT(setCurrentIdx(QString)));
           disconnect(d->editorView, SIGNAL(switchedFile(QString)),
                           this, SLOT(setCurrentIdx(QString)));

           disconnect(evs, SIGNAL(openFilesChanged()),
                          this, SLOT(rebuildOpenedFiles()));
           disconnect(evs, SIGNAL(modifiedChanged(QString,bool)),
                           this, SLOT(modifiedChanged(QString,bool)));
        }

        connect(d->closeFile, SIGNAL(clicked()),
                this, SLOT(closeCurrentFile()));
        connect(d->openFiles, SIGNAL(currentIndexChanged(int)),
                this, SLOT(switchFile(int)));

        connect(d->editorView, SIGNAL(changeFileName(QString)),
                        this, SLOT(setCurrentIdx(QString)));
        connect(d->editorView, SIGNAL(switchedFile(QString)),
                        this, SLOT(setCurrentIdx(QString)));

        connect(evs, SIGNAL(openFilesChanged()),
                       this, SLOT(rebuildOpenedFiles()));
        connect(evs, SIGNAL(modifiedChanged(QString,bool)),
                        this, SLOT(modifiedChanged(QString,bool)));
    }
}

void EditorViewTopBar::rebuildOpenedFiles()
{
    // clear old
    disconnect(d->openFiles, SIGNAL(currentIndexChanged(int)),
                   this, SLOT(switchFile(int)));
    while (d->openFiles->count())
        d->openFiles->removeItem(0);

    for (auto wrapper : EditorViewSingleton::instance()->
                                    openedBySuffix())
    {
        if (!wrapper->editor() || !wrapper->editor()->document())
            continue;

        auto doc = !wrapper->editor()->document() ?
                            wrapper->editor()->document() : nullptr;
        bool modified = doc ? doc->isModified() : false;
        auto vName = createViewName(wrapper->filename(), modified);
        d->openFiles->addItem(wrapper->editor()->windowIcon(),
                              vName, wrapper->filename());
    }

    setCurrentIdx(d->editorView->filename());

    connect(d->openFiles, SIGNAL(currentIndexChanged(int)),
                   this, SLOT(switchFile(int)));
}

void EditorViewTopBar::setCurrentIdx(const QString fileName)
{
    int idx = d->openFiles->findData(fileName);
    if (idx > -1)
        d->openFiles->setCurrentIndex(idx);
}

void EditorViewTopBar::closeCurrentFile()
{
    d->editorView->closeFile();
}

void EditorViewTopBar::switchFile(int index) const
{
    QString fileName = d->openFiles->itemData(index).toString();
    if (d->editorView && fileName != d->editorView->filename())
        d->editorView->open(fileName);
}

void EditorViewTopBar::modifiedChanged(const QString &fn, bool changed)
{
    int idx = 0;
    for (; idx < d->openFiles->count(); ++idx) {
        if (d->openFiles->itemData(idx) == fn)
            break;
    }

    if (idx >= d->openFiles->count())
        return;

    d->openFiles->setItemText(idx, createViewName(d->openFiles->itemData(idx).toString(),
                                                 changed));
}

QString EditorViewTopBar::createViewName(const QString &fn, bool changed) const
{
    // set shown name
    QFileInfo fi(fn);
    QString viewName(fi.fileName());
    if (fn.isEmpty())
        viewName = tr("untitled");

    if (changed)
        viewName += QLatin1String("*");

    return viewName;
}



// -------------------------------------------------------------------------------------

/**
 * @brief a lineedit class that shows a clear button and a settings button
 */
EditorSearchClearEdit::EditorSearchClearEdit(QWidget *parent, bool showSettingButton) : QLineEdit(parent)
{
    QHBoxLayout *editLayout = new QHBoxLayout(this);
    setLayout(editLayout);
    editLayout->setContentsMargins(0, 0, 0, 0);

    // set text to not interfere with these buttons
    QMargins margins = textMargins();

    // a search settings button to lineedit
    if (showSettingButton) {
        QPushButton *settingsBtn = new QPushButton(this);
        editLayout->addWidget(settingsBtn);
        connect(settingsBtn, SIGNAL(clicked()), this, SIGNAL(showSettings()));
        settingsBtn->setFlat(true);
        settingsBtn->setCursor(QCursor(Qt::ArrowCursor));
        QIcon settIcon = BitmapFactory().iconFromTheme("search-setting");
        settingsBtn->setIcon(settIcon);
        margins.setLeft(margins.left() +
                            settIcon.actualSize(settingsBtn->size()).width() + 10);
    }

    // add a clear button
    editLayout->addStretch();
    QPushButton *clearButton = new QPushButton(this);
    editLayout->addWidget(clearButton);
    connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(clearButton, SIGNAL(clicked()), this, SLOT(setFocus()));
    clearButton->setFlat(true);
    clearButton->setCursor(QCursor(Qt::ArrowCursor));
    QIcon clearIcon = BitmapFactory().iconFromTheme("clear-text");
    clearButton->setIcon(clearIcon);
    margins.setRight(margins.right() +
                        clearIcon.actualSize(clearButton->size()).width() + 10);

    setTextMargins(margins);
}

EditorSearchClearEdit::~EditorSearchClearEdit()
{
}

// -------------------------------------------------------------------------------------

EditorSearchBar::EditorSearchBar(EditorView *parent, EditorViewP *editorViewP)
    : QFrame(parent)
    , d(new EditorSearchBarP())
    , viewD(editorViewP)
{
    // find row
    QGridLayout *layout  = new QGridLayout(this);
    QLabel      *lblFind = new QLabel(this);
    d->foundCountLabel    = new QLabel(this);
    d->searchEdit         = new EditorSearchClearEdit(this, true);
    d->searchButton       = new QPushButton(this);
    d->upButton           = new QPushButton(this);
    d->downButton         = new QPushButton(this);
    d->hideButton         = new QPushButton(this);


    lblFind->setText(tr("Find:"));
    d->searchButton->setText(tr("Search"));
    d->upButton->setIcon(BitmapFactory().iconFromTheme("button_left"));
    d->downButton->setIcon(BitmapFactory().iconFromTheme("button_right"));
    d->searchEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    d->foundCountLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    d->hideButton->setIcon(BitmapFactory().iconFromTheme("delete"));

    // add widgets to bar
    layout->addWidget(lblFind, 0, 0);
    layout->addWidget(d->searchEdit, 0, 1);
    layout->addWidget(d->searchButton, 0, 2);
    layout->addWidget(d->foundCountLabel, 0, 3);
    layout->addWidget(d->upButton, 0, 4);
    layout->addWidget(d->downButton, 0, 5);
    layout->addWidget(d->hideButton, 0, 6);

    // replace row
    QLabel *lblReplace     = new QLabel(this);
    d->replaceEdit          = new EditorSearchClearEdit(this, false);
    d->replaceButton        = new QPushButton(this);
    d->replaceAndNextButton = new QPushButton(this);
    d->replaceAllButton     = new QPushButton(this);
    QHBoxLayout *buttonBox = new QHBoxLayout;

    lblReplace->setText(tr("Replace:"));
    d->replaceEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    layout->addWidget(lblReplace, 1, 0);
    layout->addWidget(d->replaceEdit, 1, 1);

    d->replaceButton->setText(tr("Replace"));
    d->replaceAndNextButton->setText(tr("Replace & find"));
    d->replaceAllButton->setText(tr("Replace All"));
    buttonBox->addWidget(d->replaceButton);
    buttonBox->addWidget(d->replaceAndNextButton);
    buttonBox->addWidget(d->replaceAllButton);
    layout->addLayout(buttonBox, 1, 2, 1, 5);

    setLayout(layout);

    connect(d->hideButton, SIGNAL(clicked()), this, SLOT(hide()));
    connect(d->upButton, SIGNAL(clicked()), this, SLOT(upSearch()));
    connect(d->downButton, SIGNAL(clicked()), this, SLOT(downSearch()));
    connect(d->searchButton, SIGNAL(clicked()), this, SLOT(downSearch()));
    connect(d->searchEdit, SIGNAL(returnPressed()), this, SLOT(downSearch()));
    connect(d->searchEdit, SIGNAL(textChanged(QString)),
                        this, SLOT(searchChanged(QString)));
    connect(d->searchEdit, SIGNAL(showSettings()), this, SLOT(showSettings()));

    connect(d->replaceButton, SIGNAL(clicked()), this, SLOT(replace()));
    connect(d->replaceAndNextButton, SIGNAL(clicked()),
                        this, SLOT(replaceAndFind()));
    connect(d->replaceAllButton, SIGNAL(clicked()),
                        this, SLOT(replaceAll()));

    hide();
}

EditorSearchBar::~EditorSearchBar()
{
}

void EditorSearchBar::show()
{
    QFrame::show();
    d->searchEdit->setFocus();
}

void EditorSearchBar::upSearch(bool cycle)
{
    if (!viewD->wrapper() || !viewD->wrapper()->editor() ||
        !viewD->wrapper()->editor()->document())
        return;

    if (d->searchEdit->text().size()) {
        if (!viewD->wrapper()->editor()->find(
                    d->searchEdit->text(), d->findFlags & QTextDocument::FindBackward)
            && cycle)
        {
            // start over
            QTextCursor cursor = viewD->wrapper()->editor()->textCursor();
            if (!cursor.isNull()) {
                cursor.movePosition(QTextCursor::End);
                viewD->wrapper()->editor()->setTextCursor(cursor);
            }
            return upSearch(false);
        }
    }
    viewD->wrapper()->editor()->repaint();
    d->searchEdit->setFocus();
}

void EditorSearchBar::downSearch(bool cycle)
{
    if (!viewD->wrapper() || !viewD->wrapper()->editor() ||
        !viewD->wrapper()->editor()->document())
        return;

    if (d->searchEdit->text().size()) {
        if (!viewD->wrapper()->editor()->find(d->searchEdit->text(), d->findFlags) && cycle) {
            // start over
            QTextCursor cursor = viewD->wrapper()->editor()->textCursor();
            if (!cursor.isNull()) {
                cursor.movePosition(QTextCursor::Start);
                viewD->wrapper()->editor()->setTextCursor(cursor);
            }
            return downSearch(false);
        }
    }
    viewD->wrapper()->editor()->repaint();
    d->searchEdit->setFocus();
}

void EditorSearchBar::foundCount(int foundOcurrences)
{
    // color red if not found
    if (foundOcurrences <= 0)
        d->searchEdit->setStyleSheet(QLatin1String("QLineEdit {color: red}"));
    else
        d->searchEdit->setStyleSheet(QLatin1String(""));

    QString found = QString::number(foundOcurrences);
    d->foundCountLabel->setText(tr("Found: %1 occurrences").arg(found));
}

void EditorSearchBar::searchChanged(const QString &str)
{
    if (!viewD->wrapper() || !viewD->wrapper()->editor() ||
        !viewD->wrapper()->editor()->document())
        return;

    int found = 0;
    auto edit = viewD->wrapper()->textEditor();
    if (edit) {
        // editor based on TextEditor, able to highlight
        found = edit->findAndHighlight(str, d->findFlags);
        viewD->wrapper()->editor()->repaint();
        foundCount(found);
    } else {
        viewD->wrapper()->editor()->find(str, d->findFlags);
    }
}

void EditorSearchBar::replace()
{
    if (!d->replaceEdit->text().size() || !d->searchEdit->text().size())
        return;

    if (!viewD->wrapper() || !viewD->wrapper()->editor())
        return;

    QTextCursor cursor = viewD->wrapper()->editor()->textCursor();
    if (cursor.hasSelection()) {
        cursor.insertText(d->replaceEdit->text());
        searchChanged(d->searchEdit->text());
    }
}

void EditorSearchBar::replaceAndFind()
{
    if (!d->replaceEdit->text().size() || !d->searchEdit->text().size())
        return;

    if (!viewD->wrapper() || !viewD->wrapper()->editor())
        return;

    replace();
    viewD->wrapper()->editor()->find(d->searchEdit->text(), d->findFlags);
}

void EditorSearchBar::replaceAll()
{
    if (!d->replaceEdit->text().size() || !d->searchEdit->text().size())
        return;

    if (!viewD->wrapper() || !viewD->wrapper()->editor() ||
        !viewD->wrapper()->editor()->document())
        return;

    QTextCursor cursor = viewD->wrapper()->editor()->textCursor();
    int oldPos = cursor.position();

    cursor = viewD->wrapper()->editor()->document()->find(
                d->searchEdit->text(), d->findFlags);

    while (!cursor.isNull()) {
        cursor.insertText(d->replaceEdit->text());
        cursor = viewD->wrapper()->editor()->document()->find(
                    d->searchEdit->text(), cursor, d->findFlags);
    }

    searchChanged(d->searchEdit->text());

    cursor.setPosition(oldPos);
    viewD->wrapper()->editor()->setTextCursor(cursor);
}

void EditorSearchBar::showSettings()
{
    QMenu menu;
    QAction caseSensitive(tr("Case sensitive"), &menu);
    caseSensitive.setCheckable(true);
    caseSensitive.setChecked(d->findFlags & QTextDocument::FindCaseSensitively);
    menu.addAction(&caseSensitive);

    QAction wholeWords(tr("Find whole words"), &menu);
    wholeWords.setCheckable(true);
    wholeWords.setChecked(d->findFlags & QTextDocument::FindWholeWords);
    menu.addAction(&wholeWords);


    QAction *res = menu.exec(QCursor::pos());
    if (res == &caseSensitive) {
        if (res->isChecked())
            d->findFlags |= QTextDocument::FindCaseSensitively;
        else
            d->findFlags &= ~(QTextDocument::FindCaseSensitively);
    } else if (res == &wholeWords) {
        if (res->isChecked())
            d->findFlags |= QTextDocument::FindWholeWords;
        else
            d->findFlags &= ~(QTextDocument::FindWholeWords);
    }
}

#include "moc_EditorView.cpp"
