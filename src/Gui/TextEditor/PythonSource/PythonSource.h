#ifndef PYTHONSOURCE_H
#define PYTHONSOURCE_H


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
    strncpy(TOKEN_NAME_BUF, Syntax::tokenToCStr(TOKEN->token), sizeof TOKEN_NAME_BUF); \
    strncpy(TOKEN_INFO_BUF, (QString::fromLatin1("line:%1,start:%2,end:%3") \
                    .arg(TOKEN->txtBlock()->block().blockNumber()) \
                    .arg(TOKEN->startPos).arg(TOKEN->endPos)).toLatin1(), sizeof TOKEN_INFO_BUF); \
    strncpy(TOKEN_SRC_LINE_BUF, TOKEN->txtBlock()->block().text().toLatin1(), sizeof TOKEN_SRC_LINE_BUF); \
    strncpy(TOKEN_TEXT_BUF, TOKEN->text().toLatin1(), sizeof TOKEN_TEXT_BUF); \
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


#include <QString>
#include <QStringList>


#endif // PYTHONSOURCE_H
