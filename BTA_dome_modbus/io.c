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

#include <modbus/modbus.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "io.h"


static modbus_t *modbus_ctx = NULL;

// last state of relays and inputs
static uint32_t RelaysState = 0, InputsState = 0;
// user requests to On/Off relay
static uint32_t OnFlags = 0, OffFlags = 0;
// last ADC buffer state
static uint16_t ADCBuf[NADCCHNLS];
// mutex for flags & states
static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;

// array of available relay numbers
static uint8_t RelaysAvailable[NRELAYS] = {
    [0] = 1,
    [1] = 1,
    [2] = 1,
    [3] = 1,
    [4] = 1,
    [5] = 1,
    [6] = 1,
    [7] = 1,
    [8] = 0,
    [9] = 0,
    [10] = 1,
    [11] = 1,
    [12] = 0,
    [13] = 0,
    [14] = 0,
    [15] = 0,
};

// == TRUE if something changed in buffers (reset in `io_read_inputs`)
//static uint8_t GotNew = FALSE, GotNewADC = FALSE;

// share modbus_t opened in motors.c
void io_set_ctx(modbus_t *ctx){
    DBG("Get context");
    modbus_ctx = ctx;
}

static int change_relay(int N, int state){
    if(!modbus_ctx || N < 0 || N >= NRELAYS || !RelaysAvailable[N]) return FALSE;
    int ret = FALSE;
    if(1 == modbus_write_bit(modbus_ctx, N, state)) ret = TRUE;
    else WARN("Can't set bit %d to %d", N, state);
    return ret;
}

//#define RON(relayNo)   do{if((RelaysState & (1<<relayNo)) == 0) change_relay(relayNo, 1);}while(0)
//#define ROFF(relayNo)  do{if((RelaysState & (1<<relayNo))) change_relay(relayNo, 0);}while(0)
#define RON(relayNo)   do{change_relay(relayNo, 1);}while(0)
#define ROFF(relayNo)  do{change_relay(relayNo, 0);}while(0)

// check inputs and set/reset relays according to them
static void chk_inputs(){
    int man_at_nest = FALSE;
    // unlock door & lock motors when man on the "nest"
    if((InputsState & (INP_FLOOR1 | INP_FLOOR2 | INP_DOOR_OPENED))){
        //DBG("Man on the \"nest\" (state: %04X, mask: %04X)", InputsState, (INP_FLOOR1 | INP_FLOOR2 | INP_DOOR_OPENED));
        ROFF(RELAY_BLOCK_DOOR);
        RON(RELAY_DOOR_UNLOCKED);
        RON(RELAY_BLOCK_MOTORS);
        RON(RELAY_LIGHT);
        man_at_nest = TRUE;
    }
    // unlock door when telescope is @ nest
    if(InputsState & INP_TEL_AT_NEST){
        //DBG("Telescope at the \"nest\"");
        ROFF(RELAY_BLOCK_DOOR);
        RON(RELAY_DOOR_UNLOCKED);
        // allow to open the door
        if(InputsState & INP_OPEN_DOOR){
            RON(RELAY_OPEN_DOOR);
            RON(RELAY_BLOCK_MOTORS);
        }else{
            ROFF(RELAY_OPEN_DOOR);
        }
    }else{
        if(man_at_nest){ // somebody @ nest when there's no telescope there!
            if(InputsState & INP_OPEN_DOOR){
                RON(RELAY_OPEN_DOOR);
            }else{
                ROFF(RELAY_OPEN_DOOR);
            }
        }else{
            ROFF(RELAY_OPEN_DOOR);
            RON(RELAY_BLOCK_DOOR);
            ROFF(RELAY_DOOR_UNLOCKED);
        }
    }
}

static void chk_relays(){
    // if motors are locked turn on light
    if(RelaysState & (1 << RELAY_BLOCK_MOTORS)){
        //DBG("Motors locked -> turn on light");
        RON(RELAY_LIGHT);
    }else{ // turn off light
        //DBG("Motors unlocked -> turn off light");
        ROFF(RELAY_LIGHT);
    }
    // check user flags
    pthread_mutex_lock(&sync_mutex);
    if(OnFlags){
        uint32_t flag = 1;
        for(int i = 0; i < NRELAYS; ++i, flag <<= 1){
            if(OnFlags & flag) RON(i);
        }
        OnFlags = 0;
    }
    if(OffFlags){
        uint32_t flag = 1;
        for(int i = 0; i < NRELAYS; ++i, flag <<= 1){
            if(OffFlags & flag) ROFF(i);
        }
        OffFlags = 0;
    }
    pthread_mutex_unlock(&sync_mutex);
}

