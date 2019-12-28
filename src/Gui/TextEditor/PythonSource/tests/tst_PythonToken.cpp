#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <PythonToken.h>

using namespace testing;
using namespace Python;

static const size_t lineCnt = 13u;
static const char *srcText[lineCnt] = {
"class test(): # comment",
"    def stringencodecoin(ustr):",
"        \"\"\"stringencodecoin(str): Encodes a unicode object to be used as a string in coin\"\"\"",
"        try:",
"            from pivy import coin",
"            coin4 = coin.COIN_MAJOR_VERSION >= 4",
"        except (ImportError, AttributeError):",
"            coin4 = False",
"        if coin4:",
"            return ustr.encode('utf-8')",
"        else:",
"            # comment 2",
"            return ustr.encode('latin1')"
};

// -----------------------------------------------------------------------

class TokenLinkInherit : public TokenWrapperInherit {
    bool m_notified;
public:
    TokenLinkInherit(Token *tok) :
        TokenWrapperInherit(tok),
        m_notified(false)
    {}
    ~TokenLinkInherit() override;
    bool isNotified() const { return m_notified; }
protected:
    void tokenDeletedCallback() override {
        m_notified = true;
    }
};
TokenLinkInherit::~TokenLinkInherit() {}

class TokenLinkTemplate {
    bool m_notified;
    TokenWrapper<TokenLinkTemplate> m_wrapper;
public:
    TokenLinkTemplate(Token *tok) :
        m_notified(false), m_wrapper(tok, this, &TokenLinkTemplate::tokenDelCallback)
    {
    }
    bool isNotified() const { return m_notified; }
    void tokenDelCallback(TokenWrapperBase *wrapper) {
        (void)wrapper;
        m_notified = true;
    }
};

// -----------------------------------------------------------------------

class TstVersion : public ::testing::Test
{
protected:
    Version version;
public:
    TstVersion() : version(Version::v3_7) {}
    ~TstVersion() override {}
    void SetUp() override {}
    void TearDown() override;
};
void TstVersion::TearDown() {}

// -----------------------------------------------------------------------

class TstPythonTokenList : public ::testing::Test
{
protected:
    TokenList *_list;
public:
    TstPythonTokenList() : ::testing::Test() {
        _list = new TokenList(nullptr);
    }
    ~TstPythonTokenList() override;
};
TstPythonTokenList::~TstPythonTokenList() {
    delete _list;
}

// -----------------------------------------------------------------------

class TstPythonTokenLine : public TstPythonTokenList
{
protected:
    TokenLine* _line[lineCnt];
public:
    TstPythonTokenLine() :
        TstPythonTokenList()
    {
        for(size_t i = 0; i< lineCnt; ++i)
            _line[i] = new TokenLine(nullptr, srcText[i]);
    }
    ~TstPythonTokenLine() override;
};
TstPythonTokenLine::~TstPythonTokenLine() {
    while(!_list->empty()) {
        auto l = _list->lastLine();
        delete l;
    }
}

