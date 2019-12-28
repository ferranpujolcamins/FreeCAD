#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <PythonToken.h>
#include <PythonLexer.h>
#include <PythonSource.h>

#include <regex>

using namespace testing;
using namespace Python;

static const size_t lineCnt = 14u;
static const char *srcText[lineCnt] = {
"class test(): # comment",
"    def stringencodecoin(ustr):",
"        \"\"\"stringencodecoin(str): Encodes a unicode object to be used as a string in coin\"\"\"",
"        try:",
"            from pivy import coin",
"            coin4 = coin.COIN_MAJOR_VERSION >= 4",
"        except (ImportError, AttributeError):",
"            coin4 = False",
"",
"        if coin4:",
"            return ustr.encode('utf-8')",
"        else:",
"            # comment 2",
"            return ustr.encode('latin1')"
};

// --------------------------------------------------------------------------------------

class TstLexerTokenize : public ::testing::Test
{
protected:
    Lexer *lex;
public:
    TstLexerTokenize() :
        ::testing::Test(),
        lex(new Lexer())
    {
    }
    ~TstLexerTokenize() override;
    void SetUp() override {
        for(size_t i = 0; i < lineCnt; ++i) {
            auto line = new TokenLine(nullptr, srcText[i]);
            lex->list().appendLine(line);
            lex->tokenize(line);
        }
    }
};
TstLexerTokenize::~TstLexerTokenize()
{
    delete lex;
}

// ---------------------------------------------------------------------------------------

class TstLexerReader : public ::testing::Test
{
protected:
    LexerReader _lexReader;
    bool _read;
public:
    TstLexerReader() :
        ::testing::Test(),
        _lexReader("/usr/lib/python:/usr/local/lib/python")
    {
        _lexReader.setFilePath("testscripts/test1.py");
        _read = _lexReader.readFile();
    }
    ~TstLexerReader() override;
};
TstLexerReader::~TstLexerReader() {}

// ---------------------------------------------------------------------------------------
TEST(TstLexer, testLexerDefaults) {
    Lexer lex;
    ASSERT_STREQ(lex.filePath().c_str(), "");

    lex.setFilePath("test1.py");
    ASSERT_STREQ(lex.filePath().c_str(), "test1.py");
    EXPECT_EQ(Lexer::version().version(), Version::Latest);
    Lexer::setVersion(Version::v3_0);
    EXPECT_EQ(Lexer::version().version(), Version::v3_0);
    EXPECT_EQ(lex.version().version(), Version::v3_0);

    EXPECT_EQ(lex.previousCodeLine(lex.list().firstLine()), nullptr);
    EXPECT_EQ(lex.previousCodeLine(lex.list().lineAt(9)), lex.list().lineAt(7));

    // API check
    lex.tokenTypeChanged(nullptr);
}

