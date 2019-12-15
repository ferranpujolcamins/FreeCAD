#include "PythonSourceRoot.h"
#include "PythonSource.h"
#include "PythonSourceModule.h"
#include "PythonSourceDebugTools.h"
#include "PythonToken.h"


#include <FCConfig.h>
#include "Base/Interpreter.h"
//#include <TextEditor/PythonCode.h>
//#include <TextEditor/PythonSyntaxHighlighter.h>

#include <QFileInfo>

#include <map>
#include <string>
#include <list>

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

const std::list<const std::string> Python::SourceRoot::modulesNames() const
{
    std::list<const std::string> ret;
    if (!m_modules)
        return ret;

    for(Python::SourceListNodeBase *itm = m_modules->begin();
        itm != m_modules->end();
        itm->next())
    {
        Python::SourceModule *module = dynamic_cast<Python::SourceModule*>(itm);
        if (module)
            ret.push_back(module->moduleName());
    }

    return ret;
}

const std::list<const std::string> Python::SourceRoot::modulesPaths() const
{
    std::list<const std::string> ret;

    for(Python::SourceListNodeBase *itm = m_modules->begin();
        itm != m_modules->end();
        itm->next())
    {
        Python::SourceModule *module = dynamic_cast<Python::SourceModule*>(itm);
        if (module)
            ret.push_back(module->filePath());
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

Python::SourceModule *Python::SourceRoot::moduleFromPath(const std::string &filePath) const
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

Python::SourceRoot::DataTypes Python::SourceRoot::mapMetaDataType(const std::string &typeAnnotation) const
{
    // try to determine type form type annotaion
    // more info at:
    // https://www.python.org/dev/peps/pep-0484/
    // https://www.python.org/dev/peps/pep-0526/
    // https://mypy.readthedocs.io/en/latest/cheat_sheet_py3.html
    if (typeAnnotation == "none")
        return NoneType;
    if (typeAnnotation == "Object")
        return ObjectType;
    else if (typeAnnotation == "bool")
        return BoolType;
    else if (typeAnnotation == "int")
        return IntType;
    else if (typeAnnotation == "float")
        return FloatType;
    else if (typeAnnotation == "str")
        return StringType;
    else if (typeAnnotation == "bytes")
        return BytesType;
    else if (typeAnnotation == "List")
        return ListType;
    else if (typeAnnotation == "Tuple")
        return TupleType;
    else if (typeAnnotation == "Dict")
        return DictType;
    else if (typeAnnotation == "Set")
        return SetType;
    else if (typeAnnotation == "Range")
        return RangeType;
    return CustomType;
}

Python::SourceRoot::TypeInfoPair Python::SourceRoot::identifierType(const Python::Token *tok,
                                                                const Python::SourceFrame *frame) const
{
    TypeInfoPair tp;
    if (tok || !tok->isIdentifier())
        return tp;

    switch (tok->type()) {
    case Python::Token::T_IdentifierBuiltin:
        return builtinType(tok, frame);
    case Python::Token::T_IdentifierClass:
        tp.thisType.type = ClassType;
        return tp;
    case Python::Token::T_IdentifierDecorator:
        tp.thisType.type = MethodDescriptorType;
        return tp;
    case Python::Token::T_IdentifierDefined: FALLTHROUGH
    case Python::Token::T_IdentifierDefUnknown:
        tp.thisType.type = FunctionType; // method or function not known yet
        tp.returnType.type = UnknownType;
        return tp;
    case Python::Token::T_IdentifierTrue: FALLTHROUGH
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
    default: {
        // invalid?
        std::cout << "Invalid token: id:" + std::to_string(tok->type()) +
                     " with text:" << tok->text() << std::endl;
#ifdef DEBUG_TOKENS
        assert(tp.thisType.type != Python::SourceRoot::InValidType && "Invalid Token!");
#endif
        break;
    }
    }

    return tp;
}

Python::SourceRoot::TypeInfoPair Python::SourceRoot::builtinType(const Python::Token *tok, const Python::SourceFrame *frame) const
{
    Q_UNUSED(frame)
    TypeInfoPair tp;

    if (tok && tok->type() == Python::Token::T_IdentifierBuiltin) {
        const std::string text = tok->text();
        // handle built in functions

        tp.thisType.type = BuiltinFunctionType;
        // https://docs.python.org/3/library/functions.html
        if (text == "abs") {
            tp.returnType.type = FloatType;
        } else if (text == "all") {
            tp.returnType.type = BoolType;
        } else if (text == "any") {
            tp.returnType.type = BoolType;
        } else if (text == "ascii") {
            tp.returnType.type = StringType;
        } else if (text == "bin") {
            tp.returnType.type = StringType;
        } else if (text == "bool") {
            tp.returnType.type = BoolType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "breakpoint") {
            tp.returnType.type = VoidType;
        } else if (text == "bytesarray") {
            tp.returnType.type = BytesType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "bytes") {
            tp.returnType.type = BytesType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "callable") {
            tp.returnType.type = BoolType;
        } else if (text == "chr") {
            tp.returnType.type = StringType;
        } else if (text == "compile") {
            tp.returnType.type = CodeType;
        } else if (text == "complex") {
            tp.returnType.type = ComplexType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "delattr") {
            tp.returnType.type = VoidType;
        } else if (text == "dict") {
            tp.returnType.type = DictType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "dir") {
            tp.returnType.type = ListType;
        } else if (text == "divmod") {
            tp.returnType.type = IntType;
        } else if (text == "enumerate") {
            tp.returnType.type = EnumerateType;
        } else if (text == "eval") {
            tp.returnType.type = ObjectType;
            // we dont know what type is returned,
            // might be str, int or anything realy
        } else if (text == "exec") {
            tp.returnType.type = NoneType;
        } else if (text == "filter") {
            tp.returnType.type = IterableType;
        } else if (text == "float") {
            tp.returnType.type = FloatType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "format") {
            tp.returnType.type = StringType;
        } else if (text == "frozenset") {
            tp.returnType.type = FrozenSetType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "getattr") {
            tp.returnType.type = ObjectType;
            // we dont know what value is returned
        } else if (text == "Globals") {
            tp.returnType.type = DictType;
        } else if (text == "hasattr") {
            tp.returnType.type = BoolType;
        } else if (text == "hash") {
            tp.returnType.type = IntType;
        } else if (text == "help") {
            tp.returnType.type = VoidType;
        } else if (text == "hex") {
            tp.returnType.type = StringType;
        } else if (text == "id") {
            tp.returnType.type = IntType;
        } else if (text == "input") {
            tp.returnType.type = StringType;
        } else if (text == "int") {
            tp.returnType.type = IntType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "isinstance") {
            tp.returnType.type = BoolType;
        } else if (text == "issubclass") {
            tp.returnType.type = BoolType;
        } else if (text == "iter") {
            tp.returnType.type = IterableType;
        } else if (text == "len") {
            tp.returnType.type = IntType;
        } else if (text == "list") {
            tp.returnType.type = ListType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "locals") {
            tp.returnType.type = DictType;
        } else if (text == "map") {
            tp.returnType.type = IterableType;
        } else if (text == "max") {
            tp.returnType.type = ObjectType;
        } else if (text == "memoryiew") {
            tp.returnType.type = ClassType;
        } else if (text == "min") {
            tp.returnType.type = ObjectType;
        } else if (text == "next") {
            tp.returnType.type = ObjectType;
        } else if (text == "object") {
            tp.returnType.type = ObjectType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "oct") {
            tp.returnType.type = StringType;
        } else if (text == "open") {
            tp.returnType.type = FileType;
        } else if (text == "ord") {
            tp.returnType.type = IntType;
        } else if (text == "pow") {
            tp.returnType.type = FloatType;
            // might also return int, but default to float
        } else if (text == "print") {
            tp.returnType.type = VoidType;
        } else if (text == "property") {
            tp.returnType.type = ClassType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "range") {
            tp.returnType.type = IterableType;
        } else if (text == "repr") {
            tp.returnType.type = StringType;
        } else if (text == "reversed") {
            tp.returnType.type = IterableType;
        } else if (text == "round") {
            tp.returnType.type = FloatType;
        } else if (text == "set") {
            tp.returnType.type = SetType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "setattr") {
            tp.returnType.type = VoidType;
        } else if (text == "slice") {
            tp.returnType.type = IterableType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "sorted") {
            tp.returnType.type = ListType;
        } else if (text == "str") {
            tp.returnType.type = StringType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "sum") {
            tp.returnType.type = FloatType;
        } else if (text == "super") {
            tp.returnType.type = ClassType;
        } else if (text == "tuple") {
            tp.returnType.type = TupleType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "type") {
            tp.returnType.type = TypeObjectType;
        } else if (text == "unicode") {
            tp.returnType.type = StringType;
            tp.thisType.type = BuiltinMethodType;
        } else if (text == "vars") {
            tp.returnType.type = DictType;
        } else if (text == "zip") {
            tp.returnType.type = IterableType;
        } else if (text == "__import__") {
            tp.returnType.type = ReferenceImportType;

        // handle @methods
        } else if (tok->type() == Python::Token::T_IdentifierDecorator) {
            if (text == "@classmethod") {
                tp.returnType.type = ClassMethodDescriptorType;
            }
        }
    }
    return tp;
}

Python::SourceRoot::DataTypes Python::SourceRoot::numberType(const Python::Token *tok) const
{
    switch (tok->type()) {
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
        if (tok->type() == Python::Token::T_DelimiterBackSlash)
            escaped = true;
        else
            escaped = false;
        if (tok->type() == Python::Token::T_DelimiterNewLine)
            break;
        NEXT_TOKEN(tok)
    }
    return escaped;
}

const std::string Python::SourceRoot::customTypeNameFor(Python::SourceRoot::CustomNameIdx_t customIdx)
{
    if (customIdx >= m_uniqueCustomTypeNames || customIdx < 0)
        return std::string();
    return m_customTypeNames[customIdx];
}

Python::SourceRoot::CustomNameIdx_t Python::SourceRoot::addCustomTypeName(const std::string &name)
{
    m_customTypeNames.insert({++m_uniqueCustomTypeNames, name});
    return m_uniqueCustomTypeNames;
}

Python::SourceRoot::CustomNameIdx_t Python::SourceRoot::indexOfCustomTypeName(const std::string &name)
{
    CustomNameIdx_t idx = 0;
    for (auto &p : m_customTypeNames) {
        if (name == p.second)
            return idx;
        ++idx;
    }
    return -1;
}

Python::SourceModule *Python::SourceRoot::scanCompleteModule(const std::string &filePath,
                                                            Python::Lexer *tokenizer)
{
    DEFINE_DBG_VARS
    assert(tokenizer != nullptr && "Must have a valid tokenizer");

    // delete old data
    Python::SourceModule *mod = moduleFromPath(filePath);
    if (mod) {
        m_modules->remove(mod, true);
        mod = nullptr;
    }

    // create a new module
    mod = new Python::SourceModule(this, tokenizer);
    mod->setFilePath(filePath);
    Python::FileInfo fi(filePath);
    mod->setModuleName(fi.baseName());
    m_modules->insert(mod);

    // find the first token of any kind
    if (!tokenizer->list().empty()) {
        Python::Token *tok = tokenizer->list().front();
        assert(tok != nullptr && "Tokenizer !empty but has no front item");
        DBG_TOKEN(tok)
        mod->scanFrame(tok);

        // lookup invalid identifiers again.
        // They might have been defined now that all page is scanned
        // iterate through subFrames, recursive
        mod->reparseInvalidTokens();
    }

#ifdef BUILD_PYTHON_DEBUGTOOLS
    {
        //DumpSyntaxTokens dump(document()->begin());
        //DumpModule dMod(Python::SourceRoot::instance()->moduleFromPath(filePath));
    }
#endif
    return mod;
}

Python::SourceModule *Python::SourceRoot::scanSingleRowModule(const std::string &filePath,
                                                          Python::TokenLine *row,
                                                          Python::Lexer *tokenizer)
{
    Python::SourceModule *mod = moduleFromPath(filePath);
    if (!mod) {
        // create a new module
        mod = new Python::SourceModule(this, tokenizer);
        mod->setFilePath(filePath);
        Python::FileInfo fi(filePath);
        mod->setModuleName(fi.baseName());
        m_modules->insert(mod);
    }

    if (!row->empty()) {
        mod->scanLine(row->front());
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
        switch (tok->type()) {
        case Python::Token::T_DelimiterOpenParen:
            if (parenOpenCnt != parenCloseCnt) {
                tok->ownerLine()->setSyntaxErrorMsg(tok, "Parens mismatch '('..')' in statement");
                return typeInfo;
            }
            parenPos.push_front(pos);
            parenOpenCnt++;
            break;
        case Python::Token::T_DelimiterCloseParen:
            parenPos.push_front(pos);
            parenCloseCnt++;
            if (parenOpenCnt != parenCloseCnt) {
                tok->ownerLine()->setSyntaxErrorMsg(tok, "Parens mismatch '('..')' in statement");
                return typeInfo;
            }
            break;
        case Python::Token::T_DelimiterOpenBrace:
            if (braceOpenCnt != braceCloseCnt) {
                tok->ownerLine()->setSyntaxErrorMsg(startToken, "Braces mismatch in statement '{'..'}'");
                return typeInfo;
            }
            braceOpenCnt++;
            break;
        case Python::Token::T_DelimiterCloseBrace:
            braceCloseCnt++;
            if (braceOpenCnt != braceCloseCnt) {
                tok->ownerLine()->setSyntaxErrorMsg(startToken, "Braces mismatch in statement '{'..'}'");
                return typeInfo;
            }
            break;
        case Python::Token::T_DelimiterOpenBracket:
            if (bracketOpenCnt != bracketCloseCnt) {
                tok->ownerLine()->setSyntaxErrorMsg(startToken, "Brackets mismatch in statment '['..']'");
                return typeInfo;
            }
            bracketOpenCnt++;
            break;
        case Python::Token::T_DelimiterCloseBracket:
            bracketCloseCnt++;
            if (bracketOpenCnt != bracketCloseCnt) {
                tok->ownerLine()->setSyntaxErrorMsg(startToken, "Brackets mismatch in statment '['..']'");
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
        tok->ownerLine()->setSyntaxErrorMsg(startToken, "Parens mismatch '('..')' in statement");
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
        switch (tok->type()) {
        case Python::Token::T_DelimiterOpenParen:
 //           ++parenCnt;
            // recurse into this '(..)'
            return computeStatementResultType(frame, tok->next(), typeInfo);
//            goto next_loop;
//            break;
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
                ident = frame->getIdentifier(tok->hash());
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

const std::string Python::SourceRoot::TypeInfo::typeAsStr() const
{
    return std::string(typeAsCStr());
}

const char* Python::SourceRoot::TypeInfo::typeAsCStr() const
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
            return customName().c_str();
        }
        return errStr;
    //default:
    //    return errStr;
    }

    return typeAsStr;
}

const std::string Python::SourceRoot::TypeInfo::customName() const
{
    if (customNameIdx > -1) {
        return Python::SourceRoot::instance()->customTypeNameFor(customNameIdx);
    }
    return std::string();
}

#include "moc_PythonSourceRoot.cpp"
