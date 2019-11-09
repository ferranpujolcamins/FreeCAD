#ifndef PYTHONSOURCEIMPORTS_H
#define PYTHONSOURCEIMPORTS_H

#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceRoot.h"

namespace Gui {
namespace Python {

class SourceImportPackage;
class SourceFrame;

// a single import module, not a package
class SourceImportModule : public Python::SourceListNodeBase
{
    Python::SourceFrame *m_frame;
    QString m_modulePath; // filepath to python file
    QString m_aliasName; // alias name in: import something as other <- other is alias
    Python::SourceRoot::TypeInfo m_type;

public:
    explicit SourceImportModule(Python::SourceImportPackage *parent,
                                Python::SourceFrame *frame,
                                QString alias);
    ~SourceImportModule();

    /// returns the name as used when lookup, ie baseName or alias if present
    QString name() const;
    QString alias() const { return m_aliasName; }
    QString path() const { return m_modulePath; }
    void setPath(QString path) { m_modulePath = path; }
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
    QString m_packagePath; // filepath to folder for this package
    QString m_name;
    const Python::SourceModule *m_ownerModule;
public:
    explicit SourceImportPackage(Python::SourceImportPackage *parent,
                                 QString name, const Python::SourceModule *ownerModule);
    ~SourceImportPackage();

    /// get import package from filePath
    virtual Python::SourceImportPackage *getImportPackagePath(QString filePath);
    /// get import instance from name (basename of filepath or alias)
    virtual Python::SourceImportPackage *getImportPackage(QString name);

    /// get import module instance from filePath
    virtual Python::SourceImportModule *getImportModulePath(QString filePath);
    /// get import module instance from name (basename of filepath or alias)
    virtual Python::SourceImportModule *getImportModule(QString name);


    /// lookup module and return it
    /// if not stored in list it creates it
    virtual Python::SourceImportModule *setModule(QString name, QString alias,
                                                  Python::SourceFrame *frame);

    virtual Python::SourceImportPackage *setPackage(QString name);

    QString name() const { return m_name; }
    QString path() const { return m_packagePath;}

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
    Python::SourceImportModule *getImportModulePath(QString filePath);
    /// returns module instance from list with 'name' in 'rootPackage'
    /// rootPackage might be empty, ie: 'import file2' <- here rootPackage is empty
    Python::SourceImportModule *getImportModule(QString rootPackage, QString name);
    /// returns module instance form list with module paths as QStingList
    ///  ie: import sys.path.sub
    Python::SourceImportModule *getImportModule(QStringList modInheritance);

    /// returns package instance stored in 'filePath' from list
    Python::SourceImportPackage *getImportPackagePath(QString filePath);
    /// returns package instance from list with 'name' in rootPackage
    /// rootPackage might be empty, ie: import sys <- here rootPackage is empty
    Python::SourceImportPackage *getImportPackage(QString rootPackage, QString name);
    /// returns package instance form list with module paths as QStringList
    ///  ie: import sys.path.sub
    Python::SourceImportPackage *getImportPackage(QStringList modInheritance);

    /// set import inserts module at packages path
    /// If already inserted we just return the module
    /// returns Module instance
    Python::SourceImportModule *setModule(QStringList rootPackage,
                                          QString module,
                                          QString alias = QString());

    Python::SourceImportPackage *setPackage(QStringList rootPackage);

    /// set import inserts module at packages path
    /// If already inserted we just return the module
    /// returns Module instance
    Python::SourceModule *setModuleGlob(QStringList rootPackage);
};

} // namespace Python
} // namespace Gui

#endif // PYTHONSOURCEIMPORTS_H
