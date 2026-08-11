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
#define SPEED_TOLERANCE (0.01)
// emulation acceleration, min^-2
#define EMUL_ACCEL      (20000.)
#define MAX_CURRENT     (15.)
#define DEFAULT_CURRENT (10.)
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

double motors_get_curntsetpoint();
int motors_set_curntsetpoint(double);

double motors_get_speedsetpoint();
int motors_set_speedsetpoint(double);

int motors_get_activenum();
int motors_set_activenum(int);

void motors_stop();
int motors_get_actcurrent(double*);
int motors_get_actspeed(double*);
int motors_get_actstatus(int*);

extern void (*motors_process)();

extern int (*modbus_open)(const char *, int);
extern void (*modbus_close)();

void set_emulation_mode();
