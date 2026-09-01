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
#include <modbus/modbus.h>
#include <usefull_macros.h>

#include "motors.h"
#include "esq770.h"

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

static modbus_t *modbus_ctx = NULL;
static motor_state_t motstates[MOTORS_AMOUNT] = {0};

// set points
static double currentSet = DEFAULT_CURRENT, speedSet = 0.;
// flags for main routine
// TODO: add mutex or make flags atomic
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

// close modbus connection
static void motors_close_m(){
    if(modbus_ctx){
        modbus_close(modbus_ctx);
        modbus_free(modbus_ctx);
        modbus_ctx = NULL;
    }
}
static void motors_close_e(){ // stub for emulation mode
    ;
}

// open modbus @ given speed; return FALSE if failed
static int motors_open_m(const char *path, int speed){
    if(speed < 1200 || !path){
        WARNX("Point path and right speed");
        return FALSE;
    }
    if(modbus_ctx) motors_close_m();
    modbus_ctx = modbus_new_rtu(path, speed, 'N', 8, 1);
    if(!modbus_ctx){
        WARNX("Can't open device %s @ %d", path, speed);
        LOGERR("Can't open device %s @ %d", path, speed);
        return FALSE;
    }
    modbus_set_response_timeout(modbus_ctx, 0, MODBUS_RESPONCE_TIMEOUT); // response timeout
    if(modbus_connect(modbus_ctx) < 0){
        WARNX("Can't connect to device %s", path);
        LOGERR("Can't connect to device %s", path);
        motors_close_m();
        return FALSE;
    }
    return TRUE;
}
static int motors_open_e(const char _U_ *path, int _U_ speed){ // stub for emulation mode
    return TRUE;
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
    //DBG("Set active motor=%d", N);
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
    static int curN = 0, errctr = 0;
    static int error_cnt[MOTORS_AMOUNT] = {0};
    if(!modbus_ctx || errctr > 100){
        LOGERR("Modbus not inited or other error!");
        ERRX("Modbus not inited or other error!");
    }
    if(flags.all){
        if(-1 == modbus_set_slave(modbus_ctx, 0)) goto reg_error;
        if(flags.stop){
            DBG("User asks to stop");
            speedSet = 0.;
            // send command stop
            if(-1 == modbus_write_register(modbus_ctx, REG_CMD, CMD_STOP)) goto reg_error;
            flags.stop = 0;
        }
        if(flags.change_current){
            DBG("User asks to change current to %g", currentSet);
            // TODO: send command "max current"?
            flags.change_current = 0;
        }
        if(flags.change_speed){
            DBG("User asks to change speed to %g", speedSet);
            // send command "set speed"
            uint16_t dir = (speedSet > 0.) ? CMD_FORWARD : CMD_REVERSE;
            uint16_t freq = (uint16_t)(fabs(speedSet) * FREQ_SCALE);
            if(-1 == modbus_write_register(modbus_ctx, REG_CMD, dir)) goto reg_error;
            if(-1 == modbus_write_register(modbus_ctx, REG_FREQ_SET, freq)) goto reg_error;
            flags.change_speed = 0;
        }
    }
    // set slave N
    if(-1 == modbus_set_slave(modbus_ctx, MOTOR_ID(curN))) goto reg_error;
    // ask for speed/status/current
    uint16_t regs[2];
    if(-1 == modbus_read_registers(modbus_ctx, REG_STATUS_MAIN, 2, regs)){
        if((++error_cnt[curN]) > MAX_ERRORS){ // not answer - set MOT_OFF status
            motstates[curN].status = MOT_OFF;
            LOGDBG("Motor %d not responce", curN);
        }
        if(++curN >= MOTORS_AMOUNT) curN = 0;
        return;
    }
    error_cnt[curN] = 0;
    switch(regs[0]){
        case STATUS_CW:
        case STATUS_CCW:
            motstates[curN].status = MOT_RUN;
            break;
        case STATUS_STOP:
            motstates[curN].status = MOT_SLEEP;
            break;
        default:
            motstates[curN].status = MOT_ERROR;
    }
    if(regs[1] & STATUS_READY_MASK) motstates[curN].status = MOT_ERROR;
    if(motstates[curN].status == MOT_ERROR) modbus_write_register(modbus_ctx, REG_CMD, CMD_RESET_FAULT);
    if(-1 == modbus_read_registers(modbus_ctx, REG_OUTPUT_FREQ, 1, &regs[0])) regs[0] = 0;
    if(-1 == modbus_read_registers(modbus_ctx, REG_OUTPUT_CURRENT, 1, &regs[1])) regs[1] = 0;
    motstates[curN].speed = ((double)regs[0]) / FREQ_SCALE;
    motstates[curN].current = ((double)regs[1]) / CURRENT_SCALE;
    if(++curN >= MOTORS_AMOUNT) curN = 0;
    errctr = 0;
    return;
reg_error:
    ++errctr;
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

int (*motors_open)(const char *, int) = motors_open_m;
void (*motors_close)() = motors_close_m;
void (*motors_process)() = motors_process_m;

void set_emulation_mode(){
    LOGMSG("Set emulation mode");
    motors_open =  motors_open_e;
    motors_close = motors_close_e;
    motors_process = motors_process_e;
}
