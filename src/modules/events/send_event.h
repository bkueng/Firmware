/****************************************************************************
 *
 *   Copyright (c) 2017 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#pragma once

#include "subscriber_handler.h"
#include "status_display.h"

#include <px4_workqueue.h>
#include <px4_module.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/vehicle_command_ack.h>

extern "C" __EXPORT int send_event_main(int argc, char *argv[]);

class SendEvent : public ModuleBase<SendEvent>
{
public:
	SendEvent();

	/**
	 * Initialize class in the same context as the work queue. And start the background listener.
	 * @return 0 if successful, <0 on error */
	static int taskSpawn(int argc, char *argv[]);

	/** @see ModuleBase */
	static int customCommand(int argc, char *argv[]);

	/** @see ModuleBase */
	static int printUsage(const char *reason = nullptr);

private:

	/** Start background listening for commands
	 *
	 * @return 0 if successful, <0 on error. */
	int start();


	/** Trampoline for initialisation. */
	static void initializeTrampoline(void *arg);
	/** Trampoline for the work queue. */
	static void cycleTrampoline(void *arg);

	/** call process_commands() and schedule the next cycle. */
	void cycle();

	/** check for new commands and process them. */
	void processCommands();

	/** return an ACK to a vehicle_command */
	void answerCommand(const vehicle_command_s &cmd, unsigned result);

	static struct work_s work;
	events::SubscriberHandler _subscriber_handler;
	status::StatusDisplay _status_display;
	orb_advert_t _command_ack_pub = nullptr;
};
