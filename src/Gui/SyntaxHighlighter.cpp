/***************************************************************************
 *   Copyright (c) 2008 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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

#include "SyntaxHighlighter.h"
#include "TextEdit.h"

using namespace Gui;

namespace Gui {
class SyntaxHighlighterP
{
public:
    SyntaxHighlighterP()
    {
        // set default colors
        colorArray[SyntaxHighlighter::Text]                   .setRgb(0, 0, 0);
        colorArray[SyntaxHighlighter::Comment]                .setRgb(0, 170, 0);
        colorArray[SyntaxHighlighter::BlockCommentDoubleQoute].setRgb(160, 160, 164);
        colorArray[SyntaxHighlighter::StringDoubleQoute]      .setRgb(255, 0, 0);
        colorArray[SyntaxHighlighter::Number]                 .setRgb(0, 0, 255);
        colorArray[SyntaxHighlighter::Operator]               .setRgb(130, 120, 134);
        colorArray[SyntaxHighlighter::Keyword]                .setRgb(0, 0, 255);
        colorArray[SyntaxHighlighter::Classname]              .setRgb(255, 170, 0);
        colorArray[SyntaxHighlighter::Defname]                .setRgb(255, 170, 0);
        colorArray[SyntaxHighlighter::Builtin]                .setRgb(210, 43, 216);
        colorArray[SyntaxHighlighter::StringSingleQoute]      .setRgb(98, 124, 61);
        colorArray[SyntaxHighlighter::BlockCommentSingleQoute].setRgb(166, 196, 125);
        colorArray[SyntaxHighlighter::Decorator]              .setRgb(66, 134, 244);
        colorArray[SyntaxHighlighter::Output]                 .setRgb(170, 170, 127);
        colorArray[SyntaxHighlighter::Error]                  .setRgb(255, 0, 0);

        // adding a color should be accompanied by setting its key and
        // translated name in initNames()

        static bool initNamesCalled = false;
        if (!initNamesCalled)
            initNames();
        initNamesCalled = true;
    }

    // static as it only needs to run once
    static void initNames()
    {
        // a lookup string key to lookup table
        keyToIdx[QLatin1String("Text")]               = SyntaxHighlighter::Text;
        keyToIdx[QLatin1String("Comment")]            = SyntaxHighlighter::Comment;
        keyToIdx[QLatin1String("BlockComment")]       = SyntaxHighlighter::BlockComment;
        keyToIdx[QLatin1String("BlockCommentDBlQte")] = SyntaxHighlighter::BlockCommentDoubleQoute;// is aliased of blockComment
        keyToIdx[QLatin1String("Number")]             = SyntaxHighlighter::Number;
        keyToIdx[QLatin1String("String")]             = SyntaxHighlighter::String;
        keyToIdx[QLatin1String("StringDblQte")]       = SyntaxHighlighter::StringDoubleQoute;
        keyToIdx[QLatin1String("Keyword")]            = SyntaxHighlighter::Keyword;
        keyToIdx[QLatin1String("ClassName")]          = SyntaxHighlighter::Classname;
        keyToIdx[QLatin1String("DefineName")]         = SyntaxHighlighter::Defname;
        keyToIdx[QLatin1String("Operator")]           = SyntaxHighlighter::Operator;
        keyToIdx[QLatin1String("Decorator")]          = SyntaxHighlighter::Decorator;
        keyToIdx[QLatin1String("StringSglQte")]       = SyntaxHighlighter::StringSingleQoute;
        keyToIdx[QLatin1String("BlockCommentSglQte")] = SyntaxHighlighter::BlockCommentSingleQoute;
        keyToIdx[QLatin1String("PythonOutput")]       = SyntaxHighlighter::Output;
        keyToIdx[QLatin1String("PythonError")]        = SyntaxHighlighter::Error;
        keyToIdx[QLatin1String("PythonBuiltin")]      = SyntaxHighlighter::Builtin;

        // names to editor settings dialog
        translateNames[QLatin1String("Text")]               = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Text");
        translateNames[QLatin1String("Comment")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Comment");
        translateNames[QLatin1String("BlockComment")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Block comment");
        translateNames[QLatin1String("BlockCommentDBlQte")] = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Block comment (double quoted)");
        translateNames[QLatin1String("Number")]             = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Number");
        translateNames[QLatin1String("String")]             = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "String");
        translateNames[QLatin1String("StringDblQte")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "String (double quoted)");
        translateNames[QLatin1String("Keyword")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Keyword");
        translateNames[QLatin1String("ClassName")]          = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Class name");
        translateNames[QLatin1String("DefineName")]         = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Define name");
        translateNames[QLatin1String("Operator")]           = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Operator");
        translateNames[QLatin1String("Decorator")]          = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Decorator");
        translateNames[QLatin1String("StringSglQte")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "String (single qouted)");
        translateNames[QLatin1String("BlockCommentSglQte")] = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Block comment (single qouted)");
        translateNames[QLatin1String("PythonOutput")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python output");
        translateNames[QLatin1String("PythonError")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python error");
        translateNames[QLatin1String("PythonBuiltin")]      = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python builtin");
        translateNames[QLatin1String("Unknown")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Unknown");

        // aliases, only show the more detailed name in editor settings
        // keys inserted here wont show up in editor settings dialog
        aliased << QLatin1String("String") << QLatin1String("BlockComment");
    }

    // define a c array of pointers to our colors
    // NoColorInvalid last real TColor state +1 set by compiler at compile time
    QColor colorArray[static_cast<int>(SyntaxHighlighter::NoColorInvalid)];
    static QMap<QString, SyntaxHighlighter::TColor> keyToIdx;
    static QMap<QString, const char*> translateNames;
    static QList<QString> aliased;
};
// need to declare these outside of class as they are static
QMap<QString, SyntaxHighlighter::TColor> SyntaxHighlighterP::keyToIdx;
QMap<QString, const char*> SyntaxHighlighterP::translateNames;
QList<QString> SyntaxHighlighterP::aliased;
// -------------------------------------------------------------------------------------------

struct SyntaxHighlighter::ColorDataP
{
    ColorDataP() :
        customTranslateName(0), rgbAsULong(0)
    { }
    QString key;
    const char *customTranslateName;
    unsigned long rgbAsULong;
};


} // namespace Gui


// -------------------------------------------------------------------------------------------

/**
 * Constructs a syntax highlighter.
 */
