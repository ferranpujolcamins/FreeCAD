#include "PythonSourceRoot.h"
#include "PythonSource.h"


#include <FCConfig.h>
#include "Base/Interpreter.h"
#include "PythonSourceModule.h"
#include <TextEditor/PythonCode.h>
#include <QDebug>
#include <QFileInfo>
#include <QTextDocument>

DBG_TOKEN_FILE

using namespace Gui;


// Begin Python Code AST methods (allthough no tree as we constantly keep changing src in editor)
// ------------------------------------------------------------------------

PythonSourceRoot::PythonSourceRoot() :
    m_uniqueCustomTypeNames(-1),
    m_modules(nullptr)
{
    m_modules = new PythonSourceModuleList();
}

PythonSourceRoot::~PythonSourceRoot()
{
    delete m_modules;
}

// static
PythonSourceRoot *PythonSourceRoot::instance()
{
    if (m_instance == nullptr)
        m_instance = new PythonSourceRoot;
    return m_instance;

}

QStringList PythonSourceRoot::modulesNames() const
{
    QStringList ret;
    if (!m_modules)
        return ret;

    for(PythonSourceListNodeBase *itm = m_modules->begin();
        itm != m_modules->end();
        itm->next())
    {
        PythonSourceModule *module = dynamic_cast<PythonSourceModule*>(itm);
        if (module)
            ret << module->moduleName();
    }

    return ret;
}

QStringList PythonSourceRoot::modulesPaths() const
{
    QStringList ret;

    for(PythonSourceListNodeBase *itm = m_modules->begin();
        itm != m_modules->end();
        itm->next())
    {
        PythonSourceModule *module = dynamic_cast<PythonSourceModule*>(itm);
        if (module)
            ret << module->filePath();
    }

    return ret;
}

int PythonSourceRoot::modulesCount() const
{
    return m_modules->size();
}

PythonSourceModule *PythonSourceRoot::moduleAt(int idx) const
{
    if (idx < 0)
        idx = m_modules->size() - idx; // reverse so -1 becomes last
    int i = 0;
    PythonSourceListNodeBase *mod = m_modules->begin();
    while(mod) {
        if (++i == idx)
            return dynamic_cast<PythonSourceModule*>(mod);
        mod = mod->next();
    }

    return nullptr;

}

PythonSourceModule *PythonSourceRoot::moduleFromPath(QString filePath) const
{
    for (PythonSourceListNodeBase *itm = m_modules->begin();
         itm != m_modules->end();
         itm = itm->next())
    {
        PythonSourceModule *module = dynamic_cast<PythonSourceModule*>(itm);
        if (module && module->filePath() == filePath)
            return module;
    }

    return nullptr;
}

PythonSourceRoot *PythonSourceRoot::m_instance = nullptr;

PythonSourceRoot::DataTypes PythonSourceRoot::mapMetaDataType(const QString typeAnnotation) const
{
    // try to determine type form type annotaion
    // more info at:
    // https://www.python.org/dev/peps/pep-0484/
    // https://www.python.org/dev/peps/pep-0526/
    // https://mypy.readthedocs.io/en/latest/cheat_sheet_py3.html
    if (typeAnnotation == QLatin1String("none"))
        return NoneType;
    if (typeAnnotation == QLatin1String("Object"))
        return ObjectType;
    else if (typeAnnotation == QLatin1String("bool"))
        return BoolType;
    else if (typeAnnotation == QLatin1String("int"))
        return IntType;
    else if (typeAnnotation == QLatin1String("float"))
        return FloatType;
    else if (typeAnnotation == QLatin1String("str"))
        return StringType;
    else if (typeAnnotation == QLatin1String("bytes"))
        return BytesType;
    else if (typeAnnotation == QLatin1String("List"))
        return ListType;
    else if (typeAnnotation == QLatin1String("Tuple"))
        return TupleType;
    else if (typeAnnotation == QLatin1String("Dict"))
        return DictType;
    else if (typeAnnotation == QLatin1String("Set"))
        return SetType;
    else if (typeAnnotation == QLatin1String("Range"))
        return RangeType;
    return CustomType;
}

