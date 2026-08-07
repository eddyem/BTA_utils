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

#include <math.h>
#include <usefull_macros.h>

#include "motors.h"

#if 0
Протокол (s - сеттер, g - getteer):
relay=xxx - (sg) команда реле
motnum=xx - (sg) получить сведения о двигателе с номером хх
(текущее состояние):
motstatus=xx - (g) состояние мотора
motspeed=xx - (g) реальная скорость
motcurrent=xx - (g) реальный ток
(для всех моторов):
speed=xx - (sg) уставка скорости
current=xx - (sg) уставка тока
#endif

// set points
static double currentSet = 0., speedSet = 0.;
// current motor for status etc. getters
static int motindex = 0;

// stop all
void motors_stop(){
    ;
}

// close modbus connection
void modbus_close(){
    ;
}

// current setpoint getter
double motors_get_curntsetpoint(){
    return currentSet;
}
// set setpoint of current
int motors_set_curntsetpoint(double val){
    if(val < 0. || val > MAX_CURRENT) return FALSE;
    // do something
    return TRUE;
}

// get setpoint of speed
double motors_get_speedsetpoint(){
    return speedSet;
}
// set setpoint of speed
int motors_set_speedsetpoint(double val){
    double absval = fabs(val);
    if(absval > MAX_SPEED) return FALSE;
    // do something
    return TRUE;
}

// get number of active motor
int motors_get_activenum(){ return motindex; }
// set number of active motor
int motors_set_activenum(int N){
    if(N < 1 || N > MOTORS_AMOUNT) return FALSE;
    motindex = N;
    return TRUE;
}

// get current value for active motor
int motors_get_actcurrent(double *val){
    if(val) *val = 0.;
    return TRUE;
}

// get speed for active motor
int motors_get_actspeed(double *val){
    if(val) *val = 0.;
    return TRUE;
}

// get status for active motor
int motors_get_actstatus(int *val){
    if(val) *val = 0;
    return TRUE;
}
