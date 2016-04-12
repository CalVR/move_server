
 /**
 * PS Move API - An interface for the PS Move Motion Controller
 * Copyright (c) 2011 Thomas Perl <m@thp.io>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 **/

#include "move_udp_server.h"
#include "udp_tracker.h"
#include "udp_recv.h"
#include "udp_physical.h"

#ifdef WITH_VRPN
#include "VRPNServer.h"
#endif

#include <cstring>
#include <vector>

#ifdef WIN32
#include <conio.h>

#pragma comment (lib, "Ws2_32.lib")

#endif

#ifndef WIN32
#define INVALID_SOCKET -1

// simplified version of kbhit since full functionality is not required
// http://www.flipcode.com/archives/_kbhit_for_Linux.shtml
bool kbhit()
{
    timeval timeout;
    fd_set rdset;

    FD_ZERO(&rdset);
    FD_SET(0, &rdset);
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    return select(1, &rdset, NULL, NULL, &timeout);
}

#endif

Mutex * trackerMutex = NULL;
Mutex * controllerMutex = NULL;

MoveState * createMoveState()
{
    MoveState * ms = new MoveState;
    ms->buttons = 0;
    ms->x = ms->y = ms->z = 0.0;
    ms->qx = ms->qy = ms->qw = 0.0;
    ms->qz = 1.0;
    ms->lock = new Mutex();
}

int main(int argc, char* argv[])
{
    int totalConnectedMoves;
	PSMove **controllers;
	int c;
    std::vector<MoveState*> moveStateList;

    if (!psmove_init(PSMOVE_CURRENT_VERSION)) {
        fprintf(stderr, "PS Move API init failed (wrong version?)\n");
        exit(1);
    }

	totalConnectedMoves = psmove_count_connected();
    printf("Connected controllers: %d\n", totalConnectedMoves);

	if (totalConnectedMoves == 0) {
		printf("No Moves found, shutting down.");
		exit(1);
	}

	controllers = (PSMove **)calloc(totalConnectedMoves, sizeof(PSMove *));

	for (c = 0; c < totalConnectedMoves; c++) {
		controllers[c] = psmove_connect_by_id(c);

		//TODO: Warn about usb connected moves.
		//enum PSMove_Connection_Type ctype; = psmove_connection_type(move);

		if (controllers[c] == NULL) {
			printf("Controller %d could not be connected. Exiting.", c);
			exit(1);
		}

		if (psmove_connection_type(controllers[c]) == Conn_USB) printf("WARNING: Controller &d is connected by USB, physical data will be unavailable.");
		psmove_set_orientation_fusion_type(controllers[c], (PSMoveOrientation_Fusion_Type)3);
		psmove_enable_orientation(controllers[c], PSMove_True);

	    moveStateList.push_back(createMoveState());
	}

	// Run the server. This will block until the server is exited.
	udp_move_server(controllers,moveStateList);
    
	// Shut down the api.
	for (c = 0; c < totalConnectedMoves; c++) {
		psmove_disconnect(controllers[c]);
	}
	printf("Controllers disconnected. Okay to exit. (Shutdown hangs sometimes.)");
    psmove_shutdown();
    return 0;
}

