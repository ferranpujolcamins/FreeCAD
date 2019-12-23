#include "PythonSource.h"
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <limits.h>
#include <cstring>
#include <iostream>

#if defined(_WIN32)
# include <windows.h>
#elif defined(__linux__)
# include <linux/limits.h>
# include <unistd.h>
#elif defined(FreeBSD)
# include <unistd.h>
#elif defined(__APPLE__)
# include <mach-o/dyld.h>
#endif


#if !defined(PATH_MAX)
# if defined(MAX_PATH)
#  define PATH_MAX MAX_PATH
# else
#  define PATH_MAX 1024
# endif
#endif


DBG_TOKEN_FILE

std::size_t Python::Hash::operator()(const char *cstr) const noexcept
{
    return cstrToHash(cstr, strlen(cstr));
}

std::size_t Python::Hash::operator()(const std::string &str) const noexcept
{
    return cstrToHash(str.c_str(), str.length());
}

// ----------------------------------------------------------------------------

size_t Python::cstrToHash(const char *str, size_t len)
{
    // based on MaPrime2c
    static const unsigned char sTable[256] =
    {
        0xa3,0xd7,0x09,0x83,0xf8,0x48,0xf6,0xf4,0xb3,0x21,0x15,0x78,0x99,0xb1,0xaf,0xf9,
        0xe7,0x2d,0x4d,0x8a,0xce,0x4c,0xca,0x2e,0x52,0x95,0xd9,0x1e,0x4e,0x38,0x44,0x28,
        0x0a,0xdf,0x02,0xa0,0x17,0xf1,0x60,0x68,0x12,0xb7,0x7a,0xc3,0xe9,0xfa,0x3d,0x53,
        0x96,0x84,0x6b,0xba,0xf2,0x63,0x9a,0x19,0x7c,0xae,0xe5,0xf5,0xf7,0x16,0x6a,0xa2,
        0x39,0xb6,0x7b,0x0f,0xc1,0x93,0x81,0x1b,0xee,0xb4,0x1a,0xea,0xd0,0x91,0x2f,0xb8,
        0x55,0xb9,0xda,0x85,0x3f,0x41,0xbf,0xe0,0x5a,0x58,0x80,0x5f,0x66,0x0b,0xd8,0x90,
        0x35,0xd5,0xc0,0xa7,0x33,0x06,0x65,0x69,0x45,0x00,0x94,0x56,0x6d,0x98,0x9b,0x76,
        0x97,0xfc,0xb2,0xc2,0xb0,0xfe,0xdb,0x20,0xe1,0xeb,0xd6,0xe4,0xdd,0x47,0x4a,0x1d,
        0x42,0xed,0x9e,0x6e,0x49,0x3c,0xcd,0x43,0x27,0xd2,0x07,0xd4,0xde,0xc7,0x67,0x18,
        0x89,0xcb,0x30,0x1f,0x8d,0xc6,0x8f,0xaa,0xc8,0x74,0xdc,0xc9,0x5d,0x5c,0x31,0xa4,
        0x70,0x88,0x61,0x2c,0x9f,0x0d,0x2b,0x87,0x50,0x82,0x54,0x64,0x26,0x7d,0x03,0x40,
        0x34,0x4b,0x1c,0x73,0xd1,0xc4,0xfd,0x3b,0xcc,0xfb,0x7f,0xab,0xe6,0x3e,0x5b,0xa5,
        0xad,0x04,0x23,0x9c,0x14,0x51,0x22,0xf0,0x29,0x79,0x71,0x7e,0xff,0x8c,0x0e,0xe2,
        0x0c,0xef,0xbc,0x72,0x75,0x6f,0x37,0xa1,0xec,0xd3,0x8e,0x62,0x8b,0x86,0x10,0xe8,
        0x08,0x77,0x11,0xbe,0x92,0x4f,0x24,0xc5,0x32,0x36,0x9d,0xcf,0xf3,0xa6,0xbb,0xac,
        0x5e,0x6c,0xa9,0x13,0x57,0x25,0xb5,0xe3,0xbd,0xa8,0x3a,0x01,0x05,0x59,0x2a,0x46
    };

    static const int primeMultiplier = 1717;

    std::size_t hash = len, i;

    for (i = 0; i != len; i++, str++) {
        hash ^= sTable[( static_cast<unsigned char>(*str) + i) & 255];
        hash = hash * primeMultiplier;
    }

    return hash;
}

// ------------------------------------------------------------------------
std::list<std::string> Python::split(std::string strToSplit, char delim)
{
    std::list<std::string> list;
    std::size_t current, previous = 0;
    current = strToSplit.find(delim);
    while (current != std::string::npos) {
        list.push_back(strToSplit.substr(previous, current - previous));
        previous = current + 1;
        current = strToSplit.find(delim, previous);
    }
    list.push_back(strToSplit.substr(previous, current - previous));
    return list;
}

// -------------------------------------------------------------------------


Python::FileInfo::FileInfo(const std::string path) :
    m_path(path)
{
}

Python::FileInfo::~FileInfo()
{
}

bool Python::FileInfo::fileExists() const
{
    return FileInfo::fileExists(m_path);
}

bool Python::FileInfo::dirExists() const
{
    return FileInfo::dirExists(dirPath());
}

