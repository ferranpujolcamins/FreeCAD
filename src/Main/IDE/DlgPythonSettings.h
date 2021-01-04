#ifndef DLGPYTHONSETTINGS_H
#define DLGPYTHONSETTINGS_H

#include <FCConfig.h>
#include <QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

namespace Gui {
namespace Dialog {
class DlgSettingsEditorImp;
}
} // namespace Gui

namespace Ide {

// quick throw together dialog,
// TODO rewrok the dialog things into something more proper
class DlgEditorSettings : public QDialog
{
    Q_OBJECT
public:
    DlgEditorSettings(QWidget *parent);
    virtual ~DlgEditorSettings() override;

protected:
    void accept() override;

private:
    Gui::Dialog::DlgSettingsEditorImp *m_dlgEditor;
};



class DlgPythonSettings : public QDialog
{
    Q_OBJECT
public:
    explicit DlgPythonSettings(QDialog *parent = nullptr);
    virtual ~DlgPythonSettings();

protected:
    void accept();

private:
    QComboBox *m_cmbVersion;
};

} // end namespace Ide

#endif // DLGPYTHONSETTINGS_H
