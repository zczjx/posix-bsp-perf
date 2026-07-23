#include "ISurroundView.hpp"
#include "GeometryBevSurroundView.hpp"

#include <stdexcept>

namespace bsp_perf
{
namespace svs
{

std::unique_ptr<ISurroundView> ISurroundView::create(const std::string& backend)
{
    if (backend == "geometry_bev" || backend == "opencv") {
        return std::make_unique<GeometryBevSurroundView>();
    }

    throw std::invalid_argument("unsupported surround view backend: " + backend);
}

} // namespace svs
} // namespace bsp_perf
