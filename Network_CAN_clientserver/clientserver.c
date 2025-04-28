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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "clientserver.h"
#include "parsecanmsgs.h"

static pthread_t clientthread;
sl_sock_t *serialsock = NULL;

#define CMDIN       "in"
#define CMDOUT      "out"
#define CMDMCUT     "mcut"
#define CMDLED      "led"
#define CMDINCH     "inchnls"
#define CMDOUTCH    "outchnls"
#define CMDTIME     "time"
#define CMDPING     "ping"

typedef struct{
    const char *textcmd;
    int cancmd;
    int havesetter;
} commands;

// list of all supported commands
static commands allcommands[] = {
    {CMDIN, CMD_GETESW, 0},
    {CMDOUT, CMD_RELAY, 1},
    {CMDMCUT, CMD_MCUTEMP, 0},
    {CMDLED, CMD_LED, 1},
    {CMDINCH, CMD_INCHNLS, 0},
    {CMDOUTCH, CMD_OUTCHNLS, 0},
    {CMDTIME, CMD_TIME, 1},
    {CMDPING, CMD_PING, 1},
    {NULL, 0, 0}
};

static int Relay1cmds(CAN_message *msg, char buf[BUFSIZ]){
    int L = 0;
    uint16_t cmd = MSGP_GET_CMD(msg);
    uint8_t par = PARVAL(msg->data);
    uint32_t data = MSGP_GET_U32(msg);
    commands *c = allcommands;
    while(c->textcmd){
        if(cmd == c->cancmd) break;
        ++c;
    }
    if(!c->textcmd) return 0;
    DBG("found text cmd is %s (%u)", c->textcmd, data);
    if(par == NO_PARNO) L = snprintf(buf, BUFSIZ-1, "%s=%u\n", c->textcmd, data);
    else L = snprintf(buf, BUFSIZ-1, "%s[%d]=%u\n", c->textcmd, par, data);
    return L;
}

// check CAN_message and if recognize send ans to all clients
static void gotCANans(CAN_message *msg){
    if(!msg) return;
    char buf[BUFSIZ];
    int L = 0;
    switch(msg->ID){
        case CANIDOUT:
            L = Relay1cmds(msg, buf);
        break;
        default:
            return;
    }
    if(L < 1) return;
    DBG("BUF: %s", buf);
    int N = sl_sock_sendall((uint8_t*) buf, L);
    green("Send to %d clients\n", N);
}

/**
 * @brief clientproc - process data received from serial terminal
 * @param par - socket
 * @return NULL
 */
void *clientproc(void _U_ *par){
    FNAME();
    char rbuf[BUFSIZ];
    if(!serialsock) return NULL;
    do{
        ssize_t got = sl_sock_readline(serialsock, rbuf, BUFSIZ);
        if(got < 0){ // disconnected
            WARNX("Serial server disconnected");
            if(serialsock) sl_sock_delete(&serialsock);
            return NULL;
        }else if(got == 0){ // nothing to read from serial port
            usleep(1000);
            continue;
        }
        // process data
        DBG("GOT: %s", rbuf);
        CAN_message msg;
        if(parseCANstr(rbuf, &msg)){
            DBG("Got CAN message");
            gotCANans(&msg);
        }
    } while(serialsock && serialsock->connected);
    WARNX("disconnected");
    if(serialsock) sl_sock_delete(&serialsock);
    return NULL;
}

static sl_sock_hresult_e dtimeh(sl_sock_t *client, _U_ sl_sock_hitem_t *item, _U_ const char *req){
    char buf[32];
    snprintf(buf, 31, "UNIXT=%.2f\n", sl_dtime());
    sl_sock_sendstrmessage(client, buf);
    return RESULT_SILENCE;
}

