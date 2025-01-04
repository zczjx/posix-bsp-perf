#include "BspFileUtils.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

namespace bsp_perf {
namespace shared {

std::shared_ptr<BspFileUtils::FileContext> BspFileUtils::LoadFileMmap(const std::string& path)
{
    int fd = open(path.c_str(), O_RDONLY);

    if (fd == -1)
    {
        return nullptr;
    }

    struct stat sb;

    if (fstat(fd, &sb) == -1)
    {
        close(fd);
        return nullptr;
    }

    void* mapped = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (mapped == MAP_FAILED)
    {
        return nullptr;
    }

    auto context = std::make_shared<FileContext>();
    context->data = std::shared_ptr<unsigned char>(static_cast<unsigned char*>(mapped), [sb](unsigned char* p) { munmap(p, sb.st_size); });
    context->size = sb.st_size;
    context->fd = fd;

    return context;
}

int BspFileUtils::ReleaseFileMmap(std::shared_ptr<BspFileUtils::FileContext> file)
{
    if (file == nullptr)
    {
        return 0;
    }
    if (munmap(file->data.get(), file->size) == -1)
    {
        std::cerr << "munmap failed" << std::endl;
        close(file->fd);
        return -1;
    }
    close(file->fd);
    return 0;
}

std::shared_ptr<BspFileUtils::FileContext> BspFileUtils::LoadFileMalloc(const std::string& path)
{
    int fd = open(path.c_str(), O_RDONLY);

    if (fd == -1)
    {
        return nullptr;
    }

    struct stat sb;

    if (fstat(fd, &sb) == -1)
    {
        close(fd);
        return nullptr;
    }

    auto context = std::make_shared<FileContext>();
    context->data = std::shared_ptr<unsigned char>(new unsigned char[sb.st_size], std::default_delete<unsigned char[]>());
    context->size = sb.st_size;
    context->fd = fd;

    if (read(fd, context->data.get(), sb.st_size) == -1)
    {
        close(fd);
        return nullptr;
    }

    return context;
}

int BspFileUtils::ReleaseFileMalloc(std::shared_ptr<BspFileUtils::FileContext> file)
{
    if (file == nullptr)
    {
        return 0;
    }
    close(file->fd);
    file->data.reset();
    return 0;
}

} // namespace shared
} // namespace bsp_perf