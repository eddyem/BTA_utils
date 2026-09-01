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

#include <inttypes.h>
#include <float.h>
#include <signal.h>
#include <usefull_macros.h>

#include "bta_shdata.h"
#include "client.h"
#include "cmdlnopts.h"
#include "daemon.h" // isrunning
#include "handlers_list.h"
#include "motors.h" // motors amount, motor_state_t
#include "sslsock.h"


#define NEW_HANDLER(name, unused)  \
static const char* cmd_ ## name = STR(name);
HANDLERS_LIST()
#undef NEW_HANDLER

static const char *Ecodes[RESULT_SILENCE] = {
    [RESULT_OK] = "OK",
    [RESULT_BADVAL] = "BADVAL",
    [RESULT_BADKEY] = "BADKEY",
    [RESULT_FAIL] = "FAIL",
};

typedef enum{
    ARG_TYPE_INT,
    ARG_TYPE_DOUBLE
} arg_type_t;

typedef struct{
    union{
        double d;
        int64_t i;
    };
    arg_type_t type;
} value_t;

// setter: when setspeed command sent successfully
static double set_speed = 0.;
// common state - by last motor polled
static motor_state_t CommonState = {0};
// state of each motor
static motor_state_t MotorState[MOTORS_AMOUNT] = {0};

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


/**
 * @brief send_motor_command - send motor command and parse answer
 * @param ssl - ssl
 * @param cmd - command to send
 * @param setter - command setter (or NULL for getter)
 * @param getter - pointer do value changed by getter (or NULL for setter)
 * @return server's answer (or OK for successfull getters)
 */
static sl_sock_hresult_e send_motor_command(SSL *ssl, const char *cmd, value_t *setter, value_t *getter){
    char buf[IOBUF_LEN];
    if(!ssl || !cmd) return RESULT_FAIL;
    int l;
    if(setter){
        if(setter->type == ARG_TYPE_INT){
            l = snprintf(buf, IOBUF_LEN-1, "%s=%" PRId64 "\n", cmd, setter->i);
        }else{
            l = snprintf(buf, IOBUF_LEN-1, "%s=%g\n", cmd, setter->d);
        }
    }else l = snprintf(buf, IOBUF_LEN-1, "%s\n", cmd);
    buf[l] = 0;
    //DBG("Send to server: %s", buf);
    SSL_write(ssl, buf, l);
    double t0 = sl_dtime();
    while(sl_dtime() - t0 < G.acc_timeout){
        l = SSL_nbread(ssl, buf, IOBUF_LEN-1);
        if(l == 0) continue;
        else if(l < 0){
            LOGWARN("Server disconnected or other error");
            ERRX("Disconnected");
        }
        buf[l] = 0;
        //DBG("Received %d bytes: \"%s\"", l, buf);
        // parser
        char key[SL_KEY_LEN] = {0}, val[SL_VAL_LEN] = {0};
        int got = sl_get_keyval(buf, key, val);
        //DBG("got=%d, key=%s, val=%s", got, key, val);
        if(got == 0){
            //DBG("Empty answer");
            continue;
        }
        if((setter && got == 2) || (getter && got == 1)){ // wrong answer
            DBG("wrong answer");
            continue;
        }
        if(!getter){ // check errcode in answer
            if(got == 2){
                DBG("Getter answer when setter called");
                continue;
            }
            for(int i = 0; i < RESULT_SILENCE; ++i){
                //DBG("compare '%s' and '%s'", key, Ecodes[i]);
                if(0 == strcmp(key, Ecodes[i])){
                    //DBG("Found errcode for '%s': %d", key, i);
                    return i; // found
                }
            }
        }else{ // check "cmd = val"
            if(got == 1) continue;
            if(strcmp(cmd, key)) continue;
            //if(getter){
                if(getter->type == ARG_TYPE_INT){
                    long long ll;
                    if(!sl_str2ll(&ll, val)) continue;
                    getter->i = (int64_t) ll;
                }else{
                    double d;
                    if(!sl_str2d(&d, val)) continue;
                    getter->d = d;
                }
            //}
            //DBG("Getter OK");
            return RESULT_OK;
        }
    }
    DBG("FAILED");
    return RESULT_FAIL;
}

// emergency stop motors
static void stop_all(SSL *ssl){
    int ntries = 0;
    //value_t speed = {.type = ARG_TYPE_DOUBLE, .d = 0.};
    while(ntries < 5){
        //if(RESULT_OK == send_motor_command(ssl, cmd_speed, &speed, NULL)) break;
        if(RESULT_OK == send_motor_command(ssl, cmd_stop, NULL, NULL)) break;
        ++ntries;
        usleep(100000);
    }
}