static sl_sock_hresult_e procUserCmd(sl_sock_t _U_ *client, sl_sock_hitem_t *item, const char *req){
#define CBUFLEN (128)
    char buf[CBUFLEN];
    int buflen = 0;
    CAN_message msg;
    msg.ID = CANIDIN;
    int issetter = 0;
    uint8_t parno = NO_PARNO;
    uint32_t setterval = 0;
    if(req){
        int N;
        if(!sl_str2i(&N, req) || N < 0){
            DBG("BAD value %d", N);
            return RESULT_BADVAL;
        }
        setterval = (uint32_t) N;
        issetter = 1;
    }
    commands *c = allcommands;
    while(c->textcmd){
        if(0 == strcmp(c->textcmd, item->key)) break;
        ++c;
    }
    if(!c->textcmd) return RESULT_BADKEY;
    if(issetter){
        DBG("setter");
        if(!c->havesetter) return RESULT_BADVAL;
        if(CANu32setter(c->cancmd, parno, setterval, &msg) && (buflen = formCANmsg(&msg, buf, CBUFLEN-1)) > 0){
        }else{
            WARNX("Can't form message");
            return RESULT_FAIL;
        }
    }else{
        DBG("getter");
        if(CANu32getter(c->cancmd, parno, &msg) && (buflen = formCANmsg(&msg, buf, CBUFLEN-1)) > 0){
        }else{
            WARNX("Can't form message");
            return RESULT_FAIL;
        }
    }
    if(buflen) sl_sock_sendstrmessage(serialsock, buf);
    return RESULT_SILENCE;
#undef CBUFLEN
}
/*
static sl_sock_hresult_e show(sl_sock_t *client, _U_ sl_sock_hitem_t *item, _U_ const char *req){
    if(!client) return RESULT_FAIL;
    if(client->type != SOCKT_UNIX){
        if(*client->IP){
            printf("Client \"%s\" (fd=%d) ask for flags:\n", client->IP, client->fd);
        }else printf("Can't get client's IP, flags:\n");
    }else printf("Socket fd=%d asks for flags:\n", client->fd);
    return RESULT_OK;
}*/

// Too much clients handler
static void toomuch(int fd){
    const char m[] = "Try later: too much clients connected\n";
    send(fd, m, sizeof(m)-1, MSG_NOSIGNAL);
    shutdown(fd, SHUT_WR);
    DBG("shutdown, wait");
    double t0 = sl_dtime();
    uint8_t buf[8];
    while(sl_dtime() - t0 < 11.){
        if(sl_canread(fd)){
            ssize_t got = read(fd, buf, 8);
            DBG("Got=%zd", got);
            if(got < 1) break;
        }
    }
    DBG("Disc after %gs", sl_dtime() - t0);
    LOGWARN("Client fd=%d tried to connect after MAX reached", fd);
}
// new connections handler
static void connected(sl_sock_t *c){
    if(c->type == SOCKT_UNIX) LOGMSG("New client fd=%d connected", c->fd);
    else LOGMSG("New client fd=%d, IP=%s connected", c->fd, c->IP);
}
// disconnected handler
static void disconnected(sl_sock_t *c){
    if(c->type == SOCKT_UNIX) LOGMSG("Disconnected client fd=%d", c->fd);
    else LOGMSG("Disconnected client fd=%d, IP=%s", c->fd, c->IP);
}

static sl_sock_hitem_t handlers[] = {
    {dtimeh, "dtime", "get server's UNIX time for all clients connected", NULL},
    {procUserCmd, CMDOUT, "outputs setter/getter", NULL},
    {procUserCmd, CMDIN, "inputs getter", NULL},
    {procUserCmd, CMDMCUT, "get MCU temperature (/10degC)", NULL},
    {procUserCmd, CMDLED, "onboard LED set/get", NULL},
    {procUserCmd, CMDINCH, "get available input channels mask", NULL},
    {procUserCmd, CMDOUTCH, "get available output channels mask", NULL},
    {procUserCmd, CMDTIME, "get MCU milliseconds counter value", NULL},
    {procUserCmd, CMDPING, "ping controller", NULL},
    {NULL, NULL, NULL, NULL}
};

sl_sock_t *RunSrv(sl_socktype_e type, const char *node){
    sl_sock_maxclhandler(toomuch);
    sl_sock_connhandler(connected);
    sl_sock_dischandler(disconnected);
    return sl_sock_run_server(type, node, 4096, handlers);
}

sl_sock_t *RunClt(sl_socktype_e type, const char *node){
    serialsock = sl_sock_run_client(type, node, 4096);
    if(!serialsock){
        DBG("Can't run client");
        return NULL;
    }
    if(pthread_create(&clientthread, NULL, clientproc, NULL)) return NULL;
    return serialsock;
}
