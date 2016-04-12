#ifndef UDP_PHYSICAL_H
#define UDP_PHYSICAL_H

#include "Thread.hpp"
#include "move_udp_server.h"

#include <vector>

class UDP_Physical : public Thread
{
    public:
        UDP_Physical(PSENDTHREADDATA data, std::vector<MoveState*> & stateList);
        virtual ~UDP_Physical();

        virtual void run();

    protected:
        PSENDTHREADDATA _physicalData;
        std::vector<MoveState*> _stateList;
};

#endif
