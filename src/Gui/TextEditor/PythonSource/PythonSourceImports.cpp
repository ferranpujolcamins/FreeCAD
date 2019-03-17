#include "PythonSourceImports.h"
#include "PythonSourceListBase.h"
#include "PythonSource.h"
#include "PythonSourceFrames.h"

#include <QString>
#include <QFileInfo>
#include <QDir>


DBG_TOKEN_FILE

using namespace Gui;

// ----------------------------------------------------------------------------


PythonSourceImportModule::PythonSourceImportModule(PythonSourceImportPackage *parent,
                                                   PythonSourceFrame *frame,
                                                   QString alias) :
    PythonSourceListNodeBase(parent),
    m_frame(frame), m_aliasName(alias), m_type()
{

}

PythonSourceImportModule::~PythonSourceImportModule()
{
}

QString PythonSourceImportModule::name() const
{
    if (!m_aliasName.isEmpty())
        return m_aliasName;

    QFileInfo fi(m_modulePath);
    return fi.baseName();
}

void PythonSourceImportModule::load()
{
    // FIXME implement this in PythonSourceRoot as many opened files might require the same module
}

bool PythonSourceImportModule::isBuiltIn() const
{
    // FIXME implement this in PythonSourceRoot as many opened files might require the same module
    return false;
}

// -----------------------------------------------------------------------------

PythonSourceImportPackage::PythonSourceImportPackage(PythonSourceImportPackage *parent,
                                                     QString name,
                                                     const PythonSourceModule *ownerModule) :
    PythonSourceListParentBase(parent),
    m_name(name),
    m_ownerModule(ownerModule)
{
    // FIXME resolve path in PythonSourceRoot
    m_packagePath = name;
}

PythonSourceImportPackage::~PythonSourceImportPackage()
{
}

PythonSourceImportPackage *PythonSourceImportPackage::getImportPackagePath(QString filePath)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportPackage *pkg = dynamic_cast<PythonSourceImportPackage*>(itm);
        if (pkg && pkg->path() == filePath)
            return pkg;
    }

    return nullptr;
}

PythonSourceImportPackage *PythonSourceImportPackage::getImportPackage(QString name)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportPackage *pkg = dynamic_cast<PythonSourceImportPackage*>(itm);
        if (pkg && pkg->name() == name)
            return pkg;
    }

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportPackage::getImportModulePath(QString filePath)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportModule *mod = dynamic_cast<PythonSourceImportModule*>(itm);
        if (mod && mod->path() == filePath)
            return mod;
    }

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportPackage::getImportModule(QString name)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportModule *mod = dynamic_cast<PythonSourceImportModule*>(itm);
        if (mod && mod->name() == name)
            return mod;
    }

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportPackage::setModule(QString name,
                                                               QString alias,
                                                               PythonSourceFrame *frame)
{
    QString importName = alias.isEmpty() ? name : alias;
    PythonSourceImportModule *mod = getImportModule(importName);

    if (mod)
        return mod;

    // not yet in list
    mod = new PythonSourceImportModule(this, frame, alias);
    insert(mod);
    return mod;
}

PythonSourceImportPackage *PythonSourceImportPackage::setPackage(QString name)
{
    PythonSourceImportPackage *pkg = getImportPackage(name);
    if (pkg)
        return pkg;

    pkg = new PythonSourceImportPackage(this, name, m_ownerModule);
    insert(pkg);
    return pkg;
}

int PythonSourceImportPackage::compare(const PythonSourceListNodeBase *left,
                                       const PythonSourceListNodeBase *right) const
{
    QString lName, rName;
    const PythonSourceImportModule *lm = dynamic_cast<const PythonSourceImportModule*>(left),
                                   *rm = dynamic_cast<const PythonSourceImportModule*>(right);

    const PythonSourceImportPackage *lp = dynamic_cast<const PythonSourceImportPackage*>(left),
                                    *rp = dynamic_cast<const PythonSourceImportPackage*>(right);
    if (lm)
        lName = lm->name();
    else if(lp)
        lName = lp->name();
    else
        assert("Invalid state, non module or package stored (left)" == nullptr);

    if (rm)
        rName = rm->name();
    else if (rp)
        rName = rp->name();
    else
        assert("Invalid state, non module or package stored (right)" == nullptr);

    if (lName < rName)
        return +1;
    else if (lName > rName)
        return -1;
    return 0;

}

// ----------------------------------------------------------------------------


