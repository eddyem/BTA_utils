/*
 * This file is part of the canserver project.
 * Copyright 2025 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include "canproto.h"

#define CANIDIN         1
#define CANIDOUT        2

typedef struct{
    uint16_t ID;        // message ID
    uint32_t timestamp; // milliseconds timestamp
    uint8_t length;     // data length
    uint8_t data[8];    // up to 8 bytes of data
} CAN_message;

int parseCANstr(const char *str, CAN_message *msg);
int formCANmsg(CAN_message *msg, char *buf, int buflen);

int CANu32setter(int cancmd, uint8_t par, uint32_t data, CAN_message *msg);
int CANu32getter(int cancmd, uint8_t par, CAN_message *msg);