TEST_F(TstLexerTokenize, testLexerTokenizer) {
    TokenList &list = lex->list();
    ASSERT_EQ(list.lineCount(), 14u);
    TokenLine &line0 = *list.lineAt(0),
              &line1 = *list.lineAt(1),
              &line2 = *list.lineAt(2),
              &line3 = *list.lineAt(3),
              &line4 = *list.lineAt(4),
              &line5 = *list.lineAt(5),
              &line6 = *list.lineAt(6),
              &line7 = *list.lineAt(7),
              &line8 = *list.lineAt(8),
              &line9 = *list.lineAt(9),
              &line10 = *list.lineAt(10),
              &line11 = *list.lineAt(11),
              &line12 = *list.lineAt(12),
              &line13 = *list.lineAt(13);
    EXPECT_EQ(line0.count(), 7u);
    EXPECT_EQ(line1.count(), 8u);
    EXPECT_EQ(line2.count(), 3u);
    EXPECT_EQ(line3.count(), 3u);
    EXPECT_EQ(line4.count(), 6u);
    EXPECT_EQ(line5.count(), 9u);
    EXPECT_EQ(line6.count(), 8u);
    EXPECT_EQ(line7.count(), 6u);
    EXPECT_EQ(line8.count(), 0u);
    EXPECT_EQ(line9.count(), 4u);
    EXPECT_EQ(line10.count(), 10u);
    EXPECT_EQ(line11.count(), 3u);
    EXPECT_EQ(line12.count(), 1u);
    EXPECT_EQ(line13.count(), 9u);

    // line 0
    EXPECT_EQ(line0[0]->type(), Token::T_KeywordClass);
    EXPECT_EQ(line0[1]->type(), Token::T_IdentifierClass);
    EXPECT_EQ(line0[2]->type(), Token::T_DelimiterOpenParen);
    EXPECT_EQ(line0[3]->type(), Token::T_DelimiterCloseParen);
    EXPECT_EQ(line0[4]->type(), Token::T_DelimiterColon);
    EXPECT_EQ(line0[5]->type(), Token::T_Comment);
    EXPECT_EQ(line0[6]->type(), Token::T_DelimiterNewLine);

    // line 1
    EXPECT_EQ(line1[0]->type(), Token::T_Indent);
    EXPECT_EQ(line1[1]->type(), Token::T_KeywordDef);
    EXPECT_EQ(line1[2]->type(), Token::T_IdentifierDefUnknown);
    EXPECT_EQ(line1[3]->type(), Token::T_DelimiterOpenParen);
    EXPECT_EQ(line1[4]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line1[5]->type(), Token::T_DelimiterCloseParen);
    EXPECT_EQ(line1[6]->type(), Token::T_DelimiterColon);
    EXPECT_EQ(line1[7]->type(), Token::T_DelimiterNewLine);

    // line 2
    EXPECT_EQ(line2[0]->type(), Token::T_Indent);
    EXPECT_EQ(line2[1]->type(), Token::T_LiteralBlockDblQuote);
    EXPECT_EQ(line2[2]->type(), Token::T_DelimiterNewLine);

    // line 3
    EXPECT_EQ(line3[0]->type(), Token::T_KeywordTry);
    EXPECT_EQ(line3[1]->type(), Token::T_DelimiterColon);
    EXPECT_EQ(line3[2]->type(), Token::T_DelimiterNewLine);

    // line4
    EXPECT_EQ(line4[0]->type(), Token::T_Indent);
    EXPECT_EQ(line4[1]->type(), Token::T_KeywordFrom);
    EXPECT_EQ(line4[2]->type(), Token::T_IdentifierModulePackage);
    EXPECT_EQ(line4[3]->type(), Token::T_KeywordImport);
    EXPECT_EQ(line4[4]->type(), Token::T_IdentifierModule);
    EXPECT_EQ(line4[5]->type(), Token::T_DelimiterNewLine);

    // line5
    EXPECT_EQ(line5[0]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line5[1]->type(), Token::T_OperatorEqual);
    EXPECT_EQ(line5[2]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line5[3]->type(), Token::T_DelimiterPeriod);
    EXPECT_EQ(line5[4]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line5[5]->type(), Token::T_OperatorMoreEqual);
    EXPECT_EQ(line5[6]->type(), Token::T_NumberDecimal);
    EXPECT_EQ(line5[7]->type(), Token::T_DelimiterNewLine);
    EXPECT_EQ(line5[8]->type(), Token::T_Dedent);

    // line6
    EXPECT_EQ(line6[0]->type(), Token::T_KeywordExcept);
    EXPECT_EQ(line6[1]->type(), Token::T_DelimiterOpenParen);
    EXPECT_EQ(line6[2]->type(), Token::T_IdentifierBuiltin);
    EXPECT_EQ(line6[3]->type(), Token::T_DelimiterComma);
    EXPECT_EQ(line6[4]->type(), Token::T_IdentifierBuiltin);
    EXPECT_EQ(line6[5]->type(), Token::T_DelimiterCloseParen);
    EXPECT_EQ(line6[6]->type(), Token::T_DelimiterColon);
    EXPECT_EQ(line6[7]->type(), Token::T_DelimiterNewLine);

    // line7
    EXPECT_EQ(line7[0]->type(), Token::T_Indent);
    EXPECT_EQ(line7[1]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line7[2]->type(), Token::T_OperatorEqual);
    EXPECT_EQ(line7[3]->type(), Token::T_IdentifierFalse);
    EXPECT_EQ(line7[4]->type(), Token::T_DelimiterNewLine);
    EXPECT_EQ(line7[5]->type(), Token::T_Dedent);

    // line8
    EXPECT_EQ(line8.count(), 0u);

    // line9
    EXPECT_EQ(line9[0]->type(), Token::T_KeywordIf);
    EXPECT_EQ(line9[1]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line9[2]->type(), Token::T_DelimiterColon);
    EXPECT_EQ(line9[3]->type(), Token::T_DelimiterNewLine);

    // line9
    EXPECT_EQ(line10[0]->type(), Token::T_Indent);
    EXPECT_EQ(line10[1]->type(), Token::T_KeywordReturn);
    EXPECT_EQ(line10[2]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line10[3]->type(), Token::T_DelimiterPeriod);
    EXPECT_EQ(line10[4]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line10[5]->type(), Token::T_DelimiterOpenParen);
    EXPECT_EQ(line10[6]->type(), Token::T_LiteralSglQuote);
    EXPECT_EQ(line10[7]->type(), Token::T_DelimiterCloseParen);
    EXPECT_EQ(line10[8]->type(), Token::T_DelimiterNewLine);
    EXPECT_EQ(line10[9]->type(), Token::T_Dedent);

    // line10
    EXPECT_EQ(line11[0]->type(), Token::T_KeywordElse);
    EXPECT_EQ(line11[1]->type(), Token::T_DelimiterColon);
    EXPECT_EQ(line11[2]->type(), Token::T_DelimiterNewLine);

    // line11
    EXPECT_EQ(line12[0]->type(), Token::T_Comment);

    // line12
    EXPECT_EQ(line13[0]->type(), Token::T_Indent);
    EXPECT_EQ(line13[1]->type(), Token::T_KeywordReturn);
    EXPECT_EQ(line13[2]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line13[3]->type(), Token::T_DelimiterPeriod);
    EXPECT_EQ(line13[4]->type(), Token::T_IdentifierUnknown);
    EXPECT_EQ(line13[5]->type(), Token::T_DelimiterOpenParen);
    EXPECT_EQ(line13[6]->type(), Token::T_LiteralSglQuote);
    EXPECT_EQ(line13[7]->type(), Token::T_DelimiterCloseParen);
    EXPECT_EQ(line13[8]->type(), Token::T_DelimiterNewLine);
    //EXPECT_EQ(line13[9]->type(), Token::T_Dedent);
}