// -----------------------------------------------------------------------
class TstPythonToken : public TstPythonTokenLine
{
public:
    void SetUp() override {
        TstPythonTokenLine::SetUp();
    }
    void TearDown() override;
    void fillAllLines() {
        _list->appendLine(_line[0]);
        _list->appendLine(_line[1]);
        _list->appendLine(_line[2]);
        _list->appendLine(_line[3]);
        _list->appendLine(_line[4]);
        _list->appendLine(_line[5]);
        _list->appendLine(_line[6]);
        _list->appendLine(_line[7]);
        _list->appendLine(_line[8]);
        _list->appendLine(_line[9]);
        _list->appendLine(_line[10]);
        _list->appendLine(_line[11]);
        _list->appendLine(_line[12]);
        fillLine0();
        fillLine1();
        fillLine2();
        fillLine3();
        fillLine4();
        fillLine5();
        fillLine6();
        fillLine7();
        fillLine8();
        fillLine9();
        fillLine10();
        fillLine11();
        fillLine12();
    }
    void fillLine0() {
        _line[0]->push_front(new Token(Token::T_IdentifierClass, 6, 10, 0));
        _line[0]->push_front(new Token(Token::T_KeywordClass, 0, 4, 0));
        _line[0]->push_back(new Token(Token::T_DelimiterOpenParen, 11, 12, 0));
        _line[0]->push_back(new Token(Token::T_DelimiterCloseParen, 12, 13, 0));
        _line[0]->push_back(new Token(Token::T_DelimiterColon, 13, 14, 0));
        _line[0]->push_back(new Token(Token::T_DelimiterNewLine, 14, 15, 0));
    }
    void fillLine1() {
        //_line[1]->setIndentCount(4);
        _line[1]->push_front(new Token(Token::T_IdentifierMethod, 7, 23, 0));
        _line[1]->push_front(new Token(Token::T_KeywordDef, 4, 6, 0));
        _line[1]->insert(new Token(Token::T_Indent, 0, 3, 0));
        _line[1]->insert(_line[1]->back(), new Token(Token::T_DelimiterOpenParen, 23, 24, 0));
        _line[1]->push_back(new Token(Token::T_IdentifierDefined, 24, 27, 0));
        _line[1]->push_back(new Token(Token::T_DelimiterCloseParen, 28, 29, 0));
        _line[1]->push_back(new Token(Token::T_DelimiterColon, 29, 30, 0));
        _line[1]->push_back(new Token(Token::T_DelimiterNewLine, 30, 30, 0));
    }
    void fillLine2() {
        _line[2]->push_back(new Token(Token::T_Indent, 0, 7, 0));
        _line[2]->push_back(new Token(Token::T_LiteralBlockDblQuote, 8, 73, 0));
        _line[2]->push_back(new Token(Token::T_DelimiterNewLine, 73, 73, 0));
    }
    void fillLine3() {
        _line[3]->push_back(new Token(Token::T_KeywordDef, 8, 11, 0));
        _line[3]->push_back(new Token(Token::T_DelimiterColon, 11,12, 0));
        _line[3]->push_back(new Token(Token::T_DelimiterNewLine, 12, 12, 0));
    }
    void fillLine4() {
        _line[4]->push_back(new Token(Token::T_KeywordFrom, 12, 16, 0));
        _line[4]->push_front(new Token(Token::T_Indent, 0, 11, 0));
        _line[4]->push_back(new Token(Token::T_IdentifierModulePackage, 17, 21, 0));
        _line[4]->push_back(new Token(Token::T_IdentifierModulePackage, 17, 21, 0));
        _line[4]->push_back(new Token(Token::T_DelimiterNewLine, 21, 21, 0));
    }
    void fillLine5() {
        _line[5]->push_back(new Token(Token::T_IdentifierDefined, 12, 17, 0));
        _line[5]->push_back(new Token(Token::T_OperatorEqual, 19, 20, 0));
        _line[5]->push_back(new Token(Token::T_IdentifierUnknown, 21, 25, 0));
        _line[5]->push_back(new Token(Token::T_DelimiterPeriod, 25, 26, 0));
        _line[5]->push_back(new Token(Token::T_IdentifierUnknown, 26, 43, 0));
        _line[5]->push_back(new Token(Token::T_OperatorMoreEqual, 44, 46, 0));
        _line[5]->push_back(new Token(Token::T_NumberDecInt, 47, 48, 0));
        _line[5]->push_back(new Token(Token::T_DelimiterNewLine, 48, 48, 0));
        _line[5]->push_back(new Token(Token::T_Dedent, 48, 48, 0));
    }
    void fillLine6() {
        _line[6]->push_back(new Token(Token::T_KeywordExcept, 8, 13, 0));
        _line[6]->push_back(new Token(Token::T_DelimiterOpenParen, 14, 15, 0));
        _line[6]->push_back(new Token(Token::T_IdentifierBuiltin, 15, 27, 0));
        _line[6]->push_back(new Token(Token::T_DelimiterComma, 28, 29, 0));
        _line[6]->push_back(new Token(Token::T_IdentifierBuiltin, 30, 42, 0));
        _line[6]->push_back(new Token(Token::T_DelimiterCloseParen, 42, 43, 0));
        _line[6]->push_back(new Token(Token::T_DelimiterColon, 43, 44, 0));
        _line[6]->push_back(new Token(Token::T_DelimiterNewLine, 44, 44, 0));
    }
    void fillLine7() {
        _line[7]->push_back(new Token(Token::T_Indent, 0, 11, 0));
        _line[7]->push_back(new Token(Token::T_IdentifierUnknown, 11, 14, 0));
        _line[7]->push_back(new Token(Token::T_OperatorEqual, 15, 16, 0));
        _line[7]->push_back(new Token(Token::T_IdentifierFalse, 17, 22, 0));
        _line[7]->push_back(new Token(Token::T_DelimiterNewLine, 22, 22, 0));
        _line[7]->push_back(new Token(Token::T_Dedent, 22, 22, 0));
    }
    void fillLine8() {
        _line[8]->push_back(new Token(Token::T_KeywordIf, 7, 9, 0));
        _line[8]->push_back(new Token(Token::T_IdentifierDefined, 10, 15, 0));
        _line[8]->push_back(new Token(Token::T_DelimiterColon, 15, 16, 0));
        _line[8]->push_back(new Token(Token::T_DelimiterNewLine, 16, 16, 0));
    }
    void fillLine9() {
        _line[9]->push_back(new Token(Token::T_Indent, 0, 12, 0));
        _line[9]->push_back(new Token(Token::T_KeywordReturn, 12, 18, 0));
        _line[9]->push_back(new Token(Token::T_IdentifierDefined, 19, 23, 0));
        _line[9]->push_back(new Token(Token::T_DelimiterPeriod, 23, 24, 0));
        _line[9]->push_back(new Token(Token::T_IdentifierDefined, 24, 30, 0));
        _line[9]->push_back(new Token(Token::T_DelimiterOpenParen, 30, 31, 0));
        _line[9]->push_back(new Token(Token::T_LiteralSglQuote, 31, 38, 0));
        _line[9]->push_back(new Token(Token::T_DelimiterCloseParen, 38, 39, 0));
        _line[9]->push_back(new Token(Token::T_DelimiterNewLine, 39, 39, 0));
        _line[9]->push_back(new Token(Token::T_Dedent, 39, 39, 0));
    }
    void fillLine10() {
        _line[10]->push_back(new Token(Token::T_KeywordElse, 7, 11, 0));
        _line[10]->push_back(new Token(Token::T_DelimiterColon, 11, 12, 0));
        _line[10]->push_back(new Token(Token::T_DelimiterNewLine, 12, 12, 0));
    }
    void fillLine11() {
        _line[11]->push_back(new Token(Token::T_Comment, 11, 22, 0));
    }
    void fillLine12() {
        _line[12]->push_back(new Token(Token::T_Indent, 0, 11, 0));
        _line[12]->push_back(new Token(Token::T_KeywordReturn, 11, 17, 0));
        _line[12]->push_back(new Token(Token::T_IdentifierDefined, 18, 22, 0));
        _line[12]->push_back(new Token(Token::T_DelimiterPeriod, 22, 23, 0));
        _line[12]->push_back(new Token(Token::T_IdentifierDefined, 23, 29, 0));
        _line[12]->push_back(new Token(Token::T_DelimiterOpenParen, 29, 30, 0));
        _line[12]->push_back(new Token(Token::T_LiteralSglQuote, 30, 38, 0));
        _line[12]->push_back(new Token(Token::T_DelimiterCloseParen, 38, 39, 0));
        _line[12]->push_back(new Token(Token::T_DelimiterNewLine, 39, 39, 0));
        _line[12]->push_back(new Token(Token::T_Dedent, 39, 39, 0));
    }
};
void TstPythonToken::TearDown() {
    TstPythonTokenLine::TearDown();
}
// ----------------------------------------------------------------------

