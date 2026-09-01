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

// max errors per motor to mean it OFF
#define MAX_ERRORS      5

// freq_scale == 100, so for UINT16_t we can't make speed more than 655.35
#define MAX_SPEED       655.
#define SPEED_TOLERANCE 0.01
// emulation acceleration, min^-2
#define EMUL_ACCEL      10000.
#define MAX_CURRENT     15.
#define DEFAULT_CURRENT 10.
#define MOTORS_AMOUNT   10
// low, medium and high speeds
#define LSpeed          71
#define MSpeed          350
#define HSpeed          610

// modbus responce timeout, ms
#define MODBUS_RESPONCE_TIMEOUT 100000

// ID of first motor
#define START_ID        (1)
// ID of nth motor (n=0..MOTORS_AMOUNT-1 - index)
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

extern int (*motors_open)(const char *, int);
extern void (*motors_close)();

void set_emulation_mode();