PythonSourceRoot::TypeInfoPair PythonSourceRoot::identifierType(const PythonToken *tok,
                                                                const PythonSourceFrame *frame) const
{
    TypeInfoPair tp;
    if (tok || !tok->isIdentifier())
        return tp;

    switch (tok->token) {
    case PythonSyntaxHighlighter::T_IdentifierBuiltin:
        return builtinType(tok, frame);
    case PythonSyntaxHighlighter::T_IdentifierClass:
        tp.thisType.type = ClassType;
        return tp;
    case PythonSyntaxHighlighter::T_IdentifierDecorator:
        tp.thisType.type = MethodDescriptorType;
        return tp;
    case PythonSyntaxHighlighter::T_IdentifierDefined: // fallthrough
    case PythonSyntaxHighlighter::T_IdentifierDefUnknown:
        tp.thisType.type = FunctionType; // method or function not known yet
        tp.returnType.type = UnknownType;
        return tp;
    case PythonSyntaxHighlighter::T_IdentifierTrue: // fallthrough
    case PythonSyntaxHighlighter::T_IdentifierFalse:
        tp.thisType.type = BoolType;
        return tp;
    case PythonSyntaxHighlighter::T_IdentifierFunction:
        // figure out how to lookp function return statments
    case PythonSyntaxHighlighter::T_IdentifierMethod:
        // figure out how to lookp function return statments
        return tp;
    case PythonSyntaxHighlighter::T_IdentifierModule:
    case PythonSyntaxHighlighter::T_IdentifierModuleAlias:
    case PythonSyntaxHighlighter::T_IdentifierModuleGlob:
    case PythonSyntaxHighlighter::T_IdentifierModulePackage:
        //tp.thisType.type = ; // how do we lookup modules?
        return tp;
    case PythonSyntaxHighlighter::T_IdentifierNone:
        tp.thisType.type = NoneType;
        return tp;
    case PythonSyntaxHighlighter::T_IdentifierSelf:
        tp.thisType.type = ReferenceArgumentType;
        return tp;
    case PythonSyntaxHighlighter::T_IdentifierSuperMethod:
    case PythonSyntaxHighlighter::T_IdentifierUnknown:
        tp.thisType.type = UnknownType;
        return tp;
    default:
        // invalid?
        qDebug() << QString::fromLatin1("Invalid token: id:%1 with text:%2")
                            .arg(QString::number(tok->token)).arg(tok->text()) << endl;
#ifdef DEBUG_TOKENS
        assert(tp.thisType.type != PythonSourceRoot::InValidType && "Invalid Token!");
#endif
        break;
    }

    return tp;
}

