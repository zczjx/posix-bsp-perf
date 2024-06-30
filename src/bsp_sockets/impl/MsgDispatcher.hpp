#ifndef __MSG_DISPATCHER_H__
#define __MSG_DISPATCHER_H__

#include <unordered_map>
#include "ISocketHelper.hpp"
#include "BspSocketException.hpp"

#include <functional>
#include <any>
#include <memory>
#include <vector>
#include <iostream>

namespace bsp_sockets
{

using msgCallback = std::function<void(size_t cmd_id, std::vector<uint8_t>& data, std::shared_ptr<ISocketHelper> socket_helper, std::any usr_data)>;

class MsgDispatcher
{
public:
    MsgDispatcher() {}

    int addMsgCallback(std::string& cmd_name, msgCallback msg_cb, std::any usr_data)
    {
        auto cmd_id = genCmdId(cmd_name);
        if (exist(cmd_id))
        {
            return -1;
        }

        m_dispatcher[cmd_id] = msg_cb;
        m_args[cmd_id] = usr_data;
        return 0;
    }

    bool exist(std::string& cmd_name) const
    {
        auto cmd_id = genCmdId(cmd_name);
        return m_dispatcher.find(cmd_id) != m_dispatcher.end();
    }

    bool exist(size_t cmd_id) const
    {
        return m_dispatcher.find(cmd_id) != m_dispatcher.end();
    }

    void callbackFunc(std::string& cmd_name, std::vector<uint8_t>& data, std::shared_ptr<ISocketHelper> socket_helper)
    {
        auto cmd_id = genCmdId(cmd_name);

        callbackFunc(cmd_id, data, socket_helper);
    }

    void callbackFunc(size_t cmd_id, std::vector<uint8_t>& data, std::shared_ptr<ISocketHelper> socket_helper)
    {
        if(!exist(cmd_id))
        {
            throw BspSocketException("cmd_id not exist");
        }

        auto func = m_dispatcher[cmd_id];
        auto usr_data = m_args[cmd_id];
        func(cmd_id, data, socket_helper, usr_data);
    }


private:
    std::unordered_map<size_t, msgCallback> m_dispatcher{};
    std::unordered_map<size_t, std::any> m_args{};
};

}


#endif
