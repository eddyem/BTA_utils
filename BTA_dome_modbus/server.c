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

#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "daemon.h" // isrunning
#include "handlers_list.h"
#include "motors.h"
#include "server.h"

// handlers: `index` - command index in list, `value` - setter's value or getter's answer
typedef sl_sock_hresult_e (*handler_t)(int index, char value[SL_VAL_LEN]);

// struct for setters/getters
typedef struct{
    const char *command;
    handler_t handler;
    const char *helpstring;
} command_t;

static const char *maxcl = "Max client number reached, connect later\n";
static const char *sslerr = "SSL error occured\n";

// declaration of handlers
#define NEW_HANDLER(name, unused)  \
static sl_sock_hresult_e name ## _handler(int, char[SL_VAL_LEN]); // static const char* cmd_ ## name = STR(name);
HANDLERS_LIST()
#undef NEW_HANDLER

// commands list
#define NEW_HANDLER(name, help)  \
    { STR(name), name ## _handler, help },
command_t command_list[] = {
    HANDLERS_LIST()
};
#undef NEW_HANDLER
// index of command like `current_idx`
#define NEW_HANDLER(name, help)  \
    name ## _idx,
enum{
    HANDLERS_LIST()
};
#undef NEW_HANDLER

#define HANDLERS_AMOUNT (sizeof(command_list) / sizeof(command_t))
#define ISSETTER(x) (0 != x[0])

// search handler by name
static int search_handler(const char *);

// return 0 if client disconnected
static int handle_connection(SSL *ssl){
    char buf[IOBUF_LEN], key[SL_KEY_LEN], val[SL_VAL_LEN];
    int r = read_string(ssl, buf, IOBUF_LEN);
    if(r < 0) return 0;
    int sd = SSL_get_fd(ssl);
    DBG("Client %d msg: \"%s\"\n", sd, buf);
    LOGDBG("fd=%d, message=%s", sd, buf);
    int got = sl_get_keyval(buf, key, val);
    if(got == 0){
        DBG("Comment or empty string");
        return 1; // empty string
    }
    int h_idx = search_handler(key);
    sl_sock_hresult_e result = RESULT_BADKEY;
    if(-1 != h_idx){
        if(got == 1){
            DBG("getter #%d", h_idx);
            val[0] = 0; // getter
        }else DBG("setter #%d", h_idx);
        result = command_list[h_idx].handler(h_idx, val);
        DBG("result: %d", result);
    }else{
        DBG("Command not found or help?");
        // check if user asks for help
        if(0 == strcmp(key, "help")){
            for(size_t i = 0; i < HANDLERS_AMOUNT; ++i){
                snprintf(buf,  IOBUF_LEN-1, "%s: %s\n", command_list[i].command, command_list[i].helpstring);
                SSL_write(ssl, buf, strlen(buf));
            }
            return 1;
        }
    }
    // now `val` is an answer or error code
    if(result != RESULT_SILENCE) snprintf(buf, IOBUF_LEN-1, "%s\n", sl_sock_hresult2str(result));
    else snprintf(buf, IOBUF_LEN-1, "%s=%s\n", key, val);
    SSL_write(ssl, buf, strlen(buf));
    return 1;
}

/**
 * @brief timeouted_sslaccept - SSL_accept with timeout
 * @param ssl - SSL
 * @return 1 if connection ready or 0 if error
 */
static int timeouted_sslaccept(SSL *ssl){
    double t0 = sl_dtime();
    while(sl_dtime() - t0 < G.acc_timeout){
        int x = SSL_accept(ssl);
        if(x < 0){
            int sslerr = SSL_get_error(ssl, x);
            if(SSL_ERROR_WANT_READ == sslerr ||
                SSL_ERROR_WANT_WRITE == sslerr) continue;
            DBG("SSL error %d", sslerr);
            return FALSE;
        }
        else return TRUE;
    }
    DBG("Timeout");
    return FALSE;
}

void serverproc(SSL_CTX *ctx, int fd){
    int enable = 1;
    if(ioctl(fd, FIONBIO, (void *)&enable) < 0){
        LOGERR("Can't make socket nonblocking");
        ERRX("ioctl()");
    }
    int nfd = 1; // only one listening socket @start
    struct pollfd poll_set[BACKLOG+1];
    memset(poll_set, 0, sizeof(poll_set));
    poll_set[0].fd = fd;
    poll_set[0].events = POLLIN;
    SSL *ssls[BACKLOG+1] = {0}; // !!! start from 1 - like in poll_set !!!
    //double t0 = sl_dtime(), tstart = t0;
    //char buf[64];
    //int P = 0;
    while(isrunning){
        motors_process();
        /*double tnow = sl_dtime();
        if(tnow - t0 > 5. && nfd > 1){ // broadcasting message
            //DBG("send ping");
            snprintf(buf, 63, "ping #%d; t=%g\n", ++P, tnow - tstart);
            int l = strlen(buf);
            for(int i = nfd-1; i > 0; --i){
                //DBG("To fd[%d]=%d", i, poll_set[i].fd);
                SSL_write(ssls[i], buf, l);
            }
            t0 = tnow;
        }*/
        poll(poll_set, nfd, 1); // max timeout - 1ms
        // check for accept()
        if(poll_set[0].revents & POLLIN){
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            int client = accept4(fd, (struct sockaddr*)&addr, &len, SOCK_NONBLOCK); // non-blocking for timeout of SSL_accept
            DBG("Connection: %s @ %d (fd=%d)\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client);
            LOGMSG("Client %s connected to port %d (fd=%d)", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client);
            if(nfd == BACKLOG + 1){
                LOGWARN("Max amount of connections: disconnect fd=%d", client);
                WARNX("Limit of connections reached");
                send(client, maxcl, sizeof(maxcl)-1, MSG_NOSIGNAL);
                shutdown(client, SHUT_WR);
                usleep(50000); // we can't allow client to block us
                close(client);
            }else{
                DBG("New ssl");
                SSL *ssl = SSL_new(ctx);
                SSL_set_fd(ssl, client);
                DBG("Accept");
                if(timeouted_sslaccept(ssl)){
                    DBG("OK");
                    ssls[nfd] = ssl;
                    memset(&poll_set[nfd], 0, sizeof(struct pollfd));
                    poll_set[nfd].fd = client;
                    poll_set[nfd].events = POLLIN;
                    ++nfd;
                }else{
                    LOGERR("SSL_accept()");
                    WARNX("SSL_accept()");
                    SSL_free(ssl);
                    send(client, sslerr, sizeof(sslerr)-1, MSG_NOSIGNAL);
                    close(client);
                }
            }
        }
        // scan connections
        for(int fdidx = 1; fdidx < nfd; ++fdidx){
            if((poll_set[fdidx].revents & POLLIN) == 0) continue;
            int fd = poll_set[fdidx].fd;
            if(!handle_connection(ssls[fdidx])){ // socket closed
                SSL_free(ssls[fdidx]);
                DBG("Client fd=%d disconnected", fd);
                LOGMSG("Client fd=%d disconnected", fd);
                close(fd);
                if(--nfd > fdidx){ // move last FD to current position
                    poll_set[fdidx] = poll_set[nfd];
                    ssls[fdidx] = ssls[nfd];
                }
            }
        }
    }
    for(int i = 0; i < nfd; ++i) SSL_free(ssls[i]);
    motors_stop();
    modbus_close();
}

/****************** Protocol handlers (return 0 in case of success or error code >0 if failed) ******************/
// key - keyword (command name), value - i/o buffer (value[0]==0 for getters)
/*
sl_sock_hresult_e current_handler(int _U_ index, char _U_ value[SL_VAL_LEN]){
    double D;
    if(ISSETTER(value)){
        if(!sl_str2d(&D, value) || !motors_set_curntsetpoint(D)) return RESULT_BADVAL;
        return RESULT_OK;
    }
    snprintf(value,  SL_VAL_LEN-1, "%.3f", motors_get_curntsetpoint());
    return RESULT_SILENCE;
}*/

sl_sock_hresult_e motcurrent_handler(int _U_ index, char _U_ value[SL_VAL_LEN]){
    if(ISSETTER(value)) return RESULT_BADVAL; // only getter
    double D;
    if(!motors_get_actcurrent(&D)) return RESULT_FAIL;
    snprintf(value,  SL_VAL_LEN-1, "%.3f", D);
    return RESULT_SILENCE;
}

sl_sock_hresult_e motnum_handler(int _U_ index, char _U_ value[SL_VAL_LEN]){
    int I;
    if(ISSETTER(value)){
        if(!sl_str2i(&I, value) || !motors_set_activenum(I)) return RESULT_BADVAL;
        return RESULT_OK;
    }
    snprintf(value, SL_VAL_LEN-1, "%d", motors_get_activenum());
    return RESULT_SILENCE;
}

sl_sock_hresult_e motspeed_handler(int _U_ index, char _U_ value[SL_VAL_LEN]){
    if(ISSETTER(value)) return RESULT_BADVAL; // only getter
    double D;
    if(!motors_get_actspeed(&D)) return RESULT_FAIL;
    snprintf(value,  SL_VAL_LEN-1, "%.3f", D);
    return RESULT_SILENCE;
}

sl_sock_hresult_e motstatus_handler(int _U_ index, char _U_ value[SL_VAL_LEN]){
    if(ISSETTER(value)) return RESULT_BADVAL; // only getter
    int I;
    if(!motors_get_actstatus(&I)) return RESULT_FAIL;
    snprintf(value, SL_VAL_LEN-1, "%d", I);
    return RESULT_SILENCE;
}
/*
sl_sock_hresult_e relay_handler(int _U_ index, char _U_ value[SL_VAL_LEN]){
    return RESULT_SILENCE;
}*/

sl_sock_hresult_e speed_handler(int _U_ index, char _U_ value[SL_VAL_LEN]){
    double D;
    if(ISSETTER(value)){
        if(!sl_str2d(&D, value) || !motors_set_speedsetpoint(D)) return RESULT_BADVAL;
        return RESULT_OK;
    }
    snprintf(value,  SL_VAL_LEN-1, "%.3f", motors_get_speedsetpoint());
    return RESULT_SILENCE;
}

sl_sock_hresult_e stop_handler(int _U_ index, char _U_ value[SL_VAL_LEN]){
    motors_stop();
    return RESULT_OK;
}

// binary search handler by name
static int search_handler(const char *name){
    int low = 0;
    int high = HANDLERS_AMOUNT - 1;
    int iter = 0;

    while(low <= high){
        ++iter;
        int mid = low + (high - low) / 2;
        // Compare the target string with the struct's string field
        int res = strcmp(name, command_list[mid].command);
        if(res == 0){
            DBG("Found %s by %d iterations\n", name, iter);
            return mid; // Target found, return index
        }else if(res < 0){
            high = mid - 1; // Target is smaller, search left half
        }else{
            low = mid + 1;  // Target is larger, search right half
        }
    }
    DBG("%s not found by %d iterations\n", name, iter);
    return -1;
}
