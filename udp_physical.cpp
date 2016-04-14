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
#include "udp_physical.h"

#include <cstring>

#ifndef WIN32
#include <unistd.h>
#endif

// Formats move button presses into a simple 8 bit integer.
int format_buttons(unsigned int currButtons)
{
    int btnsToReturn = 0;
    if(currButtons & Btn_CROSS) btnsToReturn += (1 << 7);
    if(currButtons & Btn_SQUARE) btnsToReturn += (1 << 6);
    if(currButtons & Btn_TRIANGLE) btnsToReturn += (1 << 5);
    if(currButtons & Btn_CIRCLE) btnsToReturn += (1 << 4);
    if(currButtons & Btn_MOVE) btnsToReturn += (1 << 3);
    if(currButtons & Btn_START) btnsToReturn += (1 << 2);
    if(currButtons & Btn_SELECT) btnsToReturn += (1 << 1);
    if(currButtons & Btn_PS) btnsToReturn += (1);
    return btnsToReturn;
}

UDP_Physical::UDP_Physical(PSENDTHREADDATA data,
                           std::vector<MoveState*> & stateList) :
        Thread()
{
    _physicalData = data;
    _stateList = stateList;
}

UDP_Physical::~UDP_Physical()
{
}

void UDP_Physical::run()
{
    // ----- physicalData variables -----
    int totalConnectedMoves = _physicalData->totalConnectedMoves;
    PSMove** controllers = _physicalData->controllers;
    // Sending address/socket is defined by udp_recv.cpp once a client connects.
    SOCKADDR_IN* sendAddress = _physicalData->sendAddress;
    SOCKET* udpSocket = _physicalData->udpSocket;
    int* okayToSend = _physicalData->okayToSend;

    int* trackingEnabled = _physicalData->trackingEnabled;
    // ControllerData can be changed by 'udp_recv.cpp' messages and also altered here.
    // Protected by the controllerMutex.
    ControllerData* controllerData = _physicalData->controllerData;

    int currButtons = 0;
    int analogVal = 0;
    int orientationEnabled = 0;
    int currPoll = 0;
    int msgNo = 0;
    int c;
    float ax, ay, az, gx, gy, gz, mx, my, mz, qx, qy, qz, qw;
    int server_length = sizeof(struct sockaddr_in);
    char sendMes[512];

    PSMove* move;

    while(1)
    {
        for(c = 0; c < totalConnectedMoves; c++)
        {
            move = controllers[c];
            // Need to poll for new Move data. Returns 0 if unsucessful poll.
            currPoll = psmove_poll(move);
            if(currPoll)
            {
                // Controller mutex
                controllerMutex->lock();

                // Set the move light to the tracker set value.
                if(controllerData[c].trackerLight)
                {
                    controllerData[c].trackerLight = 0;
                    controllerData[c].changeLight = 0;
                    if(*trackingEnabled)
                    {
                        controllerData[c].r = controllerData[c].tr;
                        controllerData[c].g = controllerData[c].tg;
                        controllerData[c].b = controllerData[c].tb;
                    }
                    psmove_set_leds(move, controllerData[c].r,
                                    controllerData[c].g, controllerData[c].b);
                }
                // Set the move light to a user defined light (not recommended if tracking)
                else if(controllerData[c].changeLight)
                {
                    controllerData[c].changeLight = 0;
                    psmove_set_leds(move, controllerData[c].r,
                                    controllerData[c].g, controllerData[c].b);
                }
                // Rumble the controller. Only runs for a certain amount of ticks, client needs to send multiple packets to keep it going.
                if(controllerData[c].rumbleTimeout > 0)
                {
                    if(controllerData[c].rumbleTimeout == RUMBLE_TIMEOUT)
                    {
                        psmove_set_rumble(move, controllerData[c].rumble);
                    }
                    controllerData[c].rumbleTimeout -= 1;
                }
                // Set the rumble on the controller to 0. (timeout goes to -1 to stop needlessly setting rumble to 0)
                else
                {
                    if(controllerData[c].rumbleTimeout == 0)
                    {
                        psmove_set_rumble(move, 0);
                        controllerData[c].rumbleTimeout = -1;
                    }
                }
                // Reset the orientation (allows user to do so in application)
                if(controllerData[c].resetOrientation)
                {
                    controllerData[c].resetOrientation = 0;
                    psmove_reset_orientation(move);
                    printf("\nController %d has been calibrated.\n >", c);
                }
                controllerMutex->unlock();
                // Controller mutex end

                // Read values from the controller.
                analogVal = psmove_get_trigger(move);
                unsigned int rawButtons = psmove_get_buttons(move);
                currButtons = format_buttons(rawButtons);
                psmove_get_accelerometer_frame(move, Frame_SecondHalf, &ax, &ay,
                                               &az);
                psmove_get_gyroscope_frame(move, Frame_SecondHalf, &gx, &gy,
                                           &gz);
                psmove_get_magnetometer_vector(move, &mx, &my, &mz);
                psmove_update_leds(move);

                // Check for orientation and get new values
                if(psmove_has_orientation(move))
                {
                    orientationEnabled = 1;
                    psmove_get_orientation(move, &qw, &qx, &qy, &qz);
                }
                else
                {
                    orientationEnabled = 0;
                }

                _stateList[c]->lock->lock();

                _stateList[c]->buttons = rawButtons;
                _stateList[c]->qx = qx;
                _stateList[c]->qy = qy;
                _stateList[c]->qz = qz;
                _stateList[c]->qw = qw;
                _stateList[c]->trigger = ((float)analogVal) / 255.0f;

                _stateList[c]->lock->unlock();

                if(*okayToSend == 1)
                {
                    // Stream data for controller to the client.
                    sprintf(sendMes,
                            "a %d %d %d %d %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %d %.3f %.3f %.3f %.3f %d %d %d",
                            msgNo, c, currButtons, analogVal, ax, ay, az, gx,
                            gy, gz, mx, my, mz, orientationEnabled, qw, qx, qy,
                            qz, controllerData[c].r, controllerData[c].g,
                            controllerData[c].b);
                    //printf("%s\n", sendMes);
                    sendto(*udpSocket, sendMes, strlen(sendMes), 0,
                           (SOCKADDR*)sendAddress, server_length);
                    //sprintf(sendMes, "%.3f %.3f %.3f", mx, my, mz);
                    //printf("%s\n", sendMes);
                }
            }
        }

        _quitMutex->lock();
        if(_quit)
        {
            _quitMutex->unlock();
            break;
        }
        _quitMutex->unlock();

#ifdef WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
        if(*okayToSend)
        {
            msgNo++;
        }
    }
}

