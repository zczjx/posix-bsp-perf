#ifndef __MSG_DISPATCHER_H__
#define __MSG_DISPATCHER_H__

#include <unordered_map>
#include "ISocketHelper.hpp"
#include "BspSocketException.hpp"

#include <functional>
#include <any>
#include <memory>
#include <vector>

namespace bsp_sockets
{

using msgCallback = std::function<void(std::vector<uint8_t>& data, int cmd_id, std::shared_ptr<ISocketHelper> socket_helper, std::any usr_data)>;

class MsgDispatcher
{
public:
    MsgDispatcher() {}

    int addMsgCallback(int cmd_id, msgCallback msg_cb, std::any usr_data)
    {
        if (m_dispatcher.find(cmd_id) != m_dispatcher.end())
        {
            return -1;
        }

        m_dispatcher[cmd_id] = msg_cb;
        m_args[cmd_id] = usr_data;
        return 0;
    }

    bool exist(int cmd_id) const
    {
        return m_dispatcher.find(cmd_id) != m_dispatcher.end();
    }

    void callbackFunc(std::vector<uint8_t>& data, int cmd_id, std::shared_ptr<ISocketHelper> socket_helper)
    {
        if(!exist(cmd_id))
        {
            throw BspSocketException("cmd_id not exist");
        }

        auto func = m_dispatcher[cmd_id];
        auto usr_data = m_args[cmd_id];
        func(data, cmd_id, socket_helper, usr_data);
    }

private:
    std::unordered_map<int, msgCallback> m_dispatcher;
    std::unordered_map<int, std::any> m_args;
};

}


#endif
