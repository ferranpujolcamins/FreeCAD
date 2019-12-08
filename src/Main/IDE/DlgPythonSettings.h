#ifndef DLGPYTHONSETTINGS_H
#define DLGPYTHONSETTINGS_H

#include <FCConfig.h>
#include <QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

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

#endif // DLGPYTHONSETTINGS_H