TEST_F(TstLexerReader, testLexerRead) {
    ASSERT_EQ(_read, true);
    EXPECT_EQ(_lexReader.list().empty(), false);
}

TEST_F(TstLexerTokenize, testLexerPersistent) {
    LexerPersistent lexP(lex);
    ASSERT_EQ(lexP.dumpToFile("testscripts/dumpfiles/test.lexdmp"), true);
    std::unique_ptr<Lexer> newLexp(new Lexer());
    LexerPersistent lexP2(newLexp.get());
    ASSERT_GT(lexP2.reconstructFromDmpFile("testscripts/dumpfiles/test.lexdmp"), 0);

    // check so each token is restored
    EXPECT_EQ(lex->list().count(), newLexp->list().count());
    uint guard = lex->list().max_size();
    auto tok1 = lex->list().front();
    auto tok2 = newLexp->list().front();
    while (tok1 && tok2 && (--guard)) {
        EXPECT_EQ(tok1->type(), tok2->type());
        EXPECT_EQ(tok1->line(), tok2->line());
        EXPECT_EQ(tok1->startPos(), tok2->startPos());
        EXPECT_EQ(tok1->endPos(), tok2->endPos());
        tok1 = tok1->next();
        tok2 = tok2->next();
    }
    EXPECT_EQ(tok1, nullptr);
    EXPECT_EQ(tok2, nullptr);
    EXPECT_GT(guard, 0u);

    // check so each line is restored
    EXPECT_EQ(lex->list().lineCount(), newLexp->list().lineCount());
    guard = lex->list().max_size();
    auto line1 = lex->list().firstLine();
    auto line2 = newLexp->list().firstLine();
    while (line1 && line2 && (--guard)) {
        EXPECT_EQ(line1->lineNr(), line2->lineNr());
        EXPECT_EQ(line1->indent(), line2->indent());
        EXPECT_EQ(line1->blockState(), line2->blockState());
        EXPECT_STREQ(line1->text().c_str(), line2->text().c_str());
        EXPECT_EQ(line1->count(), line2->count());
        EXPECT_EQ(line1->bracketCnt(), line2->bracketCnt());
        EXPECT_EQ(line1->braceCnt(), line2->braceCnt());
        EXPECT_EQ(line1->parenCnt(), line2->parenCnt());
        EXPECT_EQ(line1->isParamLine(), line2->isParamLine());
        EXPECT_EQ(line2->isContinuation(), line2->isContinuation());
        // test line previous and next
        if (line1 != lex->list().lastLine() && line2 != newLexp->list().lastLine()) {
            EXPECT_EQ(line1->nextLine()->lineNr(), line2->nextLine()->lineNr());
        }
        if (line1 != lex->list().firstLine() && line2 != newLexp->list().firstLine()) {
            EXPECT_EQ(line1->previousLine()->lineNr(), line2->previousLine()->lineNr());
        }
        // test unfinished tokens
        EXPECT_EQ(line1->unfinishedTokens().size(), line2->unfinishedTokens().size());
        auto it1 = line1->unfinishedTokens().begin();
        auto it2 = line2->unfinishedTokens().begin();
        for(;it1 != line1->unfinishedTokens().end() &&
             it2 != line2->unfinishedTokens().end();
            ++it1, ++it2)
        {
            EXPECT_EQ(*it1, *it2);
        }
        // test scaninfo for this line
        if (line1->tokenScanInfo() && !line1->tokenScanInfo()->allMessages().empty()) {
            EXPECT_NE(line2->tokenScanInfo(), nullptr);
            auto it1 = line1->tokenScanInfo()->allMessages().begin();
            auto it2 = line2->tokenScanInfo()->allMessages().begin();
            for(;it1 != line1->tokenScanInfo()->allMessages().end() &&
                 it2 != line2->tokenScanInfo()->allMessages().end();
                ++it1, ++it2)
            {
                EXPECT_EQ((*it1)->type(), (*it2)->type());
                EXPECT_EQ(line1->tokenPos((*it1)->token()), line2->tokenPos((*it2)->token()));
                EXPECT_STREQ((*it1)->message().c_str(), (*it2)->message().c_str());
            }
        }

        line1 = line1->nextLine();
        line2 = line2->nextLine();
    }
    EXPECT_EQ(line1, nullptr);
    EXPECT_EQ(line2, nullptr);
    EXPECT_GT(guard, 0u);
}

