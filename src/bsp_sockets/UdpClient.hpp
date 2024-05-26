#ifndef __UDP_CLIENT_H__
#define __UDP_CLIENT_H__

#include "MsgHead.hpp"
#include "NetCommu.hpp"

#include <netinet/in.h>

#include "impl/EventLoop.hpp"
#include "impl/MsgDispatcher.hpp"

class udp_client: public net_commu
{
public:
    udp_client(event_loop* loop, const char* ip, uint16_t port);

    ~udp_client();

    void add_msg_cb(int cmdid, msg_callback* msg_cb, void* usr_data = NULL) { _dispatcher.add_msg_cb(cmdid, msg_cb, usr_data); }

    event_loop* loop() { return _loop; }

    void handle_read();

    virtual int send_data(const char* data, int datlen, int cmdid);

    virtual int get_fd() { return _sockfd; }

private:
    int _sockfd;
    char _rbuf[MSG_LENGTH_LIMIT];
    char _wbuf[MSG_LENGTH_LIMIT];
    event_loop* _loop;
    msg_dispatcher _dispatcher;
};

#endif
