

#include "DlgPythonSettings.h"

#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <Gui/DlgEditorImp.h>

#include <Gui/TextEditor/PythonSource/PythonToken.h>
#include <Gui/TextEditor/EditorView.h>
#include <Gui/TextEditor/PythonEditor.h>


using namespace Gui;
using namespace Ide;

DlgPythonSettings::DlgPythonSettings(QDialog *parent) :
    QDialog(parent)
{
    auto gridLayout = new QGridLayout(this);
    auto lblHeader = new QLabel(tr("Python settings"));
    auto lblVersion = new QLabel(tr("Python version"));
    m_cmbVersion = new QComboBox;
    auto okButton = new QPushButton(tr("Ok"));
    auto cancelButton = new QPushButton(tr("Cancel"));
    gridLayout->addWidget(lblHeader, 0, 0, 0, 2);
    gridLayout->addWidget(lblVersion, 1, 0);
    gridLayout->addWidget(m_cmbVersion, 1, 1);
    gridLayout->addWidget(cancelButton, 2, 0);
    gridLayout->addWidget(okButton, 2, 1);
    setLayout(gridLayout);

    auto curVersion = Python::Lexer::version().current();
    for (auto &verPair: Python::Version::availableVersions()) {
        m_cmbVersion->addItem(QString::fromStdString(verPair.second), QVariant(verPair.first));
        if (verPair.first == curVersion)
            m_cmbVersion->setCurrentText(QString::fromStdString(verPair.second));
    }

    connect(okButton, &QPushButton::clicked, this, &DlgPythonSettings::accept);
    connect(cancelButton, &QPushButton::clicked, this, &DlgPythonSettings::reject);
}

DlgPythonSettings::~DlgPythonSettings()
{
}

void DlgPythonSettings::accept()
{
    auto ver = static_cast<Python::Version::versions>(m_cmbVersion->currentData().toUInt());
    if (ver != Python::Lexer::version().current()) {
        Python::Lexer::setVersion(ver);

        QStringList types;
        types << QLatin1String("py") << QLatin1String("FCMacro");
        for (auto &wrapper : EditorViewSingleton::instance()->openedBySuffix(types)) {
            auto textEdit = dynamic_cast<TextEditor*>(wrapper->editor());
            auto highlighter = dynamic_cast<PythonSyntaxHighlighter*>(textEdit->syntaxHighlighter());
            if (highlighter)
                highlighter->rehighlight();
        }
    }

    QDialog::accept();
}


// -----------------------------------------------------------------------------------


DlgEditorSettings::DlgEditorSettings(QWidget *parent)
    : QDialog(parent)
    , m_dlgEditor(new Gui::Dialog::DlgSettingsEditorImp())
{
    m_dlgEditor->loadSettings();
    QLayout *editLayout = new QVBoxLayout;
    editLayout->addWidget(m_dlgEditor);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                        QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    editLayout->addWidget(buttons);
    auto sizePolicy = new QSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Expanding);
    setSizePolicy(*sizePolicy);
    setLayout(editLayout);
}

DlgEditorSettings::~DlgEditorSettings()
{
     m_dlgEditor->deleteLater();
}

void DlgEditorSettings::accept()
{
     m_dlgEditor->saveSettings();
     QDialog::accept();
}



#include "moc_DlgPythonSettings.cpp"