// start tests for PythonVersion
TEST_F(TstVersion, testVersion) {
    EXPECT_EQ(version.version(), Version::v3_7);
    EXPECT_EQ(version.minorVersion(), 7u);
    EXPECT_EQ(version.majorVersion(), 3u);
    EXPECT_STREQ(version.versionAsString().c_str(), "3.7");
    version.setVersion(Version::v3_0);
    EXPECT_EQ(version.version(), Version::v3_0);
    EXPECT_EQ(version.minorVersion(), 0u);
    EXPECT_EQ(version.majorVersion(), 3u);
    EXPECT_STREQ(version.versionAsString().c_str(), "3.0");
    version.setVersion(Version::Latest);
    EXPECT_EQ(version.version(), Version::Latest);
    EXPECT_EQ(version.minorVersion(), 9u);
    EXPECT_EQ(version.majorVersion(), 3u);
    EXPECT_STREQ(version.versionAsString().c_str(), "Latest");

    auto available = Version::availableVersions();
    EXPECT_EQ(available.begin()->second, "2.6");
    EXPECT_EQ(available.rbegin()->second, "Latest");

    EXPECT_STREQ(Version::versionAsString(Version::v3_0).c_str(), "3.0");
}

TEST_F(TstVersion, testVersionCopy) {
    version.setVersion(Version::v2_7);
    Version ver2(version);
    EXPECT_EQ(ver2.version(), Version::v2_7);
    ver2.setVersion(Version::v3_5);
    Version ver3 = ver2;
    EXPECT_EQ(ver3.version(), Version::v3_5);
}


// start tests for tokenList, TokenLine, Token
TEST_F(TstPythonTokenList, testPythonTokenListDefaults) {
    ASSERT_EQ(_list->count(), 0u);
    ASSERT_EQ(_list->lexer(), nullptr);
    ASSERT_EQ(_list->front(), nullptr);
    ASSERT_EQ(_list->back(), nullptr);
    ASSERT_EQ((*_list)[0], nullptr);
    ASSERT_EQ((*_list)[10], nullptr);
    ASSERT_EQ((*_list)[-10], nullptr);
    ASSERT_EQ(_list->empty(), true);
    ASSERT_GT(_list->max_size(), 2000000u);

    ASSERT_EQ(_list->firstLine(), nullptr);
    ASSERT_EQ(_list->lastLine(), nullptr);
    ASSERT_EQ(_list->lineAt(0), nullptr);
    ASSERT_EQ(_list->lineAt(10), nullptr);
    ASSERT_EQ(_list->lineAt(-10), nullptr);
    ASSERT_EQ(_list->lineCount(), 0u);
}

TEST_F(TstPythonTokenLine, testPythonTokenListAppendLine) {
    _list->appendLine(_line[0]);
    ASSERT_EQ(_list->lineCount(), 1u);
    ASSERT_EQ(_list->lineAt(0), _line[0]);
    ASSERT_EQ(_list->lineAt(1), nullptr);
    ASSERT_EQ(_list->lineAt(-1), _line[0]);
    ASSERT_EQ(_list->firstLine(), _line[0]);
    ASSERT_EQ(_list->lastLine(), _line[0]);
    EXPECT_EQ(_line[0]->ownerList(), _list);

    // test remove line
    _list->removeLine(_line[0], false);
    ASSERT_EQ(_list->lineCount(), 0u);
    ASSERT_EQ(_list->lineAt(0), nullptr);
    ASSERT_EQ(_list->firstLine(), nullptr);
    ASSERT_EQ(_list->lastLine(), nullptr);
    EXPECT_EQ(_line[0]->ownerList(), nullptr);

    _list->appendLine(_line[1]);
    ASSERT_EQ(_list->lineCount(), 1u);
    _list->appendLine(_line[0]);
    ASSERT_EQ(_list->lineCount(), 2u);
    ASSERT_EQ(_list->firstLine(), _line[1]);
    ASSERT_EQ(_list->lastLine(), _line[0]);
    _list->appendLine(_line[2]);
    ASSERT_EQ(_list->lineCount(), 3u);
    ASSERT_EQ(_list->lineAt(1), _line[0]);
    ASSERT_EQ(_list->firstLine(), _line[1]);
    ASSERT_EQ(_list->lastLine(), _line[2]);

    // delete the lines we don't use here
    for (size_t i = 3; i < lineCnt; ++i)
        delete _line[i];
}