/**
 * @brief check_motor - get status etc of motor number `motno`
 * @param ssl - ssl
 * @param motno - motor number (iterates on success)
 * @return FALSE if some step failed
 */
static int check_motor(SSL *ssl, int motno){
    char msg[128];
    if(!ssl || motno < 0 || motno >= MOTORS_AMOUNT) return FALSE;
    value_t Ival = {.type = ARG_TYPE_INT, .i = motno};
    value_t Dval = {.type = ARG_TYPE_DOUBLE};
    if(RESULT_OK != send_motor_command(ssl, cmd_motnum, &Ival, NULL)) return FALSE;
    if(RESULT_OK != send_motor_command(ssl, cmd_motstatus, NULL, &Ival)) return FALSE;
    MotorState[motno].status = (int) Ival.i;
    if(RESULT_OK != send_motor_command(ssl, cmd_motspeed, NULL, &Dval)) return FALSE;
    MotorState[motno].speed = Dval.d;
    if(RESULT_OK != send_motor_command(ssl, cmd_motcurrent, NULL, &Dval)) return FALSE;
    MotorState[motno].current = Dval.d;
    // now set common state as mean of all
    if(motno != MOTORS_AMOUNT - 1) return TRUE;
    int status = 0, N = 0; // mean status is largest
    double speed = 0., current = 0.;
    for(int i = 0; i < MOTORS_AMOUNT; ++i){
        int s = MotorState[i].status;
        if(s == MOT_OFF) continue;
        if(s == MOT_ERROR){
            *msg = MesgWarn;
            printf(msg+1, "Dome: Error in motor %d!\n", i+1);
            SendMessage(msg);
            MotorState[i].status = MOT_OFF;
        }
        if(status < s) status = s;
        if(status == MOT_RUN){
            speed += MotorState[i].speed;
            current += MotorState[i].current;
            ++N;
        }
    }
    if(N){
        speed /= (double)N;
        current /= (double)N;
    }
    if(status == MOT_OFF && CommonState.status != MOT_OFF){
        *msg = MesgFault;
        sprintf(msg+1, "Dome: All motors are Off!\n");
        SendMessage(msg);
    }else if(status != MOT_OFF && CommonState.status == MOT_OFF){
        *msg = MesgInfor;
        sprintf(msg+1, "Dome: Start motors!\n");
        SendMessage(msg);
    }
    CommonState.status = status;
    CommonState.speed = speed;
    CommonState.current = current;
    return TRUE;
}

// check for speed change and send given command to server
static void chk_dome_speed(SSL *ssl){
    static int old_state = D_Off;
    static double tlast = 0.;
    int new_state = Dome_Speed;
    if(D_Locked /*|| !PEP_K_On*/){
        if(old_state != D_Off) new_state = D_Off; // stop dome in locked state
        else return;
    }
    if(new_state == old_state){
        if(sl_dtime() - tlast < G.speedchk_interval) return;
        DBG("Time - tlast = %g", sl_dtime() - tlast);
        if(CommonState.speed == set_speed){
            tlast = sl_dtime();
            return;
        }
        DBG("Speed set command still not sent");
    }
    DBG("state changed from %d to %d", old_state, new_state);
    double new_speed = 0.;
    switch(new_state){
    case D_Lplus:
        new_speed = LSpeed;
        break;
    case D_Lminus:
        new_speed = -LSpeed;
        break;
    case D_Mplus:
        new_speed = MSpeed;
        break;
    case D_Mminus:
        new_speed = -MSpeed;
        break;
    case D_Hplus:
        new_speed = HSpeed;
        break;
    case D_Hminus:
        new_speed = -HSpeed;
        break;
    default: // stop
        break;
    }
    value_t Dval = {.type = ARG_TYPE_DOUBLE, .d = new_speed};
    if(RESULT_OK == send_motor_command(ssl, cmd_speed, &Dval, NULL)){
        DBG("New speed %g sent", new_speed);
        if(RESULT_OK == send_motor_command(ssl, cmd_speed, NULL, &Dval) && fabs(Dval.d - new_speed) <= FLT_EPSILON){
            DBG("Got answer with speed set: %g", Dval.d);
            old_state = new_state; // all OK, command in work
            set_speed = new_speed;
            speedSEWD = new_speed;
            tlast = sl_dtime();
        }else WARNX("error getting speed");
    }else WARNX("error sending motor command");
}

