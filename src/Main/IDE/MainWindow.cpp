
#include "MainWindow.h"
#include "DlgPythonSettings.h"
#include <QtWidgets>
#include <QSplitter>
#include <Gui/TextEditor/TextEditor.h>
#include <Gui/TextEditor/EditorView.h>
#include <Gui/PythonDebuggerView.h>
#include <Gui/DockWindowManager.h>

#include <QDebug>



namespace Ide {


MainWindow::MainWindow()
    : Gui::MainWindow(nullptr)
    , mdiArea(nullptr)
{
    // find the MDIarea, remove the tab, go to subwindow mode
    for (auto wdgt : QApplication::topLevelWidgets()) {
        auto v = wdgt->findChildren<QMdiArea*>();
        for (auto a : v) {
            mdiArea = qobject_cast<QMdiArea*>(a);
            if (mdiArea)
                mdiArea->setViewMode(QMdiArea::SubWindowView);
        }
    }

    splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setCollapsible(0, false);
    mdiArea->addSubWindow(splitter);
    splitter->showMaximized();

    readSettings();

    if (Gui::EditorViewSingleton::instance()->editorViews().isEmpty())
        newEditorView();

    createActions();
    createStatusBar();

    setUnifiedTitleAndToolBarOnMac(true);


    createDockWindows();

    //statusBar()->hide();
    connect(statusBar(), SIGNAL(messageChanged(const QString &message)),
            this, SLOT(showMessage(const QString &message, int timeout)));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    maybeSave();

    Gui::MainWindow::closeEvent(event);
}

void MainWindow::newFile()
{
    auto ews = Gui::EditorViewSingleton::instance();
    auto *view = ews->activeView();
    if (!view)
        view = newEditorView();

    view->newFile();
}

void MainWindow::open()
{
    QString filename = QFileDialog::getOpenFileName(this);
    if (filename.isEmpty())
        return;

    openFile(filename);
}

void MainWindow::openFile(const QString& filename)
{
    auto ews = Gui::EditorViewSingleton::instance();
    auto editView = ews->activeView();
    if (!editView)
        editView = newEditorView();

    // check if already openend
    for (auto view : ews->editorViews()) {
        if (view->filename() == filename) {
            view->editor()->setFocus();
            return;
        }
    }

    if (!editView)
        editView = newEditorView();
    editView->editor()->setFocus();
    loadFile(filename); // error handling in here
}

bool MainWindow::save()
{
    auto editorView = Gui::EditorViewSingleton::instance()->activeView();
    if (editorView && !editorView->save())
        return false;

    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

bool MainWindow::saveAs()
{
    auto editorView = Gui::EditorViewSingleton::instance()->activeView();
    if (editorView && !editorView->saveAs())
        return false;

    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

void MainWindow::about()
{
   QMessageBox::about(this, tr("About Application"),
            tr("This is an atempt to make an code editor IDE for FreeCAD"));
}

void MainWindow::cut()
{
    auto editorView = Gui::EditorViewSingleton::instance()->activeView();
    if (editorView) {
        editorView->cut();
    }
}

void MainWindow::copy()
{
    auto editorView = Gui::EditorViewSingleton::instance()->activeView();
    if (editorView) {
        editorView->copy();
    }
}
void MainWindow::paste()
{
    auto editorView  = Gui::EditorViewSingleton::instance()->activeView();
    if (editorView) {
        editorView->paste();
    }
}

void MainWindow::documentWasModified()
{
    auto editorView  = Gui::EditorViewSingleton::instance()->activeView();
    if (editorView)
        setWindowModified(editorView->editor()->document()->isModified());
}

void MainWindow::showOptions()
{
    DlgPythonSettings dlg;
    dlg.exec();
}

void MainWindow::showEditorOptions()
{
    DlgEditorSettings dlg(this);
    dlg.exec();
}

void MainWindow::createActions()
{

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QToolBar *fileToolBar = addToolBar(tr("File"));
    const QIcon newIcon = QIcon::fromTheme(QLatin1String("document-new"),
                                           QIcon(QLatin1String(":/images/new.png")));
    QAction *newAct = new QAction(newIcon, tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    newAct->setStatusTip(tr("Create a new file"));
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
    fileMenu->addAction(newAct);
    fileToolBar->addAction(newAct);

    const QIcon openIcon = QIcon::fromTheme(QLatin1String("document-open"),
                                            QIcon(QLatin1String(":/images/open.png")));
    QAction *openAct = new QAction(openIcon, tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, &QAction::triggered, this, &MainWindow::open);
    fileMenu->addAction(openAct);
    fileToolBar->addAction(openAct);

    const QIcon saveIcon = QIcon::fromTheme(QLatin1String("document-save"),
                                            QIcon(QLatin1String(":/images/save.png")));
    QAction *saveAct = new QAction(saveIcon, tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the document to disk"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    fileMenu->addAction(saveAct);
    fileToolBar->addAction(saveAct);

    const QIcon saveAsIcon = QIcon::fromTheme(QLatin1String("document-save-as"));
    QAction *saveAsAct = fileMenu->addAction(saveAsIcon, tr("Save &As..."), this, &MainWindow::saveAs);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    saveAsAct->setStatusTip(tr("Save the document under a new name"));


    fileMenu->addSeparator();

    const QIcon exitIcon = QIcon::fromTheme(QLatin1String("application-exit"));
    QAction *exitAct = fileMenu->addAction(exitIcon, tr("E&xit"),[=](){
        close();
        qApp->quit();
    });
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QToolBar *editToolBar = addToolBar(tr("Edit"));
#ifndef QT_NO_CLIPBOARD
    const QIcon cutIcon = QIcon::fromTheme(QLatin1String("edit-cut"),
                                           QIcon(QLatin1String(":/images/cut.png")));
    QAction *cutAct = new QAction(cutIcon, tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    cutAct->setStatusTip(tr("Cut the current selection's contents to the "
                            "clipboard"));

    const QIcon copyIcon = QIcon::fromTheme(QLatin1String("edit-copy"),
                                            QIcon(QLatin1String(":/images/copy.png")));
    QAction *copyAct = new QAction(copyIcon, tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));

    auto editorView = Gui::EditorViewSingleton::instance()->activeView();
    if (editorView) {
        connect(cutAct, &QAction::triggered, this, &MainWindow::cut);
        editMenu->addAction(cutAct);
        editToolBar->addAction(cutAct);

        connect(copyAct, &QAction::triggered, this, &MainWindow::copy);
        editMenu->addAction(copyAct);
        editToolBar->addAction(copyAct);

        const QIcon pasteIcon = QIcon::fromTheme(QLatin1String("edit-paste"),
                                                 QIcon(QLatin1String(":/images/paste.png")));
        QAction *pasteAct = new QAction(pasteIcon, tr("&Paste"), this);
        pasteAct->setShortcuts(QKeySequence::Paste);
        pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                                  "selection"));
        connect(pasteAct, &QAction::triggered, this, &MainWindow::paste);
        editMenu->addAction(pasteAct);
        editToolBar->addAction(pasteAct);
    }

    menuBar()->addSeparator();
    auto viewMenu = menuBar()->addMenu(tr("&View"));

    auto editorOptions = new QAction(tr("&Editor options"), this);
    viewMenu->addAction(editorOptions);
    connect(editorOptions, &QAction::triggered, this, &MainWindow::showEditorOptions);
    QAction *optionAct = new QAction(tr("&Options"), this);
    connect(optionAct, &QAction::triggered, this, &MainWindow::showOptions);
    viewMenu->addAction(optionAct);

#endif // !QT_NO_CLIPBOARD

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &MainWindow::about);
    aboutAct->setStatusTip(tr("Show the application's About box"));


    QAction *aboutQtAct = helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));

#ifndef QT_NO_CLIPBOARD
    cutAct->setEnabled(false);
    copyAct->setEnabled(false);
    //connect(textEdit, &QPlainTextEdit::copyAvailable, cutAct, &QAction::setEnabled);
    //connect(textEdit, &QPlainTextEdit::copyAvailable, copyAct, &QAction::setEnabled);
#endif // !QT_NO_CLIPBOARD



