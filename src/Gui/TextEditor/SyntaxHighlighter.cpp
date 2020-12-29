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
#include "TextEditBlockData.h"
#include <QTimer>
#include <QTextDocument>
#include <App/Application.h>

#ifdef BUILD_KF5SYNTAX
# include <AbstractHighlighter>
# include <Theme>
# include <Definition>
# include <FoldingRegion>
# include <Format>
# include <State>
# include <Repository>
#endif

using namespace Gui;

namespace Gui {

#ifdef BUILD_KF5SYNTAX
struct StateContainer : public QTextBlockUserData
{
    explicit StateContainer(KSyntaxHighlighting::State state) :
        QTextBlockUserData(), vlu(state) {}
    ~StateContainer();
    KSyntaxHighlighting::State vlu;
};
StateContainer::~StateContainer() {} // outside to squelsh semantic warning, virtual base

#endif

// --------------------------------------------------------------------

class SyntaxHighlighterP : public KSyntaxHighlighting::AbstractHighlighter
{
public:
    explicit SyntaxHighlighterP(SyntaxHighlighter *owner);
    ~SyntaxHighlighterP();

    // static as it only needs to run once
    static void initNames();

    // define a c array of pointers to our colors
    // NoColorInvalid last real TColor state +1 set by compiler at compile time
    QColor colorArray[static_cast<int>(SyntaxHighlighter::NoColorInvalid)];
    static QMap<QString, SyntaxHighlighter::TColor> keyToIdx;
    static QMap<QString, const char*> translateNames;
    static QList<QString> aliased;
    QString filePath;
    QTimer rehighlightTmr;

#ifdef BUILD_KF5SYNTAX
    KSyntaxHighlighting::Repository repository;
#endif

#ifdef BUILD_KF5SYNTAX
    void highlightBlock(const QString &text);

    typedef QVector<KSyntaxHighlighting::FoldingRegion> FoldingRegions;

    static KSyntaxHighlighting::FoldingRegion foldingRegion(const QTextBlock &startBlock);
    FoldingRegions foldingRegions;


protected:
    void applyFormat(int offset, int length, const KSyntaxHighlighting::Format &format) override;
    void applyFolding(int offset, int length, KSyntaxHighlighting::FoldingRegion region) override;
#endif

private:
    void foldingToTextData(TextEditBlockData *data);
    bool compareFolding(const TextEditBlockData *data, const FoldingRegions foldings);
    TextEditBlockData::FoldingState convertType(KSyntaxHighlighting::FoldingRegion &region) const;
    SyntaxHighlighter *owner;
};
// need to declare these outside of class as they are static
QMap<QString, SyntaxHighlighter::TColor> SyntaxHighlighterP::keyToIdx;
QMap<QString, const char*> SyntaxHighlighterP::translateNames;
QList<QString> SyntaxHighlighterP::aliased;

SyntaxHighlighterP::SyntaxHighlighterP(SyntaxHighlighter *owner)
    : KSyntaxHighlighting::AbstractHighlighter()
    , owner(owner)
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
    colorArray[SyntaxHighlighter::IdentifierDefined]      .setRgb(50, 100, 80);
    colorArray[SyntaxHighlighter::IdentifierDefUnknown]   .setRgb(0, 104, 37);
    colorArray[SyntaxHighlighter::IdentifierFunction]     .setRgb(16, 145, 145);
    colorArray[SyntaxHighlighter::IdentifierMethod]       .setRgb(17, 94, 145);
    colorArray[SyntaxHighlighter::IdentifierModule]       .setRgb(0, 56, 71);
    colorArray[SyntaxHighlighter::IdentifierSuperMethod]  .setRgb(50, 98, 130);
    colorArray[SyntaxHighlighter::IdentifierUnknown]      .setRgb(58, 9, 0);
    colorArray[SyntaxHighlighter::IdentifierBuiltin]      .setRgb(210, 43, 216);
    colorArray[SyntaxHighlighter::IdentifierDecorator]    .setRgb(66, 134, 244);
    colorArray[SyntaxHighlighter::IdentifierSelf]         .setRgb(1, 163, 154);

