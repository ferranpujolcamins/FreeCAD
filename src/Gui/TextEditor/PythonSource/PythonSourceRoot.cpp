#include "PythonSourceRoot.h"
#include "PythonSource.h"
#include "PythonSourceModule.h"
#include "PythonSourceDebugTools.h"


#include <FCConfig.h>
#include "Base/Interpreter.h"
//#include <TextEditor/PythonCode.h>
#include <TextEditor/PythonSyntaxHighlighter.h>
#include <QDebug>
#include <QFileInfo>
#include <QTextDocument>

DBG_TOKEN_FILE

using namespace Gui;


// Begin Python Code AST methods (allthough no tree as we constantly keep changing src in editor)
// ------------------------------------------------------------------------

Python::SourceRoot::SourceRoot() :
    m_uniqueCustomTypeNames(-1),
    m_modules(nullptr)
{
    m_modules = new Python::SourceModuleList();
}

Python::SourceRoot::~SourceRoot()
{
    delete m_modules;
}

// static
Python::SourceRoot *Python::SourceRoot::instance()
{
    if (m_instance == nullptr)
        m_instance = new Python::SourceRoot;
    return m_instance;

}

QStringList Python::SourceRoot::modulesNames() const
{
    QStringList ret;
    if (!m_modules)
        return ret;

    for(Python::SourceListNodeBase *itm = m_modules->begin();
        itm != m_modules->end();
        itm->next())
    {
        Python::SourceModule *module = dynamic_cast<Python::SourceModule*>(itm);
        if (module)
            ret << module->moduleName();
    }

    return ret;
}

QStringList Python::SourceRoot::modulesPaths() const
{
    QStringList ret;

    for(Python::SourceListNodeBase *itm = m_modules->begin();
        itm != m_modules->end();
        itm->next())
    {
        Python::SourceModule *module = dynamic_cast<Python::SourceModule*>(itm);
        if (module)
            ret << module->filePath();
    }

    return ret;
}

int Python::SourceRoot::modulesCount() const
{
    return m_modules->size();
}

Python::SourceModule *Python::SourceRoot::moduleAt(int idx) const
{
    if (idx < 0)
        idx = m_modules->size() - idx; // reverse so -1 becomes last
    int i = 0;
    Python::SourceListNodeBase *mod = m_modules->begin();
    while(mod) {
        if (++i == idx)
            return dynamic_cast<Python::SourceModule*>(mod);
        mod = mod->next();
    }

    return nullptr;

}

Python::SourceModule *Python::SourceRoot::moduleFromPath(QString filePath) const
{
    for (Python::SourceListNodeBase *itm = m_modules->begin();
         itm != m_modules->end();
         itm = itm->next())
    {
        Python::SourceModule *module = dynamic_cast<Python::SourceModule*>(itm);
        if (module && module->filePath() == filePath)
            return module;
    }

    return nullptr;
}

Python::SourceRoot *Python::SourceRoot::m_instance = nullptr;

Python::SourceRoot::DataTypes Python::SourceRoot::mapMetaDataType(const QString typeAnnotation) const
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

