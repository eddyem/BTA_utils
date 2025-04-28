/*
 * This file is part of the canserver project.
 * Copyright 2025 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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
#include <usefull_macros.h>

#include "clientserver.h"
#include "globopts.h"

static sl_sock_t *srv = NULL, *clt = NULL;

void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
        LOGERR("Exit with status %d", sig);
    }else LOGERR("Exit");
    if(srv){
        DBG("Del server");
        sl_sock_delete(&srv);
    }
    if(clt){
        DBG("Del client");
        sl_sock_delete(&clt);
    }
    exit(sig);
}


int main(int argc, char **argv){
    sl_init();
    parseargs(argc, argv);
    if(!UserOpts.srvnode) ERRX("Point server node");
    if(!UserOpts.cltnode) ERRX("Point serial client node");
    sl_socktype_e type = (UserOpts.srvunix) ? SOCKT_UNIX : SOCKT_NET;
    sl_sock_changemaxclients(UserOpts.maxclients);
    srv = RunSrv(type, UserOpts.srvnode);
    if(!srv) ERRX("Server: can't create socket and/or run threads");
    DBG("Server done");
    type = (UserOpts.cltunix) ? SOCKT_UNIX : SOCKT_NET;
    clt = RunClt(type, UserOpts.cltnode);
    if(!clt) ERRX("Serial client: can't connect to socket and/or run threads");
    DBG("Client done");
    sl_loglevel_e lvl = UserOpts.verbose + LOGLEVEL_ERR;
    if(lvl >= LOGLEVEL_AMOUNT) lvl = LOGLEVEL_AMOUNT - 1;
    if(UserOpts.logfile) OPENLOG(UserOpts.logfile, lvl, 1);
    LOGMSG("Started");
    signal(SIGTERM, signals);
    signal(SIGINT, signals);
    signal(SIGQUIT, signals);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGHUP, signals);
    while(srv && srv->connected && clt && clt->connected){
        if(!srv->rthread){
            WARNX("Server handlers thread is dead");
            LOGERR("Server handlers thread is dead");
            break;
        }
    }
    LOGMSG("End");
    DBG("Close");
    if(srv) sl_sock_delete(&srv);
    if(clt) sl_sock_delete(&clt);
    return 0;
}
