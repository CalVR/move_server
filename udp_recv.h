#ifndef UDP_RECV_H
#define UDP_RECV_H

#include "Thread.hpp"
#include "move_udp_server.h"

class UDP_Recv : public Thread
{
    public:
        UDP_Recv(PRECVTHREADDATA data);
        virtual ~UDP_Recv();

        virtual void run();

    protected:
        PRECVTHREADDATA _recvThreadData;
};

#endif
