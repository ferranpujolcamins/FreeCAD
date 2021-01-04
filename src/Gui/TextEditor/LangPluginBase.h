/***************************************************************************
 *   Copyright (c) 2020 Fredrik Johansson github.com/mumme74               *
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
#ifndef LANGPLUGINBASE_H
#define LANGPLUGINBASE_H

#include <FCConfig.h>
#include <memory>
#include <QObject>
#include <App/DebuggerBase.h>
#include <QTextBlock>

#include "EditorView.h"

/**
 * Purpose of this file is to move all things that
 * can be common between langages to a separate class
 * such as breakpoint, debugger hide/show markers
 *
 * TextEdit can then reach these through a pointer
 * ie load a python file, then a javascript file,
 *   same editor different behaviour regarding indent new line
 */

namespace Gui {

class AbstractLangPluginP;

/**
 * @brief Abstract baseclass for all related to view behaviour
 */
class AbstractLangPlugin : public std::enable_shared_from_this<AbstractLangPlugin>
{
    AbstractLangPluginP *d;
public:
    explicit AbstractLangPlugin(const char *langName);
    virtual ~AbstractLangPlugin();

    /// name of plugin
    const char* name() const;

    /// call to this registers all build in plugins to EditorViewSingleton
    static void registerBuildInPlugins();

    /// mimetypes that matches this plugin
    virtual QStringList mimetypes() const = 0; /// used to identify if should load plugin fro this mimetype
    /// filesuffixes that matches this plugin
    virtual QStringList suffixes() const = 0; /// used to determine if we should load plugin with these fileendings
    /// true if fn or mime matches this plugin
    virtual bool matchesMimeType(const QString &fn, const QString &mime = QString()) const;

    /// called by editor
    virtual void contextMenuLineNr(TextEditor *edit, QMenu *menu,
                                   const QString &fn, int line) const;
    virtual void contextMenuTextArea(TextEditor *edit, QMenu *menu,
                                     const QString &fn, int line) const;

    virtual  bool lineNrToolTipEvent(TextEditor *edit, const QPoint &pos,
                                     int line, QString &toolTipStr) const;
    virtual  bool textAreaToolTipEvent(TextEditor *edit, const QPoint &pos,
                                       int line, QString &toolTipStr) const;

    virtual void OnChange(TextEditor *editor, Base::Subject<const char *> &rCaller,
                          const char *rcReason) const;


    virtual void paintEventTextArea(TextEditor *edit, QPainter* painter,
                                    const QTextBlock &block, QRect &coords) const;
    virtual void paintEventLineNumberArea(TextEditor *edit, QPainter* painter,
                                          const QTextBlock &block, QRect &coords) const;

    virtual bool onCut(TextEditor *edit) const;
    virtual bool onPaste(TextEditor *edit) const;
    virtual bool onCopy(TextEditor *edit) const;


    /// called by view
    virtual bool onMsg(const EditorView *view, const char* pMsg, const char** ppReturn) const = 0;
    virtual bool onHasMsg(const EditorView *view, const char* pMsg) const = 0;
    virtual void onFileOpen(const EditorView *view, const QString& fn) const;
    virtual void onFileSave(const EditorView *view, const QString& fn) const;
    virtual void onFileClose(const EditorView *view, const QString& fn) const;

protected:
    /// returns a list with all TextEditor derived editors
    /// that currentsly shown in editorViews
    QList<TextEditor*> editors(const QString &fn = QString()) const;

};

// ---------------------------------------------------------------------

class AbstractLangPluginDbgP;
/**
 * @brief The AbstractLangViewDbg class handles bridge between editor and debugger
 */
class AbstractLangPluginDbg : public AbstractLangPlugin
{
    AbstractLangPluginDbgP *d;
public:
    explicit AbstractLangPluginDbg(const char* langName);
    ~AbstractLangPluginDbg() override;

    virtual void OnChange(TextEditor *editor, Base::Subject<const char *> &rCaller,
                          const char *rcReason) const override;

    virtual App::Debugging::DebuggerBase* debugger() const = 0;

    /// returns the iconname to use for this exception, name as in resourcefile (qrc)
    virtual const char* iconNameForException(const QString &fn, int excNr)  const = 0;

    const QPixmap& breakpointIcon() const;
    const QPixmap& breakpointDisabledIcon() const;
    const QPixmap& debugMarkerIcon() const;