SyntaxHighlighter::SyntaxHighlighter(QObject* parent)
    : QSyntaxHighlighter(parent)
{
    d = new SyntaxHighlighterP;
}

/** Destroys this object. */
SyntaxHighlighter::~SyntaxHighlighter()
{
    delete d;
}

/** Sets the color \a col to the paragraph type \a type. 
 * This method is provided for convenience to specify the paragraph type
 * by its name.
 */
void SyntaxHighlighter::setColor(const QString& type, const QColor& col)
{
    if (d->keyToIdx.keys().contains(type))
        return; // no such type

    // Rehighlighting is very expensive, thus avoid it if this color is already set
    QColor old = color(type);
    if (!old.isValid())
        return; // no such type
    if (old == col)
        return;

    d->colorArray[d->keyToIdx[type]] = col;

    colorChanged(type, col);
}

QColor SyntaxHighlighter::color(const QString& type)
{
    if (d->keyToIdx.keys().contains(type))
        return d->colorArray[d->keyToIdx[type]];

    return QColor();
}

QMap<QString, SyntaxHighlighter::ColorData> SyntaxHighlighter::allColors()
{
    QMap<QString, ColorData> colors;
    for (const QString key : d->keyToIdx.keys()) {
        if (!d->aliased.contains(key)) {
            ColorData colorInfo(key, d->colorArray[d->keyToIdx[key]]);
            colors[key] = colorInfo;
        }
    }

    return colors;
}

void SyntaxHighlighter::setBatchOfColors(QMap<QString, ColorData> colors)
{
    for (const QString key : colors.keys()) {
        if (d->keyToIdx.keys().contains(key))
            d->colorArray[d->keyToIdx[key]] = colors[key].color();
    }

    rehighlight();
}

