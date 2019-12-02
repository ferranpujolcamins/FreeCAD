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

#ifdef BUILD_PYTHON_DEBUGTOOLS
# include <PythonSource/PythonSourceDebugTools.h>
# include <PythonSource/PythonSourceRoot.h>
#endif


#include <Base/Interpreter.h>
#include <Base/Parameter.h>
#include <Base/Exception.h>
#include <App/PythonDebugger.h>

using namespace Gui;
namespace Gui {
class EditorViewP {
public:
    EditorViewP(EditorView *view, TextEditor* editor) :
        view(view),
        searchBar(nullptr), activityTimer(new QTimer(view)),
        centralLayout(nullptr),
        editWrapper(EditorViewSingleton::instance()->createWrapper(QString(), editor)),
        topBar(nullptr)
    {
        editWrapper->attach(view);
    }
    ~EditorViewP()
    {
        if (activityTimer) {
            activityTimer->stop();
            delete activityTimer;
        }

        editWrapper->close(view);
        delete editWrapper;
    }
    EditorView *view;
    EditorSearchBar* searchBar;
    EditorView::DisplayName displayName;
    QTimer*  activityTimer;
    QVBoxLayout *centralLayout;
    EditorViewWrapper *editWrapper;
    EditorViewTopBar *topBar;
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
 *          makes sitching doc live easier
 */
class EditorViewWrapperP {
public:
    EditorViewWrapperP() :
        timestamp(0), lock(false),
        textEdit(nullptr)
    {}
    ~EditorViewWrapperP()
    {
        textEdit = nullptr;
    }
    EditorView *owner;
    QString fileName;
    uint timestamp;
    bool lock;
    QStringList undos;
    QStringList redos;
    TextEditor* textEdit;
};


/**
 * @brief The EditorViewSingletonP a singleton which owns all
 * opened editors so that they can be opened in different views.
 * For example split views, different tabs, etc
 *  saves memory
 */

class EditorViewSingletonP {
public:
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
    QList<EditorType* > editorTypes;

    QList<EditorViewWrapper*> wrappers;
    QList<QString> accessOrder;