Python::SourceRoot::TypeInfoPair Python::SourceRoot::identifierType(const Python::Token *tok,
                                                                const Python::SourceFrame *frame) const
{
    TypeInfoPair tp;
    if (tok || !tok->isIdentifier())
        return tp;

    switch (tok->token) {
    case Python::Token::T_IdentifierBuiltin:
        return builtinType(tok, frame);
    case Python::Token::T_IdentifierClass:
        tp.thisType.type = ClassType;
        return tp;
    case Python::Token::T_IdentifierDecorator:
        tp.thisType.type = MethodDescriptorType;
        return tp;
    case Python::Token::T_IdentifierDefined: // fallthrough
    case Python::Token::T_IdentifierDefUnknown:
        tp.thisType.type = FunctionType; // method or function not known yet
        tp.returnType.type = UnknownType;
        return tp;
    case Python::Token::T_IdentifierTrue: // fallthrough
    case Python::Token::T_IdentifierFalse:
        tp.thisType.type = BoolType;
        return tp;
    case Python::Token::T_IdentifierFunction:
        // figure out how to lookp function return statments
    case Python::Token::T_IdentifierMethod:
        // figure out how to lookp function return statments
        return tp;
    case Python::Token::T_IdentifierModule:
    case Python::Token::T_IdentifierModuleAlias:
    case Python::Token::T_IdentifierModuleGlob:
    case Python::Token::T_IdentifierModulePackage:
        //tp.thisType.type = ; // how do we lookup modules?
        return tp;
    case Python::Token::T_IdentifierNone:
        tp.thisType.type = NoneType;
        return tp;
    case Python::Token::T_IdentifierSelf:
        tp.thisType.type = ReferenceArgumentType;
        return tp;
    case Python::Token::T_IdentifierSuperMethod:
    case Python::Token::T_IdentifierUnknown:
        tp.thisType.type = UnknownType;
        return tp;
    default:
        // invalid?
        qDebug() << QString::fromLatin1("Invalid token: id:%1 with text:%2")
                            .arg(QString::number(tok->token)).arg(tok->text()) << endl;
#ifdef DEBUG_TOKENS
        assert(tp.thisType.type != Python::SourceRoot::InValidType && "Invalid Token!");
#endif
        break;
    }

    return tp;
}

Python::SourceRoot::TypeInfoPair Python::SourceRoot::builtinType(const Python::Token *tok, const Python::SourceFrame *frame) const
{
    Q_UNUSED(frame);
    TypeInfoPair tp;

    if (tok && tok->token == Python::Token::T_IdentifierBuiltin) {
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
            tp.returnType.type = ReferenceImportType;

        // handle @methods
        } else if (tok->token == Python::Token::T_IdentifierDecorator) {
            if (text == QLatin1String("@classmethod")) {
                tp.returnType.type = ClassMethodDescriptorType;
            }
        }
    }
    return tp;
}

Python::SourceRoot::DataTypes Python::SourceRoot::numberType(const Python::Token *tok) const
{
    switch (tok->token) {
    case Python::Token::T_NumberFloat:
        return FloatType;
    case Python::Token::T_NumberBinary:
    case Python::Token::T_NumberDecimal:
    case Python::Token::T_NumberHex: // falltrough
    case Python::Token::T_NumberOctal:
        return IntType;
    default:
        return InValidType;
    }
}

bool Python::SourceRoot::isLineEscaped(const Python::Token *tok) const {
    DEFINE_DBG_VARS
    PREV_TOKEN(tok)
    int guard = 40;
    bool escaped = false;
    while (tok && (guard--)) {
        if (tok->token == Python::Token::T_DelimiterLineContinue)
            escaped = true;
        else
            escaped = false;
        if (tok->token == Python::Token::T_DelimiterNewLine)
            break;
        NEXT_TOKEN(tok)
    }
    return escaped;
}

QString Python::SourceRoot::customTypeNameFor(Python::SourceRoot::CustomNameIdx_t customIdx)
{
    return m_customTypeNames[customIdx];
}

Python::SourceRoot::CustomNameIdx_t Python::SourceRoot::addCustomTypeName(const QString name)
{
    m_customTypeNames[++m_uniqueCustomTypeNames] = name;
    return m_uniqueCustomTypeNames;
}

Python::SourceRoot::CustomNameIdx_t Python::SourceRoot::indexOfCustomTypeName(const QString name)
{
    if (m_customTypeNames.values().contains(QString(name)))
        return m_customTypeNames.key(QString(name));
    return -1;
}