void SyntaxHighlighter::loadSettings()
{
    // restore colormap from settings
    ParameterGrp::handle hGrp = WindowParameter::getDefaultParameter()->GetGroup("Editor");

    for (const QString key : d->keyToIdx.keys()) {
        QColor col_d = d->colorArray[d->keyToIdx[key]];
        unsigned long defaultCol = ((col_d.red() << 24) | (col_d.green() << 16) | (col_d.blue() << 8) | 0xFF);
        unsigned long col = hGrp->GetUnsigned(key.toLatin1(), defaultCol);
        d->colorArray[d->keyToIdx[key]]
                    .setRgb((col >> 24) & 0xff, (col >> 16) & 0xff, (col >> 8) & 0xff);
    }

    rehighlight();
}

QColor SyntaxHighlighter::colorByType(SyntaxHighlighter::TColor type)
{
    if (type < TColor::NoColorInvalid)
        return d->colorArray[type];

    return QColor();
}

void SyntaxHighlighter::colorChanged(const QString& type, const QColor& col)
{
    Q_UNUSED(type); 
    Q_UNUSED(col); 
  // rehighlight
    rehighlight();
}

int SyntaxHighlighter::maximumUserState() const
{
    // used in python console so we dont copy python errors and python output
    // when issueing "Copy command"
    return static_cast<int>(Output) -1;
}

// ----------------------------------------------------------------------------------------


SyntaxHighlighter::ColorData::ColorData():
    d(new ColorDataP)
{
}

SyntaxHighlighter::ColorData::ColorData(const QString key, const QColor color):
    d(new ColorDataP)
{
    d->key = key;
    setColor(color);
}

SyntaxHighlighter::ColorData::ColorData(const QString key, unsigned long colorAsLong):
    d(new ColorDataP)
{
    d->key = key;
    d->rgbAsULong = colorAsLong;
}

SyntaxHighlighter::ColorData::ColorData(const SyntaxHighlighter::ColorData &other):
    d(new ColorDataP)
{
    d->key = other.d->key;
    d->customTranslateName = other.d->customTranslateName;
    d->rgbAsULong = other.d->rgbAsULong;
}

SyntaxHighlighter::ColorData::~ColorData()
{
    delete d;
}

SyntaxHighlighter::ColorData
&SyntaxHighlighter::ColorData::operator=(SyntaxHighlighter::ColorData &other)
{
    d->key = other.d->key;
    d->customTranslateName = other.d->customTranslateName;
    d->rgbAsULong = other.d->rgbAsULong;
    return *this;
}

QColor SyntaxHighlighter::ColorData::color() const
{
    QColor col;
    uint8_t red = static_cast<uint8_t>((d->rgbAsULong & 0xFF000000) >> 24);
    uint8_t green = static_cast<uint8_t>((d->rgbAsULong & 0x00FF0000) >> 16);
    uint8_t blue = static_cast<uint8_t>((d->rgbAsULong & 0x0000FF00) >> 8);
    // ignore alpha channel

    col.setRgb(red, green, blue);
    return col;
}

void SyntaxHighlighter::ColorData::setColor(const QColor &color)
{
    // ignore Alpha channel
    d->rgbAsULong = (color.red() << 24) | (color.green() << 16) |
                    (color.blue() << 8);
}

void SyntaxHighlighter::ColorData::setColor(unsigned long color)
{
    d->rgbAsULong = color;
}

const QString SyntaxHighlighter::ColorData::key() const
{
    return d->key;
}

const char* SyntaxHighlighter::ColorData::translateName() const
{
    if (d->customTranslateName && strlen(d->customTranslateName) > 0)
        return d->customTranslateName;

    if (SyntaxHighlighterP::translateNames.keys().contains(d->key))
        return SyntaxHighlighterP::translateNames[d->key];

    return SyntaxHighlighterP::translateNames[QLatin1String("Unknown")];
}

void SyntaxHighlighter::ColorData::setTranslateName(const char *newName)
{
    d->customTranslateName = newName;
}

unsigned long SyntaxHighlighter::ColorData::colorAsULong() const
{
    return d->rgbAsULong;
}
