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

#ifndef STR
#define SSS_(x)   #x
#define STR(x)  SSS_(x)
#endif

// Handlers in this list MUST be in sortered order (by name)!!!
#define HANDLERS_LIST() \
NEW_HANDLER(motcurrent, "maximal motor current") \
NEW_HANDLER(motnum, "active motor number for status requests") \
NEW_HANDLER(motspeed, "motor speed") \
NEW_HANDLER(motstatus, "motor status") \
NEW_HANDLER(speed, "speed setter") \
NEW_HANDLER(stop, "stop motors") \

/*NEW_HANDLER(current, "current setter") \*/
/*NEW_HANDLER(relay, "relay command") \*/
