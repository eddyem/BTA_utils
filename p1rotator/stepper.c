/*
 * stepper.c - functions for working with stepper motors by wiringPi
 *
 * Copyright 2015 Edward V. Emelianoff <eddy@sao.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <stdio.h>          // printf, getchar, fopen, perror
#include <stdlib.h>         // exit
#include <signal.h>         // signal
#include <time.h>           // time
#include <string.h>         // memcpy
#include <stdint.h>         // int types
#include <sys/time.h>       // gettimeofday
#include <math.h>           // fabs, round
#include <limits.h>         // std types
#include <unistd.h>         // usleep
#include <pthread.h>        // threads
// use wiringPi on ARM & simple echo on PC (for tests)
#ifdef __arm__
    #include <wiringPi.h>
#endif

#include "config.h"
#include "stepper.h"
#include "usefull_macros.h"
#include "bta_shdata.h"

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

#define R2D  (180./M_PI)     // rad. to degr.
#define D2R  (M_PI/180.)     // degr. to rad.
#define R2S  (648000./M_PI)  // rad. to sec
#define S2R  (M_PI/648000.)  // sec. to rad.
#define S360 (1296000.)    // sec in 360degr

// By google maps: 43.646683 (43 38 48.0588), 41.440681 (41 26 26.4516)
// (real coordinates should be measured relative to mass center, not geoid)
//static const double longitude = 149189.175;   // SAO longitude 41 26 29.175 (-2:45:45.945)
//static const double Fi = 157152.7;            // SAO latitude 43 39 12.7
static const double cos_fi = 0.7235272793;    // Cos of SAO latitude
static const double sin_fi = 0.6902957888;    // Sin  ---  ""  -----

// microsteps counter
#ifdef __arm__
static int32_t absusteps = USTEPSBAD; // rotation in both directions relative to zero
#endif // __arm__

// stop all threads @ exit
//static volatile int force_exit = 0;

#define getpval() (absusteps * PA_MINSTEP)

double calc_PA(double alpha, double delta, double stime){
    double sin_t,cos_t, sin_d,cos_d;
    double t, d, p, sp, cp;

    t = (stime - alpha) * 15.;
    if (t < 0.)
        t += S360;      // +360degr
    t *= S2R;          // -> rad
    d = delta * S2R;
    sincos(t, &sin_t, &cos_t);
    sincos(d, &sin_d, &cos_d);

    sp = sin_t * cos_fi;
    cp = sin_fi * cos_d - sin_d * cos_fi * cos_t;
    p = atan2(sp, cp);
    if (p < 0.0)
        p += 2.0*M_PI;
    return(p * R2D);
}

void print_PA(double ang){
    int d, m;
    printf("PA: %g degr == ", ang);
    d = (int)ang;
    ang = (ang - d) * 60.;
    m = (int)ang;
    ang = (ang - m) * 60.;
    printf("%02d:%02d:%02.1f\n", d, m, ang);
}

#ifdef __arm__
static void Write(int pin, int val){
    if(val) val = 1;
    digitalWrite(pin, val);
    while(digitalRead(pin) != val);
}
static void Toggle(int pin){
    int v = digitalRead(pin);
    Write(pin, !v);
}
#endif // __arm__

void setup_pins(){
#ifdef __arm__
    wiringPiSetupGpio();
    Write(EN_PIN, PIN_PASSIVE); // disable all @ start
    Write(DIR_PIN, PIN_PASSIVE);
    Write(STEP_PIN, PIN_PASSIVE);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(ESW_PIN, INPUT);
    pullUpDnControl(ESW_PIN, PUD_UP);
#else // __arm__
    green("Setup GPIO\n");
#endif // __arm__
}

/**
 * Disable stepper motor
 */
void stop_motor(){
//    force_exit = 1;
    usleep(1000);
#ifdef __arm__
    // disable motor & all other
    pullUpDnControl(ESW_PIN, PUD_OFF);
    pinMode(DIR_PIN, INPUT);
    pinMode(EN_PIN, INPUT);
    pinMode(STEP_PIN, INPUT);
    // return values to initial state
    Write(EN_PIN, 0);
    Write(DIR_PIN, 0);
    Write(STEP_PIN, 0);
    DBG("STOPPED");
#else
    green("Stop Stepper\n");
#endif // __arm__
}

// make pause for dt seconds
void mk_pause(double dt){
    int nfd;
    struct timeval tv;
    tv.tv_sec = (int)dt;
    tv.tv_usec = (int)((dt - tv.tv_sec)*1000000.);
slipping:
    nfd = select(0, (fd_set *)NULL,(fd_set *)NULL,(fd_set *)NULL, &tv);
    if(nfd < 0){
        if(errno == EINTR) goto slipping;
        WARN("select()");
    }
}

/**
 * Move motor with max speed for nusteps microsteps
 */
static void move_motor(int nusteps){
    if(nusteps == 0) return;
    int dir = 1;
    if(nusteps < 0){
        dir = -1;
        nusteps = -nusteps;
    }
#ifdef __arm__
    Write(DIR_PIN, (dir > 0) ? DIR_POSITIVE : DIR_NEGATIVE); // prepare direction
    for(; nusteps; --nusteps){
        Toggle(STEP_PIN);
        mk_pause(USTEP_DELAY);
        absusteps += dir;
    }
#else // __arm__
    green("Move motor to %c%g steps\n", (dir > 0) ? '+':'-', (double)nusteps/USTEPS);
#endif // __arm__
}