    // delmiters
    colorArray[SyntaxHighlighter::Delimiter]              .setRgb(100, 100, 115);

    // syntax error
    colorArray[SyntaxHighlighter::SyntaxError]            .setRgb(255, 0, 0);


    // in-out
    colorArray[SyntaxHighlighter::PythonConsoleOutput]    .setRgb(170, 170, 127);
    colorArray[SyntaxHighlighter::PythonConsoleError]     .setRgb(255, 0, 0);

    // current line-highlight
    colorArray[SyntaxHighlighter::HighlightCurrentLine]   .setRgb(224, 224, 224);

    // metadata
    colorArray[SyntaxHighlighter::MetaData]               .setRgb(165, 224, 4);

    // adding a color should be accompanied by setting its key and
    // translated name in initNames()

    static bool initNamesCalled = false;
    if (!initNamesCalled) // only call the first time a object is created (static)
        initNames();
    initNamesCalled = true;


#ifdef BUILD_KF5SYNTAX
        auto theme = repository.defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
        setTheme(theme);
#endif
}

SyntaxHighlighterP::~SyntaxHighlighterP()
{
}

void SyntaxHighlighterP::initNames()
{
    // a lookup string key to lookup table
    keyToIdx[QLatin1String("color_Text")]               = SyntaxHighlighter::Text;
    keyToIdx[QLatin1String("color_Comment")]            = SyntaxHighlighter::Comment;

    // strings
    keyToIdx[QLatin1String("color_StringBlockDBlQte")]  = SyntaxHighlighter::StringBlockDoubleQoute;
    keyToIdx[QLatin1String("color_StringBlockSglQte")]  = SyntaxHighlighter::StringBlockSingleQoute;
    keyToIdx[QLatin1String("color_StringDblQte")]       = SyntaxHighlighter::StringDoubleQoute;
    keyToIdx[QLatin1String("color_StringSglQte")]       = SyntaxHighlighter::StringSingleQoute;
    // is aliased of names to support old settings file
    keyToIdx[QLatin1String("color_Block comment")]      = SyntaxHighlighter::StringBlockDoubleQoute;
    keyToIdx[QLatin1String("color_String")]             = SyntaxHighlighter::StringDoubleQoute;

    // Numbers
    keyToIdx[QLatin1String("color_Number")]             = SyntaxHighlighter::Number;
    keyToIdx[QLatin1String("color_NumberHex")]          = SyntaxHighlighter::NumberHex;
    keyToIdx[QLatin1String("color_NumberBinary")]       = SyntaxHighlighter::NumberBinary;
    keyToIdx[QLatin1String("color_NumberFloat")]        = SyntaxHighlighter::NumberFloat;
    keyToIdx[QLatin1String("color_NumberOctal")]        = SyntaxHighlighter::NumberOctal;

    // keywords
    keyToIdx[QLatin1String("color_Keyword")]            = SyntaxHighlighter::Keyword;
    keyToIdx[QLatin1String("color_KeywordClass")]       = SyntaxHighlighter::KeywordClass;
    keyToIdx[QLatin1String("color_KeywordDef")]         = SyntaxHighlighter::KeywordDef;
    // aliased to support old settings files
    keyToIdx[QLatin1String("color_Class name")]         = SyntaxHighlighter::KeywordClass;
    keyToIdx[QLatin1String("color_Define name")]        = SyntaxHighlighter::KeywordDef;

    // operator
    keyToIdx[QLatin1String("color_Operator")]           = SyntaxHighlighter::Operator;

    // identifiers
    keyToIdx[QLatin1String("color_PythonUnknown")]      = SyntaxHighlighter::IdentifierUnknown;
    keyToIdx[QLatin1String("color_PythonDefined")]      = SyntaxHighlighter::IdentifierDefined;
    keyToIdx[QLatin1String("color_PythonDefUnknown")]   = SyntaxHighlighter::IdentifierDefUnknown;
    keyToIdx[QLatin1String("color_PythonModule")]       = SyntaxHighlighter::IdentifierModule;
    keyToIdx[QLatin1String("color_PythonFunction")]     = SyntaxHighlighter::IdentifierFunction;
    keyToIdx[QLatin1String("color_PythonMethod")]       = SyntaxHighlighter::IdentifierMethod;
    keyToIdx[QLatin1String("color_PythonClass")]        = SyntaxHighlighter::IdentifierClass;
    keyToIdx[QLatin1String("color_PythonSuperMethod")]  = SyntaxHighlighter::IdentifierSuperMethod;
    keyToIdx[QLatin1String("color_PythonBuiltin")]      = SyntaxHighlighter::IdentifierBuiltin;
    keyToIdx[QLatin1String("color_PythonDecorator")]    = SyntaxHighlighter::IdentifierDecorator;
    keyToIdx[QLatin1String("color_PythonSelf")]         = SyntaxHighlighter::IdentifierSelf;

    // delimiters
    keyToIdx[QLatin1String("color_Delimiter")]          = SyntaxHighlighter::Delimiter;

    // special
    keyToIdx[QLatin1String("color_SyntaxError")]        = SyntaxHighlighter::SyntaxError;

    // in-out
    keyToIdx[QLatin1String("color_PythonOutput")]       = SyntaxHighlighter::PythonConsoleOutput;
    keyToIdx[QLatin1String("color_PythonError")]        = SyntaxHighlighter::PythonConsoleError;
    //  aliased to support old settings file
    keyToIdx[QLatin1String("color_Python output")]      = SyntaxHighlighter::PythonConsoleOutput;
    keyToIdx[QLatin1String("color_Python error")]       = SyntaxHighlighter::PythonConsoleError;

    keyToIdx[QLatin1String("color_Currentline")]        = SyntaxHighlighter::HighlightCurrentLine;

    // typehints
    keyToIdx[QLatin1String("color_MetaData")]           = SyntaxHighlighter::MetaData;


    //-------------------------------
    // from here on is translation strings

    // names to editor settings dialog
    translateNames[QLatin1String("color_Text")]               = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Text");
    translateNames[QLatin1String("color_Comment")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Comment");

    // strings
    translateNames[QLatin1String("color_StringBlockDBlQte")]  = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Block string(double quoted)");
    translateNames[QLatin1String("color_StringBlockSglQte")]  = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Block string (single qouted)");
    translateNames[QLatin1String("color_StringSglQte")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "String (single qouted)");
    translateNames[QLatin1String("color_StringDblQte")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "String (double quoted)");
    // aliased old ones
    translateNames[QLatin1String("color_String")]             = translateNames[QLatin1String("StringDblQte")];
    translateNames[QLatin1String("color_Block comment")]      = translateNames[QLatin1String("StringBlockDBlQte")];

    // numbers
    translateNames[QLatin1String("color_Number")]             = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Number");
    translateNames[QLatin1String("color_HexNumber")]          = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Hex. number");
    translateNames[QLatin1String("color_BinaryNumber")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Binary number");
    translateNames[QLatin1String("color_FloatNumber")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Fractional number");
    translateNames[QLatin1String("color_OctalNumber")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Octal number");

    // keywords
    translateNames[QLatin1String("color_Keyword")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Keyword");
    translateNames[QLatin1String("color_KeywordClass")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "keyword 'class'");
    translateNames[QLatin1String("color_KeywordDef")]         = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "keyword 'def'");
    // aliased
    translateNames[QLatin1String("color_Class name")]          = translateNames[QLatin1String("color_KeywordClass")];
    translateNames[QLatin1String("color_Define name")]         = translateNames[QLatin1String("color_KeywordDef")];

    // operator
    translateNames[QLatin1String("color_Operator")]           = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Operator");

    // identifiers
    translateNames[QLatin1String("color_PythonUnknown")]      = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python unknown");
    translateNames[QLatin1String("color_PythonDefined")]      = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python defined");
    translateNames[QLatin1String("color_PythonDefUnknown")]   = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python unbound");
    translateNames[QLatin1String("color_PythonModule")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python module");
    translateNames[QLatin1String("color_PythonFunction")]     = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python function");
    translateNames[QLatin1String("color_PythonMethod")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python method");
    translateNames[QLatin1String("color_PythonClass")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python class");
    translateNames[QLatin1String("color_PythonSuperMethod")]  = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python super (__method__)");
    translateNames[QLatin1String("color_PythonBuiltin")]      = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python builtin");
    translateNames[QLatin1String("color_PythonDecorator")]    = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python decorator (@name)");
    translateNames[QLatin1String("color_PythonSelf")]    = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python self");

    // delimiters
    translateNames[QLatin1String("color_Delimiter")]          = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python delimiter");

    // special
    translateNames[QLatin1String("color_SyntaxError")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Syntax error");

    // in-out
    translateNames[QLatin1String("color_PythonOutput")]       = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python output");
    translateNames[QLatin1String("color_PythonError")]        = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Python error");
    // aliased
    translateNames[QLatin1String("color_Python output")]      = translateNames[QLatin1String("color_PythonOutput")];
    translateNames[QLatin1String("color_Python error")]       = translateNames[QLatin1String("color_PythonError")];

    // current line color
    translateNames[QLatin1String("color_Currentline")]  = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Current line");

    // default translation.....
    translateNames[QLatin1String("color_Unknown")]            = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Unknown");
    // typehints
    translateNames[QLatin1String("color_MetaData")]           = QT_TRANSLATE_NOOP("DlgSettingsEditorImp", "Typehints");

    // aliases, only show the more detailed name in editor settings
    // keys inserted here won't show up in editor settings dialog
    aliased << QLatin1String("color_String")     << QLatin1String("color_BlockComment")
            << QLatin1String("color_Class name") << QLatin1String("color_Define name");
}

#ifdef BUILD_KF5SYNTAX
void Gui::SyntaxHighlighterP::highlightBlock(const QString &text)
{
    KSyntaxHighlighting::State state;
    if (owner->currentBlock().position() > 0) {
        const auto prevBlock = owner->currentBlock().previous();
        const auto prevData = dynamic_cast<TextEditBlockData *>(prevBlock.userData());
        if (prevData) {
            const auto prevState = dynamic_cast<StateContainer*>(prevData->customData());
            state = prevState->vlu;
        }
    }
    foldingRegions.clear();
    state = highlightLine(text, state);

    auto curBlock = owner->currentBlock();
    auto data = dynamic_cast<TextEditBlockData *>(curBlock.userData());
    StateContainer *stateData = nullptr;
    if (!data) { // first time we highlight this
        data = new TextEditBlockData(curBlock);
        stateData = new StateContainer(state);
        data->setCustomData(stateData);
        foldingToTextData(data);
        owner->setCurrentBlockUserData(data);
        return;
    }

    stateData = dynamic_cast<StateContainer*>(data->customData());
    if (!stateData)
        stateData = new StateContainer(state);
    else if (stateData->vlu == state && compareFolding(data, foldingRegions)) // we ended up in the same state, so we are done here
        return;

    stateData->vlu = state;
    for(auto foldRegion : foldingRegions)
        data->addFoldingInfo(foldRegion.id(), convertType(foldRegion));

//    const auto nextBlock = owner->currentBlock().next();
//    if (nextBlock.isValid())
//        QMetaObject::invokeMethod(this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG(QTextBlock, nextBlock));
}

void SyntaxHighlighterP::applyFormat(int offset, int length, const KSyntaxHighlighting::Format &format)
{
    if (length == 0)
        return;

    QTextCharFormat tf;
    // always set the foreground color to avoid palette issues
    tf.setForeground(format.textColor(theme()));

    if (format.hasBackgroundColor(theme()))
        tf.setBackground(format.backgroundColor(theme()));
    if (format.isBold(theme()))
        tf.setFontWeight(QFont::Bold);
    if (format.isItalic(theme()))
        tf.setFontItalic(true);
    if (format.isUnderline(theme()))
        tf.setFontUnderline(true);
    if (format.isStrikeThrough(theme()))
        tf.setFontStrikeOut(true);

    owner->setFormat(offset, length, tf);
}

void Gui::SyntaxHighlighterP::applyFolding(int offset, int length, KSyntaxHighlighting::FoldingRegion region)
{
    Q_UNUSED(offset)
    Q_UNUSED(length)

    if (region.type() == KSyntaxHighlighting::FoldingRegion::Begin)
        foldingRegions.push_back(region);

    else if (region.type() == KSyntaxHighlighting::FoldingRegion::End) {
        for (int i = foldingRegions.size() - 1; i >= 0; --i) {
            if (foldingRegions.at(i).id() != region.id() || foldingRegions.at(i).type() != KSyntaxHighlighting::FoldingRegion::Begin)
                continue;
            foldingRegions.remove(i);
            return;
        }
        foldingRegions.push_back(region);
    }
}

void SyntaxHighlighterP::foldingToTextData(TextEditBlockData *data)
{
    for (auto foldRegion : foldingRegions) {
        auto type = convertType(foldRegion);
        data->addFoldingInfo(foldRegion.id(), type);
    }
}

bool SyntaxHighlighterP::compareFolding(const TextEditBlockData *data, const Gui::SyntaxHighlighterP::FoldingRegions foldings)
{
    QVector<uint> ids;
    for(auto id : foldings)
        ids.push_back(id.id());

    for(auto id : data->foldingIDs())
        if (!ids.contains(id))
            return false;

    return true;
}

TextEditBlockData::FoldingState SyntaxHighlighterP::convertType(KSyntaxHighlighting::FoldingRegion &region) const
{
    switch (region.type()) {
    case KSyntaxHighlighting::FoldingRegion::End:
        return TextEditBlockData::End;
    case KSyntaxHighlighting::FoldingRegion::Begin:
        return TextEditBlockData::Begin;
    default:
        return TextEditBlockData::Unchanged;
    }
}
#endif

// -------------------------------------------------------------------------------------------

struct SyntaxHighlighter::ColorDataP
{
    ColorDataP() :
        customTranslateName(nullptr), rgbAsULong(0)
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
    d = new SyntaxHighlighterP(this);
    d->rehighlightTmr.setInterval(50);
    d->rehighlightTmr.setSingleShot(true);
    // FIXME, diable for debug, should be activated again when scanner works
    //connect(&d->rehighlightTmr, SIGNAL(timeout()), this, SLOT(rehighlight()));
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
    for (const QString &key : d->keyToIdx.keys()) {
        if (!d->aliased.contains(key)) {
            ColorData colorInfo(key, d->colorArray[d->keyToIdx[key]]);
            colors[key] = colorInfo;
        }
    }

    return colors;
}

void SyntaxHighlighter::setBatchOfColors(QMap<QString, ColorData> colors)
{
    for (const QString &key : colors.keys()) {
        if (d->keyToIdx.keys().contains(key))
            d->colorArray[d->keyToIdx[key]] = colors[key].color();
    }

    d->rehighlightTmr.start();
}

void SyntaxHighlighter::loadSettings()
{
    // restore colormap from settings
    auto paramGrp = App::GetApplication().GetUserParameter().GetGroup("BaseApp")->GetGroup("Preferences");
    auto hGrp = paramGrp->GetGroup("Editor");

    for (const QString &key : d->keyToIdx.keys()) {
        QColor col_d = d->colorArray[d->keyToIdx[key]];
        unsigned long defaultCol = ((col_d.red() << 24) | (col_d.green() << 16) | (col_d.blue() << 8) | 0xFF);
        unsigned long col = hGrp->GetUnsigned(key.toLatin1(), defaultCol);
        d->colorArray[d->keyToIdx[key]]
                .setRgb((col >> 24) & 0xff, (col >> 16) & 0xff, (col >> 8) & 0xff);
    }

    d->rehighlightTmr.start();
}

void SyntaxHighlighter::setFilepath(QString file)
{
    d->filePath = file;
#ifdef BUILD_KF5SYNTAX
        const auto def = d->repository.definitionForFileName(file);
        if (def.name() != d->definition().name()) {
            d->setDefinition(def);
            rehighlight();
        }
#endif
}

QString SyntaxHighlighter::filePath() const
{
    return d->filePath;
}

bool SyntaxHighlighter::setSyntax(const QString &defName)
{
#ifdef BUILD_KF5SYNTAX
    const auto def = d->repository.definitionForName(defName);
    if (def.isValid() && def.name() != d->definition().name()) {
        d->setDefinition(def);
        rehighlight();
        return true;
    }
#endif
    return false;
}

QString SyntaxHighlighter::syntax() const
{
#ifdef BUILD_KF5SYNTAX
    return d->definition().name();
#else
    return QString();
#endif
}

QMap<QString, QString> SyntaxHighlighter::syntaxes()
{
    QMap<QString, QString> available;

#ifdef BUILD_KF5SYNTAX
    QString currentGroup;
    for (const auto &def : d->repository.definitions()) {
        if (def.isHidden())
            continue;
        if (currentGroup != def.section())
            currentGroup = def.translatedSection();

        available[def.name()] = currentGroup;
    }
#endif
    return available;
}

QColor SyntaxHighlighter::colorByType(SyntaxHighlighter::TColor type) const
{
    if (type < TColor::NoColorInvalid)
        return d->colorArray[type];

    return QColor();
}

void SyntaxHighlighter::updateFormat(QTextBlock block, int startPos, int length, QTextCharFormat format) const
{
    QTextDocument *doc = document();
    if (doc == block.document() && block.isValid()) {
        QTextLayout *layout = block.layout();
        if (layout) {
            QVector<QTextLayout::FormatRange> ranges = layout->formats();
            for(QTextLayout::FormatRange &range : ranges) {
                if (range.start == startPos &&
                        range.length == length)
                {
                    range.format = format;
                    layout->setFormats(ranges);
                    doc->markContentsDirty(block.position() + startPos, length);
                    return;
                }
            }
        }
    }
}

void SyntaxHighlighter::addFormat(QTextBlock block, int startPos, int length, QTextCharFormat format) const
{
    QTextDocument *doc = document();
    if (doc == block.document() && block.isValid()) {
        QTextLayout *layout = block.layout();
        if (layout) {
            QVector<QTextLayout::FormatRange> ranges = layout->formats();
            QTextLayout::FormatRange newRange;
            newRange.start = startPos;
            newRange.length = length;
            newRange.format = format;
            ranges << newRange;
            layout->setFormats(ranges);
            doc->markContentsDirty(block.position() + startPos, length);
        }
    }
}

void SyntaxHighlighter::setMessageFormat(QTextBlock block, int startPos, int length,
                                         QTextCharFormat format /*= QTextCharFormat()*/) const
{
    if (!format.isValid()) {
        format.setUnderlineColor(QColor(210, 247, 64));
        format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    }
    addFormat(block, startPos, length, format);
}

void SyntaxHighlighter::colorChanged(const QString& type, const QColor& col)
{
    Q_UNUSED(type)
    Q_UNUSED(col)
    d->rehighlightTmr.start();
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
#ifdef BUILD_KF5SYNTAX
    d->highlightBlock(text);
#else
    Q_UNUSED(text)
#endif
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

#include "moc_SyntaxHighlighter.cpp"
