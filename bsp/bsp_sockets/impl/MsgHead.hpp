#ifndef __MSG_HEAD_H__
#define __MSG_HEAD_H__

#include <any>
#include <functional>
#include <string>
#include <memory>

namespace bsp_sockets
{
class IEventLoop;
static constexpr int MSG_HEAD_LENGTH = sizeof(size_t) + sizeof(int);
static constexpr int MSG_LENGTH_LIMIT = 65536 - MSG_HEAD_LENGTH;

static inline size_t genCmdId(const std::string& cmd_name)
{
    return std::hash<std::string>{}(cmd_name);
}

using msgHead = struct msgHead
{
    size_t cmd_id;
    int length;
};

using msgTask = struct msgTask
{
    msgTask(std::function<void(std::shared_ptr<IEventLoop>, std::any)> task, std::any args):
        task(task), args(args) {}
    std::function<void(std::shared_ptr<IEventLoop>, std::any)> task;
    std::any args;
 };//for NEW_TASK, 向sub-thread下发待执行任务

//for accepter communicate with connections
//for give task to sub-thread
using queueMsg = struct queueMsg
{
    enum class MSG_TYPE
    {
        NEW_CONN,
        STOP_THD,
        NEW_TASK,
    } cmd_type;

    int connection_fd;//for NEW_CONN, 向sub-thread下发新连接
    std::shared_ptr<msgTask> task_handle{nullptr};
};

}


#endif