    QToolBar *splitToolBar = addToolBar(tr("Split"));
    QMenu *splitMenu = menuBar()->addMenu(tr("Split"));
    QAction *verticalAct = splitMenu->addAction(tr("Vertical"), [&](){
        this->splitter->setOrientation(Qt::Vertical);
    });
    QAction *horizontalAct = splitMenu->addAction(tr("Horizontal"), [&](){
        this->splitter->setOrientation(Qt::Horizontal);
    });
    QAction *split = splitMenu->addAction(tr("Split"), [&](){
        if (splitter->count() > 2) return;
        auto curView = Gui::EditorViewSingleton::instance()->activeView();
        if (curView) {
            auto view = this->newEditorView();
            view->open(curView->filename());
        }
    });
    QAction *join = splitMenu->addAction(tr("Join"), [&](){
        if (splitter->count() < 2) return;
        auto ews = Gui::EditorViewSingleton::instance();
        for (auto view : ews->editorViews()) {
            if (view != ews->activeView()) {
                view->deleteLater();
                break;
            }
        }
    });

    splitToolBar->addSeparator();
    splitToolBar->addAction(split);
    splitToolBar->addAction(join);
    splitToolBar->addSeparator();

    splitToolBar->addAction(verticalAct);
    splitToolBar->addAction(horizontalAct);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::readSettings()
{
    // to make breakpoint streamable to be persitent between sessions
    qRegisterMetaTypeStreamOperators<App::Debugging::Python::BrkPntFile>("BrkPntFile");

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value(QLatin1String("geometry"),
                                               QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }

    // restore opened files
    int viewCnt = settings.value(QLatin1String("viewscnt"), 0).toInt();
    for (int i = 0; i < viewCnt; ++i) {
        auto view = newEditorView();
        auto filesVar = settings.value(QString::fromLatin1("openedfiles_%1").arg(i), QStringList());
        auto files = filesVar.toStringList();
        for (auto &fn : files) {
            if (!fn.isEmpty() && QFileInfo::exists(fn))
                view->open(fn);
        }
    }

    // set last accessed
    QString lastfile = settings.value(QLatin1String("lastopened"), QString()).toString();
    if (!lastfile.isEmpty())
        openFile(lastfile);

    // restore breakpoints
    // is actually done when first read to settings is made (deserialize stream)
    /*auto bpfileVar = settings.value(QLatin1String("pybeakpoints"));
    if (!bpfileVar.isNull()) {
        if (bpfileVar.canConvert<QList<QVariant>>()) {
            auto bpfiles = bpfileVar.value<QList<QVariant>>();
            for (auto bpfileVar : bpfiles) {
                auto bpfile = bpfileVar.value<App::Debugging::Python::BrkPntFile>();
                auto shPtr = std::dynamic_pointer_cast<App::Debugging::Python::BrkPntFile>(bpfile.shared_from_this());
                dbgr->setBreakpointFile(shPtr);
            }
        }
    }*/
}

void MainWindow::writeSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue(QLatin1String("geometry"), saveGeometry());