int udp_move_server(PSMove **controllers, std::vector<MoveState*> & moveStateList)
{
	int totalConnectedMoves = psmove_count_connected();
	//ControllerData* controllerData = (ControllerData*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalConnectedMoves * sizeof(ControllerData));
	ControllerData* controllerData = new ControllerData[totalConnectedMoves];

	// Initialise controller data
	for (int c = 0; c < totalConnectedMoves; c++) {
		controllerData[c].r = 0;
		controllerData[c].g = 0;
		controllerData[c].b = 0;
		controllerData[c].tr = 0;
		controllerData[c].tg = 0;
		controllerData[c].tb = 0;
		controllerData[c].resetOrientation = 0;
		controllerData[c].changeLight = 1;
		controllerData[c].rumble = 0;
		controllerData[c].rumbleTimeout = 0;
		controllerData[c].trackerLight = 0;
	}

	int okayToSend = 0;
	SOCKET udpSendSocket, udpRecvSocket;
	//SOCKADDR_IN *localSendAddress = malloc(sizeof *localSendAddress);
	//SOCKADDR_IN *localRecvAddress = malloc(sizeof *localRecvAddress);
	SOCKADDR_IN *localSendAddress = new SOCKADDR_IN;
	SOCKADDR_IN *localRecvAddress = new SOCKADDR_IN;

#ifdef WIN32
	// Start up WSA (Windows Socket API)
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR)
	{
		printf("Socket Initialization: Error with WSAStartup\n");
		system("pause");
		WSACleanup();
		exit(10);
	}

	// Find connected network interfaces. Let the user decide which to use.
	char recv_address[16];
	char hostname[512];
	gethostname(hostname, 512);

	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	struct addrinfo *result = NULL;
	struct addrinfo *ptr = NULL;
	getaddrinfo(hostname, NULL, &hints, &result);
	
	// Print out (and store) the available network interfaces.
	printf("Choose network interface: \n");
	int i = 0;
	char ip_address[8][32];
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		SOCKADDR_IN* sockaddr_ipv4 = (struct sockaddr_in *) ptr->ai_addr;
		InetNtop(AF_INET, &sockaddr_ipv4->sin_addr, ip_address[i], 32);
		printf("\tInterface %d: %s\n", i+1, ip_address[i]);
		i++;
	}
	printf("\tInterface %d: 127.0.0.1 (local loop)\n> ", i+1);

	// Run a small menu where the user can select the interface
	int interfaceDecision = 0;
	int valid;
	char clearBuffer[256];
	while (interfaceDecision == 0) {
		valid = scanf("%d", &interfaceDecision);
		if (valid == 1) {
			if (interfaceDecision > 0 && interfaceDecision <= i+1) {
				break;
			}
		}
		interfaceDecision = 0;
		scanf("%s", clearBuffer);
		printf("Invalid interface.\n> ");
	}

	// Tell the user the interface and port the server is listening on.
	if (interfaceDecision == i + 1) {
		strcpy(recv_address, "127.0.0.1");
		printf("Selected: 127.0.0.1 (local loop) \n");
	}
	else {
		strcpy(recv_address, ip_address[interfaceDecision - 1]);
		printf("Listening on: %s:%d \n", ip_address[interfaceDecision - 1],RECV_PORT);
	}

	printf("------------\n");
	
	inet_pton(AF_INET, recv_address, &localRecvAddress->sin_addr);
#else
	localRecvAddress->sin_addr.s_addr = htonl(INADDR_ANY);
