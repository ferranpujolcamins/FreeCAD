#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <FCConfig.h>
#include <Gui/MainWindow.h>
//#include <QMainWindow>
#include <QSessionManager>
#include <memory>


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

    void openFile(const QString &filename);

protected:
    void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
    void newFile();
    void open();
    bool save();
    bool saveAs();
    void about();
    void cut();
    void copy();
    void paste();
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
    Gui::EditorView* newEditorView();
    //bool saveFile(const QString &fileName);
    void createDockWindows();
    QString strippedName(const QString &fullFileName);
    QList<std::shared_ptr<Gui::EditorView>> editViews;

    QString curFile;


    QListWidget *customerList;
    QListWidget *paragraphsList;
};

} // namespace IDE

#endif // MAINWINDOW_H
