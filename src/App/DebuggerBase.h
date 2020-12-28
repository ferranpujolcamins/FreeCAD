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

#ifndef DEBUGGERBASE_H
#define DEBUGGERBASE_H

#include <memory>
#include <vector>
#include <FCConfig.h>
#include <QString>
#include <QObject>
#include <Base/Exception.h>

namespace App {
namespace Debugging {

// -----------------------------------------------------

/**
 * @brief The RunningState class use this for the state machine when implementing debugger
 */
class State
{
public:
    enum States {
        Stopped,
        Running,
        SingleStep,
        StepOver,
        StepOut,
        HaltOnNext
    };

    State(): state(Stopped) {  }
    ~State() {  }
    States operator= (States s) { state = s; return state; }
    bool operator!= (States s) { return state != s; }
    bool operator== (States s) { return state == s; }

private:
    States state;
};

// foward declarations
template <typename T_dbgr, typename T_bp, typename T_bpname>
class BrkPntBaseFile;

template<typename T_dbgr, typename T_bp, typename T_bpfile>
class AbstractDbgr;

// ****************************************************************************
// These are private data containers, allows ABI changes to exported class
//  included here instead of cpp file due to template
// ****************************************************************************


template <typename T_bpfile>
class BrkPntBaseP {
public:
    explicit BrkPntBaseP(int lineNr,
                             std::weak_ptr<T_bpfile> parent)
        : bpFile(parent)
        , uniqueId(0)
        , lineNr(lineNr)
        , hitCount(0)
        , ignoreFrom(0)
        , ignoreTo(0)
        , disabled(false)
    {
    }
    ~BrkPntBaseP()
    { }

    std::weak_ptr<T_bpfile> bpFile;
    size_t uniqueId; // a global unique number
    int lineNr,
        hitCount,
        ignoreFrom,
        ignoreTo;
    bool disabled;
};

// -----------------------------------------------------------------------------

template<typename T_dbgr, typename T_bp>
class BrkPntBaseFileP {
public:
    explicit BrkPntBaseFileP(std::weak_ptr<T_dbgr> debugger) : debugger(debugger) {}
    ~BrkPntBaseFileP() {}
    QString filename;
    std::vector<std::shared_ptr<T_bp>> lines;
    std::weak_ptr<T_dbgr> debugger;
};


// -----------------------------------------------------------------------------

template<typename T_bpfile>
class AbstractDbgrP {
public:
    explicit AbstractDbgrP()
    { }
    ~AbstractDbgrP()
    { }
    State state;
    std::vector<std::shared_ptr<T_bpfile>> bps;
};

// *********************** end private data classes ****************************

/**
 * @brief The BreakpointBase class is the actual breakpoint
 */
template<typename T_bpfile>
class BrkPntBase : public std::enable_shared_from_this<BrkPntBase<T_bpfile>>
{
    BrkPntBaseP<T_bpfile>* d;
public:
    typedef std::shared_ptr<BrkPntBase<T_bpfile>> PtrType;
    //BreakpointBase(); // we should not create without lineNr
    explicit BrkPntBase(int lineNr, std::weak_ptr<T_bpfile> parent);
    BrkPntBase(const BrkPntBase &other);
    virtual ~BrkPntBase();


    BrkPntBase& operator=(const BrkPntBase &other);
    inline bool operator==(int line) const;
    inline bool operator!=(int line) const;
    inline bool operator<(const BrkPntBase &other) const;
    inline bool operator<(const int &line) const;

    int lineNr() const { return d->lineNr; }
    void setLineNr(int line);

    inline bool hit() const;
    void reset() { d->hitCount = 0; }
    int hitCount() const {return d->hitCount; }

    /**
     * @brief setIgnoreTo ignores hits on this line up to ignore hits
     * @param ignore
     */
    void setIgnoreTo(int ignore);
    int ignoreTo() const { return d->ignoreTo; }
    /**
     * @brief setIgnoreFrom ignores hits on this line from ignore hits
     * @param ignore
     */
    void setIgnoreFrom(int ignore);

    int ignoreFrom() const { return d->ignoreFrom; }

    void setDisabled(bool disable);
    bool disabled() const { return d->disabled; }

    size_t uniqueId() const { return d->uniqueId; }

