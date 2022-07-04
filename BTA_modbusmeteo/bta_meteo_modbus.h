/*
 * This file is part of the bta_meteo_modbus project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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
#ifndef BTA_METEO_MODBUS_H__
#define BTA_METEO_MODBUS_H__

// data reading timeout (in seconds)
#define METEO_TIMEOUT   15

typedef  enum{
    ANS_OK,         // all OK
    ANS_LOSTCONN,   // lost connection
    ANS_NODATA,     // no data yet
//    ANS_NOTFULL     // got not full dataset (or nothing at all)
} params_ans;

int connect2tty();

params_ans check_meteo_params();

#endif // BTA_METEO_MODBUS_H__
