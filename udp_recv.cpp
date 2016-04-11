
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

#ifdef WIN32
DWORD WINAPI run_udp_recv(LPVOID recvThreadData)
#else
void * run_udp_recv(LPVOID recvThreadData)
#endif
{

	PRECVTHREADDATA _recvThreadData = (PRECVTHREADDATA)recvThreadData;
	// The recieve address/socket are defined by the user selected network interface.
	SOCKADDR_IN* recvAddress = _recvThreadData->recvAddress;
	SOCKET* recvSocket = _recvThreadData->udpSocket;
	// The send address/socket are defined by the first connect message recieved
	int* okayToSend = _recvThreadData->okayToSend;
	SOCKADDR_IN* sendAddress = _recvThreadData->sendAddress;
	SOCKET* sendSocket = _recvThreadData->udpSocketOut;

	int* finishThread = _recvThreadData->finishThread;
	ControllerData* controllerData = _recvThreadData->controllerData;
	char recvMsg[512];

	SOCKADDR_IN* SenderAddr = (SOCKADDR_IN*)malloc(sizeof *SenderAddr);
	int SenderAddrSize;

	int c, rumble, resetOrientation, trackerLight, changeLight, r, g, b, changeRumble;

	while (1) {
		//printf("Waiting on data...\n");
		SenderAddrSize = 128;
		int n = recvfrom(*recvSocket, recvMsg, 512, 0, (SOCKADDR *)SenderAddr, (socklen_t*)&SenderAddrSize);
		if (n) {
			// Waiting on 'c'onnect message from client.
			if (!*okayToSend) {
				if (recvMsg[0] == 'c') {
					// Once this message is recieved, we save the sender's address and set up a socket to stream to.
					//char ipString[32];
					//InetNtop(AF_INET, &SenderAddr->sin_addr, ipString, 32);
					//inet_pton(AF_INET, ipString, &sendAddress->sin_addr);
				
					sendAddress->sin_addr = SenderAddr->sin_addr;	
					sendAddress->sin_family = AF_INET;
					sendAddress->sin_port = htons(SEND_PORT);
					set_up_udp_socket(sendSocket, sendAddress, 0);
					printf("Client connected. Streaming data on port %d\n", SEND_PORT);
					// The other threads now know to stream their data.
					*okayToSend = 1;
				}
			}
			else {
				// When we know where to stream data to, we now listen for messages to update controller properties.
				if (recvMsg[0] == 'd') {
					sscanf(recvMsg, "d %d %d %d %d %d %d %d %d %d", &c, &changeRumble, &rumble, &resetOrientation, &trackerLight, &changeLight, &r, &g, &b);

					// Protect controller data.
#ifdef WIN32
					WaitForSingleObject(controllerMutex, INFINITE);
#else
					pthread_mutex_lock(&controllerMutex);
#endif
					// Very slight error detection here. Up to the user to send the right packets.
					if (c >= 0 && c < _recvThreadData->totalConnectedMoves) {
						if (changeRumble) {
							controllerData[c].rumble = rumble;
							controllerData[c].rumbleTimeout = RUMBLE_TIMEOUT;
						}
						// Can only change from 0 -> 1 for these options via messages. Avoids overriding messages before their intended 
						// operation can be completed. (Eg. reseting orientation, but recieving many different rumble messages quickly.
						// The rumble messages will have resetOrientation = 0, but setting it to 0 might make the physical thread miss
						// the original resetOrientation = 1.
						if (controllerData[c].trackerLight == 0) controllerData[c].trackerLight = trackerLight;
						if (controllerData[c].resetOrientation == 0) controllerData[c].resetOrientation = resetOrientation;
						if (changeLight && !trackerLight) {
							controllerData[c].changeLight = 1;
							controllerData[c].r = r;
							controllerData[c].g = g;
							controllerData[c].b = b;
						}
					}
#ifdef WIN32
					ReleaseMutex(controllerMutex);
#else
					pthread_mutex_unlock(&controllerMutex);
#endif
				}
			}
		}
		else {
#ifdef WIN32
			int e = WSAGetLastError();
			printf("Recv error: %d, exiting thread.", e);
#else
			printf("Recv error");
#endif
			break;
		}
		//printf("%d %d %d %d %d %d\n", c, controllerData[c].rumble, controllerData[c].changeLight, controllerData[c].r, controllerData[c].g, controllerData[c].b);

		// TODO: Implement a non-blocking method to close this thread.
		if (*finishThread){
			break;
		}
	}
	return 0;
}
