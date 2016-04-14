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
#include "udp_recv.h"

#ifndef WIN32
#include <unistd.h>
#endif

UDP_Recv::UDP_Recv(PRECVTHREADDATA data) :
        Thread()
{
    _recvThreadData = data;
}

UDP_Recv::~UDP_Recv()
{
}

void UDP_Recv::run()
{
    // The recieve address/socket are defined by the user selected network interface.
    SOCKADDR_IN* recvAddress = _recvThreadData->recvAddress;
    SOCKET* recvSocket = _recvThreadData->udpSocket;
    // The send address/socket are defined by the first connect message recieved
    int* okayToSend = _recvThreadData->okayToSend;
    SOCKADDR_IN* sendAddress = _recvThreadData->sendAddress;
    SOCKET* sendSocket = _recvThreadData->udpSocketOut;

    ControllerData* controllerData = _recvThreadData->controllerData;
    char recvMsg[512];

    SOCKADDR_IN* SenderAddr = (SOCKADDR_IN*)malloc(sizeof *SenderAddr);
    int SenderAddrSize;

    int c, rumble, resetOrientation, trackerLight, changeLight, r, g, b,
            changeRumble;

    while(1)
    {
        //printf("Waiting on data...\n");
        SenderAddrSize = 128;
        int n = recvfrom(*recvSocket, recvMsg, 512, MSG_DONTWAIT,
                         (SOCKADDR *)SenderAddr, (socklen_t*)&SenderAddrSize);
        if(n > 0)
        {
            // Waiting on 'c'onnect message from client.
            if(!*okayToSend)
            {
                if(recvMsg[0] == 'c')
                {
                    sendAddress->sin_addr = SenderAddr->sin_addr;
                    sendAddress->sin_family = AF_INET;
                    sendAddress->sin_port = htons(SEND_PORT);
                    set_up_udp_socket(sendSocket, sendAddress, 0);
                    printf("Client connected. Streaming data on port %d\n",
                           SEND_PORT);
                    // The other threads now know to stream their data.
                    *okayToSend = 1;
                }
            }
            else
            {
                // When we know where to stream data to, we now listen for messages to update controller properties.
                if(recvMsg[0] == 'd')
                {
                    sscanf(recvMsg, "d %d %d %d %d %d %d %d %d %d", &c,
                           &changeRumble, &rumble, &resetOrientation,
                           &trackerLight, &changeLight, &r, &g, &b);

                    // Protect controller data.
                    controllerMutex->lock();

                    // Very slight error detection here. Up to the user to send the right packets.
                    if(c >= 0 && c < _recvThreadData->totalConnectedMoves)
                    {
                        if(changeRumble)
                        {
                            controllerData[c].rumble = rumble;
                            controllerData[c].rumbleTimeout = RUMBLE_TIMEOUT;
                        }
                        // Can only change from 0 -> 1 for these options via messages. Avoids overriding messages before their intended
                        // operation can be completed. (Eg. reseting orientation, but recieving many different rumble messages quickly.
                        // The rumble messages will have resetOrientation = 0, but setting it to 0 might make the physical thread miss
                        // the original resetOrientation = 1.
                        if(controllerData[c].trackerLight == 0) controllerData[c].trackerLight =
                                trackerLight;
                        if(controllerData[c].resetOrientation == 0) controllerData[c].resetOrientation =
                                resetOrientation;
                        if(changeLight && !trackerLight)
                        {
                            controllerData[c].changeLight = 1;
                            controllerData[c].r = r;
                            controllerData[c].g = g;
                            controllerData[c].b = b;
                        }
                    }
                    controllerMutex->unlock();
                }
            }
        }
        else
        {
#ifdef WIN32
            Sleep(10);
#else
            usleep(1000);
#endif
        }
        //printf("%d %d %d %d %d %d\n", c, controllerData[c].rumble, controllerData[c].changeLight, controllerData[c].r, controllerData[c].g, controllerData[c].b);

        _quitMutex->lock();
        if(_quit)
        {
            _quitMutex->unlock();
            break;
        }
        _quitMutex->unlock();
    }
}
