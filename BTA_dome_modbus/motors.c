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

static motor_state_t motstates[MOTORS_AMOUNT] = {0};

// set points
static double currentSet = DEFAULT_CURRENT, speedSet = 0.;
// flags for main routine
static union{
    struct{
        uint32_t change_speed : 1;
        uint32_t change_current : 1;
        uint32_t stop : 1;
    };
    uint32_t all;
} flags = {0};

// emulation mode parameters
/*static struct{
    double tlast;

} emulpar = {0};*/

// current motor for status etc. getters (0..MOTORS_AMOUNT-1)
static int motindex = 0;

// open modbus @ given speed; return FALSE if failed
static int modbus_open_m(const char _U_ *path, int _U_ speed){
    return FALSE;
}
static int modbus_open_e(const char _U_ *path, int _U_ speed){
    return TRUE;
}

// close modbus connection
static void modbus_close_m(){
    ;
}
static void modbus_close_e(){
    ;
}


// stop all
void motors_stop(){
    flags.stop = 1;
}

// current setpoint getter
double motors_get_curntsetpoint(){
    return currentSet;
}
// set setpoint of current
int motors_set_curntsetpoint(double val){
    if(val < 0. || val > MAX_CURRENT) return FALSE;
    DBG("Change max current to %g", val);
    currentSet = val;
    flags.change_current = 1;
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
    DBG("Change speed setpoint to %g", val);
    speedSet = val;
    flags.change_speed = 1;
    return TRUE;
}

// get number of active motor
int motors_get_activenum(){ return motindex; }
// set number of active motor
int motors_set_activenum(int N){
    DBG("Set active motor=%d", N);
    if(N < 0 || N >= MOTORS_AMOUNT) return FALSE;
    motindex = N;
    return TRUE;
}

// get current value for active motor
int motors_get_actcurrent(double *val){
    if(val) *val = motstates[motindex].current;
    return TRUE;
}

// get speed for active motor
int motors_get_actspeed(double *val){
    if(val) *val = motstates[motindex].speed;
    return TRUE;
}

// get status for active motor
int motors_get_actstatus(int *val){
    if(val) *val = motstates[motindex].status;
    return TRUE;
}

// main motors processing routine
static void motors_process_m(){
    ;
}
static void motors_process_e(){
    static double t0 = -1., curspeed = 0.;
    double curt = sl_dtime(), dt = curt - t0, curcurrent = 0.;
    int curstatus = motstates[0].status;
    if(t0 < 0.){
        t0 = curt;
        // init state ("turn motors on")
        for(int i = 0; i < MOTORS_AMOUNT; ++i) motstates[i].status = MOT_SLEEP;
        return;
    }
    if(flags.all){
        if(flags.stop){
            speedSet = 0.;
            if(curstatus != MOT_RUN) curstatus = MOT_SLEEP;
        }
        flags.all = 0;
    }
    if(fabs(curspeed - speedSet) > SPEED_TOLERANCE){ // need to calculate new speed value
        double acceleration = (curspeed < speedSet) ? EMUL_ACCEL : -EMUL_ACCEL;
        double newspeed = curspeed + acceleration * dt / 60.; // acceleration in min^{-2}
        if(acceleration > 0.){
            if(newspeed > speedSet) newspeed = speedSet;
        }else{
            if(newspeed < speedSet) newspeed = speedSet;
        }
        curspeed = newspeed;
        curcurrent = currentSet;
        curstatus = MOT_RUN;
        DBG("Now speed = %g", curspeed);
    }else{
        if(fabs(curspeed) < SPEED_TOLERANCE) curstatus = MOT_SLEEP; // stopped
        else curcurrent = currentSet * 0.6;
    }
    t0 = curt;
    for(int i = 0; i < MOTORS_AMOUNT; ++i){
        motstates[i].status = curstatus;
        motstates[i].current = curcurrent;
        motstates[i].speed = curspeed;
    }
}

int (*modbus_open)(const char *, int) = modbus_open_m;
void (*modbus_close)() = modbus_close_m;
void (*motors_process)() = motors_process_m;

void set_emulation_mode(){
    LOGMSG("Set emulation mode");
    modbus_open =  modbus_open_e;
    modbus_close = modbus_close_e;
    motors_process = motors_process_e;
}
