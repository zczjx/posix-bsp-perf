#ifndef __FUSION_GEO_CONVERTER_HPP__
#define __FUSION_GEO_CONVERTER_HPP__

#include <cmath>

namespace bsp_perf
{
namespace fusion
{

// 经纬度 -> 局部平面坐标 (小范围等距近似, 与 ekf_offline.py 一致)
//   x = (lon - lon0) * R * cos(lat0)
//   y = -(lat - lat0) * R   (CARLA 的 GNSS 纬度方向与 ROS +y 相反, 此处取反, 见 README)
class GeoConverter
{
public:
    static constexpr double kEarthRadius = 6378137.0;

    GeoConverter() = default;
    GeoConverter(double lat0_deg, double lon0_deg) { setOrigin(lat0_deg, lon0_deg); }

    void setOrigin(double lat0_deg, double lon0_deg)
    {
        lat0_ = lat0_deg;
        lon0_ = lon0_deg;
        cos_lat0_ = std::cos(deg2rad(lat0_deg));
    }

    void toLocal(double lat_deg, double lon_deg, double& x, double& y) const
    {
        x = deg2rad(lon_deg - lon0_) * kEarthRadius * cos_lat0_;
        y = -deg2rad(lat_deg - lat0_) * kEarthRadius;
    }

private:
    static double deg2rad(double d) { return d * 3.14159265358979323846 / 180.0; }
    double lat0_ = 0.0;
    double lon0_ = 0.0;
    double cos_lat0_ = 1.0;
};

} // namespace fusion
} // namespace bsp_perf

#endif // __FUSION_GEO_CONVERTER_HPP__
