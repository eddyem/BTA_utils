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

#include <stdint.h>
#include <modbus/modbus.h>

#define NRELAYS     16
#define NINPUTS     16
#define NADCCHNLS   8
// ADC level threshold: 50 units
#define ADCTHRESH   50

// INPUTS:
#define INP_FLOOR1          0x0001
#define INP_FLOOR2          0x0002
#define INP_DOOR_OPENED     0x0004
#define INP_OPEN_DOOR       0x0008
#define INP_TEL_AT_NEST     0x0010
//#define INP_

// RELAYS:
#define RELAY_OPEN_DOOR     1
#define RELAY_BLOCK_DOOR    2
#define RELAY_LIGHT         3
#define RELAY_POWER         4
#define RELAY_BLOCK_MOTORS  5
#define RELAY_DOOR_UNLOCKED 10
//#define RELAY_

void io_set_ctx(modbus_t *ctx);
void io_process();

void io_read_inputs(uint32_t *state);
int io_read_adc(int nch, uint16_t *val);
void io_read_relays(uint32_t *state);

int io_relay_on(int N);
int io_relay_off(int N);
void io_set_relays(uint32_t flags);
