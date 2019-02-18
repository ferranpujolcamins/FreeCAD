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


#ifndef GUI_SYNTAXHIGHLIGHTER_H
#define GUI_SYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>

namespace Gui {
class SyntaxHighlighterP;
class TextEditor;

/**
 * Abstract Syntax highlighter.
 * @author Werner Mayer
 */
class GuiExport SyntaxHighlighter : public QSyntaxHighlighter
{
public:
    SyntaxHighlighter(QObject* parent);
    virtual ~SyntaxHighlighter();

    enum TColor
    {
        // Note!
        // ordering of these values are important as pythonconsole checks for
        // last valid Color before Output to know if if should copy textblock or not
        //  meaning you should never use hardcoded values io test againt
        //    use:
        //      if (block().userData() >= (int)Output)
        //   instead of:
        //      if (block().userData() >= 19)
        Text = 0,
        Comment = 1,

        // numbers
        Number                  = 3,
        NumberHex               = 11,
        NumberBinary            = 12,
        NumberFloat             = 13,
        NumberOctal             = 14,


        // strings (Literals)
        StringBlockDoubleQoute  = 2,
        StringBlockSingleQoute  = 15,
        StringDoubleQoute       = 4,
        StringSingleQoute       = 16,

        // keywords
        Keyword                 = 5,
        KeywordClass            = 6,
        KeywordDef              = 7,

        // operators
        Operator                = 8,

        // python output colors
        // these blocks are also usedto decide if not to copy from pythonconsole to clipboard
        // meaning that Text with Values >= Output is used as a stop marker

        // in-out
        PythonConsoleOutput = 9,
        PythonConsoleError  = 10,


        // identifiers
        IdentifierUnknown       = 17, // variable not in current context
        IdentifierDefined       = 18, // variable is in current context
        IdentifierModule        = 19, // its a module definition
        IdentifierFunction      = 20, // its a function definition
        IdentifierMethod        = 21, // its a method definition
        IdentifierClass         = 22, // its a class definition
        IdentifierSuperMethod   = 23, // its a method with name: __**__
        IdentifierBuiltin       = 24, // is a built in function or property
        IdentifierDecorator     = 25, // a @property marker

        // delimiters
        Delimiter               = 26, // like ',' '.' '{' etc

        // special
        SyntaxError             = 27,

        // highligh current line color
        HighlightCurrentLine    = 28,

        // used as a Stopindicator in loops
        NoColorInvalid, // should be initialized by compiler to the value right after
                       // after the last Valid


        // alias (Support old API)
        String = StringDoubleQoute, BlockComment = StringBlockDoubleQoute,
        Classname = KeywordClass, Defname = KeywordDef,
        Output = PythonConsoleOutput, Error = PythonConsoleError
    };

    //used as return value for get/set-AllColors()
    struct ColorDataP;
    struct ColorData {
        ColorData();
        ColorData(const QString key, const QColor color);
        ColorData(const QString key, unsigned long rgbAsULong);
        ColorData(const ColorData & other);
        ~ColorData();
        const ColorData &operator= (const ColorData &other);

        QColor color() const;
        void setColor(const QColor &color);
        void setColor(unsigned long color);
        const QString key() const;
        const char *translateName() const;
        void setTranslateName(const char *newName);
        unsigned long colorAsULong() const;

        ColorDataP *d; // for binary compatibility, not sure if its required but in anycase...
    };

    int maximumUserState() const;

    void setColor(const QString& type, const QColor& col);
    QColor color(const QString& type) const;

    QMap<QString, ColorData> allColors() const;

    // rehighlights after operation but does not call colorchanged for each color
    void setBatchOfColors(QMap<QString, ColorData> colors);

    // for example when retranslateing ui or on startup of a editor
    void loadSettings();

protected:
    virtual void colorChanged(const QString& type, const QColor& col);

protected:
    QColor colorByType(TColor type) const;

private:
    SyntaxHighlighterP* d;
};

} // namespace Gui

#endif // GUI_SYNTAXHIGHLIGHTER_H
