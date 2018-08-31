#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <libgen.h>

#include "usefull_macros.h"
#include "bta_shdata.h"

#define HOST        "mirtemp.sao.ru"
#define PORT        "4444"
#define RESOURCE    "Tmean"

static char *myname, stm[200];

void clear_flags(){
    if(MeteoMode & NET_T3){ // clear "net" & "sensor" flags
        MeteoMode &= ~NET_HMD;
        if(MeteoMode & SENSOR_T3) MeteoMode &= ~SENSOR_T3;
    }
}

void signals(int sig){
    clear_flags();
    char ss[10];
    time_t  t = time(NULL);
    signal(sig, SIG_IGN);
    strftime(stm, 200, "%d.%m.%Y %H:%M:%S", localtime(&t));
    switch(sig){
        case SIGHUP : strcpy(ss,"SIGHUP");  break;
        case SIGINT : strcpy(ss,"SIGINT");  break;
        case SIGQUIT: strcpy(ss,"SIGQUIT"); break;
        case SIGFPE : strcpy(ss,"SIGFPE");  break;
        case SIGPIPE: strcpy(ss,"SIGPIPE"); break;
        case SIGSEGV: strcpy(ss,"SIGSEGV"); break;
        case SIGTERM: strcpy(ss,"SIGTERM"); break;
        default:  sprintf(ss,"SIG_%d",sig); break;
    }
    switch(sig){
        default:
        case SIGHUP :
        case SIGINT :
             fprintf(stderr,"%s %s: %s - Ignore .....\n",stm, myname, ss);
             fflush(stderr);
             signal(sig, signals);
             return;
        case SIGPIPE:
        case SIGQUIT:
        case SIGFPE :
        case SIGSEGV:
        case SIGTERM:
             signal(SIGALRM, SIG_IGN);
             fprintf(stderr, "%s %s: %s - programm stop!\n", stm, myname, ss);
             fflush(stderr);
             clear_flags();
             exit(sig);
    }
}

/**
 * get mirror temperature over network
 * @return 0 if succeed
 */
int get_mirT(double *T){
    int sockfd = 0;
    char recvBuff[64];
    memset(recvBuff, 0, sizeof(recvBuff));
    struct addrinfo h, *r, *p;
    memset(&h, 0, sizeof(h));
    h.ai_family = AF_INET;
    h.ai_socktype = SOCK_STREAM;
    h.ai_flags = AI_CANONNAME;
    char *host = HOST;
    char *port = PORT;
    if(getaddrinfo(host, port, &h, &r)) WARNX("getaddrinfo()");
    for(p = r; p; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            WARN("socket()");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            WARN("connect()");
            continue;
        }
        break; // if we get here, we must have connected successfully
    }
    if(p == NULL){
        WARNX("failed to connect");
        return 1;
    }
    freeaddrinfo(r);
    if(send(sockfd, RESOURCE, sizeof(RESOURCE), 0) != sizeof(RESOURCE)){
        WARN("send()");
        return 1;
    }
    ssize_t rd = read(sockfd, recvBuff, sizeof(recvBuff)-1);
    if(rd < 0){
        WARN("read()");
        return 1;
    }else recvBuff[rd] = 0;
    close(sockfd);
    char *eptr;
    *T = strtod(recvBuff, &eptr);
    if(eptr == recvBuff) return 1;
    return 0;
}

int main (_U_ int argc, _U_ char *argv[]){
    initial_setup();
    strncpy(stm, argv[0], 200);
    myname = strdup(basename(stm));
    DBG("my name: %s", myname);
    signal(SIGHUP, signals);
    signal(SIGINT, signals);
    signal(SIGQUIT,signals);
    signal(SIGFPE, signals);
    signal(SIGPIPE,signals);
    signal(SIGSEGV,signals);
    signal(SIGTERM,signals);
    sdat.mode |= 0200;
    sdat.atflag = 0;
    get_shm_block( &sdat, ClientSide);
    get_cmd_queue( &ocmd, ClientSide);
    time_t tlast = time(NULL);
    strftime(stm, 200, "%d.%m.%Y %H:%M:%S", localtime(&tlast));
    printf("%s start @ %s\n", myname, stm);
    while(1){
        double T;
        if(time(NULL) - tlast > 900){ // no signal for 15 minutes - clear flags
            WARNX("No signal for 15 minutes");
            //clear_flags();
            tlast = time(NULL);
        }
        if(get_mirT(&T)){
            sleep(10);
            continue;
        }
        if(0 == (MeteoMode & INPUT_T3)){ // not manual mode - change Tmir value
            val_T3 = T;
            MeteoMode |= (SENSOR_T3|NET_T3);
            DBG("got T: %.2f", T);
        }
        sleep(60);
        tlast = time(NULL);
    }
    clear_flags();
    return 0;
}