TEST(tstLexerPersistent, testLexerPersistentDumpStr) {
    LexerReader lex;
    lex.readFile("testscripts/test1.py");
    LexerPersistent lexP(&lex);
    auto str = lexP.dumpToString();
    EXPECT_GT(str.length(), 100u);
    Lexer lex2;
    LexerPersistent lexP2(&lex2);
    EXPECT_GT(lexP2.reconstructFromString(str), 0);

    // check so each token is restored
    EXPECT_EQ(lex.list().count(), lex2.list().count());
    uint guard = lex.list().max_size();
    auto tok1 = lex.list().front();
    auto tok2 = lex2.list().front();
    while (tok1 && tok2 && (--guard)) {
        EXPECT_EQ(tok1->type(), tok2->type());
        EXPECT_EQ(tok1->line(), tok2->line());
        EXPECT_EQ(tok1->startPos(), tok2->startPos());
        EXPECT_EQ(tok1->endPos(), tok2->endPos());
        tok1 = tok1->next();
        tok2 = tok2->next();
    }
    EXPECT_EQ(tok1, nullptr);
    EXPECT_EQ(tok2, nullptr);
    EXPECT_GT(guard, 0u);

    // check so each line is restored
    EXPECT_EQ(lex.list().lineCount(), lex2.list().lineCount());
    guard = lex.list().max_size();
    auto line1 = lex.list().firstLine();
    auto line2 = lex2.list().firstLine();
    while (line1 && line2 && (--guard)) {
        EXPECT_EQ(line1->lineNr(), line2->lineNr());
        EXPECT_EQ(line1->indent(), line2->indent());
        EXPECT_EQ(line1->blockState(), line2->blockState());
        EXPECT_STREQ(line1->text().c_str(), line2->text().c_str());
        EXPECT_EQ(line1->count(), line2->count());
        EXPECT_EQ(line1->bracketCnt(), line2->bracketCnt());
        EXPECT_EQ(line1->braceCnt(), line2->braceCnt());
        EXPECT_EQ(line1->parenCnt(), line2->parenCnt());
        EXPECT_EQ(line1->isParamLine(), line2->isParamLine());
        EXPECT_EQ(line2->isContinuation(), line2->isContinuation());
        // test line previous and next
        if (line1 != lex.list().lastLine() && line2 != lex2.list().lastLine()) {
            EXPECT_EQ(line1->nextLine()->lineNr(), line2->nextLine()->lineNr());
        }
        if (line1 != lex.list().firstLine() && line2 != lex2.list().firstLine()) {
            EXPECT_EQ(line1->previousLine()->lineNr(), line2->previousLine()->lineNr());
        }
        // test unfinished tokens
        EXPECT_EQ(line1->unfinishedTokens().size(), line2->unfinishedTokens().size());
        auto it1 = line1->unfinishedTokens().begin();
        auto it2 = line2->unfinishedTokens().begin();
        for(;it1 != line1->unfinishedTokens().end() &&
             it2 != line2->unfinishedTokens().end();
            ++it1, ++it2)
        {
            EXPECT_EQ(*it1, *it2);
        }
        // test scaninfo for this line
        if (line1->tokenScanInfo() && !line1->tokenScanInfo()->allMessages().empty()) {
            EXPECT_NE(line2->tokenScanInfo(), nullptr);
            auto it1 = line1->tokenScanInfo()->allMessages().begin();
            auto it2 = line2->tokenScanInfo()->allMessages().begin();
            for(;it1 != line1->tokenScanInfo()->allMessages().end() &&
                 it2 != line2->tokenScanInfo()->allMessages().end();
                ++it1, ++it2)
            {
                EXPECT_EQ((*it1)->type(), (*it2)->type());
                EXPECT_EQ(line1->tokenPos((*it1)->token()), line2->tokenPos((*it2)->token()));
                EXPECT_STREQ((*it1)->message().c_str(), (*it2)->message().c_str());
            }
        }

        line1 = line1->nextLine();
        line2 = line2->nextLine();
    }
    EXPECT_EQ(line1, nullptr);
    EXPECT_EQ(line2, nullptr);
    EXPECT_GT(guard, 0u);
}