// SHM parser; return FALSE if SHM is in erroreous state
static int process_system(SSL *ssl){
    static double t0 = 0., last_mtime = 0.;
    static int curMotNo = 0; // current motor number (we'll scan all 10 motors by one)
    double curtime = sl_dtime();
    if(t0 < 1.){ // first run
        t0 = curtime;
        last_mtime = M_time;
        return TRUE;
    }
    // check time sync
    static int brokenshmctr = 0;
    if(M_time - last_mtime > curtime - t0 + G.T_sync_lost || !check_shm_block(&sdat)){ // broken SHM
        if(brokenshmctr == 0){
            LOGERR("Stalled or broken SHM block");
        }
        WARNX("Stalled or broken SHM block; %g -- %g", M_time - last_mtime + G.T_sync_lost, curtime - t0);
        if(++brokenshmctr < 10) return TRUE;
        return FALSE;
    }else brokenshmctr = 0;
    // set/reset PEP_K_On by information from server!
    value_t Ival = {.type = ARG_TYPE_INT, .i = 0};
    if(RESULT_OK != send_motor_command(ssl, cmd_forbidden, &Ival, NULL)) return FALSE;
#ifdef EBUG
    //if(PEP_K_On != Ival.i) DBG("Set PEP_K_On to %d", !Ival.i);
#endif
    if(Ival.i) PEP_K_On = 0;
    else PEP_K_On = 1;
    static int modelused = FALSE;
    if(!G.emulmode){
        if(UseModel == FullModel){ // model
            if(!modelused){
                LOGWARN("Server is in model mode");
                modelused = TRUE;
                stop_all(ssl);
            }
            return TRUE;
        }else modelused = FALSE;
        static int serverisdead = FALSE;
        if(ServPID <= 0 || kill(ServPID, 0) < 0){ // dead server
            WARNX("Main server is dead");
            if(!serverisdead){
                LOGERR("Main server is dead");
                serverisdead = TRUE;
                return TRUE;
            }
            return FALSE;
        }else serverisdead = FALSE;
    }
    if(check_motor(ssl, curMotNo)){
        // TODO: check state for errors
        if(++curMotNo == DomeSEW_N){ // set `struct SEWdata` parameters
            motor_state_t *st = &MotorState[curMotNo];
            statusSEWD = st->status;
            vel_SEWD = st->speed;
            currentSEWD = st->current;
        }
        if(curMotNo >= MOTORS_AMOUNT) curMotNo = 0;
    }
    chk_dome_speed(ssl);
    return TRUE;
}

/*
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
*/

// open SSL connection for client
static SSL *openConn(SSL_CTX *ctx, int fd){
    SSL *ssl = SSL_new(ctx);
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
    return ssl;
}

// run main client process
void clientproc(SSL_CTX *ctx, int fd){
    FNAME();
    char buf[1024];
    SSL *ssl = openConn(ctx, fd);
    if(!ssl) return;
    sdat.mode |= 0200; // allow W
    sdat.atflag = 0; // clear SHM_RDONLY
    if(!get_shm_block(&sdat, ClientSide)){
        LOGERR("Can't get SHM block");
        ERRX("Can't get SHM block");
    }
    while(isrunning){
        if(!process_system(ssl)){
            LOGERR("Motors error");
            WARNX("ERROR!");
            stop_all(ssl);
            sleep(5);
            break;
        }
        // clear receiving buffer (TODO: parse it?)
        int bytes = SSL_nbread(ssl, buf, sizeof(buf)-1);
        if(bytes > 0){
            buf[bytes] = 0;
            fprintf(stderr, "Received: \"%s\"\n", buf);
        }else if(bytes < 0){
            LOGWARN("Server disconnected or other error");
            ERRX("Disconnected");
        }
        usleep(100000);
    }
    DBG("Exit; isrunning=%d", isrunning);
    SSL_free(ssl);
}

// run in terminal mode
void terminal(SSL_CTX *ctx, int fd){
    FNAME();
    char buf[1024], *lptr = NULL;
    size_t N = 0;
    SSL *ssl = openConn(ctx, fd);
    if(!ssl) return;
    int printed = FALSE;
    while(isrunning){
        if(!printed){
            printf("> ");
            fflush(stdout);
            printed = TRUE;
        }
        if(sl_canread(0)){
            ssize_t L = getline(&lptr, &N, stdin);
            if(L > (ssize_t)(sizeof(buf)-1)) WARNX("String too long!");
            else if(L > 0){
                SSL_write(ssl, lptr, L);
            }
        }
        int bytes = 0;
        while((bytes = SSL_nbread(ssl, buf, sizeof(buf)-1))){
            if(bytes > 0){
                buf[bytes] = 0;
                printf("< %s\n", buf);
            }else{
                LOGWARN("Server disconnected or other error");
                ERRX("Disconnected");
            }
            usleep(10000);
            printed = FALSE;
        }
    }
    SSL_free(ssl);
}
