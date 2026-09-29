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

#include <modbus/modbus.h>

#define NRELAYS     16
#define NINPUTS     16
#define NADCCHNLS   8
// ADC level threshold: 50 units
#define ADCTHRESH   50

void io_set_ctx(modbus_t *ctx);
void io_process();

int io_read_inputs(char inputs[NINPUTS]);
int io_read_adc(int nch, uint16_t *val);
void io_read_relays(char relays[NRELAYS]);

int io_relay_on(int N);
int io_relay_off(int N);
int io_set_relays(int N);
