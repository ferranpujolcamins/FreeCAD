#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <FCConfig.h>
#include <Gui/MainWindow.h>
//#include <QMainWindow>
#include <QSessionManager>

/*
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow();

signals:

public slots:
};

*/

namespace Gui {
//class TextEditor;
class EditorView;
}

QT_BEGIN_NAMESPACE
class QListWidget;
QT_END_NAMESPACE

namespace Ide {


class MainWindow : public Gui::MainWindow
{
    Q_OBJECT

public:
    MainWindow();

    void loadFile(const QString &fileName);

protected:
    void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
    void newFile();
    void open();
    bool save();
    bool saveAs();
    void about();
    void documentWasModified();
    void showOptions();
#ifndef QT_NO_SESSIONMANAGER
    void commitData(QSessionManager &);
#endif

private:
    void createActions();
    void createStatusBar();
    void readSettings();
    void writeSettings();
    bool maybeSave();
    //bool saveFile(const QString &fileName);
    void createDockWindows();
    QString strippedName(const QString &fullFileName);

    Gui::EditorView *editorView;
    QString curFile;


    QListWidget *customerList;
    QListWidget *paragraphsList;
};

} // namespace IDE

#endif // MAINWINDOW_H