    typedef typename std::shared_ptr<T_bpfile> BkrPntFilePtr;
    BkrPntFilePtr bpFile() const { return d->bpFile.lock(); }
};

// ------------------------------------------------------------------------

/**
 * @brief The BreakpointFile class
 *  contains all breakpoints for a given source file
 */
template<typename T_dbgr, typename T_bp, typename T_bpfile>
class AppExport BrkPntBaseFile : public
        std::enable_shared_from_this<BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>>
{
    BrkPntBaseFileP<T_dbgr, T_bp> *d;
public:
    typedef std::shared_ptr<BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>> PtrType;
    typedef typename std::shared_ptr<T_bp> BrkPntPtr;
    explicit BrkPntBaseFile(std::shared_ptr<T_dbgr> dbgr);
    BrkPntBaseFile(const BrkPntBaseFile& other);
    virtual ~BrkPntBaseFile() { delete d; }

    const QString& fileName() const { return d->filename; }
    void setFilename(const QString& fn) { d->filename = fn; }

    std::shared_ptr<T_dbgr> debugger() const { return d->debugger.lock(); }

    BrkPntBaseFile& operator=(const BrkPntBaseFile& rBp);
    bool operator ==(const BrkPntBaseFile& bp);
    bool operator ==(const QString& fn) { return d->filename == fn; }

    // make for : range work on this class
    typedef typename  std::vector<std::shared_ptr<T_bp>>::iterator iterator;
    iterator begin() const { return d->lines.begin(); }
    iterator rbegin() const { return d->lines.rbegin(); }
    iterator end() const { return  d->lines.end(); }
    iterator rend() const { return d->lines.rend(); }
    BrkPntPtr front() const { return d->lines.front(); }
    BrkPntPtr back() const { return d->lines.back(); }
    BrkPntPtr rfront() const { return d->lines.rfront(); }
    BrkPntPtr rback() const { return d->lines.rback(); }

    bool addBreakpoint(std::shared_ptr<T_bp> bp);
    bool addBreakpoint(int line);
    virtual void removeBreakpointByLine(int line);
    virtual void removeBreakpointById(size_t id);

    virtual void removeBreakpoint(std::shared_ptr<T_bp> bp);
    BrkPntPtr getBreakpoint(int line) const;
    /// used for loops
    BrkPntPtr getBreakpointFromIdx(size_t idx) const;
    BrkPntPtr getBreakpointFromId(size_t id) const;
    bool containsLine(int line) const;
    bool containsId(size_t id) const;
    bool setDisable(int line, bool disable);
    bool disabled(int line) const {
        const auto bp = getBreakpoint(line);
        return  bp != nullptr ? bp->disabled() : true;
    }

    size_t size() const { return d->lines.size(); }
    void clear() { return d->lines.clear(); }
    /**
     * @brief moveLines moves all breakpoints
     * @param startLine, the line to start from, all lines above this is moved
     * @param moveSteps, positive = move forward, negative = backwards
     * @return moved lines count
     */
    int moveLines(int startLine, int moveSteps) const;
};

// --------------------------------------------------------------------------

/**
 * @brief The Events class a templated class can't use signal and slots
 *        workaround, let derive this class
 */
class DebuggerBase : public QObject
{
    Q_OBJECT
public:
    explicit DebuggerBase(QObject *parent = nullptr);
    virtual ~DebuggerBase();

public Q_SLOTS:
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual void haltOnNext() = 0;
    virtual void stepOver() = 0;
    virtual void stepInto() = 0;
    virtual void stepOut() = 0;
    virtual void stepContinue() = 0;

Q_SIGNALS:
    // subclasses should impelement these
    void started();
    void stopped();
    void nextInstruction();
    void functionCalled();
    void functionExited();
    void haltAt(const QString &filename, int line);
    void releaseAt(const QString &filename, int line);
    void breakpointAdded(size_t uniqeId);
    void breakpointChanged(size_t uniqeId);
    void breakpointRemoved(size_t uniqeId);
    void exceptionOccured(std::shared_ptr<Base::Exception> exeption);
    void exceptionCleared(const QString &fn, int line);
    void allExceptionsCleared();
};


// ------------------------------------------------------------------------------------

/**
 * @brief The AbstractDebugger class base class for all debuggers
 * Used as a singleton class
 */
template<typename T_dbgr, typename T_bp, typename T_bpfile>
class AppExport AbstractDbgr :
        public DebuggerBase,
        public std::enable_shared_from_this<AbstractDbgr<T_dbgr, T_bp, T_bpfile>>
{
    AbstractDbgrP<T_bpfile>* d;

    // to allow breakpoints emitting signals as if they emanated from this class
    friend class BrkPntBase<T_bpfile>;
    friend class BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>;
public:
    typedef typename std::shared_ptr<AbstractDbgr<T_dbgr, T_bp, T_bpfile>> PtrType;
    typedef typename std::shared_ptr<T_bp> BrkPntPtr;
    typedef typename std::shared_ptr<T_bpfile> BrkPntFilePtr;

    explicit AbstractDbgr(QObject *parent = nullptr);
    virtual ~AbstractDbgr() { delete d; }

    bool hasBreakpoint(const QString &fn) const;
    /**
     * @brief the total number of breakpoints
     */
    int breakpointCount() const;

    /**
     * @brief gets the n-th breakpoint
     * @param idx = the n-th breakpoint
     * @return ptr to BreakpintLine or nullptr
     */
    BrkPntPtr getBreakpointFromIdx(int idx) const;
    BrkPntFilePtr getBreakpointFileFromIdx(int idx) const;
    BrkPntFilePtr getBreakpointFile(const QString& fn) const;
    BrkPntPtr getBreakpoint(const QString fn, int line) const;
    BrkPntPtr getBreakpointFromUniqueId(size_t uniqueId) const;
    bool setBreakpoint(const QString fn, int line);
    bool setBreakpoint(const QString fn, std::shared_ptr<T_bp> bp);
    std::list<BrkPntPtr> allBreakpoints() const;

    /**
     * @brief Returns which number in store bpl has
     * @param bpl = BreakpointLine instance
     * @return the n-th number
     */
    int getIdxFromBreakpoint(const BrkPntPtr bp) const;

    /**
     * @brief setBreakpointFile used to notify that we are tracing this file
     * @param fn = fileName
     */
    bool setBreakpointFile(BrkPntFilePtr bpFile);
    BrkPntFilePtr createBreakpointFile(const QString fn);
    /**
     * @brief deleteBreakpointFile stop tracing this file, used when editor closes file
     * @param fn = fileName
     */
    void deleteBreakpointFile(const QString &fn);
    void deleteBreakpointByLine(const QString fn, int line);
    void deleteBreakpoint(BrkPntPtr bp);
    void clearAllBreakPoints();

    /// returns the running state, halted, running etc
    inline State state() const {  return d->state; }
    /// running, not halted
    virtual bool isRunning() const { return d->state == State::Running; }
    /// stopped, debugging inspection
    virtual bool isHalted() const { return d->state == State::Stopped; }
    /// halt on next iteration
    virtual bool isHaltOnNext() const { return d->state == State::HaltOnNext; }


/// signals and slots declared in Signals class,
/// due to templatee class can't use signals and slots


protected:
    void setState(State::States state) { d->state = state; }
};


// ***********************************************************************
//   template class definitions from here down
// ***********************************************************************


template<typename T_bpfile>
BrkPntBase<T_bpfile>::BrkPntBase(int lineNr, std::weak_ptr<T_bpfile> parent)
    : d(new BrkPntBaseP<T_bpfile>(lineNr, parent))
{
    static size_t globalNumber = 0;
    d->uniqueId = ++globalNumber;
}

template<typename T_bpfile>
BrkPntBase<T_bpfile>::BrkPntBase(const BrkPntBase &other)
    : std::enable_shared_from_this<BrkPntBase>(other)
    , d(new BrkPntBaseP<T_bpfile>(other.d->lineNr, other.d->bpFile))
{
    d->uniqueId = other.d->uniqueId;
    d->disabled = other.d->disabled;
}

template<typename T_bpfile>
BrkPntBase<T_bpfile>::~BrkPntBase()
{
    delete d;
}

template<typename T_bpfile>
BrkPntBase<T_bpfile> &BrkPntBase<T_bpfile>::operator=(const BrkPntBase &other)
{
    d->uniqueId = other.d->uniqueId;
    d->bpFile = other.d->bpFile;
    return *this;
}

template<typename T_bpfile>
bool BrkPntBase<T_bpfile>::operator==(int line) const {
    return d->lineNr == line;
}

template<typename T_bpfile>
bool BrkPntBase<T_bpfile>::operator!=(int line) const {
    return d->lineNr != line;
}

template<typename T_bpfile>
bool BrkPntBase<T_bpfile>::operator<(const BrkPntBase &other) const{
    return d->lineNr < other.d->lineNr;
}

template<typename T_bpfile>
bool BrkPntBase<T_bpfile>::operator<(const int &line) const {
    return d->lineNr < line;
}

template<typename T_bpfile>
void BrkPntBase<T_bpfile>::setLineNr(int line)
{
    d->lineNr = line;
    Q_EMIT d->bpFile.lock()->debugger()->breakpointChanged(d->uniqueId);
}

template<typename T_bpfile>
bool BrkPntBase<T_bpfile>::hit() const
{
    ++d->hitCount;
    if (d->disabled)
        return false;
    if (d->ignoreFrom > 0 && d->ignoreFrom < d->hitCount)
        return true;
    if (d->ignoreTo < d->hitCount)
        return true;

    return false;
}

template<typename T_bpfile>
void BrkPntBase<T_bpfile>::setIgnoreTo(int ignore)
{
    d->ignoreFrom = ignore;
    Q_EMIT d->bpFile.lock()->debugger()->breakpointChanged(d->uniqueId);
}

template<typename T_bpfile>
void BrkPntBase<T_bpfile>::setIgnoreFrom(int ignore)
{
    d->ignoreFrom = ignore;
    Q_EMIT d->bpFile.lock()->debugger()->breakpointChanged(d->uniqueId);
}

template<typename T_bpfile>
void BrkPntBase<T_bpfile>::setDisabled(bool disable)
{
    d->disabled = disable;
    Q_EMIT d->bpFile.lock()->debugger()->breakpointChanged(d->uniqueId);
}

// -------------------------------------------------------------------------------


template<typename T_dbgr, typename T_bp, typename T_bpfile>
BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::BrkPntBaseFile(std::shared_ptr<T_dbgr> dbgr)
    : d(new BrkPntBaseFileP<T_dbgr, T_bp>(dbgr))
{
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::BrkPntBaseFile(const BrkPntBaseFile &other)
    : std::enable_shared_from_this<BrkPntBaseFile>(other)
    , d(new BrkPntBaseFileP<T_dbgr, T_bp>(other.d->debugger))
{
    d->lines = other.d->lines;
    d->filename = other.d->filename;
    d->debugger = other.d->debugger.lock();
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>&
BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::operator=(const BrkPntBaseFile &rBp) {
    if (this == &rBp)
        return *this;
    setFilename(rBp.d->filename);
    d->lines.clear();
    for (auto bp : rBp.d->lines)
        d->lines.push_back(bp);
    d->dbgr = rBp.d->dbgr.lock();
    return *this;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::operator ==(const BrkPntBaseFile &bp) {
    return d->filename == bp.d->filename;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::addBreakpoint(std::shared_ptr<T_bp> bp) {
    for (auto bpl : d->lines)
        if (bpl->lineNr() == bp->lineNr())
            return false;

    d->lines.push_back(bp);
    if (!d->debugger.expired())
        Q_EMIT d->debugger.lock()->breakpointAdded(bp->uniqueId());
    return true;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::addBreakpoint(int line) {
    if (containsLine(line))
        return false;

    auto shPtr = std::dynamic_pointer_cast<T_bpfile>(this->shared_from_this());
    auto bp = std::make_shared<T_bp>(line, shPtr);
    return addBreakpoint(bp);
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
void BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::removeBreakpointByLine(int line) {
    std::shared_ptr<T_bp> bp;

    // we need to emit before removal to make reciever lookup this breakpoint
    if (!d->debugger.expired() && bp)
        Q_EMIT d->debugger.lock()->breakpointRemoved(bp->uniqueId());

    // do the removal
    std::remove_if(d->lines.begin(), d->lines.end(),
                   [&] (std::shared_ptr<T_bp> b) {
        if (b->lineNr() == line) {
            bp = b;
            return true;
        }
        return false;
    });
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
void BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::removeBreakpointById(size_t id) {
    std::shared_ptr<T_bp> bp;
    std::remove_if(d->lines.begin(), d->lines.end(),
                   [&] (std::shared_ptr<T_bp> b) {
        if (b->uniqueId() == id) {
            bp = b;
            return true;
        }
        return false;
    });
    if (!d->debugger.expired())
        Q_EMIT d->debugger.lock()->breakpointRemoved(bp->uniqueId());
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
void BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::removeBreakpoint(std::shared_ptr<T_bp> bp) {
    std::shared_ptr<T_bp> bpd;
    std::remove_if(d->lines.begin(), d->lines.end(),
                   [&] (std::shared_ptr<T_bp> b) {
        if (b == bp) {
            bpd = b;
            return true;
        }
        return false;
    });
    if (!d->debugger.expired())
        Q_EMIT d->debugger.lock()->breakpointRemoved(bp->uniqueId());
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::BrkPntPtr
BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::getBreakpoint(int line) const {
    auto it = std::find_if(d->lines.begin(), d->lines.end(),
                           [&] (std::shared_ptr<T_bp> bp) {
            return bp->lineNr() == line;
});

    return it != d->lines.end() ? *it : nullptr;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::BrkPntPtr
BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::getBreakpointFromIdx(size_t idx) const {
    if (idx >= d->lines.size())
        return nullptr;
    return d->lines.at(idx);
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::BrkPntPtr
BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::getBreakpointFromId(size_t id) const {
    auto it = std::find_if(d->lines.begin(), d->lines.end(),
                           [id] (std::shared_ptr<T_bp> bp) {
            return bp->uniqueId() == id;
});

    return it != d->lines.end() ? *it : nullptr;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::containsLine(int line) const {
    return getBreakpoint(line) != nullptr;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::containsId(size_t id) const {
    for(auto bp : d->lines)
        if (bp->uniqueId() == id)
            return true;

    return false;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::setDisable(int line, bool disable) {
    auto bp = getBreakpoint(line);
    if (bp) {
        bp->setDisabled(disable);
        return true;
    }
    return false;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
int BrkPntBaseFile<T_dbgr, T_bp, T_bpfile>::moveLines(int startLine, int moveSteps) const {
    int count = 0;
    for (auto bp : d->lines) {
        if (bp->lineNr() >= startLine) {
            bp->setLineNr(bp->lineNr() + moveSteps);
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------

template<typename T_dbgr, typename T_bp, typename T_bpfile>
AbstractDbgr<T_dbgr, T_bp, T_bpfile>::AbstractDbgr(QObject *parent)
    : DebuggerBase(parent)
    , d(new AbstractDbgrP<T_bpfile>())
{ }

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool AbstractDbgr<T_dbgr, T_bp, T_bpfile>::hasBreakpoint(const QString &fn) const {
    for (auto bpf : d->bps)
        if (fn == bpf->fileName())
            return true;
    return false;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
int AbstractDbgr<T_dbgr, T_bp, T_bpfile>::breakpointCount() const {
    int count = 0;
    for (auto bp : d->bps)
        count += bp->size();
    return count;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename AbstractDbgr<T_dbgr, T_bp, T_bpfile>::BrkPntPtr
AbstractDbgr<T_dbgr, T_bp, T_bpfile>::getBreakpointFromIdx(int idx) const {
    int count = -1;
    for (auto bp : d->bps) {
        for (auto bpl : *bp) {
            ++count;
            if (count == idx)
                return bpl;
        }
    }
    return nullptr;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename AbstractDbgr<T_dbgr, T_bp, T_bpfile>::BrkPntFilePtr
AbstractDbgr<T_dbgr, T_bp, T_bpfile>::getBreakpointFileFromIdx(int idx) const {
    int count = -1;
    for (auto bp : d->bps) {
        for (size_t i = 0; i < bp->size(); ++i) {
            ++count;
            if (count == idx)
                return bp;
        }
    }
    return nullptr;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename AbstractDbgr<T_dbgr, T_bp, T_bpfile>::BrkPntFilePtr
AbstractDbgr<T_dbgr, T_bp, T_bpfile>::getBreakpointFile(const QString &fn) const {
    for (auto bp : d->bps) {
        if (fn == bp->fileName())
            return bp;
    }

    return nullptr;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename AbstractDbgr<T_dbgr, T_bp, T_bpfile>::BrkPntPtr
AbstractDbgr<T_dbgr, T_bp, T_bpfile>::getBreakpoint(const QString fn, int line) const {
    for (auto bp : d->bps)
        if (fn == bp->fileName()) {
            auto tmp  = bp->getBreakpoint(line);
            return tmp;
        }
    return nullptr;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename AbstractDbgr<T_dbgr, T_bp, T_bpfile>::BrkPntPtr
AbstractDbgr<T_dbgr, T_bp, T_bpfile>::getBreakpointFromUniqueId(size_t uniqueId) const {
    for (auto bpf : d->bps)
        for (auto bpl : *bpf)
            if (bpl->uniqueId() == uniqueId)
                return bpl;
    return nullptr;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool AbstractDbgr<T_dbgr, T_bp, T_bpfile>::setBreakpoint(const QString fn, int line) {
    auto bpf = getBreakpointFile(fn);
    if (!bpf)
        bpf = createBreakpointFile(fn);

    return bpf->addBreakpoint(line);
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool AbstractDbgr<T_dbgr, T_bp, T_bpfile>::setBreakpoint(
        const QString fn, std::shared_ptr<T_bp> bp)
{
    auto bpf = getBreakpointFile(fn);
    if (!bpf)
        bpf = createBreakpointFile(fn);

    return (bpf && bpf->addBreakpoint(bp->shared_from_this())) || false;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
std::list<typename AbstractDbgr<T_dbgr, T_bp, T_bpfile>::BrkPntPtr>
AbstractDbgr<T_dbgr, T_bp, T_bpfile>::allBreakpoints() const {
    std::list<std::shared_ptr<BrkPntPtr>> lst;
    for (auto bpf : d->bps)
        for(auto bp : *bpf)
            lst.push_back(bp);

    return lst;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
int AbstractDbgr<T_dbgr, T_bp, T_bpfile>::getIdxFromBreakpoint(const BrkPntPtr bp) const
{
    int count = 0;
    for (auto bpf : d->bps) {
        for (auto b : *bpf) {
            if (b->uniqueId() == bp->uniqueId())
                return count;
            ++count;
        }
    }
    return -1;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
bool AbstractDbgr<T_dbgr, T_bp, T_bpfile>::setBreakpointFile(AbstractDbgr::BrkPntFilePtr bpFile) {
    if (getBreakpointFile(bpFile->fileName()) != nullptr)
        return false;

    d->bps.push_back(bpFile);
    return true;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
typename AbstractDbgr<T_dbgr, T_bp, T_bpfile>::BrkPntFilePtr
AbstractDbgr<T_dbgr, T_bp, T_bpfile>::createBreakpointFile(const QString fn) {
    auto bpf = getBreakpointFile(fn);
    if (bpf)
        return bpf;
    auto shPtr = std::dynamic_pointer_cast<T_dbgr>(this->shared_from_this());
    bpf = std::make_shared<T_bpfile>(shPtr);
    bpf->setFilename(fn);
    d->bps.push_back(bpf);
    return bpf;
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
void AbstractDbgr<T_dbgr, T_bp, T_bpfile>::deleteBreakpointFile(const QString &fn) {
    std::remove_if(d->bps.begin(), d->bps.end(),
                   [fn] (BrkPntFilePtr bpf) {
        return bpf->fileName() == fn;
    });
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
void AbstractDbgr<T_dbgr, T_bp, T_bpfile>::deleteBreakpointByLine(const QString fn, int line) {
    auto bpf = getBreakpointFile(fn);
    if (bpf)
        bpf->removeBreakpointByLine(line);
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
void AbstractDbgr<T_dbgr, T_bp, T_bpfile>::deleteBreakpoint(AbstractDbgr::BrkPntPtr bp) {
    bp->bpFile()->removeBreakpointById(bp->uniqueId());
}

template<typename T_dbgr, typename T_bp, typename T_bpfile>
void AbstractDbgr<T_dbgr, T_bp, T_bpfile>::clearAllBreakPoints() {
    d->bps.clear();
}


}; // namespace Debugging
}; // namespace App

#endif // DEBUGGERBASE_H