TEST_F(TstPythonTokenLine, testPythonTokenListInsertLine) {
    // test insertLine first line
    _list->insertLine(nullptr, _line[0]);
    ASSERT_EQ(_list->lineAt(0), _line[0]);
    ASSERT_EQ(_list->lineAt(1), nullptr);
    ASSERT_EQ(_list->lineCount(), 1u);
    ASSERT_EQ(_line[1]->previousLine(), nullptr);
    ASSERT_EQ(_line[1]->nextLine(), nullptr);

    // test assert that we can't insert the same line again
    ASSERT_DEATH({
        _list->insertLine(nullptr, _line[0]);
    }, "insertLine contained in another list");

    // test insert before the already inserted line
    _list->insertLine(nullptr, _line[1]);
    ASSERT_EQ(_list->lineAt(0), _line[1]);
    ASSERT_EQ(_line[1]->previousLine(), nullptr);
    ASSERT_EQ(_line[1]->nextLine(), _line[0]);
    ASSERT_EQ(_list->lineAt(1), _line[0]);
    ASSERT_EQ(_line[0]->previousLine(), _line[1]);
    ASSERT_EQ(_line[0]->nextLine(), nullptr);
    ASSERT_EQ(_list->lineCount(), 2u);

    // test insert in the middle
    _list->insertLine(_line[1], _line[2]);
    ASSERT_EQ(_list->lineAt(0), _line[1]);
    ASSERT_EQ(_list->lineAt(1), _line[2]);
    ASSERT_EQ(_list->lineAt(2), _line[0]);
    ASSERT_EQ(_list->lineCount(), 3u);
    ASSERT_EQ(_list->firstLine(), _line[1]);
    ASSERT_EQ(_list->lastLine(), _line[0]);

    // test insert with idx (last)
    _list->insertLine(3, _line[3]);
    ASSERT_EQ(_list->lastLine(), _line[3]);
    _list->insertLine(0, _line[4]);
    ASSERT_EQ(_list->firstLine(), _line[4]);
    _list->insertLine(1, _line[5]);
    ASSERT_EQ(_list->lineAt(0), _line[4]);
    ASSERT_EQ(_list->lineAt(1), _line[5]);
    ASSERT_EQ(_list->lineAt(2), _line[1]);
    ASSERT_EQ(_list->lineAt(3), _line[2]);
    ASSERT_EQ(_list->lineAt(4), _line[0]);
    ASSERT_EQ(_list->lineAt(5), _line[3]);
}

TEST_F(TstPythonTokenLine, testPythonTokenLineSwap) {
    _list->appendLine(_line[0]);
    _list->appendLine(_line[1]);
    _list->appendLine(_line[2]);
    ASSERT_EQ(_list->firstLine(), _line[0]);
    ASSERT_EQ(_list->firstLine()->nextLine(), _line[1]);
    ASSERT_EQ(_list->firstLine()->nextLine()->nextLine(), _line[2]);

    _list->swapLine(2, _line[3]);
    ASSERT_EQ(_list->firstLine()->nextLine()->nextLine(), _line[3]);
    _list->swapLine(_list->firstLine()->nextLine(), _line[4]);
    ASSERT_EQ(_list->firstLine()->nextLine(), _line[4]);
    ASSERT_EQ(_list->lineCount(), 3u);
    ASSERT_EQ(_list->lineAt(0), _line[0]);
    ASSERT_EQ(_list->lastLine(), _line[3]);
    ASSERT_EQ(_list->lastLine()->previousLine(), _line[4]);
    ASSERT_EQ(_list->lastLine()->previousLine()->previousLine(), _line[0]);
}

TEST_F(TstPythonTokenLine, testPythonTokenLineRemove) {
    _list->appendLine(_line[0]);
    _list->appendLine(_line[1]);
    _list->appendLine(_line[2]);
    _list->appendLine(_line[3]);
    _list->appendLine(_line[4]);

    ASSERT_EQ(_list->lineCount(), 5u);
    _list->removeLine(0);
    ASSERT_EQ(_list->lineCount(), 4u);
    ASSERT_EQ(_list->firstLine(), _line[1]);
    EXPECT_DEATH({
        // should not remove, assert chrash
        _list->removeLine(4);
    }, "lineToRemove==nulltpr");
    ASSERT_EQ(_list->lineCount(), 4u);
    _list->removeLine(3);
    ASSERT_EQ(_list->lastLine(), _line[3]);
    ASSERT_EQ(_list->lineCount(), 3u);
    _list->removeLine(_line[2]);
    ASSERT_EQ(_list->lineCount(), 2u);
    ASSERT_EQ(_list->lineAt(0), _line[1]);
    ASSERT_EQ(_list->lineAt(1), _line[3]);
    _list->removeLine(-1);
    ASSERT_EQ(_list->lineCount(), 1u);
    ASSERT_EQ(_list->lineAt(0), _line[1]);
    ASSERT_EQ(_list->firstLine(), _line[1]);
    ASSERT_EQ(_list->lastLine(), _line[1]);
    _list->removeLine(0);
    ASSERT_EQ(_list->lineCount(), 0u);
    ASSERT_EQ(_list->firstLine(), nullptr);
    ASSERT_EQ(_list->lastLine(), nullptr);
}

