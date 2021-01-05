/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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
# include <QComboBox>
# include <QFontDatabase>
# include <QHeaderView>
#endif

#include "DlgEditorImp.h"
#include "PrefWidgets.h"
#include <TextEditor/PythonEditor.h>

#include <QAbstractListModel>

using namespace Gui;
using namespace Gui::Dialog;

namespace Gui {
namespace Dialog {

struct DlgSettingsEditorP
{
    ~DlgSettingsEditorP()
    {
        if (pythonSyntax)
            pythonSyntax->deleteLater();
    }

    Gui::PythonSyntaxHighlighter* pythonSyntax;
    DlgSettingsColorModel colormodel;
};

} // namespace Dialog
} // namespace Gui

/* TRANSLATOR Gui::Dialog::DlgSettingsEditorImp */

/**
 *  Constructs a DlgSettingsEditorImp which is a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  true to construct a modal dialog.
 */
DlgSettingsEditorImp::DlgSettingsEditorImp( QWidget* parent )
  : PreferencePage( parent )
  , ui(new Ui_DlgEditorSettings)
{
    this->ui->setupUi(this);

    d = new DlgSettingsEditorP();


    d->pythonSyntax = new PythonSyntaxHighlighter(ui->textEdit1);
    d->pythonSyntax->setDocument(ui->textEdit1->document());

    // get default colors from SyntaxHighlighter
    d->colormodel.colormap = d->pythonSyntax->allColors();

    // some other colors not related SyntaxHighlighter
    QString newKey = QLatin1String("CurrentLineHighlight");
    SyntaxHighlighter::ColorData highLightLine(newKey, QColor(224, 224, 224));
    highLightLine.setTranslateName(QT_TR_NOOP("Current line highlight"));
    d->colormodel.colormap[newKey] = highLightLine;


    // not really sure what these 2 are supposed to do?
    newKey = QLatin1String("Bookmark");
    SyntaxHighlighter::ColorData bookmark(newKey, QColor(Qt::cyan));
    bookmark.setTranslateName(QT_TR_NOOP("Bookmark"));
    d->colormodel.colormap[newKey] = bookmark;

    newKey = QLatin1String("Breakpoint");
    SyntaxHighlighter::ColorData breakpoint(newKey, QColor(Qt::red));
    breakpoint.setTranslateName(QT_TR_NOOP("Breakpoint"));
    d->colormodel.colormap[newKey] = breakpoint;


    // not actually changing anything just notifing that we have more rows
    d->colormodel.setData(d->colormodel.createNewIndex(0,0),
                          QVariant(), Qt::EditRole);

    ui->displayItems->setModel(&d->colormodel);

    QItemSelectionModel *selection = ui->displayItems->selectionModel();
    connect(selection, SIGNAL(currentRowChanged(QModelIndex, QModelIndex)),
            this, SLOT(displayItems_currentRowChanged(QModelIndex,QModelIndex)));
}

/** Destroys the object and frees any allocated resources */
DlgSettingsEditorImp::~DlgSettingsEditorImp()
{
    // no need to delete child widgets, Qt does it all for us
    delete d;
}

/** Searches for the color value corresponding to \e name in current editor
 *   settings ColorMap and assigns it to the color button
 *  @see Gui::ColorButton
 */
void DlgSettingsEditorImp::displayItems_currentRowChanged(const QModelIndex & current, const QModelIndex & previous)
{
    Q_UNUSED(previous)
    QString key = d->colormodel.colormap.keys()[current.row()];
    ui->colorButton->setColor(d->colormodel.colormap[key].color());
}

void DlgSettingsEditorImp::on_displayItems_doubleClicked(const QModelIndex &current)
{
    QString key = d->colormodel.colormap.keys()[current.row()];
    ui->colorButton->setColor(d->colormodel.colormap[key].color());
    ui->colorButton->click();
}

/** Updates the color if a color was changed */
void DlgSettingsEditorImp::on_colorButton_changed()
{
    QModelIndex index = ui->displayItems->currentIndex();
    if (!index.isValid())
        return;

    QColor col = ui->colorButton->color();

    // change color in list
    d->colormodel.setData(index, QVariant(col), Qt::DecorationRole);
}

void DlgSettingsEditorImp::saveSettings()
{
    ui->EnableLineNumber->onSave();
    ui->EnableFolding->onSave();
    ui->EnableIndentMarkers->onSave();
    ui->EnableLineWrap->onSave();
    ui->EnableLongLineMarker->onSave();
    ui->EnableTrimTrailingWhitespaces->onSave();
    ui->EnableAutoIndent->onSave();
    ui->EnableScrollToExceptionLine->onSave();
    ui->EnableHaltDebuggerOnExceptions->onSave();
    ui->EnableCodeAnalyzer->onSave();
    ui->EnableCenterDebugmarker->onSave();
    ui->LongLineWidth->onSave();
    ui->PopupTimeoutTime->onSave();
    ui->tabSize->onSave();
    ui->indentSize->onSave();
    ui->radioTabs->onSave();
    ui->radioSpaces->onSave();

    // Saves the color map
    ParameterGrp::handle hGrp = WindowParameter::getDefaultParameter()->GetGroup("Editor");

    for (const QString &key : d->colormodel.colormap.keys()) {
        hGrp->SetUnsigned(key.toLatin1(), d->colormodel.colormap[key].colorAsULong());
    }

    hGrp->SetInt( "FontSize", ui->fontSize->value() );
    hGrp->SetASCII( "Font", ui->fontFamily->currentText().toLatin1() );
}