TEST(tstLexerPersistent, testLexerPersistentDumpFiles) {
    auto files = FileInfo::filesInDir("testscripts");
    for(auto &filename : files) {
        FileInfo fi(filename);
        if (fi.ext() != "py")
            continue;
        //std::cout << "Testing to dump " + filename << std::endl;
        Lexer::setVersion(Version::v3_7);
        LexerReader lex;
        lex.readFile("testscripts/" + fi.baseName());
        LexerPersistent lexP(&lex);
        ASSERT_EQ(lexP.dumpToFile("testscripts/dumpfiles/" + fi.baseName() + ".lexdmp"), true);
        ASSERT_EQ(FileInfo::fileExists("testscripts/dumpfiles/" + fi.baseName() + ".lexdmp"), true);
        Lexer lex2;
        LexerPersistent lexP2(&lex2);
        ASSERT_GT(lexP2.reconstructFromDmpFile("testscripts/dumpfiles/" + fi.baseName() + ".lexdmp"), 0);

        // check so each token is restored
        EXPECT_EQ(lex.list().count(), lex2.list().count());
        uint guard = lex.list().max_size();
        auto tok1 = lex.list().front();
        auto tok2 = lex2.list().front();
        while (tok1 && tok2 && (--guard)) {
            EXPECT_EQ(tok1->type(), tok2->type());
            EXPECT_EQ(tok1->line(), tok2->line());
            EXPECT_EQ(tok1->startPos(), tok2->startPos());
            EXPECT_EQ(tok1->endPos(), tok2->endPos());
            tok1 = tok1->next();
            tok2 = tok2->next();
        }
        EXPECT_EQ(tok1, nullptr);
        EXPECT_EQ(tok2, nullptr);
        EXPECT_GT(guard, 0u);

        // check so each line is restored
        EXPECT_EQ(lex.list().lineCount(), lex2.list().lineCount());
        guard = lex.list().max_size();
        auto line1 = lex.list().firstLine();
        auto line2 = lex2.list().firstLine();
        while (line1 && line2 && (--guard)) {
            EXPECT_EQ(line1->lineNr(), line2->lineNr());
            EXPECT_EQ(line1->indent(), line2->indent());
            EXPECT_EQ(line1->blockState(), line2->blockState());
            EXPECT_STREQ(line1->text().c_str(), line2->text().c_str());
            EXPECT_EQ(line1->count(), line2->count());
            EXPECT_EQ(line1->bracketCnt(), line2->bracketCnt());
            EXPECT_EQ(line1->braceCnt(), line2->braceCnt());
            EXPECT_EQ(line1->parenCnt(), line2->parenCnt());
            EXPECT_EQ(line1->isParamLine(), line2->isParamLine());
            EXPECT_EQ(line2->isContinuation(), line2->isContinuation());
            // test line previous and next
            if (line1 != lex.list().lastLine() && line2 != lex2.list().lastLine()) {
                EXPECT_EQ(line1->nextLine()->lineNr(), line2->nextLine()->lineNr());
            }
            if (line1 != lex.list().firstLine() && line2 != lex2.list().firstLine()) {
                EXPECT_EQ(line1->previousLine()->lineNr(), line2->previousLine()->lineNr());
            }
            // test unfinished tokens
            EXPECT_EQ(line1->unfinishedTokens().size(), line2->unfinishedTokens().size());
            auto it1 = line1->unfinishedTokens().begin();
            auto it2 = line2->unfinishedTokens().begin();
            for(;it1 != line1->unfinishedTokens().end() &&
                 it2 != line2->unfinishedTokens().end();
                ++it1, ++it2)
            {
                EXPECT_EQ(*it1, *it2);
            }
            // test scaninfo for this line
            if (line1->tokenScanInfo() && !line1->tokenScanInfo()->allMessages().empty()) {
                ASSERT_NE(line2->tokenScanInfo(), nullptr);
                auto msgs1 = line1->tokenScanInfo()->allMessages();
                auto msgs2 = line2->tokenScanInfo()->allMessages();
                ASSERT_EQ(msgs1.size(), msgs2.size());
                auto scanIt1 = msgs1.begin();
                auto scanIt2 = msgs2.begin();
                for(; scanIt1 != msgs1.end() &&
                      scanIt2 != msgs2.end(); ++scanIt1, ++scanIt2)
                {
                    EXPECT_EQ((*scanIt1)->type(),
                              (*scanIt2)->type());
                    EXPECT_EQ(line1->tokenPos((*scanIt1)->token()), line2->tokenPos((*scanIt2)->token()));
                    EXPECT_STREQ((*scanIt1)->message().c_str(), (*scanIt2)->message().c_str());
                }
            }

            line1 = line1->nextLine();
            line2 = line2->nextLine();
        }
        EXPECT_EQ(line1, nullptr);
        EXPECT_EQ(line2, nullptr);
        EXPECT_GT(guard, 0u);
    }
}

