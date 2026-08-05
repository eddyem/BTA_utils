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

#include "bta_shdata.h"
#include "client.h"

static int SSL_nbread(SSL *ssl, char *buf, int bufsz){
    struct pollfd fds = {0};
    int fd = SSL_get_fd(ssl);
    fds.fd = fd;
    fds.events = POLLIN;
    if(poll(&fds, 1, 1) < 0){ // wait no more than 1ms
        LOGWARN("SSL_nbread(): poll() failed");
        WARNX("poll()");
        return 0;
    }
    if(fds.revents == POLLIN){
//        DBG("Got info in fd #%d", fd);
        int l = read_string(ssl, buf, bufsz);
//        DBG("read %d bytes", l);
        return l;
    }
    return 0;
}

static char *time_asc(double t){
    static char buf[128];
    int h, m;
    double s;
    h   = (int)(t/3600.);
    m = (int)((t - (double)h*3600.)/60.);
    s = t - (double)h*3600. - (double)m*60.;
    h %= 24;
    if(s>59) s=59;
    snprintf(buf, 127, "%d:%02d:%04.1f", h,m,s);
    return buf;
}


void clientproc(SSL_CTX *ctx, int fd){
    FNAME();
    SSL *ssl;
    char buf[1024];
    char acClientRequest[1024] = {0};
    int bytes;
    if(!get_shm_block(&sdat, ClientSide)){
        LOGERR("Can't get SHM block");
        ERRX("Can't get SHM block");
    }
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    int c = SSL_connect(ssl);
    if(c < 0){
        LOGERR("SSL_connect()");
        ERRX("SSL_connect() error: %d", SSL_get_error(ssl, c));
    }
    int enable = 1;
    if(ioctl(fd, FIONBIO, (void *)&enable) < 0){
        LOGERR("Can't make socket nonblocking");
        ERRX("ioctl()");
    }
    double t0 = sl_dtime();
    while(1){
        if(sl_dtime() - t0 > 3.){
            if(!check_shm_block(&sdat)){
                LOGERR("Broken SHM block");
                ERRX("Broken SHM block");
            }
            sprintf(acClientRequest, "UTC: %s\n", time_asc(M_time));
            SSL_write(ssl, acClientRequest, strlen(acClientRequest));
            //val_Hmd = 55. + drand48() * 15.;
            t0 = sl_dtime();
        }
        bytes = SSL_nbread(ssl, buf, sizeof(buf));
        if(bytes > 0){
            buf[bytes] = 0;
            printf("Received: \"%s\"\n", buf);
        }else if(bytes < 0){
            LOGWARN("Server disconnected or other error");
            ERRX("Disconnected");
        }
    }
    SSL_free(ssl);
}