// go to zero end-switch. Return 0 if all OK
int gotozero(){
#ifdef __arm__
    int nusteps = ONETURN_USTEPS * 1.1;
    Write(DIR_PIN, DIR_NEGATIVE);
    for(; nusteps; --nusteps){
        Toggle(STEP_PIN);
        mk_pause(USTEP_DELAY);
        if(digitalRead(ESW_PIN) == ESW_ACTIVE){
            DBG("ESW");
            absusteps = 0;
            return 0;
        }
    }
    // didn't catch the end-switch
    return 1;
#else // __arm__
    green("Go to zero\n");
    return 0;
#endif // __arm__
}

// go to given angle (degrees). Return 0 if catch zero-endswitch
int gotoangle(double pa){
    if(pa > 360. || pa < -360){
        int x = pa / 360.;
        pa -= x*360.;
    }
    if(pa > 180.) pa -= 360.; // the shortest way
    DBG("Rotate to %gdegr", pa);
    int nusteps = pa / PA_MINSTEP;
    move_motor(nusteps);
    return 0;
}

#if 0
/**
 * Main thread for steppers management
 */
static void *steppers_thread(_U_ void *buf){
    DBG("steppers_thr");
    //double starting_pa_value = CALC_PA(); // starting PA for convert angle into steps
    // difference in steps === (target_pa_value - starting_pa_value)/PA_MINSTEP
#ifdef __arm__
    double laststeptime, curtime;
    halfsteptime = 1. / (stepspersec * 8.);
    DBG("halfsteptime: %g", halfsteptime);
    laststeptime = dtime();
    int eswsteps = 0;
    while(!force_exit){
        while(target_pa_period < 0.); // no rotation
        // check rotation direction
        double current_pa_value = ;
        if(target_pa_value)
        if((curtime = dtime()) - laststeptime > halfsteptime + corrtime){
            Write(STEP_PIN, (++i)%2);
            laststeptime = curtime;
            ++nusteps;
            if(nusteps%10 == 0){
                double have = curtime - t0, need, delt;
                int x = stepspersec ? stepspersec : 1;
                if(x > 0) need = (double)nusteps/USTEPS/2./x;
                else need = (double)nusteps/USTEPS/2.*(-x);
                delt = have - need;
                if(fabs(delt) > fabs(olddelt)){
                    corrtime -= delt/20.;
                }else{
                    corrtime -= delt/100.;
                }
                olddelt = delt;
            }
        }
    }
#else // __arm__
    green("Main steppers' thread\n");
    while(!force_exit){
        usleep(500);
    }
#endif // __arm__
    DBG("exit motors_thr");
    return NULL;
}
#endif // 0

void stepper_process(){
    DBG("Main thread");
/*    pthread_t motor_thread;
    if(pthread_create(&motor_thread, NULL, steppers_thread, NULL)){
        ERR(_("Can't run motor thread"));
    }*/
    // target motor speed & position
    double target_pa_period = USTEP_DELAY; // max speed
    int target_usteps = 0, current_usteps = 0, dir = 0;
    double p_first = CALC_PA(); // initial PA value & value for speed calculation
    double T_last = dtime();
    green("Starting PA value: ");
    print_PA(p_first);
    DBG("minstep: %g == %g'", PA_MINSTEP, PA_MINSTEP*60.);
    double curtime = T_last, laststeptime = T_last;
    //while(!force_exit){ // === while(1)
    while(1){
        #ifndef EBUG
        // don't rotate corrector in non-tracking modes
        if(Sys_Mode != SysTrkOk){
            usleep(300000);
            DBG("Mode: %d", Sys_Mode);
            continue;
        }
        #endif
        target_usteps = (CALC_PA() - p_first)/PA_MINSTEP;
        if(target_usteps == current_usteps){ // no rotation
            continue;
        }
        curtime = dtime();
        if(curtime - T_last > 1.){ // recalculate speed
            target_pa_period = (curtime - T_last)/fabs(target_usteps - current_usteps)/2. - USTEP_DELAY;
            if(target_pa_period < USTEP_DELAY) target_pa_period = USTEP_DELAY; // max speed
            T_last = curtime;
            green("Current period: %g seconds. Steps: need=%d, curr=%d\n", target_pa_period, target_usteps, current_usteps);
        }
        // check rotation direction
        if(target_usteps > current_usteps){
            if(dir != 1){ // change direction
                DBG("Change rotation to positive");
                dir = 1;
                #ifdef __arm__
                Write(DIR_PIN, DIR_POSITIVE);
                #endif // __arm__
            }
        }else{
            if(dir != -1){ // change direction
                DBG("Change rotation to negative");
                dir = -1;
                #ifdef __arm__
                Write(DIR_PIN, DIR_NEGATIVE);
                #endif // __arm__
            }
        }
        if(curtime - laststeptime > target_pa_period){
            #ifdef __arm__
            Toggle(STEP_PIN);
            #endif // __arm__
            current_usteps += dir;
            DBG("STEP");
        }
        print_PA(CALC_PA());
    }
}

/*
#ifdef __arm__
#else // __arm__
#endif // __arm__
*/