TEST(tstLexerPersistent, testLexerPersistentCompareFiles) {
    auto compareFiles = FileInfo::filesInDir("testscripts/compare/");
    int failedCnt = 0;
    for(auto &filename : compareFiles) {
        FileInfo fi(filename);
        if (fi.ext() != "lexdmp")
            continue;

        std::string scriptname = fi.stem(),
                    ver;

        // here we try to extract the version from the filename
        // there can be several comparefiles with different version to each script
        std::regex verRegex("^(.+)_v(\\d\\.\\d+)\\.py.lexdmp$");
        std::smatch matches;
        if (std::regex_search(filename, matches, verRegex)) {
            scriptname = matches[1].str() + ".py";
            ver = matches[2].str();
        }

        EXPECT_EQ(FileInfo::fileExists("testscripts/" + scriptname), true);
        if (failedCnt < test_info_->result()->total_part_count()){
            std::cout << "**script testscripts/" << scriptname << " does not exist!\n";
            failedCnt = test_info_->result()->total_part_count();
            continue;
        }
        //std::cout << "Comparing " + filename << std::endl;
        Lexer lex2;
        LexerPersistent lexP2(&lex2);
        ASSERT_GT(lexP2.reconstructFromDmpFile("testscripts/compare/" + filename), 0);

        if (!ver.empty()) {
            EXPECT_STREQ(ver.c_str(), lex2.version().versionAsString().c_str());
        }

        LexerReader lex;
        lex.setVersion(lex2.version().version());
        lex.readFile("testscripts/" + scriptname);
        LexerPersistent lexP(&lex);

        // check so each token is restored
        EXPECT_EQ(lex.list().count(), lex2.list().count());
        uint guard = lex.list().max_size();
        auto scriptTok = lex.list().front();
        auto compareTok = lex2.list().front();
        while (scriptTok && compareTok && (--guard)) {
            EXPECT_EQ(scriptTok->type(), compareTok->type());
            EXPECT_EQ(scriptTok->line(), compareTok->line());
            EXPECT_EQ(scriptTok->startPos(), compareTok->startPos());
            EXPECT_EQ(scriptTok->endPos(), compareTok->endPos());
            if (failedCnt < test_info_->result()->total_part_count()) {
                std::cout << "\n**Failed in file:" << fi.path() <<
                             "\nline:" << std::to_string(scriptTok->line()) <<
                             " startPos:" << std::to_string(scriptTok->startPos()) <<
                             " tokenText:'" << scriptTok->text() << "'\n\n";
                failedCnt = test_info_->result()->total_part_count();
                break; // bail out
            }
            scriptTok = scriptTok->next();
            compareTok = compareTok->next();
        }
        EXPECT_EQ(scriptTok, nullptr);
        EXPECT_EQ(compareTok, nullptr);
        EXPECT_GT(guard, 0u);

        // check so each line is restored
        EXPECT_EQ(lex.list().lineCount(), lex2.list().lineCount());
        guard = lex.list().max_size();
        auto lineScript = lex.list().firstLine();
        auto lineCompare = lex2.list().firstLine();
        while (lineScript && lineCompare && (--guard)) {
            EXPECT_EQ(lineScript->lineNr(), lineCompare->lineNr());
            EXPECT_EQ(lineScript->indent(), lineCompare->indent());
            EXPECT_EQ(lineScript->blockState(), lineCompare->blockState());
            EXPECT_STREQ(lineScript->text().c_str(), lineCompare->text().c_str());
            EXPECT_EQ(lineScript->count(), lineCompare->count());
            EXPECT_EQ(lineScript->bracketCnt(), lineCompare->bracketCnt());
            EXPECT_EQ(lineScript->braceCnt(), lineCompare->braceCnt());
            EXPECT_EQ(lineScript->parenCnt(), lineCompare->parenCnt());
            EXPECT_EQ(lineScript->isParamLine(), lineCompare->isParamLine());
            EXPECT_EQ(lineCompare->isContinuation(), lineCompare->isContinuation());
            // test line previous and next
            if (lineScript != lex.list().lastLine() && lineCompare != lex2.list().lastLine()) {
                EXPECT_EQ(lineScript->nextLine()->lineNr(), lineCompare->nextLine()->lineNr());
            }
            if (lineScript != lex.list().firstLine() && lineCompare != lex2.list().firstLine()) {
                EXPECT_EQ(lineScript->previousLine()->lineNr(), lineCompare->previousLine()->lineNr());
            }
            // test unfinished tokens
            EXPECT_EQ(lineScript->unfinishedTokens().size(), lineCompare->unfinishedTokens().size());
            auto it1 = lineScript->unfinishedTokens().begin();
            auto it2 = lineCompare->unfinishedTokens().begin();
            for(;it1 != lineScript->unfinishedTokens().end() &&
                 it2 != lineCompare->unfinishedTokens().end();
                ++it1, ++it2)
            {
                EXPECT_EQ(*it1, *it2);
            }
            // test scaninfo for this line
            if (lineScript->tokenScanInfo() && !lineScript->tokenScanInfo()->allMessages().empty()) {
                ASSERT_NE(lineCompare->tokenScanInfo(), nullptr);
                auto msgs1 = lineScript->tokenScanInfo()->allMessages();
                auto msgs2 = lineCompare->tokenScanInfo()->allMessages();
                ASSERT_EQ(msgs1.size(), msgs2.size());
                auto scanIt1 = msgs1.begin();
                auto scanIt2 = msgs2.begin();
                for(; scanIt1 != msgs1.end() &&
                      scanIt2 != msgs2.end(); ++scanIt1, ++scanIt2)
                {
                    EXPECT_EQ((*scanIt1)->type(),
                              (*scanIt2)->type());
                    EXPECT_EQ(lineScript->tokenPos((*scanIt1)->token()), lineCompare->tokenPos((*scanIt2)->token()));
                    EXPECT_STREQ((*scanIt1)->message().c_str(), (*scanIt2)->message().c_str());
                }
            }
            if (failedCnt < test_info_->result()->total_part_count()) {
                std::cout << "**Failed in file:" << fi.path() <<
                             "\n when testing line:" << std::to_string(lineScript->lineNr()) << std::endl;
                failedCnt = test_info_->result()->total_part_count();
            }

            lineScript = lineScript->nextLine();
            lineCompare = lineCompare->nextLine();
        }
        EXPECT_EQ(lineScript, nullptr);
        EXPECT_EQ(lineCompare, nullptr);
        EXPECT_GT(guard, 0u);
    }
}

