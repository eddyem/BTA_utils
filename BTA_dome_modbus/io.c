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

static uint8_t RelayBuf[NRELAYS];
static uint8_t InputsBuf[NINPUTS];
static uint16_t ADCBuf[NADCCHNLS];
static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;

// == TRUE if something changed in buffers (reset in `io_read_inputs`)
static uint8_t GotNew = FALSE;//, GotNewADC = FALSE;

// share modbus_t opened in motors.c
void io_set_ctx(modbus_t *ctx){
    DBG("Get context");
    pthread_mutex_lock(&sync_mutex);
    modbus_ctx = ctx;
    pthread_mutex_unlock(&sync_mutex);
}

// process reading relays & inputs state
void io_process(){
    uint8_t buf[NRELAYS + NINPUTS];
    uint16_t adcbuf[NADCCHNLS];
    if(!modbus_ctx || G.io_ID < 1) return;
    if(-1 == modbus_set_slave(modbus_ctx, G.io_ID)){
        WARN("Can't set IO slave ID to %d", G.io_ID);
        return;
    }
    pthread_mutex_lock(&sync_mutex);
    if(-1 == modbus_read_bits(modbus_ctx, 0, NRELAYS, buf)){
        WARN("Can't read relay state");
        goto ret;
    }
    for(int i = 0; i < NRELAYS; ++i){
        if(buf[i] == RelayBuf[i]) continue;
#ifdef EBUG
        if(buf[i]) printf("Relay %d is ON\n", i);
        else printf("Relay %d is OFF\n", i);
        fflush(stdout);
#endif
        RelayBuf[i] = buf[i];
        GotNew = TRUE;
    }
    if(-1 == modbus_read_input_bits(modbus_ctx, 0, NINPUTS, buf)){
        WARN("Can't read inputs");
        goto ret;
    }
    for(int i = 0; i < NINPUTS; ++i){
        if(buf[i] == InputsBuf[i]) continue;
#ifdef EBUG
        if(buf[i]) printf("Input %d is Active\n", i);
        else printf("Input %d is Inactive\n", i);
        fflush(stdout);
#endif
        InputsBuf[i] = buf[i];
        GotNew = TRUE;
    }
    if(-1 == modbus_read_input_registers(modbus_ctx, 0, NADCCHNLS, adcbuf)){
        WARN("Can't read ADC inputs");
        goto ret;
    }
    for(int i = 0; i < NADCCHNLS; ++i){
        if(abs(adcbuf[i] - ADCBuf[i]) < ADCTHRESH) continue;
        // GotNewADC = TRUE; break;
#ifdef EBUG
        printf("ADC input %d changed to %d\n", i, adcbuf[i]);
        fflush(stdout);
#endif
    }
    memcpy(ADCBuf, adcbuf, sizeof(uint16_t) * NADCCHNLS);
ret:
    pthread_mutex_unlock(&sync_mutex);
}

// get inputs state
int io_read_inputs(char inputs[NINPUTS]){
    pthread_mutex_lock(&sync_mutex);
    int got = GotNew;
    GotNew = FALSE;
    memcpy(inputs, InputsBuf, NINPUTS);
    pthread_mutex_unlock(&sync_mutex);
    return got;
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

// get relays state
void io_read_relays(char relays[NRELAYS]){
    pthread_mutex_lock(&sync_mutex);
    memcpy(relays, RelayBuf, NRELAYS);
    pthread_mutex_unlock(&sync_mutex);
}

static int change_relay(int N, int state){
    if(!modbus_ctx || N < 0 || N >= NRELAYS) return FALSE;
    int ret = FALSE;
    pthread_mutex_lock(&sync_mutex);
    if(1 == modbus_write_bit(modbus_ctx, N, state)) ret = TRUE;
    else WARN("Can't set bit %d to %d", N, state);
    pthread_mutex_unlock(&sync_mutex);
    return ret;
}

// set relay with given number
int io_relay_on(int N){
    return change_relay(N, 1);
}

// reset relay with given number
int io_relay_off(int N){
    return change_relay(N, 0);
}

int io_set_relays(int N){
    if(!modbus_ctx || N < 0) return FALSE;
    int ret = FALSE;
    uint8_t bits[NRELAYS];
    for(int i = 0; i < NRELAYS; ++i){
        if(N & (1 << i)) bits[i] = 1;
        else bits[i] = 0;
    }
    pthread_mutex_lock(&sync_mutex);
    if(1 == modbus_write_bits(modbus_ctx, 0, NRELAYS, bits)) ret = TRUE;
    else WARN("Can't write bits %d", N);
    pthread_mutex_unlock(&sync_mutex);
    return ret;
}
