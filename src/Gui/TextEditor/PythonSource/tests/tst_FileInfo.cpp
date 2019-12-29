#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <PythonSource.h>

using namespace testing;
using namespace Python;

TEST(TstFileInfo, testFileInfoPath){
    FileInfo fi(__FILE__);
    EXPECT_STREQ(fi.path().c_str(), __FILE__);

    EXPECT_STREQ(fi.baseName().c_str(), "tst_FileInfo.cpp");
    EXPECT_STREQ(fi.ext().c_str(), "cpp");
    EXPECT_STREQ(fi.stem().c_str(), "tst_FileInfo");
    EXPECT_EQ(fi.fileExists(), true);
    EXPECT_EQ(fi.dirExists(), true);

    FileInfo fi2("tst_FileInfo.cpp");
    EXPECT_GT(fi.path().length(), fi2.path().length());
    FileInfo fi3(fi2.absolutePath());
    EXPECT_GT(fi3.path().length(), 20u);
    fi2.cdUp();
    EXPECT_EQ(fi2.path().length(), 0u);

    FileInfo fi4(__FILE__);
    fi4.cdUp(2);
    fi.cdUp(); fi.cdUp();
    EXPECT_STREQ(fi.path().c_str(), fi4.path().c_str());
}

TEST(TstFileInfo, testFileInfoDir) {
    FileInfo fi(__FILE__);
    EXPECT_GT(fi.dirPath().length(), 0u);
    EXPECT_EQ(fi.dirContains("tst_FileInfo.cpp"), true);
    auto appPath = FileInfo::applicationPath();
    EXPECT_STRNE(fi.dirPath().c_str(), FileInfo::dirPath(appPath).c_str());

}

TEST(TstFileInfo, testFilesInDir) {
    FileInfo fi(__FILE__);
    auto files = fi.filesInDir();
    EXPECT_GT(files.size(), 0u);
    // check so we don't list dirs
    EXPECT_EQ(std::find(files.begin(), files.end(), "testscripts"), files.end());
    // check so we do list files
    EXPECT_NE(std::find(files.begin(), files.end(), fi.baseName()), files.end());

}