TEST(TstLexerStrings, testLexerStringType) {

    static std::list<std::string> lines = {
        "r\"raw string\"",
        "r'raw string'",
        "R'RAW string'",
        "b\"bytes string\"",
        "b'bytes string'",
        "B'BYTES string'",
        "u'unicode string'",
        "u\"unicode string\"",
        "f\"format string\"",
        "f'format string'",
        "F'FORMAT string'",
        "br'raw bytes string'",
        "br\"raw bytes string\"",
        "BR\"RAW BYTES string\"",
        "BR'RAW BYTES string'",
        "rb''' this is",
        " a multiline",
        "string'''"
        "'unclosed"
    };

    Lexer lex;
    for(auto &line : lines) {
        auto tokLine= new TokenLine(nullptr, line);
        lex.list().appendLine(tokLine);
        lex.tokenize(tokLine);
    }

    ASSERT_EQ(lex.list().lineCount(), lines.size());
    ASSERT_EQ(lex.list().lineAt(0)->front()->isStringRaw(), true);
    ASSERT_STREQ(lex.list().lineAt(0)->front()->text().c_str(), "r\"raw string\"");
    ASSERT_STREQ(lex.list().lineAt(0)->front()->content().c_str(), "raw string");
    ASSERT_EQ(lex.list().lineAt(1)->front()->isStringRaw(), true);
    ASSERT_EQ(lex.list().lineAt(2)->front()->isStringRaw(), true);
    ASSERT_EQ(lex.list().lineAt(3)->front()->isStringBytes(), true);
    ASSERT_EQ(lex.list().lineAt(4)->front()->isStringBytes(), true);
    ASSERT_EQ(lex.list().lineAt(5)->front()->isStringBytes(), true);
    ASSERT_EQ(lex.list().lineAt(6)->front()->isStringUnicode(), true);
    ASSERT_EQ(lex.list().lineAt(7)->front()->isStringUnicode(), true);
    ASSERT_EQ(lex.list().lineAt(8)->front()->isStringFormat(), true);
    ASSERT_EQ(lex.list().lineAt(9)->front()->isStringFormat(), true);
    ASSERT_EQ(lex.list().lineAt(10)->front()->isStringFormat(), true);
    ASSERT_EQ(lex.list().lineAt(11)->front()->isStringRaw(), true);
    ASSERT_STREQ(lex.list().lineAt(11)->front()->text().c_str(), "br'raw bytes string'");
    ASSERT_STREQ(lex.list().lineAt(11)->front()->content().c_str(), "raw bytes string");
    ASSERT_EQ(lex.list().lineAt(11)->front()->isStringBytes(), true);
    ASSERT_EQ(lex.list().lineAt(12)->front()->isStringRaw(), true);
    ASSERT_EQ(lex.list().lineAt(12)->front()->isStringBytes(), true);
    ASSERT_EQ(lex.list().lineAt(13)->front()->isStringRaw(), true);
    ASSERT_EQ(lex.list().lineAt(13)->front()->isStringBytes(), true);
    ASSERT_EQ(lex.list().lineAt(14)->front()->isStringRaw(), true);
    ASSERT_EQ(lex.list().lineAt(14)->front()->isStringBytes(), true);
    ASSERT_EQ(lex.list().lineAt(15)->front()->isMultilineString(), true);
    ASSERT_STREQ(lex.list().lineAt(15)->front()->text().c_str(), "rb''' this is\n a multiline\nstring'''");
    ASSERT_STREQ(lex.list().lineAt(15)->front()->content().c_str(), " this is\n a multiline\nstring");
    ASSERT_STREQ(lex.list().lineAt(17)->front()->text().c_str(), "'unclosed\n");
    ASSERT_STREQ(lex.list().lineAt(17)->front()->content().c_str(), "unclosed");
}

