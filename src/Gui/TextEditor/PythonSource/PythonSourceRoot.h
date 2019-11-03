#ifndef PYTHONSOURCEROOT_H
#define PYTHONSOURCEROOT_H


#include <FCConfig.h>
#include "Base/Interpreter.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>

namespace Gui {

// Begin AST Python source code introspection
// -------------------------------------------------------------------

//class PythonSourceListParentBase;
//class PythonSourceIdentifierAssignment;
//class PythonSourceIdentifier;
//class PythonSourceIdentifierList;
//class PythonSourceTypeHint;
class PythonSourceFrame;
class PythonSourceModule;
class PythonSourceModuleList;
//class PythonSourceParameter;
//class PythonSourceParameterList;
//class PythonSourceImportPackage;
//class PythonSourceImportModule;
//class PythonSourceImportList;
class PythonSyntaxHighlighter;
class PythonToken;
class PythonTextBlockData;

/**
 * @brief The PythonCodeTree class hold all frames, vars and other identifiers
 * used to get info about src
 * Is intended as a singleton!
 */
class PythonSourceRoot : public QObject
{
    Q_OBJECT
public:
    typedef int CustomNameIdx_t;
    enum DataTypes {
        // changes here must be accompanied by changing in typeAsStr and mapDataType
        // and in is* functions
        InValidType,
        UnknownType,
        VoidType,

        // reference types
        /// references another variable
        /// ie. "var1 = var" where var is also a reference type
        ReferenceType,
        /// references a callable type
        /// ie. "def func():"
        ///     " ...."
        ///     "func()" func here in this line is a ReferenceCallableType
        ReferenceCallableType,
        /// references a argument in this function/method arguments list
        /// ie. "def func(arg1)" arg1 is a ReferenceArgument
        ReferenceArgumentType,
        /// is a ref. to builtin function/method identifier
        /// ie. "var1=print" var1 references builtin print
        ReferenceBuiltInType,
        /*
        /// is a ref to a import statements
        /// identifier is imported into current frame,
        /// not sure yet if it's a py module or C
        /// ie. "import foo"
        ///     "foo ..." foo is now a ReferenceImportUndetermined
        ReferenceImportUndeterminedType,
        /// is a ref to a import that could not be found
        /// ie. "import baz" and baz wasnt found it is now  a ReferenceImportErrorType
        ReferenceImportErrorType,
        /// identifier is imported into current frame, is a Py module
        /// ie. "import myPy" myPy is a regular python script (module)
        ReferenceImportPythonType,
        /// identifier is imported into current frame, is C module
        /// ie. "import fastModule" fastModule is a C module
        ReferenceImportBuiltInType,
        */
        /// identifier is imported into current frame
        /// ie. "import fastModule" fastModule is a module
        ReferenceImportType,


        // standard builtin types
        // https://docs.python.org/3/library/types.html
        FunctionType,
        LambdaType,
        GeneratorType,
        CoroutineType, // no support for this yet
        CodeType,
        MethodType,
        BuiltinFunctionType,
        BuiltinMethodType,
        WrapperDescriptorType,
        MethodWrapperType,
        MethodDescriptorType,
        ClassMethodDescriptorType,
        ModuleType, // is root frame, for imports use: ReferenceImport*
        TracebackType,
        FrameType,
        GetSetDescriptorType,
        MemberDescriptorType,
        MappingProxyType,

        TypeObjectType,
        ObjectType,
        NoneType,
        BoolType,
        IntType,
        FloatType,
        StringType,
        BytesType,
        ListType,
        TupleType,
        SetType,
        FrozenSetType,
        RangeType,
        DictType,
        ClassType,
        ComplexType,
        EnumerateType,
        IterableType,
        FileType,

        // a special Custom Type
        CustomType
    };

