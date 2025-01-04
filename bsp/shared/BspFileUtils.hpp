#ifndef __BSP_FILE_UTILS_HPP__
#define __BSP_FILE_UTILS_HPP__

#include <string>
#include <iostream>
#include <memory>

namespace bsp_perf {
namespace shared {

class BspFileUtils
{
public:

    struct FileContext
    {
        std::shared_ptr<unsigned char> data;
        size_t size;
        int fd;
    };

    static std::string getFileName(const std::string& path)
    {
        // Get the file name from the path
        return path.substr(path.find_last_of("/\\") + 1);
    }

    static std::string getFileExtension(const std::string& path)
    {
        // Get the file extension from the path
        return path.substr(path.find_last_of(".") + 1);
    }

    static std::string getFilePath(const std::string& path)
    {
        // Get the file path from the path
        return path.substr(0, path.find_last_of("/\\"));
    }

    static std::shared_ptr<FileContext> LoadFileMmap(const std::string& path);

    static int ReleaseFileMmap(std::shared_ptr<FileContext> file);

    static std::shared_ptr<FileContext> LoadFileMalloc(const std::string& path);

    static int ReleaseFileMalloc(std::shared_ptr<FileContext> file);
};
} // namespace shared
} // namespace bsp_perf

#endif // __BSP_FILE_UTILS_HPP__