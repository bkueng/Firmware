/****************************************************************************
 *
 *   Copyright (c) 2013-2016 PX4 Development Team. All rights reserved.
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

/**
 * @file MulticopterLandDetector.h
 * Land detection implementation for multicopters.
 *
 * @author Johan Jansen <jnsn.johan@gmail.com>
 * @author Morten Lysgaard <morten@lysgaard.no>
 * @author Julian Oes <julian@oes.ch>
 */

#pragma once

#include "LandDetector.h"

#include <systemlib/param/param.h>
#include <uORB/topics/vehicle_local_position.h>
#include <uORB/topics/vehicle_local_position_setpoint.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/actuator_controls.h>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/actuator_controls.h>
#include <uORB/topics/battery_status.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/sensor_bias.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/vehicle_local_position.h>

namespace land_detector
{

class MulticopterLandDetector : public LandDetector
{
public:
	MulticopterLandDetector();

protected:
	virtual void _initialize_topics() override;

	virtual void _update_params() override;

	virtual void _update_topics() override;

	virtual bool _get_landed_state() override;

	virtual bool _get_ground_contact_state() override;

	virtual bool _get_maybe_landed_state() override;

	virtual bool _get_freefall_state() override;

	virtual float _get_max_altitude() override;
private:

	/** Time in us that landing conditions have to hold before triggering a land. */
	static constexpr uint64_t land_detector_trigger_time_us = 300000;

	/** Time in us that almost landing conditions have to hold before triggering almost landed . */
	static constexpr uint64_t maybe_land_detector_trigger_time_us = 250000;

	/** Time in us that ground contact condition have to hold before triggering contact ground */
	static constexpr uint64_t ground_contact_trigger_time_us = 350000;

	/** Time interval in us in which wider acceptance thresholds are used after landed. */
	static constexpr uint64_t land_detector_land_phase_time_us = 2000000;

	/**
	* @brief Handles for interesting parameters
	**/
	struct {
		param_t maxClimbRate;
		param_t maxVelocity;
		param_t maxRotation;
		param_t minThrottle;
		param_t hoverThrottle;
		param_t throttleRange;
		param_t minManThrottle;
		param_t freefall_acc_threshold;
		param_t freefall_trigger_time;
		param_t altitude_max;
		param_t landSpeed;
	} _paramHandle;

	struct {
		float maxClimbRate;
		float maxVelocity;
		float maxRotation_rad_s;
		float minThrottle;
		float hoverThrottle;
		float throttleRange;
		float minManThrottle;
		float freefall_acc_threshold;
		float freefall_trigger_time;
		float altitude_max;
		float landSpeed;
	} _params;

	int _vehicleLocalPositionSub;
	int _vehicleLocalPositionSetpointSub;
	int _actuatorsSub;
	int _armingSub;
	int _attitudeSub;
	int _sensor_bias_sub;
	int _vehicle_control_mode_sub;
	int _battery_sub;

	struct vehicle_local_position_s				_vehicleLocalPosition;
	struct vehicle_local_position_setpoint_s	_vehicleLocalPositionSetpoint;
	struct actuator_controls_s					_actuators;
	struct actuator_armed_s						_arming;
	struct vehicle_attitude_s					_vehicleAttitude;
	struct sensor_bias_s					_sensors;
	struct vehicle_control_mode_s				_control_mode;
	struct battery_status_s						_battery;

	uint64_t _min_trust_start;		///< timestamp when minimum trust was applied first
	uint64_t _landed_time;

	/* get control mode dependent pilot throttle threshold with which we should quit landed state and take off */
	float getTakeoffThrottle();
	bool hasAltitudeLock();
	bool hasPositionLock();
	bool hasMinimalThrust();
	bool hasLowThrust();
	bool isClimbRateEnabled();
};


} // namespace land_detector