    EditorViewSingletonP() { }
    ~EditorViewSingletonP()
    {
        qDeleteAll(wrappers);
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
};

// ------------------------------------------------------------

// ** register default editors, other editor types should register like this
// in there own cpp files
struct _PyEditorRegister
{
    static PythonEditor *createPythonEditor() { return new PythonEditor; }
    _PyEditorRegister() {
        QStringList suffix;
        suffix <<  QLatin1String("py");
        QStringList mime;
        mime << QLatin1String("text/x-script.python") <<
                QLatin1String("text/x-script.phyton") <<
                QLatin1String("text/x-python");

        EditorViewSingleton::registerTextEditorType(
                 reinterpret_cast<EditorViewSingleton::createT>(&createPythonEditor),
                             QLatin1String("PythonEditor"),
                             suffix,
                             mime,
                             QLatin1String("applications-python"));
    }
} _pyEditorRegisterSingleton;

// -------------------------------------------------------

/* TRANSLATOR Gui::EditorView */

/**
 *  Constructs a EditorView which is a child of 'parent', with the
 *  name 'name'.
 */
EditorView::EditorView(TextEditor* editor, QWidget* parent)
    : MDIView(0,parent,0), WindowParameter( "Editor" )
{
    // create d pointer obj and init viewData obj (switches when switching file)
    d = new EditorViewP(this, editor);
    d->displayName = EditorView::FullName;

    // create searchbar
    d->searchBar = new EditorSearchBar(this, d);

    // Create the layout containing the workspace and a tab bar
    d->centralLayout = new QVBoxLayout;
    QFrame*     vbox = new QFrame(this);
    d->centralLayout->setMargin(0);
    d->centralLayout->addWidget(editor);
    editor->setParent(this);
    d->centralLayout->addWidget(d->searchBar);
    vbox->setLayout(d->centralLayout);

    QFrame* hbox = new QFrame(this);
    hbox->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->setMargin(1);
    hLayout->addWidget(vbox);
    hbox->setLayout(hLayout);
    setCentralWidget(hbox);

    editor->setParent(vbox);
    editor->document()->setModified(false);
    setCurrentFileName(QString());
    editor->setFocus();

    setWindowIcon(editor->windowIcon());

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
    delete d;
    getWindowParameter()->Detach( this );
}

QPlainTextEdit* EditorView::getEditor() const
{
    return d->editWrapper->editor();
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
    QFileInfo fi(d->editWrapper->fileName());
    uint timeStamp =  fi.lastModified().toTime_t();
    if (fi.exists() && timeStamp != d->editWrapper->timestamp()) {
        switch( QMessageBox::question( this, tr("Modified file"), 
                tr("%1.\n\nThis has been modified outside of the source editor. Do you want to reload it?")
                    .arg(d->editWrapper->fileName()),
                QMessageBox::Yes|QMessageBox::Default, QMessageBox::No|QMessageBox::Escape) )
        {
            case QMessageBox::Yes:
                // updates time stamp and timer
                open( d->editWrapper->fileName() );
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
bool EditorView::onMsg(const char* pMsg,const char** /*ppReturn*/)
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
        return !d->searchBar->isHidden();;
    }

    return false;
}

/** Checking on close state. */
bool EditorView::canClose(void)
{
    if ( !d->editWrapper->editor()->document()->isModified() )
        return true;
    this->setFocus(); // raises the view to front
    switch( QMessageBox::question(this, tr("Unsaved document"), 
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
        EditorViewWrapper *oldWrapper = d->editWrapper;
        EditorViewWrapper *newWrapper = EditorViewSingleton::instance()->lastAccessed(this, -1);

        bool created = false;
        if (!newWrapper || oldWrapper == newWrapper) {
            newWrapper = EditorViewSingleton::instance()->createWrapper(QString());
            created = true;
        }

        QPlainTextEdit *old = swapEditor(newWrapper->editor());
        if (!old) {
            if (created) {
                newWrapper->close(this);
                delete newWrapper;
            }
            return false;
        }

        Q_EMIT closedFile(oldWrapper->fileName());
        Q_EMIT switchedFile(newWrapper->fileName());

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
        fn = FileDialog::getSaveFileName(this, QObject::tr("Save file"),
                                    QString::null,
                                    QString::fromLatin1("%1 (*.FCMacro);;Python (*.py);;all files (*.*)")
                                                       .arg(tr("FreeCAD macro")));
    }
    if (fn.isEmpty())
        return false;

    d->editWrapper->editor()->document()->setModified(false);
    setCurrentFileName(fn);
    return saveFile();
}

/**
 * Opens the file \a fileName.
 */
bool EditorView::open(const QString& fileName)
{
    EditorViewWrapper *oldWrapper = d->editWrapper,
                      *thisWrapper = nullptr,
                      *newWrapper = nullptr;

    QList<EditorViewWrapper*> editWrappers = EditorViewSingleton::instance()->
                                                getWrappers(fileName);
    for (EditorViewWrapper *wrap : editWrappers) {
        if (wrap->owner() == this) {
            thisWrapper = wrap;
            newWrapper = wrap;
            break;
        }
        newWrapper = wrap;
    }

    if (!thisWrapper && newWrapper) {
        // file opened, but is it isn't is in this tab
        PythonEditor *pyedit = qobject_cast<PythonEditor*>(newWrapper->editor());
        TextEditor *edit = nullptr;
        if (pyedit)
            edit = new PythonEditor(this);
        else
            edit = new TextEditor(this);

        // set document
        edit->setDocument(newWrapper->editor()->document());
        edit->setSyntaxHighlighter(newWrapper->editor()->syntaxHighlighter());

        newWrapper = EditorViewSingleton::instance()->createWrapper(fileName, edit);
        if (!newWrapper)
            return false;

    } else if (!newWrapper) { // not yet opened
        newWrapper = EditorViewSingleton::instance()->createWrapper(fileName);
        if (!newWrapper)
            return false;
    }

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
    d->editWrapper->attach(this);
    if (oldWrapper->fileName().isEmpty() &&
        !oldWrapper->editor()->document()->isModified())
    {   // close the autogenerated editor from construction if its empty
        oldWrapper->close(this);
        delete oldWrapper;
    } else {
        oldWrapper->detach(this);
    }

    QFileInfo fi(fileName);
    d->editWrapper->setTimestamp(fi.lastModified().toTime_t());
    if (!fi.isWritable() && fileName.isEmpty()) // untitled files should be writable
        d->editWrapper->editor()->setReadOnly(false);
    else
        d->editWrapper->editor()->setReadOnly(!fi.isWritable());
    d->activityTimer->setSingleShot(true);
    d->activityTimer->start(3000);

    Q_EMIT switchedFile(fileName);
    setCurrentFileName(fileName);
    return true;
}

bool EditorView::save()
{
    if (fileName().isEmpty())
        return saveAs();
    return saveFile();
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

/**
 * Shows the printer dialog.
 */
void EditorView::print()
{
    QPrinter printer(QPrinter::ScreenResolution);
    printer.setFullPage(true);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() == QDialog::Accepted) {
        d->editWrapper->editor()->document()->print(&printer);
    }
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
    if (d->editWrapper->fileName() != fileName)
        Q_EMIT changeFileName(fileName);

    d->editWrapper->setFileName(fileName);

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
    setWindowModified(d->editWrapper->editor()->document()->isModified());
}

QString EditorView::fileName() const
{
    return d->editWrapper->fileName();
}

void EditorView::setFileName(QString fileName)
{
    d->editWrapper->setFileName(fileName);
}

void EditorView::setTopbar(EditorViewTopBar *topBar)
{
    d->topBar = topBar;
    d->topBar->setParent(this);
    insertWidget(d->topBar, 0);
}

EditorViewTopBar *EditorView::topBar()
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
    if (d->editWrapper->fileName().isEmpty())
        return saveAs();

    QFile file(d->editWrapper->fileName());
    if (!file.open(QFile::WriteOnly))
        return false;

    TextEditor *editor = d->editWrapper->editor();

    // trim trailing whitespace?
    // NOTE! maybe whitestrip should move to TextEditor instead?
    ParameterGrp::handle hPrefGrp = getWindowParameter();
    if (hPrefGrp->GetBool("EnableTrimTrailingWhitespaces", true )) {
        // to restore cursor and scroll position
        QTextCursor cursor = editor->textCursor();
        int oldPos = cursor.position(),
            vScroll = editor->verticalScrollBar()->value(),
            hScroll = editor->horizontalScrollBar()->value();

        QTextBlock block = editor->document()->firstBlock();
        int delCount = 0, chPos = -1;

        while (block.isValid()) {
            QString row = block.text();
            ++chPos; // for the newline
            int j =  row.size();
            while(j > 0 && row[j - 1].isSpace())
                --j;
            if (chPos + row.size() - j <= oldPos) // for restore cursorposition
                delCount += row.size() - j;
            chPos += row.size();
            int removeChrs = row.size() - j;
            if (removeChrs > 0) {
                row.remove(j, removeChrs);
                cursor.setPosition(block.position(), QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
                cursor.insertText(row);
            }
            block = block.next();
        }

        // restore cursor and scroll position
        editor->verticalScrollBar()->setValue(vScroll);
        editor->horizontalScrollBar()->setValue(hScroll);
        cursor.setPosition(oldPos - delCount);
        editor->setTextCursor(cursor);
    }

    QTextStream ts(&file);
    ts.setCodec(QTextCodec::codecForName("UTF-8"));
    ts << editor->document()->toPlainText();
    file.close();

    editor->document()->setModified(false);
    editor->onSave();

    QFileInfo fi(d->editWrapper->fileName());
    for (EditorViewWrapper *wrap : EditorViewSingleton::instance()->getWrappers(
                                            d->editWrapper->fileName()))
    {
        wrap->setTimestamp(fi.lastModified().toTime_t());
    }
#ifdef BUILD_PYTHON_DEBUGTOOLS
        {
            //DumpSyntaxTokens tok(editor->document()->begin());
            Python::DumpModule dMod(Python::SourceRoot::instance()->moduleFromPath(
                                        d->editWrapper->fileName().toStdString()));
        }
#endif
    return true;
}

QPlainTextEdit *EditorView::swapEditor(QPlainTextEdit *newEditor)
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
    if (!undo)
        d->editWrapper->undos().clear();
}

void EditorView::redoAvailable(bool redo)
{
    if (!redo)
        d->editWrapper->redos().clear();
}

void EditorView::contentsChange(int position, int charsRemoved, int charsAdded)
{
    Q_UNUSED(position); 
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

EditorViewWrapper *EditorView::editorWrapper() const
{
    return d->editWrapper;
}

// ---------------------------------------------------------

PythonEditorView::PythonEditorView(PythonEditor* editor, QWidget* parent)
  : EditorView(editor, parent)
{
    d = new PythonEditorViewP;
    EditorViewTopBar *topBar = new EditorViewTopBar(this);
    setTopbar(topBar);

    connect(this, SIGNAL(changeFileName(const QString&)),
            editor, SLOT(setFileName(const QString&)));

    App::PythonDebugger *debugger = Application::Instance->macroManager()->debugger();
    connect(debugger, SIGNAL(haltAt(QString,int)),
            this, SLOT(showDebugMarker(QString, int)));
    connect(debugger, SIGNAL(releaseAt(QString,int)),
            this, SLOT(hideDebugMarker(QString, int)));
}

PythonEditorView::~PythonEditorView()
{
    delete d;
}

/**
 * Runs the action specified by \a pMsg.
 */
bool PythonEditorView::onMsg(const char* pMsg,const char** ppReturn)
{
    if (strcmp(pMsg,"Run")==0) {
        executeScript();
        return true;
    }
    else if (strcmp(pMsg,"StartDebug")==0) {
        QTimer::singleShot(300, this, SLOT(startDebug()));
        return true;
    }
    else if (strcmp(pMsg,"ToggleBreakpoint")==0) {
        toggleBreakpoint();
        return true;
    }
    return EditorView::onMsg(pMsg, ppReturn);
}

/**
 * Checks if the action \a pMsg is available. This is for enabling/disabling
 * the corresponding buttons or menu items for this action.
 */
bool PythonEditorView::onHasMsg(const char* pMsg) const
{
    if (strcmp(pMsg,"Run")==0)  return true;
    if (strcmp(pMsg,"StartDebug")==0)  return true;
    if (strcmp(pMsg,"ToggleBreakpoint")==0)  return true;
    return EditorView::onHasMsg(pMsg);
}

/* static*/
PythonEditorView *PythonEditorView::setAsActive(QString fileName)
{
    PythonEditorView* editView = qobject_cast<PythonEditorView*>(
                                        getMainWindow()->activeWindow());

    if (!editView) {
        // not yet opened editor
        QList<QWidget*> mdis = getMainWindow()->windows();
        for (QList<QWidget*>::iterator it = mdis.begin(); it != mdis.end(); ++it) {
            editView = qobject_cast<PythonEditorView*>(*it);
            if (editView) break;
        }

        if (!editView) {
            WindowParameter param("PythonDebuggerView");
            std::string path = param.getWindowParameter()->GetASCII("MacroPath",
                App::Application::getUserMacroDir().c_str());

            if (fileName.isEmpty())
                fileName = QFileDialog::getOpenFileName(getMainWindow(), tr("Open python file"),
                                                            QLatin1String(path.c_str()),
                                                            tr("Python (*.py *.FCMacro)"));
            if (!fileName.isEmpty()) {
                PythonEditor* editor = new PythonEditor();
                editView = new PythonEditorView(editor, getMainWindow());
                editView->open(fileName);
                editView->resize(400, 300);
                getMainWindow()->addWindow(editView);
            } else {
                return nullptr;
            }
        }
    } else if (editView->fileName() != fileName && !fileName.isEmpty()) {
        editView->open(fileName);
    }

    getMainWindow()->setActiveWindow(editView);
    return editView;
}


/**
 * Runs the opened script in the macro manager.
 */
void PythonEditorView::executeScript()
{
    // always save the macro when it is modified
    if (EditorView::onHasMsg("Save"))
        EditorView::onMsg("Save", 0);
    try {
        Application::Instance->macroManager()->run(Gui::MacroManager::File,fileName().toUtf8());
    }
    catch (const Base::SystemExitException&) {
        // handle SystemExit exceptions
        Base::PyGILStateLocker locker;
        Base::PyException e;
        e.ReportException();
    }
}

void PythonEditorView::startDebug()
{
    PythonEditor *pye = qobject_cast<PythonEditor*>(editorWrapper()->editor());
    if (pye)
        pye->startDebug();
}

void PythonEditorView::toggleBreakpoint()
{
    PythonEditor *pye = qobject_cast<PythonEditor*>(editorWrapper()->editor());
    if (pye)
        pye->toggleBreakpoint();
}

void PythonEditorView::showDebugMarker(const QString &fileName, int line)
{

    PythonEditorView* editView = qobject_cast<PythonEditorView*>(
                                        getMainWindow()->activeWindow());
    if (editView != this)
        return;

    if (fileName != this->fileName())
        open(fileName);

    PythonEditor *pye = qobject_cast<PythonEditor*>(editorWrapper()->editor());
    if (pye)
        pye->showDebugMarker(line);
}

void PythonEditorView::hideDebugMarker(const QString &fileName, int line)
{
    Q_UNUSED(line);
    if (fileName == this->fileName()) {
        PythonEditor *pye = qobject_cast<PythonEditor*>(editorWrapper()->editor());
        if (pye)
            pye->hideDebugMarker();
    }
}

// -------------------------------------------------------------------------------

EditorViewWrapper::EditorViewWrapper(TextEditor *editor, const QString &fn) :
    d(new EditorViewWrapperP)
{
    d->textEdit = editor;
    d->timestamp = 0;
    d->fileName = fn;

    // store it
    EditorViewSingleton::instance()->d->wrappers.append(this);

    if (QFile::exists(d->fileName)) {
        QFile file(d->fileName);
        if (file.open(QFile::ReadOnly)) {
            d->lock = true;
            d->textEdit->setPlainText(QString::fromUtf8(file.readAll()));
            d->lock = false;
            file.close();
        }
    }
}

EditorViewWrapper::~EditorViewWrapper()
{
    delete d;
}

void EditorViewWrapper::attach(EditorView* sharedOwner)
{
    // handles automatic clenup
    d->owner = sharedOwner;

    // hook up signals with view
    QObject::connect(d->textEdit->document(), SIGNAL(modificationChanged(bool)),
            sharedOwner, SLOT(setWindowModified(bool)));
    QObject::connect(d->textEdit->document(), SIGNAL(undoAvailable(bool)),
            sharedOwner, SLOT(undoAvailable(bool)));
    QObject::connect(d->textEdit->document(), SIGNAL(redoAvailable(bool)),
            sharedOwner, SLOT(redoAvailable(bool)));
    QObject::connect(d->textEdit->document(), SIGNAL(contentsChange(int, int, int)),
            sharedOwner, SLOT(contentsChange(int, int, int)));

    sharedOwner->setWindowModified(d->textEdit->document()->isModified());
}

void EditorViewWrapper::detach(EditorView* sharedOwner)
{
    // handles automatic clenup
    d->owner = sharedOwner;

    // disconnect signals from view
    QObject::disconnect(d->textEdit->document(), SIGNAL(modificationChanged(bool)),
            sharedOwner, SLOT(setWindowModified(bool)));
    QObject::disconnect(d->textEdit->document(), SIGNAL(undoAvailable(bool)),
            sharedOwner, SLOT(undoAvailable(bool)));
    QObject::disconnect(d->textEdit->document(), SIGNAL(redoAvailable(bool)),
            sharedOwner, SLOT(redoAvailable(bool)));
    QObject::disconnect(d->textEdit->document(), SIGNAL(contentsChange(int, int, int)),
            sharedOwner, SLOT(contentsChange(int, int, int)));
}

// caller should call delete on this wrapper
bool EditorViewWrapper::close(EditorView* sharedOwner)
{
    detach(sharedOwner);

    d->textEdit->setParent(0);

    // cleanup and memory release
    EditorViewSingleton::instance()->removeWrapper(this);
    if (d->textEdit) {
        d->textEdit->deleteLater();
        d->textEdit = nullptr;
    }

    // emit changes
    Q_EMIT EditorViewSingleton::instance()->openFilesChanged();
    Q_EMIT EditorViewSingleton::instance()->fileClosed(d->fileName);

    return true;
}

TextEditor *EditorViewWrapper::editor() const
{
    return d->textEdit;
}

EditorView *EditorViewWrapper::owner() const
{
    return d->owner;
}

QString EditorViewWrapper::fileName() const
{
    return d->fileName;
}

void EditorViewWrapper::setFileName(const QString &fn)
{
    d->fileName = fn;
    // some editors need filename, for example PythonEditor
    // use Qt introspection to "textEdit->setFilename(fn)"

    // this block finds and invokes that method if it exists
    const QMetaObject *mo = d->textEdit->metaObject();
    if (mo->indexOfMethod("setFileName(QString)")) {
        mo->invokeMethod(d->textEdit, "setFileName",
                         Qt::DirectConnection, Q_ARG(QString, fn));
    }

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

QStringList &EditorViewWrapper::undos()
{
    return d->undos;
}

QStringList &EditorViewWrapper::redos()
{
    return d->redos;
}


// -------------------------------------------------------------------

EditorViewSingleton::EditorViewSingleton() :
    d(new EditorViewSingletonP)
{
    // delay this connect until application is properly initialized ie eventloop is running
    QTimer::singleShot(0, this, SLOT(connectToDebugger(void)));
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
    for (EditorViewSingletonP::EditorType *et : me->d->editorTypes) {
        if (et->name == name)
            return false; // already registered
    }

    // new editor type, register
    me->d->editorTypes.append(new EditorViewSingletonP::EditorType(factory, name,
                                                                   suffixes, mimetypes,
                                                                   icon));
    return true;
}

/* static */
EditorViewSingleton* EditorViewSingleton::instance()
{
    static EditorViewSingleton* _instance = new EditorViewSingleton();

    return _instance;
}

EditorViewWrapper* EditorViewSingleton::getWrapper(const QString &fn, EditorView *ownerView)
{
    for (EditorViewWrapper* editWrapper : d->wrappers) {
        if ((editWrapper->fileName() == fn) &&
            (editWrapper->owner() == ownerView))
        {
            d->accessOrder.removeAll(fn);
            d->accessOrder.append(fn);
            return editWrapper;
        }
    }

    return nullptr;
}

QList<EditorViewWrapper *> EditorViewSingleton::getWrappers(const QString &fn)
{
    QList<EditorViewWrapper*> wrappers;
    for (EditorViewWrapper *editWrapper : d->wrappers) {
        if (editWrapper->fileName() == fn)
            wrappers.append(editWrapper);
    }

    return wrappers;
}

EditorViewWrapper* EditorViewSingleton::createWrapper(const QString &fn,
                                                      TextEditor *editor)
{
    QString mime = QLatin1String("text/plain"); // default ;
    QFileInfo fi(fn);
    QString suffix = fi.suffix().toLower();
    if (suffix == QLatin1String("fcmacro")) {
        mime = QLatin1String("text/x-script.python");
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(fn);
        if (type.isValid())
            mime = type.name();
#else
        // FIXME: cleanup when support for Qt4 is dropped
        // workaround by using file suffix

        if (suffix == QLatin1String("py")) {
            mime = QLatin1String("text/x-python");
        } else if (suffix == QLatin1String("INP")) {
            mime = QLatin1String("text/x-abacus");
        } else if (suffix == QLatin1String("nc") ||
                   suffix == QLatin1String("gc") ||
                   suffix == QLatin1String("ncc") ||
                   suffix == QLatin1String("ngc") ||
                   suffix == QLatin1String("cnc") ||
                   suffix == QLatin1String("tap") ||
                   suffix == QLatin1String("gcode"))
        {
            mime = QLatin1String("text/x-gcode");
        }
#endif
    }

    EditorViewWrapper *ew = nullptr;
    QString icon(QLatin1String("accessories-text-editor"));

    if (editor) {
        ew = new EditorViewWrapper(editor, fn);
    } else {
        // first find via file suffix, ie .py
        for (EditorViewSingletonP::EditorType *et : d->editorTypes) {
            if (et->suffixes.contains(fi.suffix())) {
                ew = new EditorViewWrapper(et->factory(), fn);
                icon = et->iconName;
                break;
            }
        }

        if (!ew) {
            // find the registered editor for this mimetype
            for (EditorViewSingletonP::EditorType *et : d->editorTypes) {
                if (et->mimetypes.contains(mime, Qt::CaseInsensitive)) {
                    ew = new EditorViewWrapper(et->factory(), fn);
                    icon = et->iconName;
                    break;
                }
            }
        }

        // default to TextEditor
        if (!ew)
            ew = new EditorViewWrapper(new TextEditor, fn);
    }

    d->accessOrder.append(fn);

    // set icon for this type
    ew->editor()->setWindowIcon(Gui::BitmapFactory().iconFromTheme(icon.toLatin1()));

    connect(ew->editor(), SIGNAL(modificationChanged(bool)),
                        this, SLOT(docModifiedChanged(bool)));
    Q_EMIT openFilesChanged();
    Q_EMIT fileOpened(fn);
    return ew;
}

bool EditorViewSingleton::removeWrapper(EditorViewWrapper *ew)
{
    disconnect(ew->editor(), SIGNAL(modificationChanged(bool)),
               this, SLOT(docModifiedChanged(bool)));
    bool rm = false;
    for(int i = 0; i < d->accessOrder.size(); ++i) {
        if (d->accessOrder[i] == ew->fileName())
            rm = true;
        // remove all after first found
        if (rm) {
            d->accessOrder.removeAt(i);
            --i;
        }
    }

    return d->wrappers.removeAll(ew);
}

EditorViewWrapper *EditorViewSingleton::lastAccessed(EditorView *view, int backSteps)
{
    int idx = d->accessOrder.size() + backSteps -1;
    if (idx >= 0 && d->accessOrder.size() > idx)
        return getWrapper(d->accessOrder.at(idx), view);
    return nullptr;
}

QList<const EditorViewWrapper*> EditorViewSingleton::openedByType(QStringList types)
{
    QList<const EditorViewWrapper*> res;
    for (EditorViewWrapper* editWrapper : d->wrappers) {
        QFileInfo fi(editWrapper->fileName());
        if (types.size() == 0 || types.contains(fi.suffix(), Qt::CaseInsensitive))
            res.append(editWrapper);
    }
    return res;
}

void EditorViewSingleton::docModifiedChanged(bool changed)
{
    QObject *obj = sender();
    for (EditorViewWrapper *ew : d->wrappers) {
        if (ew->editor() == obj)
            Q_EMIT modifiedChanged(ew->fileName(), changed);
    }
}

void EditorViewSingleton::connectToDebugger(void)
{
    // this must be done after Application is initialized, hence this slot
    App::PythonDebugger *dbg = Application::Instance->macroManager()->debugger();
    connect(this, SIGNAL(fileOpened(QString)), dbg, SLOT(onFileOpened(QString)));
    connect(this, SIGNAL(fileClosed(QString)), dbg, SLOT(onFileClosed(QString)));
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

    for (const EditorViewWrapper *wrapper : EditorViewSingleton::instance()->
                                    openedByType())
    {
        if (!wrapper->editor() ||
            !wrapper->editor()->document())
        {
            continue;
        }

        d->openFiles->addItem(wrapper->editor()->windowIcon(),
                             createViewName(wrapper->fileName(),
                                            wrapper->editor()->document()->isModified()),
                             wrapper->fileName());
    }

    setCurrentIdx(d->editorView->fileName());

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
    if (d->editorView && fileName != d->editorView->fileName())
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
    m_findFlags(0)
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
    TextEditor *edit = qobject_cast<TextEditor*>(d->editWrapper->editor());
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
