#ifndef UDP_TRACKER_H
#define UDP_TRACKER_H

#include "Thread.hpp"
#include "move_udp_server.h"

#include <vector>

class UDP_Tracker : public Thread
{
    public:
        UDP_Tracker(PTRACKERDATA data, std::vector<MoveState*> & stateList);
        virtual ~UDP_Tracker();

        virtual void run();

    protected:
        PTRACKERDATA _trackerData;
        std::vector<MoveState*> _stateList;
};

#endif
