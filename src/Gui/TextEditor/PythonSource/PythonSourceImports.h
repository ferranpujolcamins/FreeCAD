#ifndef PYTHONSOURCEIMPORTS_H
#define PYTHONSOURCEIMPORTS_H

#include "PythonSource.h"
#include "PythonSourceListBase.h"
#include "PythonSourceRoot.h"

namespace Gui {
class PythonSourceImportPackage;
class PythonSourceFrame;

// a single import module, not a package
class PythonSourceImportModule : public PythonSourceListNodeBase
{
    PythonSourceFrame *m_frame;
    QString m_modulePath; // filepath to python file
    QString m_aliasName; // alias name in: import something as other <- other is alias
    PythonSourceRoot::TypeInfo m_type;

public:
    explicit PythonSourceImportModule(PythonSourceImportPackage *parent,
                                      PythonSourceFrame *frame,
                                      QString alias);
    ~PythonSourceImportModule();

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

    PythonSourceRoot::TypeInfo type() const { return m_type; }
    void setType(PythonSourceRoot::TypeInfo typeInfo) {
        if (typeInfo.isReferenceImported())
            m_type = typeInfo;
    }

    const PythonSourceFrame *frame() const { return m_frame; }
};

// ---------------------------------------------------------------------

class PythonSourceImportPackage : public PythonSourceListParentBase
{
    QString m_packagePath; // filepath to folder for this package
    QString m_name;
    const PythonSourceModule *m_ownerModule;
public:
    explicit PythonSourceImportPackage(PythonSourceImportPackage *parent,
                                       QString name, const PythonSourceModule *ownerModule);
    ~PythonSourceImportPackage();

    /// get import package from filePath
    virtual PythonSourceImportPackage *getImportPackagePath(QString filePath);
    /// get import instance from name (basename of filepath or alias)
    virtual PythonSourceImportPackage *getImportPackage(QString name);

    /// get import module instance from filePath
    virtual PythonSourceImportModule *getImportModulePath(QString filePath);
    /// get import module instance from name (basename of filepath or alias)
    virtual PythonSourceImportModule *getImportModule(QString name);


    /// lookup module and return it
    /// if not stored in list it creates it
    virtual PythonSourceImportModule *setModule(QString name, QString alias,
                                        PythonSourceFrame *frame);

    virtual PythonSourceImportPackage *setPackage(QString name);

    QString name() const { return m_name; }
    QString path() const { return m_packagePath;}

protected:
    int compare(const PythonSourceListNodeBase *left,
                const PythonSourceListNodeBase *right) const;
};

// ---------------------------------------------------------------------

class PythonSourceImportList : public PythonSourceImportPackage
{
    PythonSourceFrame *m_frame;
public:
    explicit PythonSourceImportList(PythonSourceFrame *owner,
                                    const PythonSourceModule *ownerModule);
    ~PythonSourceImportList();

    /// returns package instance stored in 'filePath' from list
    PythonSourceImportModule *getImportModulePath(QString filePath);
    /// returns module instance from list with 'name' in 'rootPackage'
    /// rootPackage might be empty, ie: 'import file2' <- here rootPackage is empty
    PythonSourceImportModule *getImportModule(QString rootPackage, QString name);
    /// returns module instance form list with module paths as QStingList
    ///  ie: import sys.path.sub
    PythonSourceImportModule *getImportModule(QStringList modInheritance);

    /// returns package instance stored in 'filePath' from list
    PythonSourceImportPackage *getImportPackagePath(QString filePath);
    /// returns package instance from list with 'name' in rootPackage
    /// rootPackage might be empty, ie: import sys <- here rootPackage is empty
    PythonSourceImportPackage *getImportPackage(QString rootPackage, QString name);
    /// returns package instance form list with module paths as QStringList
    ///  ie: import sys.path.sub
    PythonSourceImportPackage *getImportPackage(QStringList modInheritance);

    /// set import inserts module at packages path
    /// If already inserted we just return the module
    /// returns Module instance
    PythonSourceImportModule *setModule(QStringList rootPackage,
                                        QString module,
                                        QString alias = QString());

    PythonSourceImportPackage *setPackage(QStringList rootPackage);

    /// set import inserts module at packages path
    /// If already inserted we just return the module
    /// returns Module instance
    PythonSourceModule *setModuleGlob(QStringList rootPackage);
};

} // namespace Gui

#endif // PYTHONSOURCEIMPORTS_H
