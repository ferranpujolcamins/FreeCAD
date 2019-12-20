#include "PythonSourceImports.h"
#include "PythonSourceListBase.h"
#include "PythonSource.h"
#include "PythonSourceFrames.h"

#include <fstream>
#include <list>
#include <string>
#include <algorithm>


DBG_TOKEN_FILE

// ----------------------------------------------------------------------------


Python::SourceImportModule::SourceImportModule(Python::SourceImportPackage *parent,
                                               Python::SourceFrame *frame,
                                               const std::string &alias) :
    Python::SourceListNodeBase(parent),
    m_frame(frame), m_aliasName(alias),
    m_type(Python::SourceRoot::ReferenceImportType)
{

}

Python::SourceImportModule::~SourceImportModule()
{
}

const std::string Python::SourceImportModule::name() const
{
    if (!m_aliasName.empty())
        return m_aliasName;

    Python::FileInfo fi(m_modulePath);
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
                                                 const std::string &name,
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

Python::SourceImportPackage *Python::SourceImportPackage::getImportPackagePath(const std::string &filePath)
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

Python::SourceImportPackage *Python::SourceImportPackage::getImportPackage(const std::string &name)
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

Python::SourceImportModule *Python::SourceImportPackage::getImportModulePath(const std::string &filePath)
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

Python::SourceImportModule *Python::SourceImportPackage::getImportModule(const std::string &name)
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

Python::SourceImportModule *Python::SourceImportPackage::setModule(const std::string &name,
                                                                   const std::string &alias,
                                                                   Python::SourceFrame *frame)
{
    Python::SourceImportModule *mod = getImportModule(alias.empty() ? name : alias);

    if (mod)
        return mod;

    // not yet in list
    mod = new Python::SourceImportModule(this, frame, alias);
    insert(mod);
    return mod;
}

Python::SourceImportPackage *Python::SourceImportPackage::setPackage(const std::string &name)
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
    std::string lName, rName;
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
    Python::SourceImportPackage(this, "", ownerModule),
    m_frame(owner)
{
    m_preventAutoRemoveMe = true;
}

Python::SourceImportList::~SourceImportList()
{
}

Python::SourceImportModule *Python::SourceImportList::getImportModulePath(const std::string &filePath)
{
    Python::FileInfo fi(filePath);
    std::string dirName = fi.dirPath(1);
    return getImportModule(dirName, fi.baseName());
}

Python::SourceImportModule *Python::SourceImportList::getImportModule(const std::string &rootPackage,
                                                                      const std::string &name)
{
    Python::SourceImportPackage *pkg = getImportPackage(std::string(), rootPackage);
    if (pkg)
        return pkg->getImportModule(name);

    return nullptr;
}

Python::SourceImportModule *Python::SourceImportList::getImportModule(const std::list<const std::string> &modInheritance)
{
    if (empty() || modInheritance.size() < 1)
        return nullptr;

    std::string modName = modInheritance.back();

    if (modInheritance.size() < 2)
        return getImportModule(std::string(), modName);

    // get root package by slicing QStringList
    Python::SourceImportPackage *pkg = getImportPackage(std::string(), modInheritance.back());
    if (pkg)
        return pkg->getImportModule(modName);

    return nullptr;
}

Python::SourceImportPackage *Python::SourceImportList::getImportPackagePath(const std::string &filePath)
{
    Python::FileInfo fi(filePath);
    std::string dirName = fi.dirPath(1);
    return getImportPackage(dirName, fi.baseName());
}

Python::SourceImportPackage *Python::SourceImportList::getImportPackage(const std::string &rootPackage,
                                                                        const std::string &name)
{
    for (Python::SourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        Python::SourceImportPackage *pkg = dynamic_cast<Python::SourceImportPackage*>(itm);
        // lookup among packages, all modules are stored in packages
        if (pkg) {
            if (!rootPackage.empty()) {
                if (rootPackage == pkg->name())
                    return pkg->getImportPackage(name);
            } else {
                return pkg;
            }

        }
    }

    return nullptr;
}

Python::SourceImportPackage *Python::SourceImportList::getImportPackage(const std::list<const std::string> &modInheritance)
{
    if (empty() || modInheritance.size() < 1)
        return nullptr;

    // single name like import sys
    if (modInheritance.size() == 1)
        return getImportPackage(std::string(), modInheritance.front());

    std::list<const std::string> inheritanceList = modInheritance;

    // package.module.... like import sys.path.join
    Python::SourceImportPackage *pkg = nullptr;
    std::string curName = inheritanceList.back();
    inheritanceList.pop_back();

    // first find our root package
    for (Python::SourceListNodeBase *itm = begin();
         itm != end();
         itm = itm->next())
    {
        pkg = dynamic_cast<Python::SourceImportPackage*>(itm);
        if (pkg && pkg->name() == curName) {
            // found our root package
            while(pkg &&
                  !(curName = inheritanceList.back()).empty() && // note! action here
                  !curName.empty())
            {
                inheritanceList.pop_back();
                if (inheritanceList.size() == 0)
                    return pkg->getImportPackage(curName);
                pkg = pkg->getImportPackage(curName);
            }

        }
    }

    return nullptr;
}

Python::SourceImportModule *Python::SourceImportList::setModule(const std::list<const std::string> &rootPackage,
                                                            const std::string &module, const std::string alias)
{
    Python::SourceImportModule *mod = nullptr;
    Python::SourceImportPackage *rootPkg = getImportPackage(rootPackage);

    if (!rootPkg)
        rootPkg = setPackage(rootPackage);

    if (rootPkg) {
        mod = rootPkg->getImportModule(alias.empty() ? module : alias);
        if (mod)
            return mod;

        // we have package but not module
        return rootPkg->setModule(module, alias, m_frame);
    }

    // an error occured
    return nullptr;
}

Python::SourceImportPackage *Python::SourceImportList::setPackage(const std::list<const std::string> &rootPackage)
{
    Python::SourceImportPackage *rootPkg = nullptr;

    // no rootPackages
    if (rootPackage.size() < 1) {
       rootPkg = Python::SourceImportPackage::setPackage("");
       return rootPkg;
    }

    // lookup package
    rootPkg = getImportPackage(rootPackage);

    if (!rootPkg) {
        // we doesn't have rootPackage yet, recurse until we find it
        std::list<const std::string> pkg;
        for (auto &it : rootPackage) pkg.push_back(it);
        pkg.pop_back();
        rootPkg = setPackage(pkg);
        return rootPkg;
    }

    return rootPkg;
}

Python::SourceModule *Python::SourceImportList::setModuleGlob(const std::list<const std::string> &rootPackage)
{
    // FIXME implement
    (void)rootPackage;
    return nullptr;
}