Python::SourceModule *Python::SourceRoot::scanCompleteModule(const QString filePath,
                                                         Python::SyntaxHighlighter *highlighter)
{
    DEFINE_DBG_VARS

    // delete old data
    Python::SourceModule *mod = moduleFromPath(filePath);
    if (mod) {
        m_modules->remove(mod, true);
        mod = nullptr;
    }

    // create a new module
    mod = new Python::SourceModule(this, highlighter);
    mod->setFilePath(filePath);
    QFileInfo fi(filePath);
    mod->setModuleName(fi.baseName());
    m_modules->insert(mod);

    // find the first token of any kind
    // firstline might be empty
    if (highlighter && highlighter->document()) {
        QTextBlock block = highlighter->document()->begin();
        const Python::TextBlockData *txtData = nullptr;
        if (block.isValid())
            txtData = dynamic_cast<Python::TextBlockData*>(block.userData());

        if (!txtData)
            highlighter->rehighlight();
        else {
            // find first token, first row might be empty
            while(txtData->tokens().empty() &&
                  (txtData = txtData->next()))
                ;
            if (txtData) {
                Python::Token *tok = txtData->tokens()[0];
                if (tok) {
                    DBG_TOKEN(tok)
                    mod->scanFrame(tok);
                }
            }
        }
    }
#ifdef BUILD_PYTHON_DEBUGTOOLS
    {
        //DumpSyntaxTokens dump(document()->begin());
        DumpModule dMod(Python::SourceRoot::instance()->moduleFromPath(filePath));
    }
#endif
    return mod;
}

