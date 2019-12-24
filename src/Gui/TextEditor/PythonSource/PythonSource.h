#ifndef PYTHONSOURCE_H
#define PYTHONSOURCE_H

#include <string>

#if __cplusplus >= 201703L && __has_cpp_attribute(fallthrough)
# define FALLTHROUGH [[fallthrough]];
#elif defined(__clang__) && __cplusplus >= 201103L
# define FALLTHROUGH [[clang::fallthrough]];
#elif defined(__GNUC__) && __cplusplus >= 201103L
# define FALLTHROUGH [[gnu::fallthrough]];
#else
# define FALLTHROUGH /* intended fallthrough */
#endif


// convinience macros, sets a global variable TOKEN_TEXT and TOKEN_NAME
// usefull when debugging, you cant inspect in your variable window
#ifdef BUILD_PYTHON_DEBUGTOOLS
#ifndef DEBUG_TOKENS
#define DEBUG_TOKENS 1
#endif
#endif

#if DEBUG_TOKENS == 1
#include "PythonSourceDebugTools.h"
// include at top of cpp file
#define DBG_TOKEN_FILE \
extern char TOKEN_TEXT_BUF[350]; \
extern char TOKEN_NAME_BUF[50]; \
extern char TOKEN_INFO_BUF[40];  \
extern char TOKEN_SRC_LINE_BUF[350];

// insert in start of each method that you want to dbg in
#define DEFINE_DBG_VARS \
/* we use c str here as it doesnt require dustructor calls*/ \
    /* squelsh compiler warnings*/ \
const char *TOKEN_TEXT = TOKEN_TEXT_BUF, \
           *TOKEN_NAME = TOKEN_NAME_BUF, \
           *TOKEN_INFO = TOKEN_INFO_BUF, \
           *TOKEN_SRC_LINE = TOKEN_SRC_LINE_BUF; \
    (void)TOKEN_TEXT; \
    (void)TOKEN_NAME; \
    (void)TOKEN_INFO; \
    (void)TOKEN_SRC_LINE;
#define DBG_TOKEN(TOKEN) if (TOKEN){\
    strncpy(TOKEN_NAME_BUF, Python::Token::tokenToCStr(TOKEN->type()), sizeof TOKEN_NAME_BUF); \
    snprintf(TOKEN_INFO_BUF, sizeof TOKEN_INFO_BUF, "line:%d,start:%d,end:%d", \
                    TOKEN->line(), TOKEN->startPos(), TOKEN->endPos()); \
    strncpy(TOKEN_SRC_LINE_BUF, TOKEN->ownerLine()->text().c_str(), sizeof TOKEN_SRC_LINE_BUF); \
    strncpy(TOKEN_TEXT_BUF, TOKEN->text().c_str(), sizeof TOKEN_TEXT_BUF); \
    } else { \
    strncpy(TOKEN_NAME_BUF, "Invalid", sizeof TOKEN_NAME_BUF); \
    snprintf(TOKEN_INFO_BUF, sizeof TOKEN_INFO_BUF, "line:-1,start:-1,end:-1"); \
    strncpy(TOKEN_SRC_LINE_BUF, "nullptr", sizeof TOKEN_SRC_LINE_BUF); \
    strncpy(TOKEN_TEXT_BUF, "nullptr", sizeof TOKEN_TEXT_BUF); \
}
#define NEXT_TOKEN(TOKEN) {\
    if (TOKEN) TOKEN = TOKEN->next(); \
    DBG_TOKEN(TOKEN);}
#define PREV_TOKEN(TOKEN) {\
    if (TOKEN) TOKEN = TOKEN->previous(); \
    DBG_TOKEN(TOKEN);}
#define NEXT_TOKEN_IF(TOKEN) { \
    if (TOKEN->next()) TOKEN = TOKEN->next();\
    else return TOKEN; \
}

#else
// No debug, squelsh warnings
#define DBG_TOKEN_FILE ;
#define DEFINE_DBG_VARS ;
#define DBG_TOKEN(TOKEN) ;
#define NEXT_TOKEN(TOKEN) if (TOKEN) TOKEN = TOKEN->next();
#define PREV_TOKEN(TOKEN) if (TOKEN) TOKEN = TOKEN->previous();
#endif

namespace Python {

struct Hash
{
    std::size_t operator()(const char* cstr) const noexcept;
    std::size_t operator()(const std::string &str) const noexcept;
};

size_t cstrToHash(const char *str, size_t len);

std::list<std::string> split(const std::string &strToSplit,
                             const std::string &delim = std::string());
std::string join(std::list<std::string> strsToJoin,
                 const std::string &delim = std::string());

// ----------------------------------------------------------------------------------------

class FileInfo {
    std::string m_path;
public:
    explicit FileInfo(const std::string path);
    ~FileInfo();

    /// true is file exists
    bool fileExists() const;
    static bool fileExists(const std::string &file);

    /// true if dir exists
    bool dirExists() const;
    static bool dirExists(const std::string &dir);

    /// gets the basename (filename including extension)
    static std::string baseName(const std::string &filePath);
    std::string baseName() const;

    /// get the directory path that contains this file/dir
    /// parentFolderCnt limit to this number of folders
    ///  ie it  cdUp count
    ///   default 0 == get complete path
    std::string dirPath(uint parentFolderCnt = 0) const;
    static std::string dirPath(const std::string &path, uint parentFolderCnt = 0);

    /// returns a path which has moved up numberOfDirs
    std::string cdUp(uint numberOfDirs = 1) const;
    static std::string cdUp(const std::string &dirPath, uint numberOfDirs = 1);

    /// gets the file extension
    static std::string ext(const std::string &filename);
    std::string ext() const;

    /// get the absolute path for filePath
    /// relativePath is relative to applicationPath
    static std::string absolutePath(const std::string &relativePath);
    std::string absolutePath() const;

    /// get a vector with all files in folder
    /// is current path is a file it lists all it sibbling files incluing itself
    std::vector<std::string> filesInDir();
    static std::vector<std::string> filesInDir(const std::string &path);

    /// return true if file is contained in dir
    bool dirContains(const std::string &searchName);
    static bool dirContains(const std::string &dir, const std::string &searchName);

    /// the system separator char in paths '\' or '/'
    static const char dirSep;

    /// gets the pathname to this application
    static std::string &applicationPath();
};

} // namespace Python

#endif // PYTHONSOURCE_H
