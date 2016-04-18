/****************************************************************************
 *
 *   Copyright (c) 2012-2016 PX4 Development Team. All rights reserved.
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

#ifndef __PX4_QURT
#include <termios.h>
#include <poll.h>
#else
#include <sys/ioctl.h>
#include <dev_fs_lib_serial.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <systemlib/err.h>
#include <drivers/drv_hrt.h>
#include <uORB/topics/gps_inject_data.h>

#include "gps_helper.h"

/**
 * @file gps_helper.cpp
 *
 * @author Thomas Gubler <thomasgubler@student.ethz.ch>
 * @author Julian Oes <julian@oes.ch>
 */

#define GPS_WAIT_BEFORE_READ	20		// ms, wait before reading to save read() calls

GPSHelper::GPSHelper(int fd, bool support_inject_data)
	: _fd(fd)
{
	_orb_inject_data_fd.fill(-1);

	if (support_inject_data) {
		for (int i = 0; i < _orb_inject_data_fd.size(); ++i) {
			_orb_inject_data_fd[i] = orb_subscribe_multi(ORB_ID(gps_inject_data), i);
		}
	}
}

GPSHelper::~GPSHelper()
{
	if (_orb_inject_data_fd[0] != -1) {
		for (size_t i = 0; i < _orb_inject_data_fd.size(); ++i) {
			orb_unsubscribe(_orb_inject_data_fd[i]);
		}
	}
}

bool GPSHelper::injectData(uint8_t *data, size_t len)
{
	return ::write(_fd, data, len) == len;
}

float
GPSHelper::getPositionUpdateRate()
{
	return _rate_lat_lon;
}

float
GPSHelper::getVelocityUpdateRate()
{
	return _rate_vel;
}

void
GPSHelper::resetUpdateRates()
{
	_rate_count_vel = 0;
	_rate_count_lat_lon = 0;
	_interval_rate_start = hrt_absolute_time();
}

void
GPSHelper::storeUpdateRates()
{
	_rate_vel = _rate_count_vel / (((float)(hrt_absolute_time() - _interval_rate_start)) / 1000000.0f);
	_rate_lat_lon = _rate_count_lat_lon / (((float)(hrt_absolute_time() - _interval_rate_start)) / 1000000.0f);
}

int
GPSHelper::setBaudrate(const int &fd, unsigned baud)
{

#if __PX4_QURT
	// TODO: currently QURT does not support configuration with termios.
	dspal_serial_ioctl_data_rate data_rate;

	switch (baud) {
	case 9600: data_rate.bit_rate = DSPAL_SIO_BITRATE_9600; break;

	case 19200: data_rate.bit_rate = DSPAL_SIO_BITRATE_19200; break;

	case 38400: data_rate.bit_rate = DSPAL_SIO_BITRATE_38400; break;

	case 57600: data_rate.bit_rate = DSPAL_SIO_BITRATE_57600; break;

	case 115200: data_rate.bit_rate = DSPAL_SIO_BITRATE_115200; break;

	default:
		PX4_ERR("ERR: unknown baudrate: %d", baud);
		return -EINVAL;
	}

	int ret = ::ioctl(fd, SERIAL_IOCTL_SET_DATA_RATE, (void *)&data_rate);

	if (ret != 0) {

		return ret;
	}

#else
	/* process baud rate */
	int speed;

	switch (baud) {
	case 9600:   speed = B9600;   break;

	case 19200:  speed = B19200;  break;

	case 38400:  speed = B38400;  break;

	case 57600:  speed = B57600;  break;

	case 115200: speed = B115200; break;

	default:
		PX4_ERR("ERR: unknown baudrate: %d", baud);
		return -EINVAL;
	}

	struct termios uart_config;

	int termios_state;

	/* fill the struct for the new configuration */
	tcgetattr(fd, &uart_config);

	/* properly configure the terminal (see also https://en.wikibooks.org/wiki/Serial_Programming/termios ) */

	//
	// Input flags - Turn off input processing
	//
	// convert break to null byte, no CR to NL translation,
	// no NL to CR translation, don't mark parity errors or breaks
	// no input parity check, don't strip high bit off,
	// no XON/XOFF software flow control
	//
	uart_config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
				 INLCR | PARMRK | INPCK | ISTRIP | IXON);
	//
	// Output flags - Turn off output processing
	//
	// no CR to NL translation, no NL to CR-NL translation,
	// no NL to CR translation, no column 0 CR suppression,
	// no Ctrl-D suppression, no fill characters, no case mapping,
	// no local output processing
	//
	// config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
	//                     ONOCR | ONOEOT| OFILL | OLCUC | OPOST);
	uart_config.c_oflag = 0;

	//
	// No line processing
	//
	// echo off, echo newline off, canonical mode off,
	// extended input processing off, signal chars off
	//
	uart_config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

	/* no parity, one stop bit */
	uart_config.c_cflag &= ~(CSTOPB | PARENB);

	/* set baud rate */
	if ((termios_state = cfsetispeed(&uart_config, speed)) < 0) {
		warnx("ERR: %d (cfsetispeed)\n", termios_state);
		return -1;
	}

	if ((termios_state = cfsetospeed(&uart_config, speed)) < 0) {
		warnx("ERR: %d (cfsetospeed)\n", termios_state);
		return -1;
	}

	if ((termios_state = tcsetattr(fd, TCSANOW, &uart_config)) < 0) {
		warnx("ERR: %d (tcsetattr)\n", termios_state);
		return -1;
	}

#endif
	return 0;
}

int
GPSHelper::pollOrRead(int fd, uint8_t *buf, size_t buf_length, uint64_t timeout)
{
	/* check for new messages. Note that we assume poll_or_read is called with a higher frequency
	 * than we get new injection messages.
	 */
	handleInjectDataTopic();

#ifndef __PX4_QURT

	/* For non QURT, use the usual polling. */

	pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events = POLLIN;

	/* Poll for new data,  */
	int ret = poll(fds, sizeof(fds) / sizeof(fds[0]), timeout);

	if (ret > 0) {
		/* if we have new data from GPS, go handle it */
		if (fds[0].revents & POLLIN) {
			/*
			 * We are here because poll says there is some data, so this
			 * won't block even on a blocking device. But don't read immediately
			 * by 1-2 bytes, wait for some more data to save expensive read() calls.
			 * If more bytes are available, we'll go back to poll() again.
			 */
			usleep(GPS_WAIT_BEFORE_READ * 1000);
			return ::read(fd, buf, buf_length);

		} else {
			return -1;
		}

	} else {
		return ret;
	}

#else
	/* For QURT, just use read for now, since this doesn't block, we need to slow it down
	 * just a bit. */
	usleep(10000);
	return ::read(fd, buf, buf_length);
#endif
}

void GPSHelper::handleInjectDataTopic()
{
	if (_orb_inject_data_fd[0] == -1) {
		return;
	}

	bool updated = false;
	int orb_inject_data_cur_fd = _orb_inject_data_fd[_orb_inject_data_next];
	orb_check(orb_inject_data_cur_fd, &updated);

	if (updated) {
		struct gps_inject_data_s msg;
		orb_copy(ORB_ID(gps_inject_data), orb_inject_data_cur_fd, &msg);
		injectData(msg.data, msg.len);
		_orb_inject_data_next = (_orb_inject_data_next + 1) % _orb_inject_data_fd.size();

	}
}
