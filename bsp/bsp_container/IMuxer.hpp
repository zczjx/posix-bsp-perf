#ifndef __IMUXER_HPP__
#define __IMUXER_HPP__

#include <memory>
#include <string>
#include <any>
#include <functional>

namespace bsp_container
{
class IMuxer
{
public:

    static std::unique_ptr<IMuxer> create(const std::string& codecPlatform);
    virtual ~IMuxer() = default;

protected:
    IMuxer() = default;
    IMuxer(const IMuxer&) = delete;
    IMuxer& operator=(const IMuxer&) = delete;
};

} // namespace bsp_container

#endif // __IMUXER_HPP__