    /// for a identifier type
    struct TypeInfo {
        explicit TypeInfo();
        explicit TypeInfo(PythonSourceRoot::DataTypes type);
        TypeInfo(const TypeInfo &other);
        ~TypeInfo();
        PythonSourceRoot::DataTypes type;
        CustomNameIdx_t customNameIdx;
        QString referenceName;
        QString typeAsStr() const;
        QString customName() const;
        bool operator ==(const TypeInfo &other) const {
            return type == other.type &&
                   customNameIdx == other.customNameIdx &&
                   referenceName == other.referenceName;
        }
        bool operator !=(const TypeInfo &other) const {
            return false == (*this == other);
        }
        TypeInfo operator =(const TypeInfo &other) {
            type = other.type;
            customNameIdx = other.customNameIdx;
            referenceName = other.referenceName;
            return *this;
        }
        /// references another variable
        bool isReference() const {
            return type >= ReferenceType && type <= ReferenceImportType;
        }

        /// imported as into current frame
        bool isReferenceImported() const {
            return type >= ReferenceImportType;

            //return type >= ReferenceImportUndeterminedType &&
            //       type <= ReferenceImportBuiltInType;
        }

        /// if you can call this type
        bool isCallable() const {
            switch (type) {
            case FunctionType: case LambdaType: case CoroutineType:
            case MethodType: case BuiltinFunctionType: case BuiltinMethodType:
            case MethodDescriptorType: case ClassMethodDescriptorType:
                return true;
            default:
                return false;
            }
        }
        bool isValid() const { return type != InValidType; }
    };
    /// return type from functions/methods/properties
    struct TypeInfoPair {
        TypeInfo returnType;
        TypeInfo thisType;
        bool isReturnType() const { return !returnType.isValid(); }
        bool isValid() const { return thisType.isValid(); }
    };

    // please not that it must be a singleton!
    explicit PythonSourceRoot();
    virtual ~PythonSourceRoot();

    /// get a global reference to our singleton instance
    static PythonSourceRoot *instance();

    /// Gets a stringlist of all loaded modules names
    QStringList modulesNames() const;
    /// Gets a stringlist of all loaded modules paths
    QStringList modulesPaths() const;
    /// gets modules count, -1 if none yet
    int modulesCount() const;
    /// get Module at idx, nullptr if not found
    PythonSourceModule *moduleAt(int idx) const;
    /// get Module from path
    PythonSourceModule *moduleFromPath(QString filePath) const;
    /// get reference to our modules collection
    const PythonSourceModuleList *modules() const { return m_modules; }

    /// map typestr from metadata Type annotation, ie x: int
    DataTypes mapMetaDataType(const QString typeAnnotation) const;

    /// get the type for this token, token must be a Identifier
    /// else it returns a inValid TypeInfo
    TypeInfoPair identifierType(const PythonToken *tok,
                                const PythonSourceFrame *frame) const;
    TypeInfoPair builtinType(const PythonToken *tok,
                             const PythonSourceFrame *frame) const;

    /// get the type of number of this token
    DataTypes numberType(const PythonToken *tok) const;

    /// true if tok is a newline and previous token was a escape char
    bool isLineEscaped(const PythonToken *tok) const;

    // typename database for custom types
    QString customTypeNameFor(CustomNameIdx_t customIdx);
    CustomNameIdx_t addCustomTypeName(const QString name);
    CustomNameIdx_t indexOfCustomTypeName(const QString name);

    /// scans complete filePath, clears all old and re-doe it
    PythonSourceModule *scanCompleteModule(const QString filePath,
                                           PythonSyntaxHighlighter *highlighter);
    /// re-scan a single QTextBlock, as in we are typing
    PythonSourceModule *scanSingleRowModule(const QString filePath,
                                            PythonTextBlockData *row,
                                            PythonSyntaxHighlighter *highlighter);

    /// computes the return type of statement pointed to by startToken
    /// NOTE it has limitations, it isn' a fullblown interpreter
    TypeInfo statementResultType(const PythonToken *startToken, const PythonSourceFrame *frame) const;


private:
    QHash<CustomNameIdx_t, QString> m_customTypeNames;
    CustomNameIdx_t m_uniqueCustomTypeNames;
    PythonSourceModuleList *m_modules;
    static PythonSourceRoot *m_instance;

    const PythonToken *computeStatementResultType(const PythonSourceFrame *frame,
                                                  const PythonToken *startTok,
                                                  TypeInfo &typeInfo) const;

    //PythonToken *splitStmtParts(PythonToken, );

};

} // namespace Gui

#endif // PYTHONSOURCEROOT_H
