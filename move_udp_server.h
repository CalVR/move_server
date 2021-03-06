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
#ifndef MOVE_UDP_SERVER_H
#define MOVE_UDP_SERVER_H

#include "Mutex.hpp"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include "Synchapi.h"

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define WINAPI
typedef unsigned int DWORD;
typedef void * LPVOID;
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include <psmoveapi/psmove_tracker.h>
#include <psmoveapi/psmove.h>

void set_up_udp_socket(SOCKET *newSocket, SOCKADDR_IN *socketAddress, int recv);

/**
 *Data struct for controller LEDs/Rumble control via UDP.
 **/
typedef struct _ControllerData
{
        unsigned char rumble; // Current rumble level of the controller
        int rumbleTimeout; // Timeout after recieving rumble message. Stops battery wasting.
        unsigned char r; // Current color of the controller.
        unsigned char g;
        unsigned char b;
        unsigned char tr; // Tracked color of the controller. Can be returned to if user manually changes color (which won't be tracked).
        unsigned char tg;
        unsigned char tb;
        int resetOrientation; // If 1, will calibrate the orientation of the controller
        int changeLight; // If 1, will set the color of the controller to r, g, b.
        int trackerLight; // If 1, will set the color of the controller to tr, tg, tb.
} ControllerData;

/**
 * Structure to send to the tracking thread.
 **/
typedef struct TrackerData
{
        PSMoveTracker* tracker;
        PSMove **controllers;
        int totalConnectedMoves;
        int *showTracker;
        int *okayToSend;
        SOCKET *udpSocket;
        SOCKADDR_IN *sendAddress;
        void * frame;
        Mutex * frameMutex;
} TRACKERDATA, *PTRACKERDATA;

/**
 * Structure to send to the recv thread.
 **/
typedef struct RecvThreadData
{
        int totalConnectedMoves;
        ControllerData *controllerData;
        SOCKET *udpSocket;
        SOCKADDR_IN *recvAddress;
        int *okayToSend;
        SOCKET *udpSocketOut;
        SOCKADDR_IN *sendAddress;
} RECVTHREADDATA, *PRECVTHREADDATA;

/**
 * Structure to send to the recv thread.
 **/
typedef struct SendThreadData
{
        int totalConnectedMoves;
        PSMove **controllers;
        ControllerData *controllerData;
        int *okayToSend;
        int *trackingEnabled;
        SOCKET *udpSocket;
        SOCKADDR_IN *sendAddress;
} SENDTHREADDATA, *PSENDTHREADDATA;

struct MoveState
{
        unsigned int buttons;
        float qw, qx, qy, qz;
        float x, y, z;
        float trigger;
        Mutex * lock;
};

/*
 * \brief Sets up a simple UDP server which locally sends out PSMove data.
 * Sent in two formats: a Buttons Analogue ax ay az gx gy gz mx my mz
 *					 : b tx ty tz currentlyTracking
 */
int udp_move_server(PSMove **controllers,
                    std::vector<MoveState*> & moveStateList);

// Showing/hiding tracker info mutex. Protects showTracker.
extern Mutex * trackerMutex;

// Protects rumble/led.
extern Mutex * controllerMutex;

#define RUMBLE_TIMEOUT 150
#define SEND_PORT 23459
#define RECV_PORT 23460

#endif
