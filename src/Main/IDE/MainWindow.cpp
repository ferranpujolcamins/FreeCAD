
#include "MainWindow.h"
#include "DlgPythonSettings.h"
//#include <Python.h>
#include <QtWidgets>
#include <Gui/TextEditor/TextEditor.h>
#include <Gui/TextEditor/EditorView.h>

/*
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
    setVisible(true);
}

MainWindow::~MainWindow()
{
}

*/

namespace Ide {


MainWindow::MainWindow():
    Gui::MainWindow(nullptr),
    editorView(nullptr)
{
    Gui::TextEditor *edit = new Gui::TextEditor(this);
    editorView = new Gui::EditorView(edit, this);
    editorView->resize(400, 300);
    //setCentralWidget(editorView);
    addWindow(editorView);

    createActions();
    createStatusBar();

    readSettings();

    connect(edit, SIGNAL(documentWasModified()),
            this, SLOT(documentWasModified));

#ifndef QT_NO_SESSIONMANAGER
    QGuiApplication::setFallbackSessionManagementEnabled(false);
    connect(qApp, &QGuiApplication::commitDataRequest,
            this, &MainWindow::commitData);
#endif

    setUnifiedTitleAndToolBarOnMac(true);


    createDockWindows();

    setUnifiedTitleAndToolBarOnMac(true);


}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::newFile()
{
    if (maybeSave()) {
        editorView->closeFile();
        //setCurrentFile(QString());
    }
}

void MainWindow::open()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this);
        if (!fileName.isEmpty())
            loadFile(fileName);
    }
}

bool MainWindow::save()
{
    if (!editorView->save())
        return false;

    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

bool MainWindow::saveAs()
{
    if (!editorView->saveAs())
        return false;

    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

void MainWindow::about()
{
   QMessageBox::about(this, tr("About Application"),
            tr("The <b>Application</b> example demonstrates how to "
               "write modern GUI applications using Qt, with a menu bar, "
               "toolbars, and a status bar."));
}

void MainWindow::documentWasModified()
{
    setWindowModified(editorView->getEditor()->document()->isModified());
}

void MainWindow::showOptions()
{
    DlgPythonSettings dlg;
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
    QAction *exitAct = fileMenu->addAction(exitIcon, tr("E&xit"), this, &QWidget::close);
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
    connect(cutAct, &QAction::triggered, editorView, &Gui::EditorView::cut);
    editMenu->addAction(cutAct);
    editToolBar->addAction(cutAct);

    const QIcon copyIcon = QIcon::fromTheme(QLatin1String("edit-copy"),
                                            QIcon(QLatin1String(":/images/copy.png")));
    QAction *copyAct = new QAction(copyIcon, tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));
    connect(copyAct, &QAction::triggered, editorView, &Gui::EditorView::copy);
    editMenu->addAction(copyAct);
    editToolBar->addAction(copyAct);

    const QIcon pasteIcon = QIcon::fromTheme(QLatin1String("edit-paste"),
                                             QIcon(QLatin1String(":/images/paste.png")));
    QAction *pasteAct = new QAction(pasteIcon, tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                              "selection"));
    connect(pasteAct, &QAction::triggered, editorView, &Gui::EditorView::paste);
    editMenu->addAction(pasteAct);
    editToolBar->addAction(pasteAct);

    menuBar()->addSeparator();

    QAction *optionAct = new QAction(tr("&options"), this);
    connect(optionAct, &QAction::triggered, this, &MainWindow::showOptions);
    editMenu->addAction(optionAct);

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
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::readSettings()
{
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
}

void MainWindow::writeSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue(QLatin1String("geometry"), saveGeometry());
}

bool MainWindow::maybeSave()
{
    if (!editorView->getEditor()->document()->isModified())
        return true;
    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("Application"),
                               tr("The document has been modified.\n"
                                  "Do you want to save your changes?"),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return save();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

void MainWindow::loadFile(const QString &fileName)
{
    if (!editorView->open(fileName)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot read file %1:\n.")
                             .arg(QDir::toNativeSeparators(fileName)));
        return;
    }

    statusBar()->showMessage(tr("File loaded"), 2000);
}

/*
bool MainWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(QDir::toNativeSeparators(fileName),
                                  file.errorString()));
        return false;
    }

    editorView->saveAs(fileName);
    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}
*/

QString MainWindow::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}
#ifndef QT_NO_SESSIONMANAGER
void MainWindow::commitData(QSessionManager &manager)
{
    if (manager.allowsInteraction()) {
        if (!maybeSave())
            manager.cancel();
    } else {
        // Non-interactive: save without asking
        if (editorView->getEditor()->document()->isModified())
            save();
    }
}
#endif

void MainWindow::createDockWindows()
{
    QMenu men;
    QMenu *viewMenu = &men;
    menuBar()->addMenu(viewMenu);
    QDockWidget *dock = new QDockWidget(tr("Customers"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    customerList = new QListWidget(dock);
    customerList->addItems(QStringList()
            << QLatin1String("John Doe, Harmony Enterprises, 12 Lakeside, Ambleton")
            << QLatin1String("Jane Doe, Memorabilia, 23 Watersedge, Beaton")
            << QLatin1String("Tammy Shea, Tiblanka, 38 Sea Views, Carlton")
            << QLatin1String("Tim Sheen, Caraba Gifts, 48 Ocean Way, Deal")
            << QLatin1String("Sol Harvey, Chicos Coffee, 53 New Springs, Eccleston")
            << QLatin1String("Sally Hobart, Tiroli Tea, 67 Long River, Fedula"));
    dock->setWidget(customerList);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());

    dock = new QDockWidget(tr("Paragraphs"), this);
    paragraphsList = new QListWidget(dock);
    paragraphsList->addItems(QStringList()
            << QLatin1String("Thank you for your payment which we have received today.")
            << QLatin1String("Your order has been dispatched and should be with you "
               "within 28 days.")
            << QLatin1String("We have dispatched those items that were in stock. The "
               "rest of your order will be dispatched once all the "
               "remaining items have arrived at our warehouse. No "
               "additional shipping charges will be made.")
            << QLatin1String("You made a small overpayment (less than $5) which we "
               "will keep on account for you, or return at your request.")
            << QLatin1String("You made a small underpayment (less than $1), but we have "
               "sent your order anyway. We'll add this underpayment to "
               "your next bill.")
            << QLatin1String("Unfortunately you did not send enough money. Please remit "
               "an additional $. Your order will be dispatched as soon as "
               "the complete amount has been received.")
            << QLatin1String("You made an overpayment (more than $5). Do you wish to "
               "buy more items, or should we return the excess to you?"));
    dock->setWidget(paragraphsList);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());

    //connect(customerList, &QListWidget::currentTextChanged,
    //        this, &MainWindow::insertCustomer);
    //connect(paragraphsList, &QListWidget::currentTextChanged,
    //        this, &MainWindow::addParagraph);
}

} // namespace Ide

#include "moc_MainWindow.cpp"