PythonSourceRoot::TypeInfoPair PythonSourceRoot::builtinType(const PythonToken *tok, const PythonSourceFrame *frame) const
{
    Q_UNUSED(frame);
    TypeInfoPair tp;

    if (tok && tok->token == PythonSyntaxHighlighter::T_IdentifierBuiltin) {
        QString text = tok->text();
        // handle built in functions

        tp.thisType.type = BuiltinFunctionType;
        // https://docs.python.org/3/library/functions.html
        if (text == QLatin1String("abs")) {
            tp.returnType.type = FloatType;
        } else if (text == QLatin1String("all")) {
            tp.returnType.type = BoolType;
        } else if (text == QLatin1String("any")) {
            tp.returnType.type = BoolType;
        } else if (text == QLatin1String("ascii")) {
            tp.returnType.type = StringType;
        } else if (text == QLatin1String("bin")) {
            tp.returnType.type = StringType;
        } else if (text == QLatin1String("bool")) {
            tp.returnType.type = BoolType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("breakpoint")) {
            tp.returnType.type = VoidType;
        } else if (text == QLatin1String("bytesarray")) {
            tp.returnType.type = BytesType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("bytes")) {
            tp.returnType.type = BytesType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("callable")) {
            tp.returnType.type = BoolType;
        } else if (text == QLatin1String("chr")) {
            tp.returnType.type = StringType;
        } else if (text == QLatin1String("compile")) {
            tp.returnType.type = CodeType;
        } else if (text == QLatin1String("complex")) {
            tp.returnType.type = ComplexType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("delattr")) {
            tp.returnType.type = VoidType;
        } else if (text == QLatin1String("dict")) {
            tp.returnType.type = DictType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("dir")) {
            tp.returnType.type = ListType;
        } else if (text == QLatin1String("divmod")) {
            tp.returnType.type = IntType;
        } else if (text == QLatin1String("enumerate")) {
            tp.returnType.type = EnumerateType;
        } else if (text == QLatin1String("eval")) {
            tp.returnType.type = ObjectType;
            // we dont know what type is returned,
            // might be str, int or anything realy
        } else if (text == QLatin1String("exec")) {
            tp.returnType.type = NoneType;
        } else if (text == QLatin1String("filter")) {
            tp.returnType.type = IterableType;
        } else if (text == QLatin1String("float")) {
            tp.returnType.type = FloatType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("format")) {
            tp.returnType.type = StringType;
        } else if (text == QLatin1String("frozenset")) {
            tp.returnType.type = FrozenSetType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("getattr")) {
            tp.returnType.type = ObjectType;
            // we dont know what value is returned
        } else if (text == QLatin1String("Globals")) {
            tp.returnType.type = DictType;
        } else if (text == QLatin1String("hasattr")) {
            tp.returnType.type = BoolType;
        } else if (text == QLatin1String("hash")) {
            tp.returnType.type = IntType;
        } else if (text == QLatin1String("help")) {
            tp.returnType.type = VoidType;
        } else if (text == QLatin1String("hex")) {
            tp.returnType.type = StringType;
        } else if (text == QLatin1String("id")) {
            tp.returnType.type = IntType;
        } else if (text == QLatin1String("input")) {
            tp.returnType.type = StringType;
        } else if (text == QLatin1String("int")) {
            tp.returnType.type = IntType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("isinstance")) {
            tp.returnType.type = BoolType;
        } else if (text == QLatin1String("issubclass")) {
            tp.returnType.type = BoolType;
        } else if (text == QLatin1String("iter")) {
            tp.returnType.type = IterableType;
        } else if (text == QLatin1String("len")) {
            tp.returnType.type = IntType;
        } else if (text == QLatin1String("list")) {
            tp.returnType.type = ListType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("locals")) {
            tp.returnType.type = DictType;
        } else if (text == QLatin1String("map")) {
            tp.returnType.type = IterableType;
        } else if (text == QLatin1String("max")) {
            tp.returnType.type = ObjectType;
        } else if (text == QLatin1String("memoryiew")) {
            tp.returnType.type = ClassType;
        } else if (text == QLatin1String("min")) {
            tp.returnType.type = ObjectType;
        } else if (text == QLatin1String("next")) {
            tp.returnType.type = ObjectType;
        } else if (text == QLatin1String("object")) {
            tp.returnType.type = ObjectType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("oct")) {
            tp.returnType.type = StringType;
        } else if (text == QLatin1String("open")) {
            tp.returnType.type = FileType;
        } else if (text == QLatin1String("ord")) {
            tp.returnType.type = IntType;
        } else if (text == QLatin1String("pow")) {
            tp.returnType.type = FloatType;
            // might also return int, but default to float
        } else if (text == QLatin1String("print")) {
            tp.returnType.type = VoidType;
        } else if (text == QLatin1String("property")) {
            tp.returnType.type = ClassType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("range")) {
            tp.returnType.type = IterableType;
        } else if (text == QLatin1String("repr")) {
            tp.returnType.type = StringType;
        } else if (text == QLatin1String("reversed")) {
            tp.returnType.type = IterableType;
        } else if (text == QLatin1String("round")) {
            tp.returnType.type = FloatType;
        } else if (text == QLatin1String("set")) {
            tp.returnType.type = SetType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("setattr")) {
            tp.returnType.type = VoidType;
        } else if (text == QLatin1String("slice")) {
            tp.returnType.type = IterableType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("sorted")) {
            tp.returnType.type = ListType;
        } else if (text == QLatin1String("str")) {
            tp.returnType.type = StringType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("sum")) {
            tp.returnType.type = FloatType;
        } else if (text == QLatin1String("super")) {
            tp.returnType.type = ClassType;
        } else if (text == QLatin1String("tuple")) {
            tp.returnType.type = TupleType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("type")) {
            tp.returnType.type = TypeObjectType;
        } else if (text == QLatin1String("unicode")) {
            tp.returnType.type = StringType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == QLatin1String("vars")) {
            tp.returnType.type = DictType;
        } else if (text == QLatin1String("zip")) {
            tp.returnType.type = IterableType;
        } else if (text == QLatin1String("__import__")) {
            tp.returnType.type = ReferenceImportBuiltInType;

        // handle @methods
        } else if (tok->token == PythonSyntaxHighlighter::T_IdentifierDecorator) {
            if (text == QLatin1String("@classmethod")) {
                tp.returnType.type = ClassMethodDescriptorType;
            }
        }
    }
    return tp;
}

