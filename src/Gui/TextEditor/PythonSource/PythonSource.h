#ifndef PYTHONSOURCE_H
#define PYTHONSOURCE_H

#include <string>


// convinience macros, sets a global variable TOKEN_TEXT and TOKEN_NAME
// usefull when debugging, you cant inspect in your variable window
#ifdef BUILD_PYTHON_DEBUGTOOLS
#ifndef DEBUG_TOKENS
#define DEBUG_TOKENS 1
#endif
#endif

#if DEBUG_TOKENS == 1
#include "PythonCodeDebugTools.h"
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
    strncpy(TOKEN_NAME_BUF, Syntax::tokenToCStr(TOKEN->type()), sizeof TOKEN_NAME_BUF); \
    snprintf(TOKEN_INFO_BUF, sizeof TOKEN_INFO_BUF, "line:%d,start:%d,end:%d", \
                    TOKEN->line(), TOKEN->startPos(), TOKEN->endPos()); \
    strncpy(TOKEN_SRC_LINE_BUF, TOKEN->ownerLine()->text().c_str(), sizeof TOKEN_SRC_LINE_BUF); \
    strncpy(TOKEN_TEXT_BUF, TOKEN->text().c_str(), sizeof TOKEN_TEXT_BUF); \
}
#define NEXT_TOKEN(TOKEN) {\
    if (TOKEN) TOKEN = TOKEN->next(); \
    DBG_TOKEN(TOKEN);}
#define PREV_TOKEN(TOKEN) {\
    if (TOKEN) TOKEN = TOKEN->previous(); \
    DBG_TOKEN(TOKEN);}

#else
// No debug, squelsh warnings
#define DBG_TOKEN_FILE ;
#define DEFINE_DBG_VARS ;
#define DBG_TOKEN(TOKEN) ;
#define NEXT_TOKEN(TOKEN) if (TOKEN) TOKEN = TOKEN->next();
#define PREV_TOKEN(TOKEN) if (TOKEN) TOKEN = TOKEN->previous();
#endif

namespace Gui {

namespace Python {

class FileInfo {
    std::string m_path;
public:
    explicit FileInfo(const std::string path);
    ~FileInfo();
    bool fileExists(const std::string &file);
    bool dirExists(const std::string &dir);
    std::string baseName() const;
    std::string dirName(int parentFolderCnt = 0) const;
    inline static char dirSeparator();

    std::string baseName(const std::string &filename) const;
};

} // namespace Python

} // namespace Gui

#endif // PYTHONSOURCE_H
