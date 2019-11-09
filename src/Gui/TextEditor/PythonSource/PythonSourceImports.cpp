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


Python::SourceImportModule::SourceImportModule(Python::SourceImportPackage *parent,
                                               Python::SourceFrame *frame,
                                               QString alias) :
    Python::SourceListNodeBase(parent),
    m_frame(frame), m_aliasName(alias),
    m_type(Python::SourceRoot::ReferenceImportType)
{

}

Python::SourceImportModule::~SourceImportModule()
{
}

QString Python::SourceImportModule::name() const
{
    if (!m_aliasName.isEmpty())
        return m_aliasName;

    QFileInfo fi(m_modulePath);
    return fi.baseName();
}

void Python::SourceImportModule::load()
{
    // FIXME implement this in Python::SourceRoot as many opened files might require the same module
}

bool Python::SourceImportModule::isBuiltIn() const
{
    // FIXME implement this in Python::SourceRoot as many opened files might require the same module
    return false;
}

// -----------------------------------------------------------------------------

Python::SourceImportPackage::SourceImportPackage(Python::SourceImportPackage *parent,
                                                 QString name,
                                                 const Python::SourceModule *ownerModule) :
    Python::SourceListParentBase(parent),
    m_name(name),
    m_ownerModule(ownerModule)
{
    // FIXME resolve path in Python::SourceRoot
    m_packagePath = name;
}

Python::SourceImportPackage::~SourceImportPackage()
{
}

Python::SourceImportPackage *Python::SourceImportPackage::getImportPackagePath(QString filePath)
{
    for (Python::SourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        Python::SourceImportPackage *pkg = dynamic_cast<Python::SourceImportPackage*>(itm);
        if (pkg && pkg->path() == filePath)
            return pkg;
    }

    return nullptr;
}

Python::SourceImportPackage *Python::SourceImportPackage::getImportPackage(QString name)
{
    for (Python::SourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        Python::SourceImportPackage *pkg = dynamic_cast<Python::SourceImportPackage*>(itm);
        if (pkg && pkg->name() == name)
            return pkg;
    }

    return nullptr;
}

Python::SourceImportModule *Python::SourceImportPackage::getImportModulePath(QString filePath)
{
    for (Python::SourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        Python::SourceImportModule *mod = dynamic_cast<Python::SourceImportModule*>(itm);
        if (mod && mod->path() == filePath)
            return mod;
    }

    return nullptr;
}

Python::SourceImportModule *Python::SourceImportPackage::getImportModule(QString name)
{
    for (Python::SourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        Python::SourceImportModule *mod = dynamic_cast<Python::SourceImportModule*>(itm);
        if (mod && mod->name() == name)
            return mod;
    }

    return nullptr;
}

Python::SourceImportModule *Python::SourceImportPackage::setModule(QString name,
                                                               QString alias,
                                                               Python::SourceFrame *frame)
{
    QString importName = alias.isEmpty() ? name : alias;
    Python::SourceImportModule *mod = getImportModule(importName);

    if (mod)
        return mod;

    // not yet in list
    mod = new Python::SourceImportModule(this, frame, alias);
    insert(mod);
    return mod;
}

Python::SourceImportPackage *Python::SourceImportPackage::setPackage(QString name)
{
    Python::SourceImportPackage *pkg = getImportPackage(name);
    if (pkg)
        return pkg;

    pkg = new Python::SourceImportPackage(this, name, m_ownerModule);
    insert(pkg);
    return pkg;
}

int Python::SourceImportPackage::compare(const Python::SourceListNodeBase *left,
                                       const Python::SourceListNodeBase *right) const
{
    QString lName, rName;
    const Python::SourceImportModule *lm = dynamic_cast<const Python::SourceImportModule*>(left),
                                   *rm = dynamic_cast<const Python::SourceImportModule*>(right);

    const Python::SourceImportPackage *lp = dynamic_cast<const Python::SourceImportPackage*>(left),
                                    *rp = dynamic_cast<const Python::SourceImportPackage*>(right);
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


Python::SourceImportList::SourceImportList(Python::SourceFrame *owner,
                                               const Python::SourceModule *ownerModule) :
    Python::SourceImportPackage(this, QLatin1String(""), ownerModule),
    m_frame(owner)
{
    m_preventAutoRemoveMe = true;
}

Python::SourceImportList::~SourceImportList()
{
}

Python::SourceImportModule *Python::SourceImportList::getImportModulePath(QString filePath)
{
    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    dir.cdUp();
    return getImportModule(dir.dirName(), fi.baseName());
}

Python::SourceImportModule *Python::SourceImportList::getImportModule(QString rootPackage, QString name)
{
    Python::SourceImportPackage *pkg = getImportPackage(QString(), rootPackage);
    if (pkg)
        return pkg->getImportModule(name);

    return nullptr;
}

Python::SourceImportModule *Python::SourceImportList::getImportModule(QStringList modInheritance)
{
    if (empty() || modInheritance.size() < 1)
        return nullptr;

    QString modName = modInheritance[modInheritance.size() -1];

    if (modInheritance.size() < 2)
        return getImportModule(QString(), modName);

    // get root package by slicing QStringList
    Python::SourceImportPackage *pkg = getImportPackage(QString(), modInheritance.last());
    if (pkg)
        return pkg->getImportModule(modName);

    return nullptr;
}

Python::SourceImportPackage *Python::SourceImportList::getImportPackagePath(QString filePath)
{
    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    dir.cdUp();
    return getImportPackage(dir.dirName(), fi.baseName());
}

Python::SourceImportPackage *Python::SourceImportList::getImportPackage(QString rootPackage, QString name)
{
    for (Python::SourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        Python::SourceImportPackage *pkg = dynamic_cast<Python::SourceImportPackage*>(itm);
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

Python::SourceImportPackage *Python::SourceImportList::getImportPackage(QStringList modInheritance)
{
    if (empty() || modInheritance.size() < 1)
        return nullptr;

    // single name like import sys
    if (modInheritance.size() == 1)
        return getImportPackage(QString(), modInheritance[0]);

    // package.module.... like import sys.path.join
    Python::SourceImportPackage *pkg = nullptr;
    QString curName = modInheritance.takeLast();

    // first find our root package
    for (Python::SourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        pkg = dynamic_cast<Python::SourceImportPackage*>(itm);
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

Python::SourceImportModule *Python::SourceImportList::setModule(QStringList rootPackage,
                                                            QString module, QString alias)
{
    Python::SourceImportModule *mod = nullptr;
    Python::SourceImportPackage *rootPkg = getImportPackage(rootPackage);

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

Python::SourceImportPackage *Python::SourceImportList::setPackage(QStringList rootPackage)
{
    Python::SourceImportPackage *rootPkg = nullptr;

    // no rootPackages
    if (rootPackage.size() < 1) {
       rootPkg = Python::SourceImportPackage::setPackage(QLatin1String(""));
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

Python::SourceModule *Python::SourceImportList::setModuleGlob(QStringList rootPackage)
{
    // FIXME implement
    Q_UNUSED(rootPackage)
    return nullptr;
}