TEST_F(TstPythonTokenLine, testPythonTokenLineTrimLineEnds)
{
    std::string expectStr(srcText[0]);
    expectStr += "\n";
    EXPECT_STREQ(_line[0]->text().c_str(), expectStr.c_str());

    std::string rnStr(srcText[0]);
    rnStr += "\r\n";
    TokenLine rnLine(nullptr, rnStr);
    EXPECT_STREQ(rnLine.text().c_str(), expectStr.c_str());

    std::string rStr(srcText[0]);
    rStr += "\r";
    TokenLine rLine(nullptr, rStr);
    EXPECT_STREQ(rLine.text().c_str(), expectStr.c_str());

    std::string nStr(srcText[0]);
    nStr += "\n";
    TokenLine nLine(nullptr, nStr);
    EXPECT_STREQ(nLine.text().c_str(), expectStr.c_str());

    std::string nrStr(srcText[0]);
    nrStr += "\n\r";
    TokenLine nrLine(nullptr, nrStr);
    EXPECT_STREQ(nrLine.text().c_str(), expectStr.c_str());
}

TEST_F(TstPythonTokenLine, testPythonTokenLineLineNr) {
    _list->appendLine(_line[0]);
    _list->appendLine(_line[1]);
    _list->appendLine(_line[2]);
    _list->appendLine(_line[3]);
    _list->appendLine(_line[4]);
    _list->appendLine(_line[5]);
    EXPECT_EQ(_line[0]->lineNr(), 0u);
    EXPECT_EQ(_line[1]->lineNr(), 1u);
    EXPECT_EQ(_line[2]->lineNr(), 2u);
    EXPECT_EQ(_line[3]->lineNr(), 3u);
    EXPECT_EQ(_line[4]->lineNr(), 4u);
    EXPECT_EQ(_line[5]->lineNr(), 5u);
    _list->insertLine(_line[0], _line[6]);
    EXPECT_EQ(_line[0]->lineNr(), 0u);
    EXPECT_EQ(_line[6]->lineNr(), 1u);
    EXPECT_EQ(_line[1]->lineNr(), 2u);
    EXPECT_EQ(_line[2]->lineNr(), 3u);
    EXPECT_EQ(_line[3]->lineNr(), 4u);
    EXPECT_EQ(_line[4]->lineNr(), 5u);
    EXPECT_EQ(_line[5]->lineNr(), 6u);
    _list->insertLine(nullptr, _line[7]);
    EXPECT_EQ(_line[7]->lineNr(), 0u);
    EXPECT_EQ(_line[0]->lineNr(), 1u);
    EXPECT_EQ(_line[6]->lineNr(), 2u);
    _list->insertLine(_line[5], _line[8]);
    EXPECT_EQ(_line[7]->lineNr(), 0u);
    EXPECT_EQ(_line[5]->lineNr(), 7u);
    EXPECT_EQ(_line[8]->lineNr(), 8u);

    _list->removeLine(_line[5]);
    EXPECT_EQ(_line[8]->lineNr(), 7u);
    _list->removeLine(_line[7]);
    EXPECT_EQ(_line[0]->lineNr(), 0u);
    EXPECT_EQ(_line[6]->lineNr(), 1u);
}

TEST_F(TstPythonTokenLine, testPythonTokenLineLineAt) {
    _list->appendLine(_line[0]);
    _list->appendLine(_line[1]);
    _list->appendLine(_line[2]);
    _list->appendLine(_line[3]);
    _list->appendLine(_line[4]);
    _list->appendLine(_line[5]);
    _list->appendLine(_line[6]);
    ASSERT_EQ(_list->lineAt(0), _line[0]);
    ASSERT_EQ(_list->lineAt(1), _line[1]);
    ASSERT_EQ(_list->lineAt(2), _line[2]);
    ASSERT_EQ(_list->lineAt(3), _line[3]);
    ASSERT_EQ(_list->lineAt(4), _line[4]);
    ASSERT_EQ(_list->lineAt(5), _line[5]);
    ASSERT_EQ(_list->lineAt(6), _line[6]);
    ASSERT_EQ(_list->lineAt(7), nullptr);

    ASSERT_EQ(_list->lineAt(-1), _line[6]);
    ASSERT_EQ(_list->lineAt(-2), _line[5]);
    ASSERT_EQ(_list->lineAt(-3), _line[4]);
    ASSERT_EQ(_list->lineAt(-4), _line[3]);
    ASSERT_EQ(_list->lineAt(-5), _line[2]);
    ASSERT_EQ(_list->lineAt(-6), _line[1]);
    ASSERT_EQ(_list->lineAt(-7), _line[0]);
    ASSERT_EQ(_list->lineAt(-8), nullptr);
}

TEST_F(TstPythonTokenLine, testPythonTokenLineClearLines) {
    _list->appendLine(_line[0]);
    _list->appendLine(_line[1]);
    _list->appendLine(_line[2]);
    _list->appendLine(_line[3]);
    _list->appendLine(_line[4]);
    _list->appendLine(_line[5]);
    _list->appendLine(_line[6]);
    ASSERT_EQ(_list->lineCount(), 7u);
    ASSERT_EQ(_list->empty(), false);
    _list->clear();
    ASSERT_EQ(_list->lineCount(), 0u);
    ASSERT_EQ(_list->firstLine(), nullptr);
    ASSERT_EQ(_list->lastLine(), nullptr);
    ASSERT_EQ(_list->empty(), true);
}