bool PythonSourceRoot::isLineEscaped(const PythonToken *tok) const {
    DEFINE_DBG_VARS
    PREV_TOKEN(tok)
    int guard = 40;
    bool escaped = false;
    while (tok && (guard--)) {
        if (tok->token == PythonSyntaxHighlighter::T_DelimiterLineContinue)
            escaped = true;
        else
            escaped = false;
        if (tok->token == PythonSyntaxHighlighter::T_DelimiterNewLine)
            break;
        NEXT_TOKEN(tok)
    }
    return escaped;
}

QString PythonSourceRoot::customTypeNameFor(PythonSourceRoot::CustomNameIdx_t customIdx)
{
    return m_customTypeNames[customIdx];
}

PythonSourceRoot::CustomNameIdx_t PythonSourceRoot::addCustomTypeName(const QString name)
{
    m_customTypeNames[++m_uniqueCustomTypeNames] = name;
    return m_uniqueCustomTypeNames;
}

PythonSourceRoot::CustomNameIdx_t PythonSourceRoot::indexOfCustomTypeName(const QString name)
{
    if (m_customTypeNames.values().contains(QString(name)))
        return m_customTypeNames.key(QString(name));
    return -1;
}

PythonSourceModule *PythonSourceRoot::scanCompleteModule(const QString filePath,
                                                         PythonSyntaxHighlighter *highlighter)
{
    DEFINE_DBG_VARS

    // delete old data
    PythonSourceModule *mod = moduleFromPath(filePath);
    if (mod) {
        m_modules->remove(mod, true);
        mod = nullptr;
    }

    // create a new module
    mod = new PythonSourceModule(this, highlighter);
    mod->setFilePath(filePath);
    QFileInfo fi(filePath);
    mod->setModuleName(fi.baseName());
    m_modules->insert(mod);

    // find the first token of any kind
    // firstline might be empty
    const PythonTextBlockData *txtData = dynamic_cast<PythonTextBlockData*>(
                                            highlighter->document()->begin().userData());
    if (txtData) {
        PythonToken *tok = txtData->tokenAt(0);
        DBG_TOKEN(tok)
        if (tok)
            mod->scanFrame(tok);
    }

    return mod;
}

PythonSourceModule *PythonSourceRoot::scanSingleRowModule(const QString filePath,
                                                          PythonTextBlockData *row,
                                                          PythonSyntaxHighlighter *highlighter)
{
    PythonSourceModule *mod = moduleFromPath(filePath);
    if (!mod) {
        // create a new module
        mod = new PythonSourceModule(this, highlighter);
        mod->setFilePath(filePath);
        QFileInfo fi(filePath);
        mod->setModuleName(fi.baseName());
        m_modules->insert(mod);
    }

    if (!row->isEmpty()) {
        mod->scanLine(row->tokenAt(0));
    }

    return mod;
}


