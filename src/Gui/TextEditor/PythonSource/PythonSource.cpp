#include "PythonSource.h"
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <limits.h>

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

// -------------------------------------------------------------------------

using namespace Gui;

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
    struct stat info;

    if(stat(file.c_str(), &info ) == 0 &&
       info.st_mode & S_IFREG)
    {
        return true;
    }
    return false;
}

// static
bool Python::FileInfo::dirExists(const std::string &dir)
{
    struct stat info;

    if(stat(dir.c_str(), &info ) == 0 &&
       info.st_mode & S_IFDIR)
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
    std::string dirSeparator = {dirSep, 0};
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
}

std::string Python::FileInfo::cdUp(uint numberOfDirs) const
{
    return cdUp(m_path, numberOfDirs);
}

std::string Python::FileInfo::cdUp(const std::string &dirPath, uint numberOfDirs)
{
    auto end = dirPath.end();
    while (dirPath.back() == dirSep)
        --end; // trim trailing '/'

    std::string dirSeparator = {dirSep, 0};
    auto pos = end;
    for (uint i = 0; i < numberOfDirs; ++i) {
        pos = std::find_end(dirPath.begin(), pos,
                            dirSeparator.begin(), dirSeparator.end());
    }

    return std::string(dirPath.begin(), pos);
}

std::string Python::FileInfo::ext() const
{
    return ext(m_path);
}

// static
std::string Python::FileInfo::absolutePath(const std::string &relativePath)
{
    std::string cdUpStr = {'.', '.', dirSep, 0};
    uint cdUpCnt = 0;
    auto end = relativePath.end();
    auto pos = relativePath.begin();
    while ((pos = std::find_first_of(pos, end, cdUpStr.begin(), cdUpStr.end())) != end)
        ++cdUpCnt;

    std::string path = cdUp(applicationPath(), cdUpCnt) + dirSep;
    path.append(pos, end);
    return path;
}

std::string Python::FileInfo::absolutePath() const
{
    return absolutePath(m_path);
}

// static
#ifdef _WIN32
const char Python::FileInfo::dirSep = '/';
#else
const char Python::FileInfo::dirSep = '\\';
#endif