Python::SourceModule *Python::SourceRoot::scanSingleRowModule(const QString filePath,
                                                          Python::TextBlockData *row,
                                                          Python::SyntaxHighlighter *highlighter)
{
    Python::SourceModule *mod = moduleFromPath(filePath);
    if (!mod) {
        // create a new module
        mod = new Python::SourceModule(this, highlighter);
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
Python::SourceRoot::TypeInfo
Python::SourceRoot::statementResultType(const Python::Token *startToken,
                                      const Python::SourceFrame *frame) const
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


    const Python::Token *tok = startToken;
    DBG_TOKEN(tok)

    // first we scan to end of statement
    // then operate on that list
    QList<const Python::Token*> allTokens;
    QList<int> parenPos;


    static const int maxStatements = 50;
    int pos = 0; // use as guard for max tokens and parenPos
    do {
        switch (tok->token) {
        case Python::Token::T_DelimiterOpenParen:
            if (parenOpenCnt != parenCloseCnt) {
                frame->module()->setSyntaxError(tok, QString::fromLatin1("Parens mismatch '('..')' in statement"));
                return typeInfo;
            }
            parenPos.push_front(pos);
            parenOpenCnt++;
            break;
        case Python::Token::T_DelimiterCloseParen:
            parenPos.push_front(pos);
            parenCloseCnt++;
            if (parenOpenCnt != parenCloseCnt) {
                frame->module()->setSyntaxError(tok, QString::fromLatin1("Parens mismatch '('..')' in statement"));
                return typeInfo;
            }
            break;
        case Python::Token::T_DelimiterOpenBrace:
            if (braceOpenCnt != braceCloseCnt) {
                frame->module()->setSyntaxError(startToken, QString::fromLatin1("Braces mismatch in statement '{'..'}'"));
                return typeInfo;
            }
            braceOpenCnt++;
            break;
        case Python::Token::T_DelimiterCloseBrace:
            braceCloseCnt++;
            if (braceOpenCnt != braceCloseCnt) {
                frame->module()->setSyntaxError(startToken, QString::fromLatin1("Braces mismatch in statement '{'..'}'"));
                return typeInfo;
            }
            break;
        case Python::Token::T_DelimiterOpenBracket:
            if (bracketOpenCnt != bracketCloseCnt) {
                frame->module()->setSyntaxError(startToken, QString::fromLatin1("Brackets mismatch in statment '['..']'"));
                return typeInfo;
            }
            bracketOpenCnt++;
            break;
        case Python::Token::T_DelimiterCloseBracket:
            bracketCloseCnt++;
            if (bracketOpenCnt != bracketCloseCnt) {
                frame->module()->setSyntaxError(startToken, QString::fromLatin1("Brackets mismatch in statment '['..']'"));
                return typeInfo;
            }
            break;
        case Python::Token::T_DelimiterSemiColon:
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
    QList<QList<const Python::Token*> > subStatements;
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

const Python::Token
*Python::SourceRoot::computeStatementResultType(const Python::SourceFrame *frame,
                                              const Python::Token *startTok,
                                              TypeInfo &typeInfo) const
{
    DEFINE_DBG_VARS
    // first look for encapsulating brackets and parens ie '[..]' or '(..)'
    // a '{..}' is a dict and retuned as that, advances startToken

    const Python::Token *tok = startTok;

    int guard = 10;

//next_loop:
    DBG_TOKEN(tok)

    while(tok && (guard--)) {
        switch (tok->token) {
        case Python::Token::T_DelimiterOpenParen:
 //           ++parenCnt;
            // recurse into this '(..)'
            return computeStatementResultType(frame, tok->next(), typeInfo);
//            goto next_loop;
            break;
        case Python::Token::T_DelimiterCloseParen:
//            --parenCnt;
            break;
        case Python::Token::T_DelimiterOpenBracket:
            // recurese into '[..]'
//            ++bracketCnt;
            return computeStatementResultType(frame, tok->next(), typeInfo);
//            goto next_loop;
        case Python::Token::T_DelimiterCloseBracket:
//            --bracketCnt;
            break;
        case Python::Token::T_DelimiterOpenBrace:
            typeInfo.type = DictType;
            // TODO go to end of dict
            break;
        case Python::Token::T_DelimiterSemiColon:
            // end of statement
            return tok;
        case Python::Token::T_DelimiterNewLine:
            if (!isLineEscaped(tok))
                return tok;
            break;
        default: {
            const Python::SourceIdentifier *ident = nullptr;
            Python::SourceIdentifierAssignment *assign = nullptr;
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
                            Python::SourceFrameReturnType *frmType = dynamic_cast<Python::SourceFrameReturnType*>(ident->frame()->returnTypes().begin());
                            if (frmType)
                                typeInfo.type = frmType->typeInfo().type;

                            return tok;
                        }
                        // not callable,
                        const Python::SourceFrame *frm = ident->module()->getFrameForToken(assign->token(), ident->module()->rootFrame());
                        TypeInfoPair typePair = identifierType(assign->token(), frm);
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
// this struct is contained by Python::SourceRoot
Python::SourceRoot::TypeInfo::TypeInfo() :
    type(Python::SourceRoot::InValidType),
    customNameIdx(-1)
{
}

Python::SourceRoot::TypeInfo::TypeInfo(Python::SourceRoot::DataTypes type) :
    type(type),
    customNameIdx(-1)
{
}

Python::SourceRoot::TypeInfo::TypeInfo(const Python::SourceRoot::TypeInfo &other) :
    type(other.type),
    customNameIdx(other.customNameIdx),
    referenceName(other.referenceName)
{
}


Python::SourceRoot::TypeInfo::~TypeInfo()
{
}

QString Python::SourceRoot::TypeInfo::typeAsStr() const
{
    const char *typeAsStr;
    static const char *errStr = "<Typename lookup error>";

    switch (type) {
    case Python::SourceRoot::InValidType:         typeAsStr = "InValidType"; break;
    case Python::SourceRoot::UnknownType:         typeAsStr = "UnknownType"; break;
    // when no return value
    case Python::SourceRoot::VoidType:            typeAsStr = "VoidType"; break;

    case Python::SourceRoot::ReferenceType:       typeAsStr = "ReferenceType"; break;
    case Python::SourceRoot::ReferenceCallableType:
                                                typeAsStr = "ReferenceCallableType"; break;
    case Python::SourceRoot::ReferenceArgumentType:typeAsStr = "ReferenceArgumentType"; break;
    case Python::SourceRoot::ReferenceBuiltInType:typeAsStr = "ReferenceBuiltInType"; break;
/*
    case Python::SourceRoot::ReferenceImportUndeterminedType:
                                                typeAsStr = "ReferenceImportUndeterminedType"; break;
    case Python::SourceRoot::ReferenceImportErrorType:
                                                typeAsStr = "ReferenceImportErrorType"; break;
    case Python::SourceRoot::ReferenceImportPythonType:
                                                typeAsStr = "ReferenceImportPythonType"; break;
    case Python::SourceRoot::ReferenceImportBuiltInType:
                                                typeAsStr = "ReferenceImportBuiltInType"; break;
*/
    case Python::SourceRoot::ReferenceImportType: typeAsStr = "ReferenceImportType"; break;


    case Python::SourceRoot::FunctionType:        typeAsStr = "FunctionType"; break;
    case Python::SourceRoot::LambdaType:          typeAsStr = "LambdaType";   break;
    case Python::SourceRoot::GeneratorType:       typeAsStr = "GeneratorType";break;
    case Python::SourceRoot::CoroutineType:       typeAsStr = "CoroutineType";break;
    case Python::SourceRoot::CodeType:            typeAsStr = "CodeType";     break;
    case Python::SourceRoot::MethodType:          typeAsStr = "MethodType";   break;
    case Python::SourceRoot::BuiltinFunctionType: typeAsStr = "BuiltinFunctionType"; break;
    case Python::SourceRoot::BuiltinMethodType:   typeAsStr = "BuiltinMethodType"; break;
    case Python::SourceRoot::WrapperDescriptorType: typeAsStr = "WrapperDescriptorType"; break;
    case Python::SourceRoot::MethodWrapperType:   typeAsStr = "MethodWrapperType"; break;
    case Python::SourceRoot::MethodDescriptorType:typeAsStr = "MethodDescriptorType"; break;
    case Python::SourceRoot::ClassMethodDescriptorType: typeAsStr = "ClassMethodDescriptorType"; break;
    case Python::SourceRoot::ModuleType:          typeAsStr = "ModuleType"; break;// is root frame for file
    case Python::SourceRoot::TracebackType:       typeAsStr = "TracebackType"; break;
    case Python::SourceRoot::FrameType:           typeAsStr = "FrameType";   break;
    case Python::SourceRoot::GetSetDescriptorType:typeAsStr = "GetSetDescriptorType"; break;
    case Python::SourceRoot::MemberDescriptorType:typeAsStr = "MemberDescriptorType"; break;
    case Python::SourceRoot::MappingProxyType:    typeAsStr = "MappingProxyType";   break;

    case Python::SourceRoot::TypeObjectType:      typeAsStr = "TypeObjectType"; break;
    case Python::SourceRoot::ObjectType:          typeAsStr = "ObjectType";   break;
    case Python::SourceRoot::NoneType:            typeAsStr = "NoneType";     break;
    case Python::SourceRoot::BoolType:            typeAsStr = "BoolType";     break;
    case Python::SourceRoot::IntType:             typeAsStr = "IntType";      break;
    case Python::SourceRoot::FloatType:           typeAsStr = "FloatType";    break;
    case Python::SourceRoot::StringType:          typeAsStr = "StringType";   break;
    case Python::SourceRoot::BytesType:           typeAsStr = "BytesType";    break;
    case Python::SourceRoot::ListType:            typeAsStr = "ListType";     break;
    case Python::SourceRoot::TupleType:           typeAsStr = "TupleType";    break;
    case Python::SourceRoot::SetType:             typeAsStr = "SetType";      break;
    case Python::SourceRoot::FrozenSetType:       typeAsStr = "FrozensetType";break;
    case Python::SourceRoot::RangeType:           typeAsStr = "RangeType";    break;
    case Python::SourceRoot::DictType:            typeAsStr = "DictType";     break;
    case Python::SourceRoot::ClassType:           typeAsStr = "ClassType";    break;
    case Python::SourceRoot::ComplexType:         typeAsStr = "ComplexType";  break;
    case Python::SourceRoot::EnumerateType:       typeAsStr = "EnumerateType";break;
    case Python::SourceRoot::IterableType:        typeAsStr = "IterableType"; break;
    case Python::SourceRoot::FileType:            typeAsStr = "FileType";     break;
    case Python::SourceRoot::CustomType:
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

QString Python::SourceRoot::TypeInfo::customName() const
{
    if (customNameIdx > -1) {
        return Python::SourceRoot::instance()->customTypeNameFor(customNameIdx);
    }
    return QString();
}

#include "moc_PythonSourceRoot.cpp"
