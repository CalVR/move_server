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
#include "opencv2/core/core_c.h"
#include "opencv2/highgui/highgui_c.h"

DWORD WINAPI run_tracker(LPVOID trackerData) {

	PTRACKERDATA _trackerData = (PTRACKERDATA)trackerData;

	// ----- trackerData variables. -----
	PSMoveTracker* tracker = _trackerData->tracker;
	PSMove** controllers = _trackerData->controllers;

	// Sending address/socket is defined by udp_recv.cpp once a client connects.
	SOCKADDR_IN* sendAddress = _trackerData->sendAddress;
	SOCKET* udpSocket = _trackerData->udpSocket;
	int* okayToSend = _trackerData->okayToSend;

	int totalConnectedMoves = _trackerData->totalConnectedMoves;
	// showTracker changed by the main menu in 'move_udp_server.cpp'
	int* showTracker = _trackerData->showTracker;
	// finishThread is also changed by the main menu, specifically on exit.
	int* finishThread = _trackerData->finishThread;
	
	// ----- Sending variables -----
	enum PSMoveTracker_Status status;
	char trackerMsg[256];
	int posUpdateNumber = 0;
	int trackingMove = 0;
	int c;
	PSMove* move;
	float tx, ty, tz, ux, uy, rad;
	int width, height;
	psmove_tracker_get_size(tracker, &width, &height);
	int server_length = sizeof(struct sockaddr_in);
	// Used for displaying the debug image.
	IplImage *frame;
	//DWORD waitResult;

	while (1)
	{

		// Update tracker image
		psmove_tracker_update_image(tracker);

		// Track each controller individually.
		for (c = 0; c < totalConnectedMoves; c++) {
			move = controllers[c];

			psmove_tracker_update(tracker, move);
			status = psmove_tracker_get_status(tracker, move);
			if (status == Tracker_TRACKING) {
				// Create normailised position values to the size of the camera image plane.
				psmove_tracker_get_position(tracker, move, &ux, &uy, &rad);
				ux /= (float)width;
				uy /= (float)height;
				psmove_tracker_get_location(tracker, move, &tx, &ty, &tz);
				trackingMove = 1;
			}
			else {
				trackingMove = 0;
			}
			if (*okayToSend) {
				sprintf(trackerMsg, "b %d %d %f %f %f %f %f %d", posUpdateNumber, c, tx, ty, tz, ux, uy, trackingMove);
				sendto(*udpSocket, trackerMsg, strlen(trackerMsg), 0, (SOCKADDR*)sendAddress, server_length);
			}
			else {
				// While we aren't sending, the physical thread doesn't update. We temporarily do it here.
				char r, g, b;
				psmove_tracker_get_color(tracker, move, &r, &g, &b);
				psmove_set_leds(move, r, g, b);
				psmove_update_leds(move);
			}
		}
		if (*okayToSend) posUpdateNumber++;

		// Wait for the main menu to decide showTracker's value.
		WaitForSingleObject(trackerMutex, INFINITE);
		if (*showTracker == 1) {
			// Show an annotated stream of the tracking footage.
			psmove_tracker_annotate(tracker);
			frame = (IplImage*)psmove_tracker_get_frame(tracker);
			if (frame) {
				cvShowImage("Debug Window", frame);
				cvWaitKey(1);
			}
		}
		ReleaseMutex(trackerMutex);

		if (*finishThread){
			break;
		}
	}
	return 0;
}