#endif


	localRecvAddress->sin_family = AF_INET;
	localRecvAddress->sin_port = htons(RECV_PORT);

	// Create the receiving UDP socket. 
	set_up_udp_socket(&udpRecvSocket, localRecvAddress, 1);

	// Attempt to initialise the tracker, with custom settings.
	PSMoveTrackerSettings settings;
	psmove_tracker_settings_set_default(&settings);
	settings.exposure_mode = Exposure_LOW;
	settings.color_mapping_max_age = 0;
	settings.camera_mirror = PSMove_True;
	settings.color_save_colormapping = PSMove_False; // Means we need to calibrate each time, saves us from saving bad calibrations.
	settings.color_list_start_ind = 0;  // TODO: Allow user to select this. (Starting tracking color of the Move)

	PSMoveTracker* tracker = psmove_tracker_new_with_settings(&settings);

	// Calibrate each controller with the tracker.
	int c;
	int tracking_enabled = 0;
	int show_tracker = 0;
	PSMove* move;
	int currPoll;

	UDP_Tracker * tracker_thread = NULL;

	// Check if the tracker was successfully initialised.
	if (tracker) {
		tracking_enabled = 1;
		printf("------------\nTracker calibration (Hold wand about 10cm from camera)\n------------\n");
		for (c = 0; c < totalConnectedMoves; c++) {
			printf("Calibrating tracker for controller: %d", c); 
			while (psmove_tracker_enable(tracker, controllers[c]) != Tracker_CALIBRATED) {
				printf(".");
			}

			printf(" Tracker Calibrated!\n");

			// Save the tracker color values. Used in the case of the client changing colors and wanting to revert.
			psmove_tracker_get_color(tracker, controllers[c], &controllerData[c].tr, &controllerData[c].tg, &controllerData[c].tb);
			controllerData[c].r = controllerData[c].tr;
			controllerData[c].g = controllerData[c].tg;
			controllerData[c].b = controllerData[c].tb;
			psmove_tracker_set_auto_update_leds(tracker, controllers[c], PSMove_False);
			
			// Calibrating the controller's orientation quaternion. (Checking if its connected via bluetooth)
			if (psmove_connection_type(controllers[c]) == Conn_Bluetooth) {
				printf("Hold PSMove flat facing your screen and press the MOVE button to calibrate orientation.\n", c);
				while (1) {
					move = controllers[c];
					currPoll = psmove_poll(move);
					if (currPoll) {
						if (psmove_get_buttons(move) & Btn_MOVE) {
							psmove_reset_orientation(move);
							printf("Controller %d has been calibrated.\n", c);
							break;
						}
					}
				}
			}
		}

		trackerMutex = new Mutex();
		if(trackerMutex->error())
		{
		    printf("Error creating trackerMutex.\n");
		    return 1;
		}

		// Create the trackerData struct to send to the tracking thread.
		//PTRACKERDATA trackerData = (PTRACKERDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TRACKERDATA));
		PTRACKERDATA trackerData = new TRACKERDATA;
		trackerData->controllers = controllers;
		trackerData->tracker = tracker;
		trackerData->totalConnectedMoves = totalConnectedMoves;
		trackerData->showTracker = &show_tracker;
		trackerData->sendAddress = localSendAddress;
		trackerData->udpSocket = &udpSendSocket;
		trackerData->okayToSend = &okayToSend;

		tracker_thread = new UDP_Tracker(trackerData, moveStateList);
		tracker_thread->startThread();
	}
	else {
		printf("WARNING: Couldn't initialise tracker. Only physical move data will be sent. \n");
	}

	// ----- Initialising the 'Receive Thread' -----
	controllerMutex = new Mutex();
	if(controllerMutex->error())
	{
	    printf("Error creating controllerMutex.\n");
	    return 1;
	}

	// Create the recvData struct to send to the receive thread.
	//PRECVTHREADDATA recvData = (PRECVTHREADDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RECVTHREADDATA));
	PRECVTHREADDATA recvData = new RECVTHREADDATA;
	
	recvData->recvAddress = localRecvAddress;
	recvData->udpSocket = &udpRecvSocket;
	recvData->totalConnectedMoves = totalConnectedMoves;
	recvData->controllerData = controllerData;
	recvData->okayToSend = &okayToSend;
	recvData->udpSocketOut = &udpSendSocket;
	recvData->sendAddress = localSendAddress;

	UDP_Recv * recv_thread = new UDP_Recv(recvData);
	recv_thread->startThread();

	// ----- Initialising the 'Send Physical Thread' -----

	// Create the sendData struct to send to the 'physical send' thread.
	//PSENDTHREADDATA sendData = (PSENDTHREADDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SENDTHREADDATA));
	PSENDTHREADDATA sendData = new SENDTHREADDATA;
	sendData->controllerData = controllerData;
	sendData->totalConnectedMoves = totalConnectedMoves;
	sendData->controllers = controllers;
	sendData->udpSocket = &udpSendSocket;
	sendData->sendAddress = localSendAddress;
	sendData->okayToSend = &okayToSend;
	sendData->trackingEnabled = &tracking_enabled;

	UDP_Physical * send_thread = new UDP_Physical(sendData,moveStateList);
	send_thread->startThread();

#ifdef WITH_VRPN
	VRPNServer * vrpn = new VRPNServer(moveStateList, vrpn_create_server_connection(":VRPN_PORT",NULL,NULL));
	vrpn->startThread();
