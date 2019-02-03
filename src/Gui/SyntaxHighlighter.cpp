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

        // strings
        colorArray[SyntaxHighlighter::StringBlockDoubleQoute] .setRgb(126, 166, 105);
        colorArray[SyntaxHighlighter::StringDoubleQoute]      .setRgb(48, 104, 41);
        colorArray[SyntaxHighlighter::StringSingleQoute]      .setRgb(98, 124, 61);
        colorArray[SyntaxHighlighter::StringBlockSingleQoute] .setRgb(166, 196, 125);

        // numbers
        colorArray[SyntaxHighlighter::Number]                 .setRgb(0, 0, 255);
        colorArray[SyntaxHighlighter::NumberHex]              .setRgb(111, 46, 232);
        colorArray[SyntaxHighlighter::NumberBinary]           .setRgb(66, 134, 244);
        colorArray[SyntaxHighlighter::NumberFloat]            .setRgb(0, 70, 255);
        colorArray[SyntaxHighlighter::NumberOctal]            .setRgb(168, 88, 3);

        // keywords
        colorArray[SyntaxHighlighter::Keyword]                .setRgb(0, 0, 255);
        colorArray[SyntaxHighlighter::KeywordClass]           .setRgb(229, 48, 20);
        colorArray[SyntaxHighlighter::KeywordDef]             .setRgb(229, 38, 9);

        // operators
        colorArray[SyntaxHighlighter::Operator]               .setRgb(130, 120, 134);

        // identifiers
        colorArray[SyntaxHighlighter::IdentifierClass]        .setRgb(1, 122, 153);
        colorArray[SyntaxHighlighter::IdentifierDefined]      .setRgb(0, 61, 22);
        colorArray[SyntaxHighlighter::IdentifierFunction]     .setRgb(16, 145, 145);
        colorArray[SyntaxHighlighter::IdentifierMethod]       .setRgb(17, 94, 145);
        colorArray[SyntaxHighlighter::IdentifierModule]       .setRgb(0, 56, 71);
        colorArray[SyntaxHighlighter::IdentifierSuperMethod]  .setRgb(50, 98, 130);
        colorArray[SyntaxHighlighter::IdentifierUnknown]      .setRgb(58, 9, 0);
        colorArray[SyntaxHighlighter::IdentifierBuiltin]      .setRgb(210, 43, 216);
        colorArray[SyntaxHighlighter::IdentifierDecorator]    .setRgb(66, 134, 244);

        // delmiters
        colorArray[SyntaxHighlighter::Delimiter]              .setRgb(100, 100, 115);

        // syntax error
        colorArray[SyntaxHighlighter::SyntaxError]            .setRgb(255, 0, 0);


        // in-out
        colorArray[SyntaxHighlighter::PythonConsoleOutput]    .setRgb(170, 170, 127);
        colorArray[SyntaxHighlighter::PythonConsoleError]     .setRgb(255, 0, 0);

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

        // strings
        keyToIdx[QLatin1String("StringBlockDBlQte")]  = SyntaxHighlighter::StringBlockDoubleQoute;
        keyToIdx[QLatin1String("StringBlockSglQte")]  = SyntaxHighlighter::StringBlockSingleQoute;
        keyToIdx[QLatin1String("StringDblQte")]       = SyntaxHighlighter::StringDoubleQoute;
        keyToIdx[QLatin1String("StringSglQte")]       = SyntaxHighlighter::StringSingleQoute;
        // is aliased of names to support old settings file
        keyToIdx[QLatin1String("Block comment")]      = SyntaxHighlighter::StringBlockDoubleQoute;
        keyToIdx[QLatin1String("String")]             = SyntaxHighlighter::StringDoubleQoute;

        // Numbers
        keyToIdx[QLatin1String("Number")]             = SyntaxHighlighter::Number;
        keyToIdx[QLatin1String("NumberHex")]          = SyntaxHighlighter::NumberHex;
        keyToIdx[QLatin1String("NumberBinary")]       = SyntaxHighlighter::NumberBinary;
        keyToIdx[QLatin1String("NumberFloat")]        = SyntaxHighlighter::NumberFloat;
        keyToIdx[QLatin1String("NumberOctal")]        = SyntaxHighlighter::NumberOctal;

        // keywords
        keyToIdx[QLatin1String("Keyword")]            = SyntaxHighlighter::Keyword;
        keyToIdx[QLatin1String("KeywordClass")]       = SyntaxHighlighter::KeywordClass;
        keyToIdx[QLatin1String("KeywordDef")]         = SyntaxHighlighter::KeywordDef;
        // aliased to support old settings files
        keyToIdx[QLatin1String("Class name")]         = SyntaxHighlighter::KeywordClass;
        keyToIdx[QLatin1String("Define name")]        = SyntaxHighlighter::KeywordDef;

        // operator
        keyToIdx[QLatin1String("Operator")]           = SyntaxHighlighter::Operator;

        // identifiers
        keyToIdx[QLatin1String("PythonUnknown")]      = SyntaxHighlighter::IdentifierUnknown;
        keyToIdx[QLatin1String("PythonDefined")]      = SyntaxHighlighter::IdentifierDefined;
        keyToIdx[QLatin1String("PythonModule")]       = SyntaxHighlighter::IdentifierModule;
        keyToIdx[QLatin1String("PythonFunction")]     = SyntaxHighlighter::IdentifierFunction;
        keyToIdx[QLatin1String("PythonMethod")]       = SyntaxHighlighter::IdentifierMethod;
        keyToIdx[QLatin1String("PythonClass")]        = SyntaxHighlighter::IdentifierClass;
        keyToIdx[QLatin1String("PythonSuperMethod")]  = SyntaxHighlighter::IdentifierSuperMethod;
        keyToIdx[QLatin1String("PythonBuiltin")]      = SyntaxHighlighter::IdentifierBuiltin;
        keyToIdx[QLatin1String("PythonDecorator")]    = SyntaxHighlighter::IdentifierDecorator;

        // delimiters
        keyToIdx[QLatin1String("Delimiter")]          = SyntaxHighlighter::Delimiter;

        // special
        keyToIdx[QLatin1String("SyntaxError")]        = SyntaxHighlighter::SyntaxError;

        // in-out
        keyToIdx[QLatin1String("PythonOutput")]       = SyntaxHighlighter::PythonConsoleOutput;
        keyToIdx[QLatin1String("PythonError")]        = SyntaxHighlighter::PythonConsoleError;
        //  aliased to support old settings file
        keyToIdx[QLatin1String("Python output")]      = SyntaxHighlighter::PythonConsoleOutput;
        keyToIdx[QLatin1String("Python error")]       = SyntaxHighlighter::PythonConsoleError;




        //-------------------------------
        // from here on is translation strings

        // names to editor settings dialog
        translateNames[QLatin1String("Text")]               = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Text");
        translateNames[QLatin1String("Comment")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Comment");

        // strings
        translateNames[QLatin1String("StringBlockDBlQte")]  = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Block string(double quoted)");
        translateNames[QLatin1String("StringBlockSglQte")]  = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Block string (single qouted)");
        translateNames[QLatin1String("StringSglQte")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "String (single qouted)");
        translateNames[QLatin1String("StringDblQte")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "String (double quoted)");
        // aliased old ones
        translateNames[QLatin1String("String")]             = translateNames[QLatin1String("StringDblQte")];
        translateNames[QLatin1String("Block comment")]      = translateNames[QLatin1String("StringBlockDBlQte")];

        // numbers
        translateNames[QLatin1String("Number")]             = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Number");
        translateNames[QLatin1String("HexNumber")]          = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Hex. number");
        translateNames[QLatin1String("BinaryNumber")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Binary number");
        translateNames[QLatin1String("FloatNumber")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Fractional number");
        translateNames[QLatin1String("OctalNumber")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Octal number");

        // keywords
        translateNames[QLatin1String("Keyword")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Keyword");
        translateNames[QLatin1String("KeywordClass")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "keyword 'class'");
        translateNames[QLatin1String("KeywordDef")]         = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "keyword 'def'");
        // aliased
        translateNames[QLatin1String("Class name")]          = translateNames[QLatin1String("KeywordClass")];
        translateNames[QLatin1String("Define name")]         = translateNames[QLatin1String("KeywordDef")];

        // operator
        translateNames[QLatin1String("Operator")]           = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Operator");

        // identifiers
        translateNames[QLatin1String("PythonUnknown")]      = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python unknown");
        translateNames[QLatin1String("PythonDefined")]      = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python defined");
        translateNames[QLatin1String("PythonModule")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python module");
        translateNames[QLatin1String("PythonFunction")]     = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python function");
        translateNames[QLatin1String("PythonMethod")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python method");
        translateNames[QLatin1String("PythonClass")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python class");
        translateNames[QLatin1String("PythonSuperMethod")]  = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python super (__method__)");
        translateNames[QLatin1String("PythonBuiltin")]      = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python builtin");
        translateNames[QLatin1String("PythonDecorator")]    = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python decorator (@name)");

        // delimiters
        translateNames[QLatin1String("Delimiter")]                = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python delimiter");

        // special
        translateNames[QLatin1String("SyntaxError")]              = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Syntax error");

        // in-out
        translateNames[QLatin1String("PythonOutput")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python output");
        translateNames[QLatin1String("PythonError")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python error");
        // aliased
        translateNames[QLatin1String("Python output")]      = translateNames[QLatin1String("PythonOutput")];
        translateNames[QLatin1String("Python error")]       = translateNames[QLatin1String("PythonError")];

        // default translation.....
        translateNames[QLatin1String("Unknown")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Unknown");


        // aliases, only show the more detailed name in editor settings
        // keys inserted here won't show up in editor settings dialog
        aliased << QLatin1String("String")     << QLatin1String("BlockComment")
                << QLatin1String("Class name") << QLatin1String("Define name");
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

QColor SyntaxHighlighter::color(const QString& type) const
{
    if (d->keyToIdx.keys().contains(type))
        return d->colorArray[d->keyToIdx[type]];

    return QColor();
}

QMap<QString, SyntaxHighlighter::ColorData> SyntaxHighlighter::allColors() const
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

QColor SyntaxHighlighter::colorByType(SyntaxHighlighter::TColor type) const
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
    // used in python console so we don't copy python errors and python output
    // when issuing "Copy command"
    // also in PythonSyntaxHighLighter to set state on textBlock from previous block
    return static_cast<int>(PythonConsoleOutput) -1;
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

const SyntaxHighlighter::ColorData
&SyntaxHighlighter::ColorData::operator=(const SyntaxHighlighter::ColorData &other)
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
