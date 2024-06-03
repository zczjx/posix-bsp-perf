#ifndef __MSG_DISPATCHER_H__
#define __MSG_DISPATCHER_H__

#include <assert.h>
#include <pthread.h>
#include <unordered_map>
#include <bsp_sockets/ISocketConnection.hpp>
#include <functional>
#include <any>
#include <span>
#include <memory>

using msgCallback = std::function<void(std::span<const uint8_t> data, uint32_t len, int cmd_id, std::shared_ptr<ISocketConnection> connection, std::any usr_data)>;

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

        m_dispatcher[msg_cb] = msg_cb;
        m_args[msg_cb] = usr_data;
        return 0;
    }

    bool exist(int cmd_id) const { return m_dispatcher.find(cmd_id) != m_dispatcher.end(); }

    void callbackFunc(std::span<const uint8_t> data, uint32_t len, int cmd_id, std::shared_ptr<ISocketConnection> connection)
    {
        assert(exist(cmd_id));

        auto func = m_dispatcher[cmd_id];
        auto usr_data = m_args[cmd_id];
        func(data, len, cmd_id, connection, usr_data);
    }

private:
    std::unordered_map<int, msgCallback> m_dispatcher;
    std::unordered_map<int, std::any> m_args;
};

#endif
