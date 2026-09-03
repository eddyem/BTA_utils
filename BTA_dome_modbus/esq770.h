/*
 * This file is part of the sslsosk project.
 * Copyright 2026 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

// Registers and their fields for ESQ-770 frequency converter

// command register (rw)
#define REG_CMD                 0x2000  // 8192
// set frequency (rw)
#define REG_FREQ_SET            0x3000  // 12288
// torque limit (rw)
#define REG_TORQUE_LIMIT        0x3006  // 12294
#define REG_BRAKE_TORQUE_LIMIT  0x3007  // 12295
// status (r)
#define REG_STATUS_MAIN         0x6000  // 24576
#define REG_STATUS_EXT          0x6001  // 24577
// monitoring (r)
#define REG_OUTPUT_FREQ         0xA201  // 41473 (freq * 100)
#define REG_OUTPUT_CURRENT      0xA204  // 41476 (current * 10)
#define REG_MOTOR_SPEED         0xA205  // 41477 (speed, depends on settings)

// REG_CMD bits
#define CMD_FORWARD             1
#define CMD_REVERSE             2
#define CMD_STOP                5
#define CMD_FAST_STOP           6
#define CMD_RESET_FAULT         7

// REG_STATUS_MAIN bits
#define STATUS_CW               1
#define STATUS_CCW              2
#define STATUS_STOP             3
#define STATUS_ERR              4
#define STATUS_LOW_V            5

// REG_STATUS_EXT bit
#define STATUS_READY_MASK       0x01

// convert rev/min to frequency register value and back
// 100 * revmin / 60 * 4; 100 - scale, 60 - min2sec, 4 - pole amount
#define REVMIN2FREQ(r)          (uint16_t)((100. * (r)) / 15.)
#define FREQ2REVMIN(f)          ((((double)(f)) * 15.) / 100.)
#define CURRENT_SCALE           10.