TEST_F(TstPythonToken, testPythonTokenLineTokenInsert) {
    ASSERT_EQ(_list->empty(), true);

    _list->appendLine(_line[0]);
    fillLine0();
    EXPECT_EQ(_list->count(), 6u);
    EXPECT_EQ(_line[0]->count(), 6u);
    ASSERT_EQ(_list->empty(), false);
    EXPECT_EQ(_list->front(), _line[0]->front());
    EXPECT_EQ(_list->back(), _line[0]->back());
    EXPECT_EQ(_line[0]->front()->type(), Token::T_KeywordClass);
    EXPECT_EQ(_line[0]->back()->type(), Token::T_DelimiterNewLine);
    EXPECT_EQ((*_line[0])[1]->type(), Token::T_IdentifierClass);
    EXPECT_EQ((*_line[0])[2]->type(), Token::T_DelimiterOpenParen);
    EXPECT_EQ((*_line[0])[3]->type(), Token::T_DelimiterCloseParen);
    EXPECT_EQ((*_line[0])[4]->type(), Token::T_DelimiterColon);
    EXPECT_EQ((*_line[0])[5]->type(), Token::T_DelimiterNewLine);
    EXPECT_EQ(_line[0]->isCodeLine(), true);
    // check so bounds are set correctly
    Token *first = _list->front(),
          *next = _list->front()->next();
    EXPECT_EQ(first->previous(), nullptr);
    EXPECT_EQ(first->next(), next);
    EXPECT_EQ(next->previous(), first);
    Token *last = _list->back();
    next = last->previous();
    EXPECT_EQ(last->next(), nullptr);
    EXPECT_EQ(next, last->previous());
    EXPECT_EQ(next->next(), last);

    _list->appendLine(_line[1]);
    fillLine1();
    EXPECT_EQ(_line[1]->count(), 8u);
    EXPECT_EQ(_list->count(), 14u);
    EXPECT_EQ(_list->back(), (*_line[1])[7]);
    EXPECT_EQ(_list->front(), (*_line[0])[0]);
    EXPECT_EQ(_list->lastLine()->isCodeLine(), true);
    // test ined markers
    EXPECT_EQ(&(*_line[1]->end()), nullptr);
    EXPECT_EQ(&(*_line[1]->rend()), _line[0]->back());
    EXPECT_EQ(&(*_line[0]->rend()), nullptr);
    EXPECT_EQ(&(*_line[0]->end()), _line[1]->front());

    // test swapLine
    _list->swapLine(_line[1], _line[11]);
    fillLine11();
    EXPECT_EQ(_list->lastLine()->isCodeLine(), false);
    EXPECT_EQ(_list->back()->type(), Token::T_Comment);
    EXPECT_EQ(_list->front()->type(), Token::T_KeywordClass);
}

TEST_F(TstPythonToken, testPythonTokenListIterator) {
    fillAllLines();
    uint sz = 0;
    EXPECT_EQ(76u, _list->count());
    for (auto &tok : *_list) {
        ++sz;
        (void)tok;
    }
    ASSERT_EQ(sz, _list->count());
    sz = 0;
    for (auto it = _list->rbegin(); it != _list->rend(); --it)
        ++sz;
    ASSERT_EQ(sz, _list->count());
    sz = 0;
    auto itF = _list->begin();
    auto itR = _list->rbegin();
    int pos = 0, rpos = static_cast<int>(_list->count()) -1;
    while (itF != _list->end() && itR != _list->rend() && rpos > -1) {
        ASSERT_EQ((*_list)[pos++], &(*itF++));
        ASSERT_EQ((*_list)[rpos--], &(*itR--));
    }
    EXPECT_EQ(&(*itF), nullptr);
    EXPECT_EQ(&(*itR), nullptr);

    pos = 0; rpos = static_cast<int>(_list->count()) -1;
    while (itF != _list->end() && itR != _list->rend() && rpos > -1) {
        ASSERT_EQ((*_list)[++pos], &(*(++itF)));
        ASSERT_EQ((*_list)[--rpos], &(*(--itR)));
    }
}

TEST_F(TstPythonToken, testPythonTokenLineIterator) {

    fillAllLines();
    uint sz = 0;
    TokenLine *line = _list->lineAt(9u);
    EXPECT_EQ(10u, line->count());
    for (auto &tok : *line) {
        ++sz;
        (void)tok;
    }
    ASSERT_EQ(sz, line->count());
    sz = 0;
    for (auto it = line->rbegin(); it != line->rend(); --it)
        ++sz;
    ASSERT_EQ(sz, line->count());
    sz = 0;
    auto itF = line->begin();
    auto itR = line->rbegin();
    int pos = 0, rpos = static_cast<int>(line->count()) -1;
    while (itF != line->end() && itR != line->rend() && rpos > -1) {
        ASSERT_EQ((*line)[pos++], &(*itF++));
        ASSERT_EQ((*line)[rpos--], &(*itR--));
    }
    EXPECT_EQ(&(*itF), line->nextLine()->front());
    EXPECT_EQ(&(*itR), line->previousLine()->back());

    pos = 0; rpos = static_cast<int>(line->count()) -1;
    while (itF != line->end() && itR != line->rend() && rpos > -1) {
        ASSERT_EQ((*line)[++pos], &(*(++itF)));
        ASSERT_EQ((*line)[--rpos], &(*(--itR)));
    }
}

TEST_F(TstPythonToken, testPythonTokenLineAccessor) {
    fillAllLines();
    EXPECT_EQ(_line[0]->tokenAt(9)->type(), Token::T_IdentifierClass);
    EXPECT_EQ(_list->count(), 76u);
    auto tokensList = _line[9]->tokens();
    EXPECT_EQ(tokensList.size(), 10u);
    EXPECT_EQ(*tokensList.begin(), _line[9]->front());
    EXPECT_EQ((*tokensList.begin())->type(), Token::T_Indent);
    EXPECT_EQ(*tokensList.rbegin(), _line[9]->back());
    EXPECT_EQ((*tokensList.rbegin())->type(), Token::T_Dedent);
    EXPECT_EQ(_line[9]->tokenPos(*tokensList.rbegin()), 9);
}

