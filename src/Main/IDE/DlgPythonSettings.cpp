
#include "DlgPythonSettings.h"


#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>

#include <Gui/TextEditor/PythonSource/PythonToken.h>
#include <Gui/TextEditor/EditorView.h>
#include <Gui/TextEditor/PythonEditor.h>

using namespace Gui;

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

    auto curVersion = Python::Lexer::version().version();
    for (auto &verPair: Python::Version::availableVersions()) {
        m_cmbVersion->addItem(QString::fromStdString(verPair.second), QVariant(verPair.first));
        if (verPair.first == curVersion)
            m_cmbVersion->setCurrentText(QString::fromStdString(verPair.second));
    }

    connect(m_cmbVersion, &QComboBox::currentTextChanged, this, &DlgPythonSettings::changedVersion);
    connect(okButton, &QPushButton::click, this, &DlgPythonSettings::accept);
    connect(cancelButton, &QPushButton::click, this, &DlgPythonSettings::reject);
}

DlgPythonSettings::~DlgPythonSettings()
{
}

void DlgPythonSettings::changedVersion()
{
    auto ver = static_cast<Gui::Python::Version::versions>(m_cmbVersion->currentData().toUInt());
    Gui::Python::Lexer::setVersion(ver);

    QStringList types;
    types << QLatin1String("py") << QLatin1String("FCMacro");
    for (auto &wrapper : EditorViewSingleton::instance()->openedByType(types)) {
        auto highlighter = dynamic_cast<Python::SyntaxHighlighter*>(wrapper->editor()->syntaxHighlighter());
        if (highlighter)
            highlighter->rehighlight();
    }

}

#include "moc_DlgPythonSettings.cpp"
