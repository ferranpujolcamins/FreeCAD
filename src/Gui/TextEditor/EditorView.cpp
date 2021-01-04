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

/// list of plugins for language, file scope on this one so all classes can access it
static QList<AbstractLangPlugin*> langPlugins; // deleted in EditorViewSingletonP

class EditorViewP {
public:
    EditorViewP(EditorView *view, QPlainTextEdit* editor) :
        view(view),
        searchBar(nullptr),
        activityTimer(new QTimer()),
        centralLayout(nullptr),
        editWrapper(nullptr),
        topBar(nullptr)
    {
        if (!EditorViewSingleton::instance()->wrapper(QString(), view))
            editWrapper = EditorViewSingleton::instance()->
                            createWrapper(QString(), editor);
    }
    ~EditorViewP()
    {
        if (activityTimer) {
            activityTimer->stop();
            delete activityTimer;
        }
        EditorViewSingleton::instance()->removeWrapper(editWrapper);
        // delete editWrapper; // owned by EditorWrapperSingelton
    }

    QList<AbstractLangPlugin*> currentPlugins;
    EditorView *view;
    EditorSearchBar* searchBar;
    QTimer*  activityTimer;
    QVBoxLayout *centralLayout;
    EditorViewWrapper* editWrapper; // owned by EditorViewSingelton, avoid circular depedencies
    EditorViewTopBar* topBar;
    EditorView::DisplayName displayName;
};

class PythonEditorViewP {
public:
    PythonEditorViewP(/*PythonEditorView *parent*/)
     //  : topBar(new EditorViewTopBar(parent))
    { }
    ~PythonEditorViewP()
    {
    //    delete topBar;
    }
    //EditorViewTopBar *topBar;
};

/**
 * @brief The EditorViewDocP stores the view specific things for each doc
 *          makes switching doc live easier
 */
class EditorViewWrapperP {
public:
    EditorViewWrapperP(QPlainTextEdit* editor)
        : view(nullptr)
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
    uint timestamp;
    bool lock;
};


/// different types of editors, could have a special editor for xml for example
/// or Py for that matter
struct EditorType {
    EditorType(EditorViewSingleton::createT factory, const QString &name,
               const QStringList &suffixes, const QStringList &mimetypes,
               const QString &icon) :
        factory(factory), name(name), suffixes(suffixes),
        mimetypes(mimetypes), iconName(icon)
    { }
    EditorViewSingleton::createT factory;
    const QString name;
    const QStringList suffixes,
                      mimetypes;
    const QString iconName;
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

    QList<EditorViewWrapper*> wrappers;
    QList<QString> accessOrder;