// we try to find out if statement is a compare statement or normal assigment
// if it contains a compare operator result is Bool
// otherwise its a type from Indentifier on the right
// 1 and 2     <- Bool
// var1 < var2 <- Bool
// var2 + var 3 & (intvar4) <- int due to bitwise operator
// 0.3 + 1 * intVar         <- float due to 0.3
// var1[2]     <- unknown due to we don't know what type is in list
// var2[var4]  <- unknown same as above
// var1.attr   <- lookup identifiers 'var1' and 'attr' to determine this type, limit in lookup chain
// var3 += 2   <- int due to '2'
PythonSourceRoot::TypeInfo
PythonSourceRoot::statementResultType(const PythonToken *startToken,
                                      const PythonSourceFrame *frame) const
{
    DEFINE_DBG_VARS

    TypeInfo typeInfo;
    if (!startToken)
        return typeInfo;

    int parenOpenCnt = 0,
        parenCloseCnt = 0,
        braceOpenCnt = 0,
        braceCloseCnt = 0,
        bracketOpenCnt = 0,
        bracketCloseCnt = 0;


    const PythonToken *tok = startToken;
    DBG_TOKEN(tok)

    // first we scan to end of statement
    // then operate on that list
    QList<const PythonToken*> allTokens;
    QList<int> parenPos;


    static const int maxStatements = 50;
    int pos = 0; // use as guard for max tokens and parenPos
    do {
        switch (tok->token) {
        case PythonSyntaxHighlighter::T_DelimiterOpenParen:
            if (parenOpenCnt != parenCloseCnt) {
                frame->module()->setSyntaxError(tok, QString::fromLatin1("Parens mismatch '('..')' in statement"));
                return typeInfo;
            }
            parenPos.push_front(pos);
            parenOpenCnt++;
            break;
        case PythonSyntaxHighlighter::T_DelimiterCloseParen:
            parenPos.push_front(pos);
            parenCloseCnt++;
            if (parenOpenCnt != parenCloseCnt) {
                frame->module()->setSyntaxError(tok, QString::fromLatin1("Parens mismatch '('..')' in statement"));
                return typeInfo;
            }
            break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBrace:
            if (braceOpenCnt != braceCloseCnt) {
                frame->module()->setSyntaxError(startToken, QString::fromLatin1("Braces mismatch in statement '{'..'}'"));
                return typeInfo;
            }
            braceOpenCnt++;
            break;
        case PythonSyntaxHighlighter::T_DelimiterCloseBrace:
            braceCloseCnt++;
            if (braceOpenCnt != braceCloseCnt) {
                frame->module()->setSyntaxError(startToken, QString::fromLatin1("Braces mismatch in statement '{'..'}'"));
                return typeInfo;
            }
            break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBracket:
            if (bracketOpenCnt != bracketCloseCnt) {
                frame->module()->setSyntaxError(startToken, QString::fromLatin1("Brackets mismatch in statment '['..']'"));
                return typeInfo;
            }
            bracketOpenCnt++;
            break;
        case PythonSyntaxHighlighter::T_DelimiterCloseBracket:
            bracketCloseCnt++;
            if (bracketOpenCnt != bracketCloseCnt) {
                frame->module()->setSyntaxError(startToken, QString::fromLatin1("Brackets mismatch in statment '['..']'"));
                return typeInfo;
            }
            break;
        case PythonSyntaxHighlighter::T_DelimiterSemiColon:
            pos = 0;  break; // break do while loop
        default:
            allTokens.push_back(tok);
            break;
        }
        if (!tok->isText())
            break;
        NEXT_TOKEN(tok)
    } while (tok && (pos++) > maxStatements);

    // track a dangling ( at end
    if (parenPos.size() % 2) {
        frame->module()->setSyntaxError(startToken, QString::fromLatin1("Parens mismatch '('..')' in statement"));
        return typeInfo;
    }

    // we have already type determined
    if (typeInfo.isValid())
        return typeInfo;

    // split on paren sub statements ie var1 * (var2 +1)
    QList<QList<const PythonToken*> > subStatements;
    int endPos = 0;
    for (int pos : parenPos) {
        // close paren is stored first ie even pos
        if (endPos % 2) {
            // opening paren
            subStatements.push_back(allTokens.mid(pos+1, pos+1 - endPos-1));
        } else {
            // closing paren
            endPos = pos;
        }
    }

    TypeInfo tpNext;
    tok = computeStatementResultType(frame, tok, tpNext);
    DBG_TOKEN(tok)
    if (!tpNext.isValid())
        return typeInfo; // got to something we couldn't handle

    return tpNext;
}

