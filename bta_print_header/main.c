/*
 * This file is part of the btaprinthdr project.
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

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "bta_print.h"
#include "bta_shdata.h"

static pid_t childpid;
static glob_pars *G = NULL;

void signals(int signo){
    signal(signo, SIG_IGN);
    if(childpid){ // master process
        if(signo == SIGUSR1){ // kill child
            kill(childpid, signo);
            signal(signo, signals);
            return;
        }
        WARNX("Master killed with sig=%d", signo);
        if(G && G->pidfile) unlink(G->pidfile);
    }
    exit(signo);
}

int main(int argc, char **argv){
    char *self = strdup(argv[0]);
    initial_setup();
    G = parse_args(argc, argv);
    if(!G->outfile){
        ERRX("Point output file name");
    }
    if(G->refresh < 0.1 || G->refresh > 30.){
        ERRX("Refresh rate should be from 0.1 to 30 seconds");
    }
    FILE *f = fopen(G->outfile, "w");
    if(!f) ERRX("Can't create file %s", G->outfile);
    fclose(f); unlink(G->outfile);
    check4running(self, G->pidfile);
    signal(SIGINT, signals);
    signal(SIGQUIT, signals);
    signal(SIGABRT, signals);
    signal(SIGTERM, signals);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, signals);
#ifndef EBUG
    unsigned int pause = 5;
    while(1){
        childpid = fork();
        if(childpid){ // master
            double t0 = dtime();
            wait(NULL);
            if(dtime() - t0 < 1.) pause += 5;
            else pause = 1;
            if(pause > 900) pause = 900;
            sleep(pause); // wait a little before respawn
        }else{ // slave
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            break;
        }
    }
#endif
    if(!get_shm_block(&sdat, ClientSide)){
        ERRX("BTA daemon isn't running?");
    }
    unsigned s = (unsigned)G->refresh;
    useconds_t us = (G->refresh - s)* 1e6;
    while(1){
        if(!check_shm_block(&sdat)) return 1;
        print_header(G->outfile);
        if(s) sleep(s);
        usleep(us);
    }
    return 0;
}

