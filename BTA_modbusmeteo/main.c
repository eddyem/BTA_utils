/*
 * This file is part of the bta_meteo_modbus project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <sys/prctl.h> // prctl
#include <sys/wait.h>  // wait
#include <time.h>

#include "usefull_macros.h"
#include "bta_shdata.h"
#include "bta_meteo_modbus.h"

int gotsegm = 0; // used also in bta_meteo_modbus.c
static pid_t childpid = 0;

void clear_flags(){
    WARNX("Clear flags");
    LOG("Clear flags");
    if(!gotsegm) return;
    if(MeteoMode & NET_HMD){ // humidity now isn't from net
       MeteoMode &= ~NET_HMD;
       if(MeteoMode & SENSOR_HMD)
           MeteoMode &= ~SENSOR_HMD;
    }
    if(MeteoMode & NET_WND){ // wind
       MeteoMode &= ~NET_WND;
       if(MeteoMode & SENSOR_WND)
           MeteoMode &= ~SENSOR_WND;
    }
    if(MeteoMode & NET_B){ // pressure
       MeteoMode &= ~NET_B;
       if(MeteoMode & SENSOR_B)
           MeteoMode &= ~SENSOR_B;
    }
    if(MeteoMode & NET_T1){ // ext. temperature
       MeteoMode &= ~NET_T1;
       if(MeteoMode & SENSOR_T1)
           MeteoMode &= ~SENSOR_T1;
    }
}

void signals(int sig){
    signal(sig, SIG_IGN);
    switch(sig){
        default:
        case SIGHUP :
             LOG("%s - Ignore ...", strsignal(sig));
             fflush(stderr);
             signal(sig, signals);
             return;
        case SIGINT :
        case SIGPIPE:
        case SIGQUIT:
        case SIGFPE :
        case SIGSEGV:
        case SIGTERM:
             signal(SIGALRM, SIG_IGN);
             LOG("%s - Stop!", strsignal(sig));
             if(childpid){ // master process
                 clear_flags();
             }
             exit(sig);
    }
}

int main(int argc, char *argv[]){
    setenv("LC_MESSAGES", "C", 1);
    initial_setup();
    if(argc == 2){
        printf("Log file: %s\n", argv[1]);
        Cl_createlog(argv[1]);
    }
    signal(SIGHUP, signals);
    signal(SIGINT, signals);
    signal(SIGQUIT,signals);
    signal(SIGFPE, signals);
    signal(SIGPIPE,signals);
    signal(SIGSEGV,signals);
    signal(SIGTERM,signals);

    LOG("\nStarted\n");

    #ifndef EBUG
    if(daemon(1, 0)){
        ERR("daemon()");
    }
    while(1){ // guard for dead processes
        pid_t childpid = fork();
        if(childpid){
            LOG("create child with PID %d", childpid);
            wait(NULL);
            LOG("child %d died\n", childpid);
            sleep(1);
        }else{
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            break; // go out to normal functional
        }
    }
    #endif

    if(!connect2tty()){
        ERRX("Can't open TTY device");
        return 1; // never reached
    }

    time_t tlast = time(NULL);
    int errlogged = FALSE;
    while(1){
        if(!gotsegm){
            sdat.mode |= 0200;
            sdat.atflag = 0;
            gotsegm = get_shm_block(&sdat, ClientSide);
            if(!gotsegm){
                LOG("Can't find SHM segment");
            }else get_cmd_queue(&ocmd, ClientSide);
        }
        if(time(NULL) - tlast > 60){ // no signal for 5 minutes - clear flags
            if(!errlogged){
                LOG("5 minutes - no signal!");
                errlogged = TRUE;
            }
            tlast = time(NULL);
            clear_flags(); // return to Z1000 meteo
        }
        params_ans a = check_meteo_params();
        if(a == ANS_LOSTCONN){
            LOG("Lost connection with device, reconnect!");
            return 1;
        }
        if(a == ANS_OK){
            errlogged = FALSE;
            tlast = time(NULL);
        }
    }
    return 0;
}