void DlgSettingsEditorImp::loadSettings()
{
    ui->EnableLineNumber->onRestore();
    ui->EnableFolding->onRestore();
    ui->EnableIndentMarkers->onRestore();
    ui->EnableLineWrap->onRestore();
    ui->EnableLongLineMarker->onRestore();
    ui->EnableTrimTrailingWhitespaces->onRestore();
    ui->EnableAutoIndent->onRestore();
    ui->EnableScrollToExceptionLine->onRestore();
    ui->EnableHaltDebuggerOnExceptions->onRestore();
    ui->EnableCodeAnalyzer->onRestore();
    ui->EnableCodeAnalyzer->onRestore();
    ui->EnableCenterDebugmarker->onRestore();
    ui->LongLineWidth->onRestore();
    ui->PopupTimeoutTime->onRestore();
    ui->tabSize->onRestore();
    ui->indentSize->onRestore();
    ui->radioTabs->onRestore();
    ui->radioSpaces->onRestore();

    ui->textEdit1->setPlainText(QString::fromLatin1(
        "# Short Python sample\n"
        "import sys\n"
        "def foo(begin, end):\n"
        "	i=begin\n"
        "	while (i<end):\n"
        "		print i\n"
        "		i=i+1\n"
        "		print \"Text\"\n"
        "\n"
        "foo(0, 20))\n"));

    // Restores the color map
    ParameterGrp::handle hGrp = WindowParameter::getDefaultParameter()->GetGroup("Editor");
    for (const QString &key : d->colormodel.colormap.keys()) {
        unsigned long col = hGrp->GetUnsigned(key.toLatin1(), d->colormodel.colormap[key].colorAsULong());
        d->colormodel.colormap[key].setColor(col);
    }

    ui->displayItems->update();

    // inform (current forms sampleview) syntax highlighter of these changes
    d->pythonSyntax->setBatchOfColors(d->colormodel.colormap);


    // fill up font styles
    //
    ui->fontSize->setValue(10);
    ui->fontSize->setValue(static_cast<int>(hGrp->GetInt("FontSize", ui->fontSize->value())));

    QByteArray fontName = this->font().family().toLatin1();

    QFontDatabase fdb;
    QStringList familyNames = fdb.families( QFontDatabase::Any );
    ui->fontFamily->addItems(familyNames);
    int index = familyNames.indexOf(QString::fromLatin1(hGrp->GetASCII("Font", fontName).c_str()));
    if (index < 0) index = 0;
    ui->fontFamily->setCurrentIndex(index);
    on_fontFamily_activated(ui->fontFamily->currentText());

    ui->displayItems->setCurrentIndex(d->colormodel.createNewIndex(0,0));
}

/**
 * Sets the strings of the subwidgets using the current language.
 */
void DlgSettingsEditorImp::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        for (int i = 0; i < d->colormodel.colormap.size(); ++i) {
            QModelIndex idx = d->colormodel.createNewIndex(i, 0);
            d->colormodel.setData(idx, QVariant(), Qt::EditRole); // no data is acutally changed, just mark as a update internally
                                                                  // to model
        }

        ui->retranslateUi(this);
    } else {
        QWidget::changeEvent(e);
    }
}

void DlgSettingsEditorImp::on_fontFamily_activated(const QString& fontFam)
{
    int fontSize = ui->fontSize->value();
    QFont ft(fontFam, fontSize);
    ui->textEdit1->setFont(ft);
}

void DlgSettingsEditorImp::on_fontSize_valueChanged(const QString&)
{
    on_fontFamily_activated(ui->fontFamily->currentText());
}

// ---------------------------------------------------------------------------------------

// model for color selections
DlgSettingsColorModel::DlgSettingsColorModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

DlgSettingsColorModel::~DlgSettingsColorModel()
{
}

QVariant DlgSettingsColorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() > colormap.size() -1)
        return QVariant();

    QString key = colormap.keys()[index.row()];

    if (role == Qt::DisplayRole) {
        const char *str = colormap[key].translateName();
        QString trStr(DlgSettingsEditorImp::tr(str));
        return QVariant(trStr);

    } else if (role == Qt::BackgroundRole) {
        QBrush brush(colormap[key].color());
        return brush;

    }
    return QVariant();
}

int DlgSettingsColorModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return colormap.keys().size() -1;
}

bool DlgSettingsColorModel::setData(const QModelIndex &index,
                                    const QVariant &value, int role)
{
    if (!index.isValid() || index.row() > colormap.size() -1)
        return false;

    QString key = colormap.keys()[index.row()];
    if  (role == Qt::EditRole) {
        if (!value.toString().isEmpty())
            colormap[key].setTranslateName(value.toString().toLatin1());
        Q_EMIT dataChanged(index, index);
        return true;

    } else if (role == Qt::DecorationRole) {
        QColor col = value.value<QColor>();
        colormap[key].setColor(col);
        Q_EMIT dataChanged(index, index);
        return true;
    }
    return false;
}

void DlgSettingsColorModel::signalRowsInserted()
{
    beginInsertRows(QModelIndex(), 0, rowCount()-1);

    endInsertRows();
}

// make public
QModelIndex DlgSettingsColorModel::createNewIndex(int row, int col)
{
    return createIndex(row, col);
}


#include "moc_DlgEditorImp.cpp"
