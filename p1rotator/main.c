/*
 * main.c - main file
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
#include <termios.h>        // tcsetattr
#include <unistd.h>         // tcsetattr, close, read, write
#include <sys/ioctl.h>      // ioctl
#include <stdio.h>          // printf, getchar, fopen, perror
#include <stdlib.h>         // exit
#include <sys/stat.h>       // read
#include <fcntl.h>          // read
#include <signal.h>         // signal
#include <time.h>           // time
#include <string.h>         // memcpy
#include <stdint.h>         // int types
#include <sys/time.h>       // gettimeofday
#include <assert.h>         // assert
//#include <pthread.h>        // threads
#include <math.h>

#include "config.h"
#include "tmout.h"
#include "bta_shdata.h"
#include "cmdlnopts.h"
#include "usefull_macros.h"
#include "ch4run.h"
#include "stepper.h"

#ifndef PIDFILE
#define PIDFILE  "/tmp/p1move.pid"
#endif

#define BUFLEN 1024

glob_pars *Global_parameters = NULL;

void signals(int sig){
    if(sig > 0)
        WARNX(_("Get signal %d, quit.\n"), sig);
    unlink(PIDFILE);
    stop_motor();
    exit(sig);
}

int main(int argc, char *argv[]){
    check4running(PIDFILE, NULL);
    initial_setup();
    Global_parameters = parse_args(argc, argv);
    assert(Global_parameters);
    if(!get_shm_block(&sdat, ClientSide) || !check_shm_block(&sdat)){
        ERRX("Can't get SHM block!");
    }
    #ifndef EBUG
    PRINT(_("Test multicast connection\n"));
    double last = M_time;
    WAIT_EVENT((fabs(M_time - last) > 0.02), 5.);
    if(tmout && fabs(M_time - last) < 4.)
        ERRX(_("Multicasts stale!"));
    #endif

    signal(SIGTERM, signals); // kill (-15)
    signal(SIGHUP, SIG_IGN);  // hup - daemon
    signal(SIGINT, signals);  // ctrl+C
    signal(SIGQUIT, signals); // ctrl+\   .
    signal(SIGTSTP, SIG_IGN); // ctrl+Z
    setbuf(stdout, NULL);

    setup_pins();
    if(Global_parameters->absmove && gotozero()) ERRX(_("Can't locate zero-endswitch"));

    if(Global_parameters->gotoangle > -360. && Global_parameters->gotoangle < 360.){
        if(Global_parameters->absmove) Global_parameters->gotoangle += PA_ZEROVAL;
        if(gotoangle(Global_parameters->gotoangle)) ERRX(_("Can't move for given angle"));
    }

    stepper_process();
    return 0;
}