const PythonToken
*PythonSourceRoot::computeStatementResultType(const PythonSourceFrame *frame,
                                              const PythonToken *startTok,
                                              TypeInfo &typeInfo) const
{
    DEFINE_DBG_VARS
    // first look for encapsulating brackets and parens ie '[..]' or '(..)'
    // a '{..}' is a dict and retuned as that, advances startToken

    const PythonToken *tok = startTok;

    int guard = 10;

//next_loop:
    DBG_TOKEN(tok)

    while(tok && (guard--)) {
        switch (tok->token) {
        case PythonSyntaxHighlighter::T_DelimiterOpenParen:
 //           ++parenCnt;
            // recurse into this '(..)'
            return computeStatementResultType(frame, tok->next(), typeInfo);
//            goto next_loop;
            break;
        case PythonSyntaxHighlighter::T_DelimiterCloseParen:
//            --parenCnt;
            break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBracket:
            // recurese into '[..]'
//            ++bracketCnt;
            return computeStatementResultType(frame, tok->next(), typeInfo);
//            goto next_loop;
        case PythonSyntaxHighlighter::T_DelimiterCloseBracket:
//            --bracketCnt;
            break;
        case PythonSyntaxHighlighter::T_DelimiterOpenBrace:
            typeInfo.type = DictType;
            // TODO go to end of dict
            break;
        case PythonSyntaxHighlighter::T_DelimiterSemiColon:
            // end of statement
            return tok;
        case PythonSyntaxHighlighter::T_DelimiterNewLine:
            if (!isLineEscaped(tok))
                return tok;
            break;
        default: {
            const PythonSourceIdentifier *ident = nullptr;
            PythonSourceIdentifierAssignment *assign = nullptr;
            if (tok->isIdentifierVariable()) {
                ident = frame->getIdentifier(tok->text());
                if (ident) {
                    assign = ident->getTypeHintAssignment(tok);
                    if (assign) {
                        // has typehint
                        typeInfo = assign->typeInfo();
                        return tok;
                    } else {
                        // traverse lookup chain to find identifier declaration
                        ident = frame->identifiers().getIdentifierReferencedBy(tok);
                    }
                }

                if (ident) {
                    // get the last assignment to this variable
                    assign = ident->getFromPos(tok); //ident->token());
                    if (assign->typeInfo().isValid()) {
                        if (assign->typeInfo().isCallable()) {
                            // maybe this frame has a return typehint
                            typeInfo = ident->frame()->returnTypeHint();
                            if (typeInfo.isValid())
                                return tok;
                            // if this callable has muliple return types we can't determine which
                            // so we bail out
                            PythonSourceFrameReturnType *frmType = dynamic_cast<PythonSourceFrameReturnType*>(ident->frame()->returnTypes().begin());
                            if (frmType)
                                typeInfo.type = frmType->typeInfo().type;

                            return tok;
                        }
                        // not callable,
                        TypeInfoPair typePair = identifierType(assign->token(),
                                                               ident->frame());
                        typeInfo = typePair.thisType;
                    }
                }

                // not found set type to unknown
                typeInfo.type = UnknownType;


            } else if (tok->isIdentifierDeclaration()) {
                // is known at parse time, such as a "str" or a number '123'
                TypeInfoPair typePair = this->identifierType(tok, frame);
                typeInfo = typePair.thisType;
            }
            break;
        } // end default
        } // end switch
        NEXT_TOKEN(tok)
    }
    return tok;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// this struct is contained by PythonSourceRoot
PythonSourceRoot::TypeInfo::TypeInfo() :
    type(PythonSourceRoot::InValidType),
    customNameIdx(-1)
{
}

PythonSourceRoot::TypeInfo::TypeInfo(PythonSourceRoot::DataTypes type) :
    type(type),
    customNameIdx(-1)
{
}

PythonSourceRoot::TypeInfo::TypeInfo(const PythonSourceRoot::TypeInfo &other) :
    type(other.type),
    customNameIdx(other.customNameIdx),
    referenceName(other.referenceName)
{
}


PythonSourceRoot::TypeInfo::~TypeInfo()
{
}

QString PythonSourceRoot::TypeInfo::typeAsStr() const
{
    const char *typeAsStr;
    static const char *errStr = "<Typename lookup error>";

    switch (type) {
    case PythonSourceRoot::InValidType:         typeAsStr = "InValidType"; break;
    case PythonSourceRoot::UnknownType:         typeAsStr = "UnknownType"; break;
    // when no return value
    case PythonSourceRoot::VoidType:            typeAsStr = "VoidType"; break;

    case PythonSourceRoot::ReferenceType:       typeAsStr = "ReferenceType"; break;
    case PythonSourceRoot::ReferenceArgumentType:typeAsStr = "ReferenceArgumentType"; break;
    case PythonSourceRoot::ReferenceBuiltInType:typeAsStr = "ReferenceBuiltInType"; break;

    case PythonSourceRoot::ReferenceImportUndeterminedType:
                                                typeAsStr = "ReferenceImportUndeterminedType"; break;
    case PythonSourceRoot::ReferenceImportErrorType:
                                                typeAsStr = "ReferenceImportErrorType"; break;
    case PythonSourceRoot::ReferenceImportPythonType:
                                                typeAsStr = "ReferenceImportPythonType"; break;
    case PythonSourceRoot::ReferenceImportBuiltInType:
                                                typeAsStr = "ReferenceImportBuiltInType"; break;


    case PythonSourceRoot::FunctionType:        typeAsStr = "FunctionType"; break;
    case PythonSourceRoot::LambdaType:          typeAsStr = "LambdaType";   break;
    case PythonSourceRoot::GeneratorType:       typeAsStr = "GeneratorType";break;
    case PythonSourceRoot::CoroutineType:       typeAsStr = "CoroutineType";break;
    case PythonSourceRoot::CodeType:            typeAsStr = "CodeType";     break;
    case PythonSourceRoot::MethodType:          typeAsStr = "MethodType";   break;
    case PythonSourceRoot::BuiltinFunctionType: typeAsStr = "BuiltinFunctionType"; break;
    case PythonSourceRoot::BuiltinMethodType:   typeAsStr = "BuiltinMethodType"; break;
    case PythonSourceRoot::WrapperDescriptorType: typeAsStr = "WrapperDescriptorType"; break;
    case PythonSourceRoot::MethodWrapperType:   typeAsStr = "MethodWrapperType"; break;
    case PythonSourceRoot::MethodDescriptorType:typeAsStr = "MethodDescriptorType"; break;
    case PythonSourceRoot::ClassMethodDescriptorType: typeAsStr = "ClassMethodDescriptorType"; break;
    case PythonSourceRoot::ModuleType:          typeAsStr = "ModuleType"; break;// is root frame for file
    case PythonSourceRoot::TracebackType:       typeAsStr = "TracebackType"; break;
    case PythonSourceRoot::FrameType:           typeAsStr = "FrameType";   break;
    case PythonSourceRoot::GetSetDescriptorType:typeAsStr = "GetSetDescriptorType"; break;
    case PythonSourceRoot::MemberDescriptorType:typeAsStr = "MemberDescriptorType"; break;
    case PythonSourceRoot::MappingProxyType:    typeAsStr = "MappingProxyType";   break;

    case PythonSourceRoot::TypeObjectType:      typeAsStr = "TypeObjectType"; break;
    case PythonSourceRoot::ObjectType:          typeAsStr = "ObjectType";   break;
    case PythonSourceRoot::NoneType:            typeAsStr = "NoneType";     break;
    case PythonSourceRoot::BoolType:            typeAsStr = "BoolType";     break;
    case PythonSourceRoot::IntType:             typeAsStr = "IntType";      break;
    case PythonSourceRoot::FloatType:           typeAsStr = "FloatType";    break;
    case PythonSourceRoot::StringType:          typeAsStr = "StringType";   break;
    case PythonSourceRoot::BytesType:           typeAsStr = "BytesType";    break;
    case PythonSourceRoot::ListType:            typeAsStr = "ListType";     break;
    case PythonSourceRoot::TupleType:           typeAsStr = "TupleType";    break;
    case PythonSourceRoot::SetType:             typeAsStr = "SetType";      break;
    case PythonSourceRoot::FrozenSetType:       typeAsStr = "FrozensetType";break;
    case PythonSourceRoot::RangeType:           typeAsStr = "RangeType";    break;
    case PythonSourceRoot::DictType:            typeAsStr = "DictType";     break;
    case PythonSourceRoot::ClassType:           typeAsStr = "ClassType";    break;
    case PythonSourceRoot::ComplexType:         typeAsStr = "ComplexType";  break;
    case PythonSourceRoot::EnumerateType:       typeAsStr = "EnumerateType";break;
    case PythonSourceRoot::IterableType:        typeAsStr = "IterableType"; break;
    case PythonSourceRoot::FileType:            typeAsStr = "FileType";     break;
    case PythonSourceRoot::CustomType:
        if (customNameIdx > -1) {
            return customName();
        }
        return QLatin1String(errStr);
        break;
    default:
        return QLatin1String(errStr);
    }

    return QLatin1String(typeAsStr);
}

QString PythonSourceRoot::TypeInfo::customName() const
{
    if (customNameIdx > -1) {
        return PythonSourceRoot::instance()->customTypeNameFor(customNameIdx);
    }
    return QString();
}

#include "moc_PythonSourceRoot.cpp"
