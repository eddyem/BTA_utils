/*
 * This file is part of the sslsosk project.
 * Copyright 2023 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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
#include <sys/prctl.h>      // prctl
#include <sys/wait.h>       // wait
#include <unistd.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "daemon.h"
#include "sslsock.h"

static pid_t childpid = -1;
static double tstart = 0.; // time of fork()

void signals(int sig){
    int savelogs = (sl_dtime() - tstart > 30.) ? TRUE : FALSE;
    if(childpid == 0){
        if(savelogs) LOGWARN("Child killed with sig=%d", sig);
        exit(sig); // slave process
    }
    // master process
    if(sig){
        signal(sig, SIG_IGN);
        if(savelogs) LOGERR("Exit with signal %d", sig);
    }else if(savelogs) LOGERR("Exit");
    if(G.pidfile) unlink(G.pidfile);
    exit(sig);
}

/**
 * @brief start_daemon - daemonize
 * @return error code or 0
 */
int start_daemon(){
    FNAME();
    // check args
    int port = atoi(G.port);
    if(port < 1024 || port > 65535){
        LOGERR("Wrong port value: %d", port);
        return 1;
    }
    FILE *f = fopen(G.cert, "r");
    if(!f) ERR("Can't open certificate file %s", G.cert);
    fclose(f);
    f = fopen(G.key, "r");
    if(!f) ERR("Can't open certificate key file %s", G.key);
    fclose(f);
#ifdef EBUG
    printf("cert: %s, key: %s\n", G.cert, G.key);
#endif
#ifdef CLIENT
    //DBG("server: %s", G.serverhost);
    if(!G.serverhost) ERRX("Point server name");
#endif
    if(G.logfile){
        int lvl = LOGLEVEL_WARN + G.verbose;
        DBG("level = %d", lvl);
        if(lvl > LOGLEVEL_ANY) lvl = LOGLEVEL_ANY;
        green("Log file %s @ level %d\n", G.logfile, lvl);
        OPENLOG(G.logfile, lvl, 1);
    }
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    LOGMSG("Started");
#ifndef EBUG
    sl_check4running(NULL, G.pidfile);
    int savelogs = TRUE;
    tstart = sl_dtime();
    while(1){
        childpid = fork();
        if(childpid){ // master
            if(savelogs) LOGMSG("Created child with pid %d", childpid);
            wait(NULL);
            if(savelogs) LOGWARN("Child %d died", childpid);
            double twork = sl_dtime() - tstart;
            if(twork < 30.) savelogs = FALSE; // too fast respawn
            else{
                const char* Tstr[] = {"seconds", "minutes", "hours", "days"};
                double Tw;
                int idx;
                if(twork < 60.){ Tw = twork; idx = 0; }
                else if(twork < 3600.){ Tw = twork / 60.; idx = 1; }
                else if(twork < 86400.){ Tw = twork / 3600.; idx = 2; }
                else{ Tw = twork / 86400.; idx = 3; }
                LOGMSG("Child %d works for %.1f %s", childpid, Tw, Tstr[idx]);
                savelogs = TRUE;
            }
            sleep(2); // wait a little before respawn
            tstart = sl_dtime();
        }else{ // slave
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            break;
        }
    }
#endif
    // parent should never reach this part of code
    return open_socket();
}