// static
bool Python::FileInfo::fileExists(const std::string &file)
{
#ifdef _WIN32
    struct _stat info;
    if(_stat(file.c_str(), &info ) == 0 &&
       info.st_mode & _S_IFREG)
#else
    struct stat info;
    if(stat(file.c_str(), &info ) == 0 &&
       info.st_mode & S_IFREG)
#endif
    {
        return true;
    }
    return false;
}

// static
bool Python::FileInfo::dirExists(const std::string &dir)
{
    std::string path = dirPath(dir);

#ifdef _WIN32
    struct _stat info;
    if(_stat(path.c_str(), &info ) == 0 &&
       info.st_mode & _S_IFDIR)
#else
    struct stat info;
    if(stat(path.c_str(), &info ) == 0 &&
       info.st_mode & S_IFDIR)
#endif

    {
        return true;
    }
    return false;
}

std::string Python::FileInfo::baseName() const
{
    return baseName(m_path);
}

// static
std::string Python::FileInfo::baseName(const std::string &filePath)
{
    std::string dirSeparator = { dirSep, 0};
    auto pos = std::find_end(filePath.begin(), filePath.end(),
                             dirSeparator.begin(), dirSeparator.end());
    if (pos == filePath.end())
        return filePath;  // no separator char here

    std::string ret;
    std::copy(pos, filePath.end(), ret.begin());
    return ret;
}

// static
std::string Python::FileInfo::ext(const std::string &filename)
{
    static const std::string dotStr(".");
    auto dotPos = std::find_end(filename.begin(), filename.end(),
                                dotStr.begin(), dotStr.end());
    if (dotPos != filename.end())
        return std::string(dotPos+1, filename.end());
    return std::string();
}

// static
std::string &Python::FileInfo::applicationPath()
{
    static std::string appPath;
    if (!appPath.empty())
        return appPath; // we have it cached!

    char buf[PATH_MAX + 1];

    // lookup from system
#if defined(_WIN32)
    appPath = std::string(buf, GetModuleFileNameA(nullptr, buf, MAX_PATH));
#elif defined(__linux__)
  ssize_t count = readlink("/proc/self/exe", buf, PATH_MAX);
  appPath = std::string(buf, (count > 0) ? static_cast<size_t>(count) : 0);
#elif defined(__APPLE__)
    uint32_t bufsize = PATH_MAX;
    if(!_NSGetExecutablePath(buf, &bufsize)) {
//        // resolve symlinks, ., .. if possible
//        char *realPath = realpath(buf, NULL);
//        if (realPath != nullptr) {
//            strncpy(buf, realPath, sizeof(buf));
//            free(realPath);
//        }
        appPath = buf;
    }
#elif defined(__FreeBSD__)
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    sysctl(mib, 4, buf, sizeof(buffer), NULL, 0);
    path = buffer;
#endif
    return appPath;
}


std::string Python::FileInfo::dirPath(int parentFolderCnt) const
{
    return dirPath(m_path, parentFolderCnt);
}

// static
std::string Python::FileInfo::dirPath(const std::string &path, int parentFolderCnt)
{
    auto list = split(path, dirSep);
    // trim file from foo/bar/baz.txt
    if (path.back() != dirSep && list.size() > 1)
        list.pop_back();
    std::string ret;
    // insert first '/'
    if (path.front() == dirSep)
        ret += dirSep;

    for(auto &itm : list) {
        if (!itm.empty())
            ret += itm + dirSep;
    }

    return ret;

    /*std::string dirSeparator = {dirSep};
    auto end = path.end();
    auto posRight = end;
    auto posLeft = end;
    int iterations = -1;
    do {
        posLeft = std::find_end(path.begin(), posRight,
                                dirSeparator.begin(), dirSeparator.end());
        if (posLeft != end) {
            ++iterations;
            posRight = posLeft;
        }
        if (iterations == parentFolderCnt)
            break;
    } while (posRight != path.begin());

    if (posRight == end)
        return "";  // no separator char here

    return std::string(posLeft, posRight);
    */
}

std::string Python::FileInfo::cdUp(uint numberOfDirs) const
{
    return cdUp(m_path, numberOfDirs);
}

std::string Python::FileInfo::cdUp(const std::string &dirPath, uint numberOfDirs)
{
    size_t pos = dirPath.length();
    while (dirPath.back() == dirSep)
        --pos; // trim trailing '/'

    if (dirPath.empty())
        return "";

    std::string dirSeparator = {dirSep};
    for (uint i = 0; i < numberOfDirs && pos != std::string::npos; ++i) {
        pos = dirPath.rfind(dirSeparator, pos -1);
    }

    return dirPath.substr(0, pos);
}

std::string Python::FileInfo::ext() const
{
    return ext(m_path);
}

// static
std::string Python::FileInfo::absolutePath(const std::string &relativePath)
{
    std::string cdUpStr = {'.', '.', dirSep};
    uint cdUpCnt = 0;
    size_t pos = 0;
    while((pos = relativePath.find(cdUpStr, pos)) != std::string::npos) {
        ++cdUpCnt;
        pos += 2;
    }

    if (relativePath.back() != dirSep)
        ++cdUpCnt;

    std::string path = cdUp(applicationPath(), cdUpCnt) + dirSep;
    path.append(relativePath.substr(pos!=std::string::npos ? pos : 0));
    return path;
}

std::string Python::FileInfo::absolutePath() const
{
    return absolutePath(m_path);
}

// static
#ifdef _WIN32
const char Python::FileInfo::dirSep = '\\';
#else
const char Python::FileInfo::dirSep = '/';
#endif