TEST_F(TstPythonToken, testPythonTokenComparison) {
    fillAllLines();
    // test comparison of tokens
    EXPECT_LT(*(*_line[7])[0], *(*_line[9])[0]);
    EXPECT_GT(*(*_line[9])[0], *(*_line[7])[0]);
    EXPECT_EQ(*(*_line[9])[1], *(*_line[9])[1]);
    EXPECT_LT(*(*_line[9])[1], *(*_line[9])[2]);
    EXPECT_LT(*(*_line[9])[8], *(*_line[9])[9]);
}

TEST_F(TstPythonToken, testPythonTokenProperties) {
    fillAllLines();
    EXPECT_EQ(_line[0]->front()->line(), 0);
    EXPECT_EQ(_line[1]->front()->line(), 1);
    EXPECT_EQ(_line[2]->front()->line(), 2);
    EXPECT_EQ(_line[3]->front()->line(), 3);
    EXPECT_EQ(_line[4]->front()->line(), 4);
    EXPECT_EQ(_line[5]->front()->line(), 5);
    EXPECT_EQ(_line[6]->front()->line(), 6);
    EXPECT_EQ(_line[7]->front()->line(), 7);
    EXPECT_EQ(_line[8]->front()->line(), 8);
    EXPECT_EQ(_line[9]->front()->line(), 9);
    EXPECT_EQ(_line[10]->front()->line(), 10);
    EXPECT_EQ(_line[11]->front()->line(), 11);
    EXPECT_EQ(_line[12]->front()->line(), 12);

    EXPECT_STREQ((*_line[9])[0]->text().c_str(), "            ");
    EXPECT_STREQ((*_line[9])[1]->text().c_str(), "return");
    EXPECT_STREQ((*_line[9])[2]->text().c_str(), "ustr");
    EXPECT_STREQ((*_line[9])[3]->text().c_str(), ".");

    EXPECT_EQ((*_line[9])[0]->startPos(), 0u);
    EXPECT_EQ((*_line[9])[0]->startPosInt(), 0);
    EXPECT_EQ((*_line[9])[0]->endPos(), 12u);
    EXPECT_EQ((*_line[9])[0]->endPosInt(), 12);
    EXPECT_EQ((*_line[9])[1]->textLength(), 6u);

    EXPECT_NE((*_line[9])[2]->hash(), (*_line[9])[4]->hash());

    EXPECT_EQ(_line[8]->front()->ownerLine(), _line[8]);
    EXPECT_NE(_line[0]->front()->ownerLine(), _line[8]);

    EXPECT_EQ(_line[7]->front()->ownerList(), _list);

    EXPECT_EQ(_line[11]->back()->isCode(), false);
    EXPECT_EQ(_line[11]->back()->isText(), true);
}

TEST_F(TstPythonToken, testPythonTokenTypeTest) {
    Token inValidTok(Token::T_Invalid, 0, 0, 0);
    EXPECT_EQ(inValidTok.isInValid(), true);
    EXPECT_EQ(inValidTok.isInt(), false);

    fillAllLines();
    EXPECT_EQ((*_line[0])[0]->isKeyword(), true);
    EXPECT_EQ((*_line[0])[1]->isIdentifier(), true);
    EXPECT_EQ((*_line[0])[1]->isIdentifierFrameStart(), true);
    EXPECT_EQ((*_line[0])[1]->isIdentifierVariable(), false);
    EXPECT_EQ((*_line[0])[2]->isDelimiter(), true);
    EXPECT_EQ((*_line[0])[3]->isDelimiter(), true);
    EXPECT_EQ((*_line[0])[5]->isDelimiter(), true);
    EXPECT_EQ((*_line[1])[0]->isText(), false);
    EXPECT_EQ((*_line[2])[1]->isText(), true);
    EXPECT_EQ((*_line[2])[1]->isCode(), true);
    EXPECT_EQ((*_line[5])[1]->isOperator(), true);
    EXPECT_EQ((*_line[5])[1]->isOperatorAssignment(), true);
    EXPECT_EQ((*_line[7])[1]->isIdentifier(), true);
    EXPECT_EQ((*_line[7])[3]->isBoolean(), true);
    EXPECT_EQ((*_line[9])[6]->isString(), true);
}

TEST_F(TstPythonToken, testPythonTokenRemove) {
    fillAllLines();
    EXPECT_EQ(_list->count(), 76u);
    EXPECT_EQ(_line[12]->count(), 10u);
    EXPECT_EQ(_line[12]->back()->type(), Token::T_Dedent);
    EXPECT_EQ(_line[0]->front()->type(), Token::T_KeywordClass);
    _list->lastLine()->pop_back();
    EXPECT_EQ(_list->count(), 75u);
    EXPECT_EQ(_line[12]->count(), 9u);
    EXPECT_EQ(_line[12]->back()->type(), Token::T_DelimiterNewLine);
    _list->firstLine()->pop_front();
    EXPECT_EQ(_list->count(), 74u);
    EXPECT_EQ(_line[0]->front()->type(), Token::T_IdentifierClass);
    EXPECT_EQ(_line[0]->count(), 5u);
    _list->lineAt(5)->remove((*_line[5])[4]);
    EXPECT_EQ(_list->count(), 73u);
    EXPECT_EQ(_line[5]->count(), 8u);
    EXPECT_EQ((*_line[5])[4]->type(), Token::T_OperatorMoreEqual);
    EXPECT_EQ((*_line[5])[3]->type(), Token::T_DelimiterPeriod);
}



