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

#define MAX_SPEED       (700.)
#define MAX_CURRENT     (15.)
#define MOTORS_AMOUNT   (10)
// low, medium and high speeds
#define LSpeed    71
#define MSpeed    350
#define HSpeed    610

// ID of first motor minus 1
#define START_ID        (0)
// ID of motor (n=1..MOTORS_AMOUNT)
#define MOTOR_ID(n)     (n + START_ID)

// motors' status
enum{
    MOT_OFF,
    MOT_SLEEP,
    MOT_RUN,
    MOT_ERROR
};

typedef struct{
    int status;
    double speed;
    double current;
} motor_state_t;

void motors_stop();
double motors_get_curntsetpoint();
int motors_set_curntsetpoint(double val);
double motors_get_speedsetpoint();
int motors_set_speedsetpoint(double val);
int motors_get_activenum();
int motors_set_activenum(int N);
int motors_get_actcurrent(double *val);
int motors_get_actspeed(double *val);
int motors_get_actstatus(int *val);

void modbus_close();
