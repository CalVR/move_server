#include "VRPNServer.h"

VRPNServer::VRPNServer(std::vector<MoveState*> & stateList, vrpn_Connection * con) : Thread(), vrpn_Button("Device0",con), vrpn_Analog("Device0",con), vrpn_Tracker_Server("Device0",con)
{
    _stateList = stateList;
    _con = con;

    _buttonMasks.push_back(Btn_TRIANGLE);
    _buttonMasks.push_back(Btn_CIRCLE);
    _buttonMasks.push_back(Btn_CROSS);
    _buttonMasks.push_back(Btn_SQUARE);
    _buttonMasks.push_back(Btn_SELECT);
    _buttonMasks.push_back(Btn_START);
    _buttonMasks.push_back(Btn_PS);
    _buttonMasks.push_back(Btn_MOVE);
    _buttonMasks.push_back(Btn_T);

    num_buttons = _buttonMasks.size() * _stateList.size();
    num_channel = 0;

    for(int i = 0; i < num_buttons; ++i)
    {
	buttons[i] = 0;
    }
    for(int i = 0; i < num_channel; ++i)
    {
	channel[i] = 0.0;
    }

    num_sensors = _stateList.size();
}

VRPNServer::~VRPNServer()
{
}

void VRPNServer::run()
{
    while(1)
    {
	_quitMutex->lock();
	if(_quit)
	{
	    _quitMutex->unlock();
	    break;
	}
	_quitMutex->unlock();

	mainloop();

	_con->mainloop();

	// rate limit
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 14000000;
	nanosleep(&ts,NULL);
    }
}

void VRPNServer::mainloop()
{
    vrpn_float64 position[3];
    vrpn_float64 quat[4];
    struct timeval tv;
    gettimeofday(&tv,NULL);
    int bindexOffset = 0;
    for(int j = 0; j < _stateList.size(); ++j)
    {
	_stateList[j]->lock->lock();
	unsigned int buttonState = _stateList[j]->buttons;

	for(int i = 0; i < _buttonMasks.size(); ++i)
	{
	    if(buttonState & _buttonMasks[i])
	    {
		buttons[bindexOffset + i] = 1;
	    }
	    else
	    {
		buttons[bindexOffset + i] = 0;
	    }
	}
	bindexOffset += _buttonMasks.size();

	position[0] = _stateList[j]->x;
	position[1] = _stateList[j]->y;
	position[2] = _stateList[j]->z;

	quat[0] = _stateList[j]->qw;
	quat[1] = _stateList[j]->qx;
	quat[2] = _stateList[j]->qy;
	quat[3] = _stateList[j]->qz;

	_stateList[j]->lock->unlock();

	report_pose(j,tv,position,quat);
    }

    vrpn_Button::report_changes();
    vrpn_Analog::report_changes();
}