/*
TEST_F(TstPythonToken, testPythonTokenSegfault) {
    fillAllLines();
    std::cout << "-----begin-----------------------------------------------------------" << std::endl;
    _line[0]->tokenScanInfo(true);
    _line[0]->tokenScanInfo()->setParseMessage(_line[0]->front(), "Test indentError", TokenScanInfo::IndentError);
    _line[0]->tokenScanInfo()->setParseMessage(_line[0]->front(), "Test Message", TokenScanInfo::Message);
    _line[0]->tokenScanInfo()->setParseMessage(_line[0]->back(), "Test lookupError", TokenScanInfo::LookupError);
    _line[0]->tokenScanInfo()->setParseMessage((*_line[0])[1], "Test Issue", TokenScanInfo::Issue);

    _line[0]->remove((*_line[0])[0]);
    std::cout << "-----end-----------------------------------------------------------" << std::endl;
}

TEST_F(TstPythonToken, testSeg) {
    fillAllLines();
}
*/

TEST_F(TstPythonToken, testPythonTokenScanInfo) {
    fillAllLines();
    _line[0]->tokenScanInfo(true)->setParseMessage(_line[0]->front(), "Test indentError", TokenScanInfo::IndentError);
    EXPECT_EQ(_line[0]->tokenScanInfo()->allMessages().size(), 1u);
    _line[0]->tokenScanInfo()->setParseMessage(_line[0]->front(), "Test Message", TokenScanInfo::Message);
    EXPECT_EQ(_line[0]->tokenScanInfo()->allMessages().size(), 2u);
    EXPECT_STREQ(_line[0]->tokenScanInfo()->allMessages().front()->message().c_str(), "Test indentError");
    EXPECT_EQ(_line[0]->tokenScanInfo()->allMessages().front()->type(), TokenScanInfo::IndentError);
    EXPECT_STREQ(_line[0]->tokenScanInfo()->allMessages().back()->message().c_str(), "Test Message");
    EXPECT_STREQ(_line[0]->tokenScanInfo()->allMessages().front()->msgTypeAsString().c_str(), "IndentError");

    _line[0]->tokenScanInfo()->setParseMessage(_line[0]->back(), "Test lookupError", TokenScanInfo::LookupError);
    EXPECT_EQ(_line[0]->tokenScanInfo()->allMessages().size(), 3u);
    EXPECT_STREQ(_line[0]->tokenScanInfo()->allMessages().back()->message().c_str(), "Test lookupError");

    EXPECT_EQ(_line[0]->tokenScanInfo()->parseMessages(_line[0]->front()).front()->type(),
                                                          TokenScanInfo::IndentError);
    EXPECT_EQ(_line[0]->tokenScanInfo()->parseMessages((*_line[0])[1]).empty(), true);
    EXPECT_EQ(_line[0]->tokenScanInfo()->parseMessages((*_line[0])[0], TokenScanInfo::Message).size(), 1u);

    _line[0]->tokenScanInfo()->setParseMessage((*_line[0])[1], "Test Issue", TokenScanInfo::Issue);
    EXPECT_EQ(_line[0]->tokenScanInfo()->parseMessages((*_line[0])[1]).front()->type(), TokenScanInfo::Issue);
    EXPECT_STREQ(_line[0]->tokenScanInfo()->parseMessages((*_line[0])[1]).front()->message().c_str(), "Test Issue");

    _line[0]->remove((*_line[0])[0]);
    EXPECT_EQ(_line[0]->tokenScanInfo()->allMessages().size(),2u);

    _line[0]->remove((*_line[0])[1]);
    EXPECT_EQ(_line[0]->tokenScanInfo()->parseMessages(_line[0]->back()).size(), 1u);

    _line[0]->remove(_line[0]->back());
    EXPECT_NE(_line[0]->tokenScanInfo(), nullptr);
}


TEST_F(TstPythonToken, testTokenWrapper) {
    fillAllLines();
    TokenLinkInherit link(_line[0]->front());
    ASSERT_EQ(link.isNotified(), false);

    Token *tok = _line[0]->front();
    _line[0]->remove(tok, false);
    ASSERT_EQ(link.isNotified(), false);
    delete tok;
    ASSERT_EQ(link.isNotified(), true);

    TokenLinkInherit link1(_line[0]->front()),
                     link2(_line[0]->front());
    ASSERT_EQ(link1.isNotified(), false);
    ASSERT_EQ(link2.isNotified(), false);
    _line[0]->remove(_line[0]->front());
    ASSERT_EQ(link1.isNotified(), true);
    ASSERT_EQ(link2.isNotified(), true);

}

TEST_F(TstPythonToken, testTokenWrapperTemplate) {
    fillAllLines();
    TokenLinkTemplate link(_line[0]->front());
    ASSERT_EQ(link.isNotified(), false);
    Token *tok = _line[0]->front();
    _line[0]->remove(tok, false);
    ASSERT_EQ(link.isNotified(), false);
    delete tok;
    ASSERT_EQ(link.isNotified(), true);

    TokenLinkTemplate link1(_line[0]->front()),
                      link2(_line[0]->front());
    ASSERT_EQ(link1.isNotified(), false);
    ASSERT_EQ(link2.isNotified(), false);
    _line[0]->remove(_line[0]->front());
    ASSERT_EQ(link1.isNotified(), true);
    ASSERT_EQ(link2.isNotified(), true);
}


