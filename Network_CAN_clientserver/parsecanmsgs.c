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

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "parsecanmsgs.h"


/**
 * @brief parseCANstr - parser of string like "timestamp #ID [data0 data1 ...]"
 * @param str - input string
 * @param msg - message to fill
 * @return FALSE if given string isn't CAN message
 */
int parseCANstr(const char *str, CAN_message *msg){
    if(!str || !msg) return FALSE;
    const char *IDstart = strchr(str, '#');
    if(!IDstart) return FALSE;
    ++IDstart;
    msg->timestamp = atoi(str);
    DBG("Timestamp: %u", msg->timestamp);
    unsigned bytes[9];
    int N = sscanf(IDstart, "%x %x %x %x %x %x %x %x %x",
            &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5], &bytes[6], &bytes[7], &bytes[8]);
    if(N < 1) return FALSE; // where's ID?
    DBG("ID=%u", bytes[0]);
    msg->length = N - 1;
    msg->ID = bytes[0];
    for(int i = 0; i < msg->length; ++i) msg->data[i] = bytes[i + 1];
    DBG("length: %u", msg->length);
    return TRUE;
}

// return result of snprintf
int formCANmsg(CAN_message *msg, char *buf, int buflen){
    if(!msg || !buf || buflen < 8) return 0;
    DBG("msg ID=%u, len=%u", msg->ID, msg->length);
    int L = snprintf(buf, buflen, "s %u ", msg->ID);
    for(int i = 0; i < msg->length && L < buflen; ++i)
        L += snprintf(buf + L, buflen - L, "%u ", msg->data[i]);
    if(L >= buflen){
        WARNX("buffer overflow");
        return 0;
    }
    L += snprintf(buf + L, buflen - L, "\n");
    DBG("convert CAN message to %d symbols: _%s_", L, buf);
    return L;
}

// fill command and parameter into message
// !!!!! msg->ID should be set by user !!!!!
static int fillcmd(int cancmd, uint8_t par, CAN_message *msg){
    if(!msg || cancmd > 0xffff) return FALSE;
    MSGP_SET_CMD(msg, cancmd);
    MSGP_SET_PARNO(msg, par);
    MSGP_SET_ERR(msg, 0);
    msg->length = 4;
    return TRUE;
}

int CANu32setter(int cancmd, uint8_t par, uint32_t data, CAN_message *msg){
    if(!msg) return FALSE;
    par |= SETTER_FLAG;
    if(!fillcmd(cancmd, par, msg)) return FALSE;
    MSGP_SET_U32(msg, data);
    msg->length = 8;
    return TRUE;
}

int CANu32getter(int cancmd, uint8_t par, CAN_message *msg){
    if(!msg) return FALSE;
    if(!fillcmd(cancmd,  par, msg)) return FALSE;
    return TRUE;
}
