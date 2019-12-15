#ifndef PYTHONSOURCEIMPORTS_H
#define PYTHONSOURCEIMPORTS_H

#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceRoot.h"

namespace Python {

class SourceImportPackage;
class SourceFrame;

// a single import module, not a package
class SourceImportModule : public Python::SourceListNodeBase
{
    Python::SourceFrame *m_frame;
    std::string m_modulePath; // filepath to python file
    std::string m_aliasName; // alias name in: import something as other <- other is alias
    Python::SourceRoot::TypeInfo m_type;

public:
    explicit SourceImportModule(Python::SourceImportPackage *parent,
                                Python::SourceFrame *frame,
                                const std::string &alias);
    ~SourceImportModule();

    /// returns the name as used when lookup, ie baseName or alias if present
    const std::string name() const;
    const std::string &alias() const { return m_aliasName; }
    const std::string &path() const { return m_modulePath; }
    void setPath(const std::string path) { m_modulePath = path; }
    /// we use defered load, this function explicitly loads module
    void load();
    bool isLoaded() const;

    /// true when its a C module
    bool isBuiltIn() const;
    /// false when import couldn't find file
    bool isValid() const;

    Python::SourceRoot::TypeInfo type() const { return m_type; }
    void setType(Python::SourceRoot::TypeInfo typeInfo) {
        if (typeInfo.isReferenceImported())
            m_type = typeInfo;
    }

    const Python::SourceFrame *frame() const { return m_frame; }
};

// ---------------------------------------------------------------------

class SourceImportPackage : public Python::SourceListParentBase
{
    std::string m_packagePath; // filepath to folder for this package
    std::string m_name;
    const Python::SourceModule *m_ownerModule;
public:
    explicit SourceImportPackage(Python::SourceImportPackage *parent,
                                 const std::string &name,
                                 const Python::SourceModule *ownerModule);
    ~SourceImportPackage();

    /// get import package from filePath
    virtual Python::SourceImportPackage *getImportPackagePath(const std::string &filePath);
    /// get import instance from name (basename of filepath or alias)
    virtual Python::SourceImportPackage *getImportPackage(const std::string &name);

    /// get import module instance from filePath
    virtual Python::SourceImportModule *getImportModulePath(const std::string &filePath);
    /// get import module instance from name (basename of filepath or alias)
    virtual Python::SourceImportModule *getImportModule(const std::string &name);


    /// lookup module and return it
    /// if not stored in list it creates it
    virtual Python::SourceImportModule *setModule(const std::string &name,
                                                  const std::string &alias,
                                                  Python::SourceFrame *frame);

    virtual Python::SourceImportPackage *setPackage(const std::string &name);

    const std::string &name() const { return m_name; }
    const std::string &path() const { return m_packagePath;}

protected:
    int compare(const Python::SourceListNodeBase *left,
                const Python::SourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------

class SourceImportList : public Python::SourceImportPackage
{
    Python::SourceFrame *m_frame;
public:
    explicit SourceImportList(Python::SourceFrame *owner,
                              const Python::SourceModule *ownerModule);
    ~SourceImportList();

    /// returns package instance stored in 'filePath' from list
    Python::SourceImportModule *getImportModulePath(const std::string &filePath);
    /// returns module instance from list with 'name' in 'rootPackage'
    /// rootPackage might be empty, ie: 'import file2' <- here rootPackage is empty
    Python::SourceImportModule *getImportModule(const std::string &rootPackage,
                                                const std::string &name);
    /// returns module instance form list with module paths as list of strings
    ///  ie: import sys.path.sub
    Python::SourceImportModule *getImportModule(const std::list<const std::string> &modInheritance);

    /// returns package instance stored in 'filePath' from list
    Python::SourceImportPackage *getImportPackagePath(const std::string &filePath);
    /// returns package instance from list with 'name' in rootPackage
    /// rootPackage might be empty, ie: import sys <- here rootPackage is empty
    Python::SourceImportPackage *getImportPackage(const std::string &rootPackage,
                                                  const std::string &name);
    /// returns package instance form list with module paths as QStringList
    ///  ie: import sys.path.sub
    Python::SourceImportPackage *getImportPackage(const std::list<const std::string> &modInheritance);

    /// set import inserts module at packages path
    /// If already inserted we just return the module
    /// returns Module instance
    Python::SourceImportModule *setModule(const std::list<const std::string> &rootPackage,
                                          const std::string &module,
                                          const std::string alias = std::string());

    Python::SourceImportPackage *setPackage(const std::list<const std::string> &rootPackage);

    /// set import inserts module at packages path
    /// If already inserted we just return the module
    /// returns Module instance
    Python::SourceModule *setModuleGlob(const std::list<const std::string> &rootPackage);
};

} // namespace Python

#endif // PYTHONSOURCEIMPORTS_H