TEST(TstLexerOperators, testLexerOperators) {
    std::vector<std::string> lines = {
        "v=0",
        "v==1",
        "v!=2",
        "v<3",
        "v>4",
        "v>=5",
        "v<=6",
        "v*=7",
        "v/=8",
        "v+9",
        "v-10",
        "v**11",
        "v//12",
        "v%13",
        "v@14",
        "v<<15",
        "v>>16",
        "v&17",
        "v|18",
        "v^19",
        "v~20",
        "v:=21"
    };
    Lexer lex;
    lex.setVersion(Version::v3_8);
    for (auto &str : lines) {
        auto tokLine = new TokenLine(nullptr, str);
        lex.list().appendLine(tokLine);
        lex.tokenize(tokLine);
    }
    EXPECT_EQ(lex.list().lineAt(0)->front()->next()->type(), Token::T_OperatorEqual);
    EXPECT_EQ(lex.list().lineAt(1)->front()->next()->type(), Token::T_OperatorCompareEqual);
    EXPECT_EQ(lex.list().lineAt(2)->front()->next()->type(), Token::T_OperatorNotEqual);
    EXPECT_EQ(lex.list().lineAt(3)->front()->next()->type(), Token::T_OperatorLess);
    EXPECT_EQ(lex.list().lineAt(4)->front()->next()->type(), Token::T_OperatorMore);
    EXPECT_EQ(lex.list().lineAt(5)->front()->next()->type(), Token::T_OperatorMoreEqual);
    EXPECT_EQ(lex.list().lineAt(6)->front()->next()->type(), Token::T_OperatorLessEqual);
    EXPECT_EQ(lex.list().lineAt(7)->front()->next()->type(), Token::T_OperatorMulEqual);
    EXPECT_EQ(lex.list().lineAt(8)->front()->next()->type(), Token::T_OperatorDivEqual);
    EXPECT_EQ(lex.list().lineAt(9)->front()->next()->type(), Token::T_OperatorPlus);
    EXPECT_EQ(lex.list().lineAt(10)->front()->next()->type(), Token::T_OperatorMinus);
    EXPECT_EQ(lex.list().lineAt(11)->front()->next()->type(), Token::T_OperatorExponential);
    EXPECT_EQ(lex.list().lineAt(12)->front()->next()->type(), Token::T_OperatorFloorDiv);
    EXPECT_EQ(lex.list().lineAt(13)->front()->next()->type(), Token::T_OperatorModulo);
    EXPECT_EQ(lex.list().lineAt(14)->front()->next()->type(), Token::T_OperatorMatrixMul);
    EXPECT_EQ(lex.list().lineAt(15)->front()->next()->type(), Token::T_OperatorBitShiftLeft);
    EXPECT_EQ(lex.list().lineAt(16)->front()->next()->type(), Token::T_OperatorBitShiftRight);
    EXPECT_EQ(lex.list().lineAt(17)->front()->next()->type(), Token::T_OperatorBitAnd);
    EXPECT_EQ(lex.list().lineAt(18)->front()->next()->type(), Token::T_OperatorBitOr);
    EXPECT_EQ(lex.list().lineAt(19)->front()->next()->type(), Token::T_OperatorBitXor);
    EXPECT_EQ(lex.list().lineAt(20)->front()->next()->type(), Token::T_OperatorBitNot);
    EXPECT_EQ(lex.list().lineAt(21)->front()->next()->type(), Token::T_OperatorWalrus);

    lex.setVersion(Version::v3_7);
    lex.list().clear();
    auto tokLine = new TokenLine(nullptr, lines.at(21));
    lex.list().appendLine(tokLine);
    lex.tokenize(tokLine);

    EXPECT_EQ(lex.list().lineAt(0)->front()->next()->type(), Token::T_SyntaxError);

}
