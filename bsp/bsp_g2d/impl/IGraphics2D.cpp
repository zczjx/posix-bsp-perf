#include <bsp_g2d/IGraphics2D.hpp>

#ifdef BUILD_PLATFORM_RK35XX
#include "rk_rga/rkrga.hpp"
#endif

#ifdef BUILD_PLATFORM_JETSON
#include "nv_vic/NvVicGraphics2D.hpp"
#endif

#include <stdexcept>
#include <memory>
namespace bsp_g2d
{
std::unique_ptr<IGraphics2D> IGraphics2D::create(const std::string& g2dPlatform)
{
#ifdef BUILD_PLATFORM_RK35XX
    if (g2dPlatform.compare("rkrga") == 0)
    {
        return std::make_unique<rkrga>();
    }
#endif
#ifdef BUILD_PLATFORM_JETSON
    if (g2dPlatform.compare("nvvic") == 0)
    {
        return std::make_unique<NvVicGraphics2D>();
    }
#endif
    else
    {
        throw std::invalid_argument("Invalid G2D platform specified: " + g2dPlatform);
    }
}

}