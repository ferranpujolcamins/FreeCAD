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
#include <App/PythonDebugger.h>
#include <QDialog>
#include <QTextBlock>

#include "EditorView.h"

/**
 * Purpose of this file is to move all things that
 * can be common between langages to a separate class
 * such as breakpoint, debugger hide/show markers
 *
 * TextEdit can then reach these through a smart pointer
 * ie load a python file, then a javascript file,
 *   same editor different behaviour regarding indent new line
 */

QT_BEGIN_NAMESPACE
class QSpinBox;
class QLineEdit;
class QCheckBox;
QT_END_NAMESPACE

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

    const char* name() const;

    virtual QStringList mimetypes() const = 0; /// used to identify if should load plugin fro this mimetype
    virtual QStringList suffixes() const = 0; /// used to determine if we should load plugin with these fileendings
    bool matchesMimeType(const QString &fn, const QString &mime = QString()) const;

    virtual void contextMenuLineNr(EditorView *view, QMenu *menu,
                                   const QString &fn, int line) const = 0;
    virtual void contextMenuTextArea(EditorView *view, QMenu *menu,
                                     const QString &fn, int line) const = 0;

    virtual  bool lineNrToolTipEvent(TextEditor *edit, const QPoint &pos,
                                     int line, QString &toolTipStr) const = 0;
    virtual  bool textAreaToolTipEvent(TextEditor *edit, const QPoint &pos,
                                       int line, QString &toolTipStr) const = 0;

    virtual void OnChange(EditorView *view, Base::Subject<const char *> &rCaller,
                          const char *rcReason) const = 0;

    virtual void paintEventTextArea(TextEditor *edit, QPainter* painter,
                                    const QTextBlock &block, QRect &coords) const = 0;
    virtual void paintEventLineNumberArea(TextEditor *edit, QPainter* painter,
                                          const QTextBlock &block, QRect &coords) const = 0;


public: // Q_SLOTS
    virtual void onFileOpened(const QString& fn) = 0;
    virtual void onFileClosed(const QString& fn) = 0;
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
    ~AbstractLangPluginDbg();

    virtual App::Debugging::DebuggerBase* debugger() = 0;


    void OnChange(EditorView *view, Base::Subject<const char *> &rCaller,
                  const char *rcReason) const override;

    /// returns the iconname to use for this exception, name as in resourcefile (qrc)
    virtual const char* iconNameForException(const QString &fn, int excNr)  const = 0;

    /// returns a list with all TextEditor derived editors
    /// that currentsly shown in editorViews
    QList<TextEditor*> editors(const QString &fn = QString()) const;


    const QPixmap& breakpointIcon() const;
    const QPixmap& breakpointDisabledIcon() const;
    const QPixmap& debugMarkerIcon() const;

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
};

// ---------------------------------------------------------------------

class PythonLangPluginDbgP;
class PythonLangPluginDbg : public QObject,
                          public AbstractLangPluginDbg
{
    Q_OBJECT
    PythonLangPluginDbgP *d;
public:
    explicit PythonLangPluginDbg();
    ~PythonLangPluginDbg() override;

    App::Debugging::Python::Debugger* debugger() override;

    QStringList mimetypes() const override;
    QStringList suffixes() const override;

    void OnChange(EditorView *view, Base::Subject<const char *> &rCaller,
                  const char *rcReason) const override;

    const char *iconNameForException(const QString &fn, int line) const override;

    bool lineNrToolTipEvent(TextEditor *edit, const QPoint &pos,
                            int line, QString &toolTipStr) const;
    bool textAreaToolTipEvent(TextEditor *edit, const QPoint &pos,
                              int line, QString &toolTipStr) const;



    void contextMenuLineNr(EditorView *view, QMenu *menu, const QString &fn, int line) const override;
    void contextMenuTextArea(EditorView *view, QMenu *menu, const QString &fn, int line) const override;

    void paintEventTextArea(TextEditor *edit, QPainter* painter,
                            const QTextBlock& block, QRect &coords) const override;
    void paintEventLineNumberArea(TextEditor *edit, QPainter* painter,
                                  const QTextBlock& block, QRect &coords) const override;

public Q_SLOTS:
    /// render line marker area when these changes
    void onBreakpointAdded(size_t uniqueId) override;
    void onBreakPointClear(size_t uniqueId) override;
    void onBreakPointChanged(size_t uniqueId) override;

    // haltAt and releaseAt slots
    /// shows the debug marker arrow in filename editor at line
    bool editorShowDbgMrk(const QString &fn, int line) override;

    /// hides the debug marker arrow in filename  editor
    bool editorHideDbgMrk(const QString &fn, int line) override;


    void exceptionOccured(Base::Exception* exception) override;
    void exceptionCleared(const QString &fn, int line) override;
    void allExceptionsCleared() override;

    void onFileOpened(const QString &fn) override;
    void onFileClosed(const QString &fn) override;

private:
    Base::PyExceptionInfo* exceptionFor(const QString &fn, int line) const;
};


/************************************************************************
 * Gui things such as dialogs from here on
 ***********************************************************************/

class PythonBreakpointDlg : public QDialog
{
    Q_OBJECT
public:
    PythonBreakpointDlg(QWidget *parent,
                        std::shared_ptr<App::Debugging::Python::BrkPnt> bp);
    ~PythonBreakpointDlg();
protected:
    void accept();
 private:
    std::shared_ptr<App::Debugging::Python::BrkPnt> m_bpl;

    QSpinBox  *m_ignoreToHits;
    QSpinBox  *m_ignoreFromHits;
    QLineEdit *m_condition;
    QCheckBox *m_enabled;
};



}; // namespace Gui

#endif // LANGPLUGINBASE_H
