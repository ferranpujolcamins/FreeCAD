#include "PythonSource.h"
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>

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
    return FileInfo::dirExists(dirName());
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

std::string Python::FileInfo::baseName(const std::string &filename) const
{
    std::string dirSep = {dirSeparator(), 0};
    auto pos = std::find_end(filename.begin(), filename.end(), dirSep.begin(), dirSep.end());
    if (pos == filename.end())
        return filename;  // no separator char here

    std::string ret;
    std::copy(pos, filename.end(), ret.begin());
    return ret;
}

std::string Python::FileInfo::dirName(int parentFolderCnt) const
{
    std::string dirSep = {dirSeparator(), 0};
    auto end = m_path.end();
    auto posRight = end;
    auto posLeft = end;
    int iterations = -1;
    do {
        posLeft = std::find_end(m_path.begin(), posRight, dirSep.begin(), dirSep.end());
        if (iterations == parentFolderCnt)
            break;
        if (posLeft != end) {
            ++iterations;
            posRight = posLeft;

        }
    } while (posRight != m_path.begin());

    if (posRight == m_path.end())
        return "";  // no separator char here

    std::string ret;
    std::copy(posLeft, posRight, ret.begin());
    return ret;
}

// static
inline char Python::FileInfo::dirSeparator()
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}
