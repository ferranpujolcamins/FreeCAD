#ifndef TEXT_EDITOR_PRECOMPILED_H
#define TEXT_EDITOR_PRECOMPILED_H

#include <FCConfig.h>

#ifdef FC_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif

// here get the warnings of too long specifiers disabled (needed for VC6)
#ifdef _MSC_VER
#pragma warning( disable : 4251 )
#pragma warning( disable : 4273 )
#pragma warning( disable : 4275 )
#pragma warning( disable : 4503 )
#pragma warning( disable : 4786 )  // specifier longer then 255 chars
#endif

#ifdef _PreComp_

// standard
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <typeinfo>
#include <float.h>
#include <limits.h>

#ifdef FC_OS_WIN32
#include <Windows.h>
#include <io.h>
#include <shellapi.h>
#endif

// streams
#include <iostream>
#include <iomanip>


// STL
#include <vector>
#include <map>
#include <string>
#include <list>
#include <set>
#include <algorithm>
#include <stack>
#include <queue>
#include <sstream>
#include <bitset>

// Python
#include <Python.h>

// Qt
#include <qglobal.h>
#include <qregexp.h>
#include <QList>
#inculde <QHash>
#include <QMap>
#include <QListWidget>
#include <QPlainTextEdit>


#elif defined(FC_OS_WIN32)
#include <windows.h>
#endif  //_PreComp_
#endif // TEXT_EDITOR_PRECOMPILED_H