    //virtual bool onCut(TextEditor *edit) const;
    //virtual bool onPaste(TextEditor *edit) const;

public: // Q_SLOTS:
    /// render line marker area when these changes
    virtual void onBreakpointAdded(size_t uniqueId) = 0;
    virtual void onBreakPointClear(size_t uniqueId) = 0;
    virtual void onBreakPointChanged(size_t uniqueId) = 0;

    // haltAt and releaseAt slots
    /// shows the debug marker arrow in filename editor at line
    virtual bool editorShowDbgMrk(const QString &fn, int line) = 0;

    /// hides the debug marker arrow in filename  editor
    virtual bool editorHideDbgMrk(const QString &fn, int line) = 0;


    virtual void exceptionOccured(Base::Exception* exeption)  = 0;
    virtual void exceptionCleared(const QString &fn, int line) = 0;
    virtual void allExceptionsCleared() = 0;

protected:
    /// scrolls to line
    void scrollToPos(TextEditor *edit, int line) const;

    virtual const QColor &exceptionScrollbarMarkerColor() const;
    virtual const QColor &breakpointScrollbarMarkerColor() const;
    //void breakpointPasteOrCut(TextEditor *edit, bool doCut) const;
};

// ---------------------------------------------------------------------

class AbstractLangPluginCodeP;
class AbstractLangPluginCode : public AbstractLangPlugin
{
    AbstractLangPluginCodeP *d;
public:
    explicit AbstractLangPluginCode(const char *pluginName);
    ~AbstractLangPluginCode() override;

    virtual bool onTabPressed(TextEditor *edit) const;
    virtual bool onDelPressed(TextEditor *edit) const;
    virtual bool onBacktabPressed(TextEditor *edit) const;      // shift + tab
    virtual bool onBackspacePressed(TextEditor *edit) const;
    virtual bool onSpacePressed(TextEditor *edit) const;
    virtual bool onEnterPressed(TextEditor *edit) const;
    virtual bool onPeriodPressed(TextEditor *edit) const;       // .
    virtual bool onEscPressed(TextEditor *edit) const;
    virtual bool onParenLeftPressed(TextEditor *edit) const;    // (
    virtual bool onParenRightPressed(TextEditor *edit) const;   // )
    virtual bool onBraceLeftPressed(TextEditor *edit) const;    // {
    virtual bool onBraceRightPressed(TextEditor *edit) const;   // }
    virtual bool onBracketLeftPressed(TextEditor *edit) const;  // [
    virtual bool onBracketRightPressed(TextEditor *edit) const; // }
    virtual bool onAtPressed(TextEditor *edit) const;           // @
    virtual bool onQuoteSglPressed(TextEditor *edit) const;     // '
    virtual bool onQuoteDblPressed(TextEditor *edit) const;     // "
    virtual bool onAmpersandPressed(TextEditor *edit) const;    // #
    virtual bool onCommaPressed(TextEditor *edit) const;        // ,
    virtual bool onColonPressed(TextEditor *edit) const;        // :
    virtual bool onSemiColonPressed(TextEditor *edit) const;    // ;


    // get called before the above, takes precedece, if returns true above won't be called
    virtual bool onKeyPress(TextEditor *edit, QKeyEvent *evt) const;
};

// ---------------------------------------------------------------------

// *********************** Actually working plugins from here down ********

class CommonCodeLangPluginP;
class CommonCodeLangPlugin : public QObject,
                             public AbstractLangPluginCode
{
    Q_OBJECT
    CommonCodeLangPluginP *d;
public:
    CommonCodeLangPlugin(const char* pluginName = "commoncode");
    ~CommonCodeLangPlugin() override;

    virtual QStringList mimetypes() const override;
    virtual QStringList suffixes() const override;

    /// called by editor
    void OnChange(TextEditor *editor, Base::Subject<const char *> &rCaller,
                  const char *rcReason) const override;

    virtual bool onTabPressed(TextEditor *edit) const override;
    virtual bool onBacktabPressed(TextEditor *edit) const override;

    virtual void contextMenuTextArea(TextEditor *edit, QMenu *menu,
                                     const QString &fn, int line) const override;


    /// called by view
    virtual bool onMsg(const EditorView *view, const char* pMsg,
                       const char** ppReturn) const override;
    virtual bool onHasMsg(const EditorView *view, const char* pMsg) const override;

    virtual void onFileSave(const EditorView *view, const QString &fn) const override;
};

// ---------------------------------------------------------------------




}; // namespace Gui

#endif // LANGPLUGINBASE_H
