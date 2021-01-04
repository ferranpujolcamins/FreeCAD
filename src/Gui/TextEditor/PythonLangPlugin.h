/***************************************************************************
 *   Copyright (c) 2021 Fredrik Johansson github.com/mumme74               *
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

#ifndef PYTHONLANGPLUGIN_H
#define PYTHONLANGPLUGIN_H

#include "FCConfig.h"
#include "LangPluginBase.h"
#include <App/PythonDebugger.h>
#include <QDialog>
#include "EditorView.h"
#include "TextEditor.h"


QT_BEGIN_NAMESPACE
class QSpinBox;
class QLineEdit;
class QCheckBox;
QT_END_NAMESPACE

namespace Gui {

class PythonLangPluginDbgP;
class PythonLangPluginDbg : public QObject,
                          public AbstractLangPluginDbg
{
    Q_OBJECT
    PythonLangPluginDbgP *d;
public:
    explicit PythonLangPluginDbg();
    ~PythonLangPluginDbg() override;

    App::Debugging::Python::Debugger* debugger() const override;

    QStringList mimetypes() const override;
    QStringList suffixes() const override;

    /// called by editor
    void OnChange(TextEditor *editor, Base::Subject<const char *> &rCaller,
                  const char *rcReason) const override;

    const char *iconNameForException(const QString &fn, int line) const override;

    bool lineNrToolTipEvent(TextEditor *edit, const QPoint &pos,
                            int line, QString &toolTipStr) const override;
    bool textAreaToolTipEvent(TextEditor *edit, const QPoint &pos,
                              int line, QString &toolTipStr) const override;

    void contextMenuLineNr(TextEditor *edit, QMenu *menu,
                           const QString &fn, int line) const override;
    void contextMenuTextArea(TextEditor *edit, QMenu *menu,
                             const QString &fn, int line) const override;

    void paintEventTextArea(TextEditor *edit, QPainter* painter,
                            const QTextBlock& block, QRect &coords) const override;
    void paintEventLineNumberArea(TextEditor *edit, QPainter* painter,
                                  const QTextBlock& block, QRect &coords) const override;

    /// called by view
    bool onMsg(const EditorView *view, const char* pMsg, const char** ppReturn) const override;
    bool onHasMsg(const EditorView *view, const char* pMsg) const override;

    // from the old PythonEditorView
    void executeScript() const;
    void startDebug() const;
    void toggleBreakpoint() const;


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

    virtual void onFileOpen(const EditorView *view, const QString &fn) const override;
    void onFileClose(const EditorView *view, const QString &fn) const override;

private:
    Base::PyExceptionInfo* exceptionFor(const QString &fn, int line) const;
};

// ----------------------------------------------------------------------

class PythonLangPluginCodeP;
class PythonLangPluginCode : public CommonCodeLangPlugin
{
    Q_OBJECT
    PythonLangPluginCodeP *d;
public:
    explicit PythonLangPluginCode();
    virtual  ~PythonLangPluginCode() override;

    virtual QStringList mimetypes() const override;
    virtual QStringList suffixes() const override;

    virtual bool onPaste(TextEditor *edit) const override;

    virtual bool onEnterPressed(TextEditor *edit) const override;
    virtual bool onParenLeftPressed(TextEditor *edit) const override;
    virtual bool onParenRightPressed(TextEditor *edit) const override;
    virtual bool onBracketLeftPressed(TextEditor *edit) const  override;
    virtual bool onBracketRightPressed(TextEditor *edit) const override;
    virtual bool onBraceLeftPressed(TextEditor *edit) const override;
    virtual bool onBraceRightPressed(TextEditor *edit) const override;
    virtual bool onQuoteDblPressed(TextEditor *edit) const override;
    virtual bool onQuoteSglPressed(TextEditor *edit) const override;


    void contextMenuTextArea(TextEditor *edit, QMenu *menu,
                             const QString &fn, int line) const override;

private Q_SLOTS:
    void toggleComment(TextEditor *edit) const;
    void onAutoIndent(TextEditor *edit) const;

private:
    bool insertOpeningChar(TextEditor *edit, const QString &insertTxt) const;
    bool insertClosingChar(TextEditor *edit, const QString &insertTxt) const;

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

} // namespace Gui

#endif // PYTHONLANGPLUGIN_H
