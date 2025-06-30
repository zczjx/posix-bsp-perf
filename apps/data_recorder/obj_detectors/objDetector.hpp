#ifndef __OBJ_DETECTOR_HPP__
#define __OBJ_DETECTOR_HPP__

#include <string>
#include <nlohmann/json.hpp>
#include <zeromq_ipc/sharedMemSubscriber.hpp>
#include <zeromq_ipc/sharedMemPublisher.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_dnn/dnnObjDetector.hpp>

using json = nlohmann::json;
using namespace midware::zeromq_ipc;
using namespace bsp_perf::shared;

namespace apps
{
namespace data_recorder
{

class ObjDetector
{
public:
    ObjDetector(ArgParser&& args, const json& nodes_ipc);

    void runLoop();

private:
    std::shared_ptr<SharedMemSubscriber> m_input_shmem_port;
    std::shared_ptr<SharedMemPublisher> m_output_shmem_port;
    std::unique_ptr<bsp_dnn::dnnObjDetector> m_dnnObjDetector;
};

} // namespace data_recorder
} // namespace apps

#endif