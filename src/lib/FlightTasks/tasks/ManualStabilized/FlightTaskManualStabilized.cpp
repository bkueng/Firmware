/****************************************************************************
 *
 *   Copyright (c) 2018 PX4 Development Team. All rights reserved.
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
 * @file FlightManualStabilized.cpp
 */

#include "FlightTaskManualStabilized.hpp"
#include <mathlib/mathlib.h>
#include <float.h>

using namespace matrix;

bool FlightTaskManualStabilized::activate()
{
	bool ret = FlightTaskManual::activate();
	_thrust_setpoint = matrix::Vector3f(0.0f, 0.0f, -_throttle_hover.get());
	_yaw_setpoint = _yaw;
	_yaw_offset_prev = 0.f;
	_yawspeed_setpoint = 0.0f;
	_constraints.tilt = math::radians(_tilt_max_man.get());
	return ret;
}

bool FlightTaskManualStabilized::updateInitialize()
{
	bool ret = FlightTaskManual::updateInitialize();
	// need a valid yaw-state
	return ret && PX4_ISFINITE(_yaw);
}

void FlightTaskManualStabilized::_scaleSticks()
{
	/* Scale sticks to yaw and thrust using
	 * linear scale for yaw and piecewise linear map for thrust. */
	_yawspeed_setpoint = _sticks_expo(3) * math::radians(_yaw_rate_scaling.get());
	_throttle = _throttleCurve();
}

void FlightTaskManualStabilized::_updateHeadingSetpoints()
{
	/* Yaw-lock depends on stick input. If not locked, _yaw_locked is set to current yaw.
	 *
	 * We always set a valid _yaw_setpoint because it's used and possibly changed later on
	 * (if we were to set it to NAN, the position controller would set it to _yaw anyway).
	 */
	if (fabsf(_yawspeed_setpoint) > FLT_EPSILON) {
		/* no fixed heading when rotating around yaw by stick */
		_yaw_setpoint = _yaw - _yaw_offset_prev;

	} else {
		/* hold the current heading when no more rotation commanded */
		_yaw_setpoint -= _yaw_offset_prev;

		/* check reset counter and update yaw setpoint if necessary */
		if (_sub_attitude->get().quat_reset_counter != _heading_reset_counter) {
			_yaw_setpoint += matrix::Eulerf(matrix::Quatf(_sub_attitude->get().delta_q_reset)).psi();
			_heading_reset_counter = _sub_attitude->get().quat_reset_counter;
		}
	}

	/* check if an external yaw handler is active and if yes, let it compute the yaw setpoints */
	if (_ext_yaw_handler != nullptr && _ext_yaw_handler->is_active()) {
		_yaw_setpoint = _yaw - _yaw_offset_prev;
		_yawspeed_setpoint += _ext_yaw_handler->get_weathervane_yawrate();
	}

	_yaw_offset_prev = 0.f;
}

void FlightTaskManualStabilized::_updateThrustSetpoints()
{
	Vector2f sp(&_sticks(0));

	/* Ensure that maximum tilt is in [0.001, Pi] */
	float tilt_max = math::constrain(_constraints.tilt, 0.001f, M_PI_F);

	const float x = sp(0) * tilt_max;
	const float y = sp(1) * tilt_max;

	/* The norm of the xy stick input provides the pointing
	 * direction of the horizontal desired thrust setpoint. The magnitude of the
	 * xy stick inputs represents the desired tilt. Both tilt and magnitude can
	 * be captured through Axis-Angle.
	 *
	 * The Axis-Angle is the perpendicular vector to xy-stick input */
	Vector2f v = Vector2f(y, -x);
	float v_norm = v.norm(); // the norm of v defines the tilt angle

	if (v_norm > tilt_max) { // limit to the configured maximum tilt angle
		v *= tilt_max / v_norm;
	}

	/* The thrust setpoint is found by rotating the scaled unit vector pointing
	 * upward by the Axis-Angle.
	 * Make sure that the attitude can be controlled even at 0 throttle.
	 */
	Quatf q_sp = AxisAnglef(v(0), v(1), 0.0f);
	_thrust_setpoint = q_sp.conjugate(Vector3f(0.0f, 0.0f, -1.0f)) * math::max(_throttle, 0.0001f);

	/* The thrust setpoint is expected to be in the heading of the yaw setpoint, so rotate it */
	matrix::Quaternionf q = matrix::AxisAnglef(matrix::Vector3f(0.f, 0.f, 1.f), _yaw_setpoint);
	_thrust_setpoint = q.conjugate(_thrust_setpoint);


	static bool prev_yaw_offset = false;
	bool use_yaw_offset = _sub_manual_control_setpoint->get().aux1 > 0.5f;

	if (use_yaw_offset != prev_yaw_offset) {
		prev_yaw_offset = use_yaw_offset;
		PX4_WARN("using yaw offset: %i", (int)use_yaw_offset);
	}


	/* The Axis-Angle setpoint contains a yaw-component: we add it to the yaw setpoint as it leads to a
	 * more intuitive flight behavior (best seen at higher tilt angles around 60+ deg).
	 * This yaw component is 0 for roll or pitch stick input == 0. It is maximal for |roll| == |pitch|.
	 * For roll input == pitch input it is given by atan(1/cos(tilt_angle)) - pi/4,
	 * so about 26 degrees for a tilt angle of 70 degrees.
	 */
	const float yaw_offset = matrix::Eulerf(q_sp)(2);

	if (use_yaw_offset) {
		_yaw_setpoint += yaw_offset;
		_yaw_offset_prev = yaw_offset;
	}

}

void FlightTaskManualStabilized::_rotateIntoHeadingFrame(Vector2f &v)
{
	float yaw_rotate = PX4_ISFINITE(_yaw_setpoint) ? _yaw_setpoint : _yaw;
	Vector3f v_r = Vector3f(Dcmf(Eulerf(0.0f, 0.0f, yaw_rotate)) * Vector3f(v(0), v(1), 0.0f));
	v(0) = v_r(0);
	v(1) = v_r(1);
}

void FlightTaskManualStabilized::_updateSetpoints()
{
	_updateHeadingSetpoints();
	_updateThrustSetpoints();
}

float FlightTaskManualStabilized::_throttleCurve()
{
	/* Scale stick z from [-1,1] to [min thrust, max thrust]
	 * with hover throttle at 0.5 stick */
	float throttle = -((_sticks(2) - 1.0f) * 0.5f);

	if (throttle < 0.5f) {
		return (_throttle_hover.get() - _throttle_min_stabilized.get()) / 0.5f * throttle + _throttle_min_stabilized.get();

	} else {
		return (_throttle_max.get() - _throttle_hover.get()) / 0.5f * (throttle - 1.0f) + _throttle_max.get();
	}
}

bool FlightTaskManualStabilized::update()
{
	_scaleSticks();
	_updateSetpoints();

	return true;
}