    auto ews = Gui::EditorViewSingleton::instance();

    // save opened files
    auto views = ews->editorViews();
    int i = 0;
    for (auto view : views) {
        QStringList files;
        for (auto wrp : view->wrappers()) {
            if (!wrp->filename().isEmpty() && QFileInfo::exists(wrp->filename()))
                files.push_back(wrp->filename());
        }

        auto viewName = QString::fromLatin1("openedfiles_%1").arg(i++);
        settings.setValue(viewName, files);
    }
    settings.setValue(QLatin1String("viewscnt"), i);


    auto view = Gui::EditorViewSingleton::instance()->activeView();
    QString lastfile;
    if (view && !view->filename().isEmpty())
        lastfile = view->filename();

    settings.setValue(QLatin1String("lastopened"), lastfile);


    // save breakpoints set
    QList<QVariant> bpFiles;
    auto brkpntfiles = App::Debugging::Python::Debugger::instance()->allBreakpointFiles();
    for (auto bpfile : brkpntfiles) {
        auto bpfileVar = QVariant::fromValue<App::Debugging::Python::BrkPntFile>(*bpfile);
        bpFiles.push_back(bpfileVar);
    }

    settings.setValue(QLatin1String("pybreakpoints"), bpFiles);

    auto var = settings.value(QLatin1String("pybreakpoints"));
}

void MainWindow::maybeSave()
{
    auto editorView = Gui::EditorViewSingleton::instance()->activeView();
    if (!editorView || !editorView->editor() ||
        !editorView->editor()->document() ||
        !editorView->editor()->document()->isModified())
    {
        return;
    }

    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("Application"),
                               tr("The document has been modified.\n"
                                  "Do you want to save your changes?"),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save)
        save();
}

Gui::EditorView* MainWindow::newEditorView()
{
    auto edit = new Gui::TextEditor(this);
    auto editorView = new Gui::EditorView(edit, QString(), this);
    editorView->resize(400, 300);
    editorView->setDisplayName(Gui::EditorView::FileName);
    //setCentralWidget(editorView);
    splitter->addWidget(editorView);
    //setActiveWindow(editorView);

    connect(edit->document(), &QTextDocument::contentsChanged,
            this, &MainWindow::documentWasModified);

    return editorView;
}

void MainWindow::loadFile(const QString &fileName)
{
    auto editorView = Gui::EditorViewSingleton::instance()->activeView();
    if (!editorView)
        editorView = newEditorView();

    if (editorView && !editorView->open(fileName)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot read file %1:\n.")
                             .arg(QDir::toNativeSeparators(fileName)));
        return;
    }
    statusBar()->showMessage(tr("File loaded"), 2000);
}

QString MainWindow::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

void MainWindow::createDockWindows()
{
    QMenu *viewMenu = new QMenu(this);
    menuBar()->addMenu(viewMenu);


    auto pyDock = new QDockWidget(tr("Python"), this);
    pyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    pyDock->setWidget(new Gui::DockWnd::PythonDebuggerView(pyDock));
    pyDock->setObjectName(QString::fromLatin1(QT_TRANSLATE_NOOP("QDockWidget","Debugger view")));
    viewMenu->addAction(pyDock->toggleViewAction());
    addDockWidget(Qt::RightDockWidgetArea, pyDock);

    // accept drops on the window, get handled in dropEvent, dragEnterEvent
    setAcceptDrops(true);
}

} // namespace Ide

#include "moc_MainWindow.cpp"
