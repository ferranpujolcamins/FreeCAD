#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <PythonToken.h>
#include <PythonLexer.h>

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

