/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: jflyper
 */

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "platform.h"

#ifdef USE_DSHOT

#include "build/atomic.h"

#include "common/maths.h"
#include "common/time.h"

#include "config/feature.h"

#include "drivers/motor.h"
#include "drivers/timer.h"

#include "drivers/dshot.h"
#include "drivers/dshot_dpwm.h" // for motorDmaOutput_t, should be gone
#include "drivers/dshot_command.h"
#include "drivers/nvic.h"
#include "drivers/pwm_output.h" // for PWM_TYPE_* and others

#include "fc/rc_controls.h" // for flight3DConfig_t

#include "pg/motor.h"

#include "rx/rx.h"

void dshotInitEndpoints(float outputLimit, float *outputLow, float *outputHigh, float *disarm, float *deadbandMotor3dHigh, float *deadbandMotor3dLow) {
    float outputLimitOffset = (DSHOT_MAX_THROTTLE - DSHOT_MIN_THROTTLE) * (1 - outputLimit);
    *disarm = DSHOT_CMD_MOTOR_STOP;
    if (featureIsEnabled(FEATURE_3D)) {
        *outputLow = DSHOT_MIN_THROTTLE + ((DSHOT_3D_FORWARD_MIN_THROTTLE - 1 - DSHOT_MIN_THROTTLE) / 100.0f) * CONVERT_PARAMETER_TO_PERCENT(motorConfig()->digitalIdleOffsetValue);
        *outputHigh = DSHOT_MAX_THROTTLE - outputLimitOffset / 2;
        *deadbandMotor3dHigh = DSHOT_3D_FORWARD_MIN_THROTTLE + ((DSHOT_MAX_THROTTLE - DSHOT_3D_FORWARD_MIN_THROTTLE) / 100.0f) * CONVERT_PARAMETER_TO_PERCENT(motorConfig()->digitalIdleOffsetValue);
        *deadbandMotor3dLow = DSHOT_3D_FORWARD_MIN_THROTTLE - 1 - outputLimitOffset / 2;
    } else {
        *outputLow = DSHOT_MIN_THROTTLE + ((DSHOT_MAX_THROTTLE - DSHOT_MIN_THROTTLE) / 100.0f) * CONVERT_PARAMETER_TO_PERCENT(motorConfig()->digitalIdleOffsetValue);
        *outputHigh = DSHOT_MAX_THROTTLE - outputLimitOffset;
    }
}

float dshotConvertFromExternal(uint16_t externalValue)
{
    uint16_t motorValue;

    externalValue = constrain(externalValue, PWM_RANGE_MIN, PWM_RANGE_MAX);

    if (featureIsEnabled(FEATURE_3D)) {
        if (externalValue == PWM_RANGE_MIDDLE) {
            motorValue = DSHOT_CMD_MOTOR_STOP;
        } else if (externalValue < PWM_RANGE_MIDDLE) {
            motorValue = scaleRange(externalValue, PWM_RANGE_MIN, PWM_RANGE_MIDDLE - 1, DSHOT_3D_FORWARD_MIN_THROTTLE - 1, DSHOT_MIN_THROTTLE);
        } else {
            motorValue = scaleRange(externalValue, PWM_RANGE_MIDDLE + 1, PWM_RANGE_MAX, DSHOT_3D_FORWARD_MIN_THROTTLE, DSHOT_MAX_THROTTLE);
        }
    } else {
        motorValue = (externalValue == PWM_RANGE_MIN) ? DSHOT_CMD_MOTOR_STOP : scaleRange(externalValue, PWM_RANGE_MIN + 1, PWM_RANGE_MAX, DSHOT_MIN_THROTTLE, DSHOT_MAX_THROTTLE);
    }

    return (float)motorValue;
}

uint16_t dshotConvertToExternal(float motorValue)
{
    uint16_t externalValue;

    if (featureIsEnabled(FEATURE_3D)) {
        if (motorValue == DSHOT_CMD_MOTOR_STOP || motorValue < DSHOT_MIN_THROTTLE) {
            externalValue = PWM_RANGE_MIDDLE;
        } else if (motorValue <= DSHOT_3D_FORWARD_MIN_THROTTLE - 1) {
            externalValue = scaleRange(motorValue, DSHOT_MIN_THROTTLE, DSHOT_3D_FORWARD_MIN_THROTTLE - 1, PWM_RANGE_MIDDLE - 1, PWM_RANGE_MIN);
        } else {
            externalValue = scaleRange(motorValue, DSHOT_3D_FORWARD_MIN_THROTTLE, DSHOT_MAX_THROTTLE, PWM_RANGE_MIDDLE + 1, PWM_RANGE_MAX);
        }
    } else {
        externalValue = (motorValue < DSHOT_MIN_THROTTLE) ? PWM_RANGE_MIN : scaleRange(motorValue, DSHOT_MIN_THROTTLE, DSHOT_MAX_THROTTLE, PWM_RANGE_MIN + 1, PWM_RANGE_MAX);
    }

    return externalValue;
}

FAST_CODE uint16_t prepareDshotPacket(dshotProtocolControl_t *pcb)
{   
    uint16_t packet;

    ATOMIC_BLOCK(NVIC_PRIO_DSHOT_DMA) {
        packet = (pcb->value << 1) | (pcb->requestTelemetry ? 1 : 0);
        pcb->requestTelemetry = false;    // reset telemetry request to make sure it's triggered only once in a row
    }

    // compute checksum
    unsigned csum = 0; 
    unsigned csum_data = packet;
    for (int i = 0; i < 3; i++) {
        csum ^=  csum_data;   // xor data by nibbles
        csum_data >>= 4;
    }
    // append checksum
#ifdef USE_DSHOT_TELEMETRY 
    if (useDshotTelemetry) {
        csum = ~csum;
    }
#endif
    csum &= 0xf;
    packet = (packet << 4) | csum;

    return packet;
}
#endif // USE_DSHOT
