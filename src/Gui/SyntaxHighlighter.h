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
        // ordering of these values as pythonconsole checks for last valid state
        // as the one before Output
        Text = 0, Comment = 1, BlockComment = 2, Number = 3, String = 4, Keyword = 5,
        Classname = 6, Defname = 7, Operator = 8, Builtin = 9, StringSingleQoute = 10,
        BlockCommentSingleQoute = 11, Decorator = 12,
        // alias
        StringDoubleQoute = String, BlockCommentDoubleQoute = BlockComment,

        // these blocks are not copied to mimedata in pythonconsole
        Output = 13, Error = 14,

        // used as a Stopindicator in loops
        NoColorInvalid // should be initialized by compiler to value after the last
    };

    //used as return value for get/set-AllColors()
    struct ColorDataP;
    struct ColorData {
        ColorData();
        ColorData(const QString key, const QColor color);
        ColorData(const QString key, unsigned long rgbAsULong);
        ColorData(const ColorData & other);
        ~ColorData();
        ColorData &operator= (ColorData &other);

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
    QColor color(const QString& type);

    QMap<QString, ColorData> allColors();

    // rehighlights after operation but does not call colorchanged for each color
    void setBatchOfColors(QMap<QString, ColorData> colors);

    // for example when retranslateing ui or on startup of a editor
    void loadSettings();

protected:
    virtual void colorChanged(const QString& type, const QColor& col);

protected:
    QColor colorByType(TColor type);


private:
    SyntaxHighlighterP* d;
};

} // namespace Gui

#endif // GUI_SYNTAXHIGHLIGHTER_H