PythonSourceImportList::PythonSourceImportList(PythonSourceFrame *owner,
                                               const PythonSourceModule *ownerModule) :
    PythonSourceImportPackage(this, QLatin1String(""), ownerModule),
    m_frame(owner)
{
    m_preventAutoRemoveMe = true;
}

PythonSourceImportList::~PythonSourceImportList()
{
}

PythonSourceImportModule *PythonSourceImportList::getImportModulePath(QString filePath)
{
    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    dir.cdUp();
    return getImportModule(dir.dirName(), fi.baseName());
}

PythonSourceImportModule *PythonSourceImportList::getImportModule(QString rootPackage, QString name)
{
    PythonSourceImportPackage *pkg = getImportPackage(QString(), rootPackage);
    if (pkg)
        return pkg->getImportModule(name);

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportList::getImportModule(QStringList modInheritance)
{
    if (empty() || modInheritance.size() < 1)
        return nullptr;

    QString modName = modInheritance[modInheritance.size() -1];

    if (modInheritance.size() < 2)
        return getImportModule(QString(), modName);

    // get root package by slicing QStringList
    PythonSourceImportPackage *pkg = getImportPackage(QString(), modInheritance.last());
    if (pkg)
        return pkg->getImportModule(modName);

    return nullptr;
}

PythonSourceImportPackage *PythonSourceImportList::getImportPackagePath(QString filePath)
{
    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    dir.cdUp();
    return getImportPackage(dir.dirName(), fi.baseName());
}

PythonSourceImportPackage *PythonSourceImportList::getImportPackage(QString rootPackage, QString name)
{
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        PythonSourceImportPackage *pkg = dynamic_cast<PythonSourceImportPackage*>(itm);
        // lookup among packages, all modules are stored in packages
        if (pkg) {
            if (!rootPackage.isEmpty()) {
                if (rootPackage == pkg->name())
                    return pkg->getImportPackage(name);
            } else {
                return pkg;
            }

        }
    }

    return nullptr;
}

PythonSourceImportPackage *PythonSourceImportList::getImportPackage(QStringList modInheritance)
{
    if (empty() || modInheritance.size() < 1)
        return nullptr;

    // single name like import sys
    if (modInheritance.size() == 1)
        return getImportPackage(QString(), modInheritance[0]);

    // package.module.... like import sys.path.join
    PythonSourceImportPackage *pkg = nullptr;
    QString curName = modInheritance.takeLast();

    // first find our root package
    for (PythonSourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        pkg = dynamic_cast<PythonSourceImportPackage*>(itm);
        if (pkg && pkg->name() == curName) {
            // found our root package
            while(pkg &&
                  !(curName = modInheritance.takeLast()).isEmpty() && // note! action here
                  !curName.isEmpty())
            {
                if (modInheritance.size() == 0)
                    return pkg->getImportPackage(curName);
                pkg = pkg->getImportPackage(curName);
            }

        }
    }

    return nullptr;
}

PythonSourceImportModule *PythonSourceImportList::setModule(QStringList rootPackage,
                                                            QString module, QString alias)
{
    PythonSourceImportModule *mod = nullptr;
    PythonSourceImportPackage *rootPkg = getImportPackage(rootPackage);

    if (!rootPkg)
        rootPkg = setPackage(rootPackage);

    if (rootPkg) {
        mod = rootPkg->getImportModule(alias.isEmpty() ? module : alias);
        if (mod)
            return mod;

        // we have package but not module
        return rootPkg->setModule(module, alias, m_frame);
    }

    // an error occured
    return nullptr;
}

PythonSourceImportPackage *PythonSourceImportList::setPackage(QStringList rootPackage)
{
    PythonSourceImportPackage *rootPkg = nullptr;

    // no rootPackages
    if (rootPackage.size() < 1) {
       rootPkg = PythonSourceImportPackage::setPackage(QLatin1String(""));
       return rootPkg;
    }

    // lookup package
    rootPkg = getImportPackage(rootPackage);

    if (!rootPkg) {
        // we doesn't have rootPackage yet, recurse until we find it
        rootPkg = setPackage(rootPackage.mid(0, rootPackage.size() -1));
        return rootPkg;
    }

    return rootPkg;
}

PythonSourceModule *PythonSourceImportList::setModuleGlob(QStringList rootPackage)
{
    // FIXME implement
    Q_UNUSED(rootPackage);
    return nullptr;
}
