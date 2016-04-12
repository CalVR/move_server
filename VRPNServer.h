#ifndef VRPN_SERVER_MOVE_H
#define VRPN_SERVER_MOVE_H

#include "Thread.hpp"
#include "move_udp_server.h"

#include <vrpn_Button.h>
#include <vrpn_Analog.h>
#include <vrpn_Tracker.h>

#include <vector>

#define VRPN_PORT 8701

class VRPNServer : public Thread, public vrpn_Button, public vrpn_Analog, public vrpn_Tracker_Server
{
    public:
        VRPNServer(std::vector<MoveState*> & stateList, vrpn_Connection * con);
        virtual ~VRPNServer();

        virtual void run();
        void mainloop();

    protected:
        std::vector<MoveState*> _stateList;

        vrpn_Connection * _con;
        std::vector<unsigned int> _buttonMasks;
};

#endif