#endif

	printf("------------\nServer Started. (Waiting on client connection.)\n");

	int close_server = 0;
	int controllerToCalibrate = 0;

	printf("------------\nCommands:\n------------\n");
	printf(" showtracker : Shows annotated tracker footage if available.\n");
	printf(" hidetracker : Stops updating the tracker footage.\n");
	printf(" calibrate c : Resets the quaternion for controller 'c' (0-3).\n");
	printf(" exit        : Shutdown the server\n");
	printf("------------\n");
	while (!close_server)
	{
		// Run main menu. Adapted from TUIO.cpp
		if (kbhit()) {
			char s[2048];
			memset(s, 0, sizeof(s));
			if (fgets(s, sizeof(s), stdin) != NULL) {
				// strip trailing newline
				if (s[strlen(s) - 1] == '\n') {
					s[strlen(s) - 1] = '\0';
				}

				if (strlen(s) == 0) {
					// Do nothing
				}
				else if (strcmp(s, "exit") == 0) {
					close_server = 1;
					break;
				}
				else if (strcmp(s, "showtracker") == 0) {
					if (tracking_enabled) {
						// Need to tell the tracking thread to annotate and show the tracking footage.
						trackerMutex->lock();
						show_tracker = 1;
						trackerMutex->unlock();
					}
					else {
						printf("Error: Tracker not enabled.\n");
					}
				}
				else if (strcmp(s, "hidetracker") == 0) {
					if (tracking_enabled) {
						trackerMutex->lock();
						show_tracker = 0;
						trackerMutex->unlock();
					}
					else {
						printf("Error: Tracker not enabled.\n");
					}
				}
				else if (memcmp(s, "calibrate ", 10) == 0) {
					sscanf(s, "calibrate %d\n", &controllerToCalibrate);
					if (controllerToCalibrate >= 0 && controllerToCalibrate < totalConnectedMoves) {
						printf("Hold PSMove flat facing your screen and press the MOVE button to calibrate orientation.\n", controllerToCalibrate);
						while (1) {
							move = controllers[controllerToCalibrate];
							currPoll = psmove_poll(move);
							if (currPoll) {
								if (psmove_get_buttons(move) & Btn_MOVE) {
									psmove_reset_orientation(move);
									printf("Controller %d has been calibrated.\n", controllerToCalibrate);
									break;
								}
							}
						}
					}
					else {
						printf("Error: Controller %d is not a valid option.\n", controllerToCalibrate);
					}

				}
				else {
					printf("Invalid command: '%s'\n", s);
				}

				printf("> ");
				fflush(stdout);
			}
		}
	}
	recv_thread->join();
	delete recv_thread;
	send_thread->join();
	delete send_thread;
#ifdef WITH_VRPN
	vrpn->join();
	delete vrpn;
#endif
	if (tracking_enabled) {
		tracker_thread->join();
		delete tracker_thread;
		psmove_tracker_free(tracker);
		printf("Thread closed.\n");
	}
	return 0;
}

void network_error_exit(int code)
{
#ifdef WIN32
    system("pause");
    WSACleanup();
#endif
    exit(code);
}

void set_up_udp_socket(SOCKET *newSocket, SOCKADDR_IN *socketAddress, int recv) {
	// Create the UDP socket.
	*newSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (*newSocket == INVALID_SOCKET)
	{
		printf("Socket Initialization: Error creating socket\n");
		network_error_exit(11);
	}

	// Set up the local address.
	//socketAddress->sin_family = AF_INET;
	//inet_pton(AF_INET, ipAddress, &socketAddress->sin_addr);
	//socketAddress->sin_port = htons(port);

	// Finally, connect the socket ready for sending data.
	// (Note that this really isn't needed for sending data, as you can't 'connect' with UDP)
	// (It is necessary for recv though.)
	if (!recv) {
		if (connect(*newSocket, (struct sockaddr *)socketAddress, sizeof(*socketAddress)) < 0) {
			printf("Error: Send Socket failed to connect\n");
			network_error_exit(14);
		}
	}
	else {
		if (bind(*newSocket, (struct sockaddr *)socketAddress, sizeof(*socketAddress)) < 0) {
			printf("Error: Recv Socket failed to bind\n");
			network_error_exit(14);
		}
	}
}