// process reading relays & inputs state
void io_process(){
    uint8_t buf[NRELAYS + NINPUTS];
    uint16_t adcbuf[NADCCHNLS];
    uint32_t flag = 1, rstate = 0, istate = 0;

    if(!modbus_ctx || G.io_ID < 1) return;
    if(-1 == modbus_set_slave(modbus_ctx, G.io_ID)){
        WARN("Can't set IO slave ID to %d", G.io_ID);
        return;
    }

    if(-1 == modbus_read_input_bits(modbus_ctx, 0, NINPUTS, buf)){
        WARN("Can't read inputs");
        return;
    }
    for(int i = 0; i < NINPUTS; ++i, flag <<= 1){
        if(buf[i]) istate |= flag;
#ifdef EBUG
        int oldstate = (InputsState & flag) ? 1 : 0;
        if(buf[i] == oldstate) continue;
        if(buf[i]) printf("Input %d is Active\n", i);
        else printf("Input %d is Inactive\n", i);
        fflush(stdout);
#endif
        //GotNew = TRUE;
    }
    //DBG("Inputs old/new: 0x%04X / 0x%04X", InputsState, istate);
    pthread_mutex_lock(&sync_mutex);
    InputsState = istate;
    pthread_mutex_unlock(&sync_mutex);
    // check inputs before reading relays' state
    chk_inputs();

    if(-1 == modbus_read_bits(modbus_ctx, 0, NRELAYS, buf)){
        WARN("Can't read relay state");
        return;
    }
    flag = 1;
    for(int i = 0; i < NRELAYS; ++i, flag <<= 1){
        if(buf[i]) rstate |= flag;
#ifdef EBUG
        int oldstate = (RelaysState & flag) ? 1 : 0;
        if(buf[i] == oldstate) continue;
        if(buf[i]) printf("Relay %d is ON\n", i);
        else printf("Relay %d is OFF\n", i);
        fflush(stdout);
#endif
        // GotNew = TRUE;
    }
    //DBG("Relays old/new: 0x%04X / 0x%04X", RelaysState, rstate);
    chk_relays();

    if(-1 == modbus_read_input_registers(modbus_ctx, 0, NADCCHNLS, adcbuf)){
        WARN("Can't read ADC inputs");
        return;
    }
#ifdef EBUG
    for(int i = 0; i < NADCCHNLS; ++i){
        if(abs(adcbuf[i] - ADCBuf[i]) < ADCTHRESH) continue;
        // GotNewADC = TRUE; break;

        printf("ADC input %d changed to %d\n", i, adcbuf[i]);
        fflush(stdout);
    }
#endif
    pthread_mutex_lock(&sync_mutex);
    RelaysState = rstate;
    memcpy(ADCBuf, adcbuf, sizeof(uint16_t) * NADCCHNLS);
    pthread_mutex_unlock(&sync_mutex);
}

// get relays state
void io_read_relays(uint32_t *state){
    if(!state) return;
    pthread_mutex_lock(&sync_mutex);
    *state = RelaysState;
    pthread_mutex_unlock(&sync_mutex);
}

// get inputs state
void io_read_inputs(uint32_t *state){
    if(!state) return;
    pthread_mutex_lock(&sync_mutex);
    //int got = GotNew;
    //GotNew = FALSE;
    *state = InputsState;
    pthread_mutex_unlock(&sync_mutex);
    //return got;
}

int io_read_adc(int nch, uint16_t *val){
    if(!val || nch < 0 || nch >= NADCCHNLS) return FALSE;
    pthread_mutex_lock(&sync_mutex);
    /*int got = GotNewADC;
    GotNewADC = FALSE;*/
    *val = ADCBuf[nch];
    pthread_mutex_unlock(&sync_mutex);
    return TRUE;
}

// set relay with given number
int io_relay_on(int N){
    if(N >= 0 && N < NRELAYS && RelaysAvailable[N]){
        pthread_mutex_lock(&sync_mutex);
        OnFlags |= 1 << N;
        OffFlags &= ~(1 << N);
        pthread_mutex_unlock(&sync_mutex);
        return TRUE;
    }
    return FALSE;
}

// reset relay with given number
int io_relay_off(int N){
    if(N >= 0 && N < NRELAYS && RelaysAvailable[N]){
        pthread_mutex_lock(&sync_mutex);
        OffFlags |= 1 << N;
        OnFlags &= ~(1 << N);
        pthread_mutex_unlock(&sync_mutex);
        return TRUE;
    }
    return FALSE;
}

void io_set_relays(uint32_t flags){
    pthread_mutex_lock(&sync_mutex);
    OnFlags = flags;
    OffFlags = ~flags;
    pthread_mutex_unlock(&sync_mutex);
}
