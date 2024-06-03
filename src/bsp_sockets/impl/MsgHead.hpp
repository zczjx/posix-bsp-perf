#ifndef __MSG_HEAD_H__
#define __MSG_HEAD_H__

#include <any>
#include <functional>
#include <memory>

namespace bsp_sockets
{
class EventLoop;
constexpr int msg_head_length = 8;
constexpr int msg_length_limit = 65536 - msg_head_length;

using msgHead = struct msgHead
{
    int cmd_id;
    int length;
};

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
    union
    {
        int connection_fd;//for NEW_CONN, 向sub-thread下发新连接
        struct
        {
            std::function<void(std::shared_ptr<EventLoop>, std::any)> task;
            std::any args;
        };//for NEW_TASK, 向sub-thread下发待执行任务
    };
};
}


#endif
