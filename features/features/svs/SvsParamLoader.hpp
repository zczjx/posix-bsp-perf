#ifndef __SVS_PARAM_LOADER_HPP__
#define __SVS_PARAM_LOADER_HPP__

#include "SvsTypes.hpp"
#include <string>

namespace bsp_perf
{
namespace svs
{

class SvsParamLoader
{
public:
    bool loadCameraParameters(const std::string& path, CameraParameters& params) const;
    bool saveProjectMatrix(const std::string& path, const CameraParameters& params) const;
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_PARAM_LOADER_HPP__