    EditorViewSingletonP() { }
    ~EditorViewSingletonP()
    {
        qDeleteAll(editorTypes);
        qDeleteAll(wrappers);
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

// ------------------------------------------------------------

// ** register default editors, other editor types should register like this
// in there own cpp files
static struct _PyEditorRegister
{
    static QPlainTextEdit* createPythonEditor() { return new PythonEditor(); }
    _PyEditorRegister() {
        QStringList suffix;
        suffix <<  QLatin1String("py") << QLatin1String("fcmacro");
        QStringList mime;
        mime << QLatin1String("text/x-script.python") <<
                QLatin1String("text/x-script.phyton") <<
                QLatin1String("text/x-python");

        QTimer::singleShot(0, [=](){
            EditorViewSingleton::registerTextEditorType(
                 reinterpret_cast<EditorViewSingleton::createT>(&createPythonEditor),
                             QLatin1String("PythonEditor"),
                             suffix,
                             mime,
                             QLatin1String("applications-python")
            );
        });
    }
}_pyEditorRegisterSingleton;

}; // namespace Gui

// -------------------------------------------------------

/* TRANSLATOR Gui::EditorView */

/**
 *  Constructs a EditorView which is a child of 'parent', with the
 *  name 'name'.
 */
EditorView::EditorView(QPlainTextEdit *editor, QWidget* parent)
    : MDIView(nullptr, parent, nullptr), WindowParameter( "Editor" )
{
    // we must have a editor
    if (!editor)
        editor = new TextEditor(this);

    // create d pointer obj and init viewData obj (switches when switching file)
    d = new EditorViewP(this, editor);
    d->displayName = EditorView::FullName;


    // create searchbar
    d->searchBar = new EditorSearchBar(this, d);

    // Create the layout containing the workspace and a tab bar
    d->centralLayout = new QVBoxLayout;
    QFrame*     vbox = new QFrame(this);
    d->centralLayout->setMargin(0);

    d->editWrapper->attach(this);

    d->centralLayout->addWidget(editor);
    editor->setParent(this);
    d->centralLayout->addWidget(d->searchBar);
    vbox->setLayout(d->centralLayout);

    editor->setParent(vbox);
    editor->document()->setModified(false);
    setCurrentFileName(QString());
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
    return d->editWrapper->editor();
}

TextEditor *EditorView::textEditor() const
{
    return dynamic_cast<TextEditor*>(d->editWrapper->editor());
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
    QFileInfo fi(d->editWrapper->filename());
    uint timeStamp =  fi.lastModified().toTime_t();
    if (fi.exists() && timeStamp != d->editWrapper->timestamp()) {
        switch( QMessageBox::question( this, tr("Modified file"), 
                tr("%1.\n\nThis has been modified outside of the source editor. Do you want to reload it?")
                    .arg(d->editWrapper->filename()),
                QMessageBox::Yes|QMessageBox::Default, QMessageBox::No|QMessageBox::Escape) )
        {
            case QMessageBox::Yes:
                // updates time stamp and timer
                open(d->editWrapper->filename());
                return;
            case QMessageBox::No:
                d->editWrapper->setTimestamp(timeStamp);
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
    if (strcmp(pMsg,"Run")==0)  return true;
    if (strcmp(pMsg,"DebugStart")==0)  return true;
    if (strcmp(pMsg,"DebugStop")==0)  return true;
    if (strcmp(pMsg,"SaveAs")==0)  return true;
    if (strcmp(pMsg,"Print")==0) return true;
    if (strcmp(pMsg,"PrintPreview")==0) return true;
    if (strcmp(pMsg,"PrintPdf")==0) return true;
    if (strcmp(pMsg,"Save")==0) { 
        return d->editWrapper->editor()->document()->isModified();
    } else if (strcmp(pMsg,"Cut")==0) {
        bool canWrite = !d->editWrapper->editor()->isReadOnly();
        return (canWrite && (d->editWrapper->editor()->textCursor().hasSelection()));
    } else if (strcmp(pMsg,"Copy")==0) {
        if (d->editWrapper->editor() &&
            d->editWrapper->editor()->document())
            return d->editWrapper->editor()->textCursor().hasSelection();
        return false;
    } else if (strcmp(pMsg,"Paste")==0) {
        QClipboard *cb = QApplication::clipboard();
        QString text;

        // Copy text from the clipboard (paste)
        text = cb->text();

        bool canWrite = !d->editWrapper->editor()->isReadOnly();
        return ( !text.isEmpty() && canWrite );
    } else if (strcmp(pMsg,"Undo")==0) {
        return d->editWrapper->editor()->document()->isUndoAvailable ();
    } else if (strcmp(pMsg,"Redo")==0) {
        return d->editWrapper->editor()->document()->isRedoAvailable ();
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
    if (!d->editWrapper->editor()->document()->isModified())
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
    if (canClose()) {
        // let plugins know
        for (auto plug : currentPlugins())
            plug->onFileClose(this, d->editWrapper->filename());

        auto oldWrapper = d->editWrapper;
        auto newWrapper = EditorViewSingleton::instance()->lastAccessedEditor(this, -1);

        bool created = false;
        if (!newWrapper || oldWrapper == newWrapper) {
            newWrapper = EditorViewSingleton::instance()->createWrapper(QString());
            created = true;
        }

        auto old = swapEditor(newWrapper->editor());
        if (!old) {
            if (created) {
                newWrapper->close(this);
                delete newWrapper;
            }
            return false;
        }

        if (oldWrapper)
            Q_EMIT closedFile(oldWrapper->filename());
        if (newWrapper)
            Q_EMIT switchedFile(newWrapper->filename());

        d->editWrapper = newWrapper;
        newWrapper->attach(this);
        oldWrapper->close(this);
        delete oldWrapper;
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
    if (fn.isEmpty()) {
        auto ew = d->editWrapper;
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
    }
    if (fn.isEmpty())
        return false;

    if (!d->editWrapper)
        d->editWrapper->editor()->document()->setModified(false);
    setCurrentFileName(fn);
    return saveFile();
}

/**
 * Opens the file \a fileName.
 */
bool EditorView::open(const QString& fileName)
{
    EditorViewWrapper* oldWrapper = d->editWrapper,
                     * thisWrapper = nullptr,
                     * otherViewWrapper = nullptr,
                     * newWrapper = nullptr;

   auto editWrappers = EditorViewSingleton::instance()->wrappers(fileName);
    for (auto wrap : editWrappers) {
        if (wrap->view() == this) {
            thisWrapper = wrap;
            otherViewWrapper = wrap;
            break;
        }
        otherViewWrapper = wrap;
    }

    if (!thisWrapper && otherViewWrapper) {
        // file opened, but is it isn't is in this view/tab
        auto isPyEdit = dynamic_cast<PythonEditor*>(otherViewWrapper->editor()) != nullptr;
        auto isTxtEdit = otherViewWrapper->textEditor() != nullptr;
        QPlainTextEdit* edit;
        if (isPyEdit)
            edit = new PythonEditor(this);
        else if (isTxtEdit)
            edit = new TextEditor(this);
        else
            edit = new QPlainTextEdit(this);

        if (isTxtEdit) { // true for all but QPlainTextEditor
            auto ed = dynamic_cast<TextEditor*>(edit);
            if (ed) {
                ed->setFilename(fileName);
                //auto sh = newWrapper->textEditor()->syntaxHighlighter();
                //ed->setSyntaxHighlighter(sh);
            }
        }

        newWrapper = EditorViewSingleton::instance()->createWrapper(fileName, edit);
        if (!newWrapper)
            return false;
        if (otherViewWrapper->editor() && newWrapper->editor()) {
            newWrapper->setMirrorDoc(otherViewWrapper);
            otherViewWrapper->setMirrorDoc(newWrapper);
        }

    } else if (!newWrapper) { // not yet opened
        newWrapper = EditorViewSingleton::instance()->createWrapper(fileName);
        if (!newWrapper)
            return false;
    } else if (oldWrapper == newWrapper)
        return true;

    // opened in this View/tab
    // swap currently viewed editor
    QPlainTextEdit *old = swapEditor(newWrapper->editor());
    if (!old) {
        newWrapper->close(this); // memory collect
        delete newWrapper;
        return false;
    }

    // swap out
    d->editWrapper = newWrapper;
    newWrapper->attach(this);
    if (oldWrapper->filename().isEmpty() &&
        !oldWrapper->editor()->document()->isModified())
    {   // close the autogenerated editor from construction if its empty
        oldWrapper->close(this);
        delete oldWrapper;
    } else {
        oldWrapper->detach(this);
    }

    QFileInfo fi(fileName);
    newWrapper->setTimestamp(fi.lastModified().toTime_t());

    // untitled files should be writable
    bool writable = (!fi.isWritable() && fileName.isEmpty()) ||
            !fi.isWritable();
    newWrapper->editor()->setReadOnly(writable);

    d->activityTimer->setSingleShot(true);
    d->activityTimer->start(3000);

    Q_EMIT switchedFile(fi.absoluteFilePath());
    setCurrentFileName(fi.absoluteFilePath());

    // let plugins do whitespace trim etc
    for (auto plug : currentPlugins())
        plug->onFileOpen(this, d->editWrapper->filename());

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
    d->editWrapper->editor()->cut();
}

/**
 * Copies any selected text to the clipboard.
 */
void EditorView::copy(void)
{
    d->editWrapper->editor()->copy();
}

/**
 * Pastes the text from the clipboard into the text edit at the current cursor position. 
 * If there is no text in the clipboard nothing happens.
 */
void EditorView::paste(void)
{
    d->editWrapper->editor()->paste();
}

/**
 * Undoes the last operation.
 * If there is no operation to undo, i.e. there is no undo step in the undo/redo history, nothing happens.
 */
void EditorView::undo(void)
{
    d->editWrapper->setLocked(true);
    if (!d->editWrapper->undos().isEmpty()) {
        d->editWrapper->redos() << d->editWrapper->undos().back();
        d->editWrapper->undos().pop_back();
    }
    d->editWrapper->editor()->document()->undo();
    d->editWrapper->setLocked(false);
}

/**
 * Redoes the last operation.
 * If there is no operation to undo, i.e. there is no undo step in the undo/redo history, nothing happens.
 */
void EditorView::redo(void)
{
    d->editWrapper->setLocked(true);
    if (!d->editWrapper->redos().isEmpty()) {
        d->editWrapper->undos() << d->editWrapper->redos().back();
        d->editWrapper->redos().pop_back();
    }
    d->editWrapper->editor()->document()->redo();
    d->editWrapper->setLocked(false);
}

void EditorView::newFile()
{
    auto ews = Gui::EditorViewSingleton::instance();
    auto editor = ews->createEditor(QString());
    swapEditor(editor);
}

/**
 * Shows the printer dialog.
 */
void EditorView::print()
{
    QPrinter printer(QPrinter::ScreenResolution);
    printer.setFullPage(true);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() == QDialog::Accepted)
        d->editWrapper->editor()->document()->print(&printer);
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
    d->editWrapper->editor()->document()->print(printer);
}

void EditorView::refreshLangPlugins()
{
    if (!d->editWrapper)
        return;

    auto editor = dynamic_cast<TextEditor*>(d->editWrapper->editor());
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
    QString fileName = FileDialog::getSaveFileName(this, tr("Export PDF"), QString(),
        QString::fromLatin1("%1 (*.pdf)").arg(tr("PDF file")));
    if (!fileName.isEmpty()) {
        QPrinter printer(QPrinter::ScreenResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        d->editWrapper->editor()->document()->print(&printer);
    }
}

void EditorView::setCurrentFileName(const QString &fileName)
{
    if (d->editWrapper->filename() != fileName)
        Q_EMIT changeFileName(fileName);

    d->editWrapper->setFilename(fileName);

    QString name;
    QFileInfo fi(fileName);
    switch (d->displayName) {
    case FullName:
        name = fileName;
        break;
    case FileName:
        name = fi.fileName();
        break;
    case BaseName:
        name = fi.baseName();
        break;
    }

    QString shownName;
    if (fileName.isEmpty())
        shownName = tr("untitled[*]");
    else
        shownName = QString::fromLatin1("%1[*]").arg(name);

    if (d->editWrapper->editor()->isReadOnly())
        shownName += tr(" - Read-only");
    else
        shownName += tr(" - Editor");

    setWindowTitle(shownName);
    if (!d->editWrapper->editor()->document())
        d->editWrapper->editor()->setDocument(new QTextDocument(d->editWrapper->editor()));
    setWindowModified(d->editWrapper->editor()->document()->isModified());

    refreshLangPlugins();
}

QString EditorView::filename() const
{
    return d->editWrapper->filename();
}

void EditorView::setFilename(QString fileName)
{
    d->editWrapper->setFilename(fileName);
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
    if (d->editWrapper->filename().isEmpty())
        return saveAs();

    // let plugins do whitespace trim etc
    for (auto plug : currentPlugins())
        plug->onFileSave(this, d->editWrapper->filename());

    QFile file(d->editWrapper->filename());
    if (!file.open(QFile::WriteOnly))
        return false;

    auto editor = d->editWrapper->editor();

    QTextStream ts(&file);
    ts.setCodec(QTextCodec::codecForName("UTF-8"));
    ts << editor->document()->toPlainText();
    file.close();

    editor->document()->setModified(false);
    auto textEditor = dynamic_cast<TextEditor*>(editor);
    if (textEditor)
        textEditor->onSave();

    QFileInfo fi(d->editWrapper->filename());
    for (auto wrap : EditorViewSingleton::instance()->wrappers(
                                            d->editWrapper->filename()))
    {
        wrap->setTimestamp(fi.lastModified().toTime_t());
    }
#ifdef BUILD_PYTHON_DEBUGTOOLS
        {
            //DumpSyntaxTokens tok(editor->document()->begin());
            Python::DumpModule dMod(Python::SourceRoot::instance()->moduleFromPath(
                                        d->editWrapper->filename().toStdString()));
        }
#endif
    return true;
}

QPlainTextEdit *EditorView::swapEditor(QPlainTextEdit* newEditor)
{
    // find and replace texteditor widget
    for (int i =0; i < d->centralLayout->count(); ++i) {
        // first check if its the text editor
        QWidget *itm = qobject_cast<QPlainTextEdit*>(
                            d->centralLayout->itemAt(i)->widget());
        if (!itm)
            continue;

        itm = d->centralLayout->takeAt(i)->widget();
        if (!itm)
            return nullptr;
        else
            itm->hide();

        // remove, and let EditorViewSingleton handle memory cleanup
        d->centralLayout->insertWidget(i, newEditor);
        newEditor->setParent(static_cast<QWidget*>(d->centralLayout->parent()));
        newEditor->setFocus();
        setWindowIcon(newEditor->windowIcon());
        newEditor->show();

        return qobject_cast<QPlainTextEdit*>(itm);
    }

    return nullptr;
}

void EditorView::undoAvailable(bool undo)
{
    Q_UNUSED(undo)
    d->editWrapper->undos().clear();
}

void EditorView::redoAvailable(bool redo)
{
    if (!redo)
        d->editWrapper->redos().clear();
}

void EditorView::contentsChange(int position, int charsRemoved, int charsAdded)
{
    Q_UNUSED(position)

    if (d->editWrapper->isLocked())
        return;
    if (charsRemoved > 0 && charsAdded > 0)
        return; // syntax highlighting
    else if (charsRemoved > 0)
        d->editWrapper->undos() << tr("%1 chars removed").arg(charsRemoved);
    else if (charsAdded > 0)
        d->editWrapper->undos() << tr("%1 chars added").arg(charsAdded);
    else
        d->editWrapper->undos() << tr("Formatted");
    d->editWrapper->redos().clear();
}

/**
 * Get the undo history.
 */
QStringList EditorView::undoActions() const
{
    return d->editWrapper->undos();
}

/**
 * Get the redo history.
 */
QStringList EditorView::redoActions() const
{
    return d->editWrapper->redos();
}

void EditorView::focusInEvent (QFocusEvent *)
{
    d->editWrapper->editor()->setFocus();
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
    return d->editWrapper;
}

// -------------------------------------------------------------------------------

EditorViewWrapper::EditorViewWrapper(QPlainTextEdit *editor, const QString &fn)
    : QObject()
    , d(new EditorViewWrapperP(editor))
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
    detach(d->view);
    d->view = nullptr;

    EditorViewSingleton::instance()->removeWrapper(this);
    delete d;
}

void EditorViewWrapper::attach(EditorView* sharedOwner)
{
    // handles automatic clenup
    d->view = sharedOwner;

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
    }

    sharedOwner->setWindowModified(d->plainEdit->document()->isModified());
}

void EditorViewWrapper::detach(EditorView* sharedOwner)
{
    // handles automatic clenup
    d->view = sharedOwner;

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

bool EditorViewWrapper::close(EditorView* sharedOwner)
{
    detach(sharedOwner);

    d->plainEdit->setParent(nullptr);

    // cleanup and memory release
    EditorViewSingleton::instance()->removeWrapper(this);
    if (d->plainEdit)
        d->plainEdit->setDocument(nullptr);

    // emit changes
    Q_EMIT EditorViewSingleton::instance()->openFilesChanged();
    Q_EMIT EditorViewSingleton::instance()->fileClosed(d->filename);

    return true;
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

uint EditorViewWrapper::timestamp() const
{
    return d->timestamp;
}

void EditorViewWrapper::setTimestamp(uint timestamp)
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

EditorViewSingleton::EditorViewSingleton() :
    d(new EditorViewSingletonP())
{
    _PyEditorRegister();

    // do these after eventloop is running
    AbstractLangPlugin::registerBuildInPlugins();
}

EditorViewSingleton::~EditorViewSingleton()
{
    delete d;
}

/* static */
bool EditorViewSingleton::registerTextEditorType(EditorViewSingleton::createT factory,
                                                 const QString &name,
                                                 const QStringList &suffixes,
                                                 const QStringList &mimetypes,
                                                 const QString &icon)
{
    EditorViewSingleton *me = EditorViewSingleton::instance();
    for (auto et : me->d->editorTypes) {
        if (et->name == name)
            return false; // already registered
    }

    // new editor type, register
    auto editType = new EditorType(factory, name, suffixes, mimetypes, icon);
    me->d->editorTypes.append(editType);
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

    for(const auto &wrp : instance()->d->wrappers) {
        if (wrp && wrp->editor() == wrp->view()->editor()) {
            if (plugin->matchesMimeType(wrp->filename(), wrp->mimetype()))
                wrp->view()->refreshLangPlugins();
        }
    }
}

/* static */
EditorViewSingleton* EditorViewSingleton::instance()
{
    static EditorViewSingleton* _instance = new EditorViewSingleton();
    return _instance;
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

EditorViewWrapper*
EditorViewSingleton::wrapper(const QString &fn, EditorView* ownerView) const
{
    for (auto editWrapper : d->wrappers) {
        if ((editWrapper->filename() == fn) &&
            (editWrapper->view() == ownerView))
        {
            // move to top of access stack
            d->accessOrder.removeAll(fn);
            d->accessOrder.append(fn);

            return editWrapper;
        }
    }

    return nullptr;
}

QList<EditorViewWrapper*>
EditorViewSingleton::wrappers(const QString &fn) const
{
    QList<EditorViewWrapper*> wrappers;
    for (auto editWrapper : d->wrappers) {
        if (editWrapper->filename() == fn || fn.isEmpty())
            wrappers.append(editWrapper);
    }

    return wrappers;
}

QList<EditorViewWrapper *> EditorViewSingleton::wrappersForView(const EditorView *view) const
{
    QList<EditorViewWrapper*> wrappers;
    for (auto editWrapper : d->wrappers) {
        if (editWrapper->view() == view)
            wrappers.append(editWrapper);
    }

    return wrappers;
}

EditorViewWrapper*
EditorViewSingleton::createWrapper(const QString &fn, QPlainTextEdit* editor) const
{
    EditorViewWrapper* ew = nullptr;
    QString icon(QLatin1String("accessories-text-editor"));

    if (editor) {
        ew = new EditorViewWrapper(editor, fn);
    } else {
        auto et = editorTypeForFile(fn);
        if (et) {
            // create on a special editor
            ew = new EditorViewWrapper(et->factory(), fn);
            icon = et->iconName;
        } else {
            // default to TextEditor
            ew = new EditorViewWrapper(new TextEditor(), fn);
        }
    }

    d->accessOrder.append(fn);

    // set icon for this type
    ew->editor()->setWindowIcon(Gui::BitmapFactory().iconFromTheme(icon.toLatin1()));
    auto edit = ew->textEditor();
    if (edit)
        edit->setFilename(fn);

    d->wrappers.append(ew);

    connect(ew->editor(), &QPlainTextEdit::modificationChanged,
            this, &EditorViewSingleton::docModifiedChanged);
    Q_EMIT openFilesChanged();
    Q_EMIT fileOpened(fn);
    return ew;
}

EditorView *EditorViewSingleton::createView(QPlainTextEdit *edit, bool addToMainWindow) const
{
    auto mainWindow = MainWindow::getInstance();
    auto editorView = new EditorView(edit, mainWindow);
    editorView->setDisplayName(EditorView::FileName);
    if (addToMainWindow)
        MainWindow::getInstance()->addWindow(editorView);
    return editorView;
}

QPlainTextEdit *EditorViewSingleton::createEditor(const QString &fn) const
{
    auto et = editorTypeForFile(fn);
    if (et)
        return et->factory();
    return new TextEditor();
}

bool EditorViewSingleton::removeWrapper(EditorViewWrapper* ew)
{
    disconnect(ew->editor(), &QPlainTextEdit::modificationChanged,
               this, &EditorViewSingleton::docModifiedChanged);
    bool rm = false;
    for(int i = 0; i < d->accessOrder.size(); ++i) {
        if (d->accessOrder[i] == ew->filename())
            rm = true;
        // remove all after first found
        if (rm) {
            d->accessOrder.removeAt(i);
            --i;
        }
    }

    auto beforeSize = d->wrappers.size();
    for (auto &wrp : d->wrappers)
        if (wrp == ew)
            d->wrappers.removeOne(wrp);

    return beforeSize != d->wrappers.size();
}

EditorViewWrapper*
EditorViewSingleton::lastAccessedEditor(EditorView* view, int backSteps) const
{
    int idx = d->accessOrder.size() + backSteps -1;
    if (idx >= 0 && d->accessOrder.size() > idx)
        return wrapper(d->accessOrder.at(idx), view);
    return nullptr;
}

QList<const EditorViewWrapper*>
EditorViewSingleton::openedBySuffix(QStringList types) const
{
    QList<const EditorViewWrapper*> res;
    for (auto editWrapper : d->wrappers) {
        QFileInfo fi(editWrapper->filename());
        if (types.size() == 0 || types.contains(fi.suffix(), Qt::CaseInsensitive))
            res.append(editWrapper);
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
    for (auto ew : d->wrappers) {
        if (ew->editor() == obj)
            Q_EMIT modifiedChanged(ew->filename(), changed);
    }
}

const EditorType *EditorViewSingleton::editorTypeForFile(const QString &fn) const
{
    QString mime = QLatin1String("text/plain"); // default ;
    QFileInfo fi(fn);
    QString suffix = fi.suffix().toLower();
    if (suffix == QLatin1String("fcmacro")) {
        mime = QLatin1String("text/x-script.python");
    } else {
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(fn);
        if (type.isValid())
            mime = type.name();
    }

    // first find via file suffix, ie .py
    for (const auto et : d->editorTypes)
        if (et->suffixes.contains(fi.suffix(), Qt::CaseInsensitive))
            return et;

    // find the registered editor for this mimetype
    for (auto et : d->editorTypes)
        if (et->mimetypes.contains(mime, Qt::CaseInsensitive))
            return et;

    return nullptr;
}

// -------------------------------------------------------------------------------------

EditorViewTopBar::EditorViewTopBar(EditorView *parent):
    QWidget(parent)
{
    d = new EditorViewTopBarP;

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

EditorSearchBar::EditorSearchBar(EditorView *parent, EditorViewP *editorViewP) :
    QFrame(parent),
    d(editorViewP),
    m_findFlags(nullptr)
{
    // find row
    QGridLayout *layout  = new QGridLayout(this);
    QLabel      *lblFind = new QLabel(this);
    m_foundCountLabel    = new QLabel(this);
    m_searchEdit         = new EditorSearchClearEdit(this, true);
    m_searchButton       = new QPushButton(this);
    m_upButton           = new QPushButton(this);
    m_downButton         = new QPushButton(this);
    m_hideButton         = new QPushButton(this);


    lblFind->setText(tr("Find:"));
    m_searchButton->setText(tr("Search"));
    m_upButton->setIcon(BitmapFactory().iconFromTheme("button_left"));
    m_downButton->setIcon(BitmapFactory().iconFromTheme("button_right"));
    m_searchEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_foundCountLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_hideButton->setIcon(BitmapFactory().iconFromTheme("delete"));

    // add widgets to bar
    layout->addWidget(lblFind, 0, 0);
    layout->addWidget(m_searchEdit, 0, 1);
    layout->addWidget(m_searchButton, 0, 2);
    layout->addWidget(m_foundCountLabel, 0, 3);
    layout->addWidget(m_upButton, 0, 4);
    layout->addWidget(m_downButton, 0, 5);
    layout->addWidget(m_hideButton, 0, 6);

    // replace row
    QLabel *lblReplace     = new QLabel(this);
    m_replaceEdit          = new EditorSearchClearEdit(this, false);
    m_replaceButton        = new QPushButton(this);
    m_replaceAndNextButton = new QPushButton(this);
    m_replaceAllButton     = new QPushButton(this);
    QHBoxLayout *buttonBox = new QHBoxLayout;

    lblReplace->setText(tr("Replace:"));
    m_replaceEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    layout->addWidget(lblReplace, 1, 0);
    layout->addWidget(m_replaceEdit, 1, 1);

    m_replaceButton->setText(tr("Replace"));
    m_replaceAndNextButton->setText(tr("Replace & find"));
    m_replaceAllButton->setText(tr("Replace All"));
    buttonBox->addWidget(m_replaceButton);
    buttonBox->addWidget(m_replaceAndNextButton);
    buttonBox->addWidget(m_replaceAllButton);
    layout->addLayout(buttonBox, 1, 2, 1, 5);

    setLayout(layout);

    connect(m_hideButton, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_upButton, SIGNAL(clicked()), this, SLOT(upSearch()));
    connect(m_downButton, SIGNAL(clicked()), this, SLOT(downSearch()));
    connect(m_searchButton, SIGNAL(clicked()), this, SLOT(downSearch()));
    connect(m_searchEdit, SIGNAL(returnPressed()), this, SLOT(downSearch()));
    connect(m_searchEdit, SIGNAL(textChanged(QString)),
                        this, SLOT(searchChanged(QString)));
    connect(m_searchEdit, SIGNAL(showSettings()), this, SLOT(showSettings()));

    connect(m_replaceButton, SIGNAL(clicked()), this, SLOT(replace()));
    connect(m_replaceAndNextButton, SIGNAL(clicked()),
                        this, SLOT(replaceAndFind()));
    connect(m_replaceAllButton, SIGNAL(clicked()),
                        this, SLOT(replaceAll()));


    hide();
}

EditorSearchBar::~EditorSearchBar()
{
}

void EditorSearchBar::show()
{
    QFrame::show();
    m_searchEdit->setFocus();
}

void EditorSearchBar::upSearch(bool cycle)
{
    if (m_searchEdit->text().size()) {
        if (!d->editWrapper->editor()->find(
                    m_searchEdit->text(), m_findFlags & QTextDocument::FindBackward)
            && cycle)
        {
            // start over
            QTextCursor cursor = d->editWrapper->editor()->textCursor();
            if (!cursor.isNull()) {
                cursor.movePosition(QTextCursor::End);
                d->editWrapper->editor()->setTextCursor(cursor);
            }
            return upSearch(false);
        }
    }
    d->editWrapper->editor()->repaint();
    m_searchEdit->setFocus();
}

void EditorSearchBar::downSearch(bool cycle)
{
    if (m_searchEdit->text().size()) {
        if (!d->editWrapper->editor()->find(m_searchEdit->text(), m_findFlags) && cycle) {
            // start over
            QTextCursor cursor = d->editWrapper->editor()->textCursor();
            if (!cursor.isNull()) {
                cursor.movePosition(QTextCursor::Start);
                d->editWrapper->editor()->setTextCursor(cursor);
            }
            return downSearch(false);
        }
    }
    d->editWrapper->editor()->repaint();
    m_searchEdit->setFocus();
}

void EditorSearchBar::foundCount(int foundOcurrences)
{
    // color red if not found
    if (foundOcurrences <= 0)
        m_searchEdit->setStyleSheet(QLatin1String("QLineEdit {color: red}"));
    else
        m_searchEdit->setStyleSheet(QLatin1String(""));

    QString found = QString::number(foundOcurrences);
    m_foundCountLabel->setText(tr("Found: %1 occurrences").arg(found));
}

void EditorSearchBar::searchChanged(const QString &str)
{
    int found = 0;
    auto edit = d->editWrapper->textEditor();
    if (edit) {
        // editor based on TextEditor, able to highlight
        found = edit->findAndHighlight(str, m_findFlags);
        d->editWrapper->editor()->repaint();
        foundCount(found);
    } else {
        d->editWrapper->editor()->find(str, m_findFlags);
    }
}

void EditorSearchBar::replace()
{
    if (!m_replaceEdit->text().size() || !m_searchEdit->text().size())
        return;

    QTextCursor cursor = d->editWrapper->editor()->textCursor();
    if (cursor.hasSelection()) {
        cursor.insertText(m_replaceEdit->text());
        searchChanged(m_searchEdit->text());
    }
}

void EditorSearchBar::replaceAndFind()
{
    if (!m_replaceEdit->text().size() || !m_searchEdit->text().size())
        return;

    replace();
    d->editWrapper->editor()->find(m_searchEdit->text(), m_findFlags);
}

void EditorSearchBar::replaceAll()
{
    if (!m_replaceEdit->text().size() || !m_searchEdit->text().size())
        return;

    QTextCursor cursor = d->editWrapper->editor()->textCursor();
    int oldPos = cursor.position();

    cursor = d->editWrapper->editor()->document()->find(
                m_searchEdit->text(), m_findFlags);

    while (!cursor.isNull()) {
        cursor.insertText(m_replaceEdit->text());
        cursor = d->editWrapper->editor()->document()->find(
                    m_searchEdit->text(), cursor, m_findFlags);
    }

    searchChanged(m_searchEdit->text());

    cursor.setPosition(oldPos);
    d->editWrapper->editor()->setTextCursor(cursor);
}

void EditorSearchBar::showSettings()
{
    QMenu menu;
    QAction caseSensitive(tr("Case sensitive"), &menu);
    caseSensitive.setCheckable(true);
    caseSensitive.setChecked(m_findFlags & QTextDocument::FindCaseSensitively);
    menu.addAction(&caseSensitive);

    QAction wholeWords(tr("Find whole words"), &menu);
    wholeWords.setCheckable(true);
    wholeWords.setChecked(m_findFlags & QTextDocument::FindWholeWords);
    menu.addAction(&wholeWords);


    QAction *res = menu.exec(QCursor::pos());
    if (res == &caseSensitive) {
        if (res->isChecked())
            m_findFlags |= QTextDocument::FindCaseSensitively;
        else
            m_findFlags &= ~(QTextDocument::FindCaseSensitively);
    } else if (res == &wholeWords) {
        if (res->isChecked())
            m_findFlags |= QTextDocument::FindWholeWords;
        else
            m_findFlags &= ~(QTextDocument::FindWholeWords);
    }
}

#include "moc_EditorView.cpp"
