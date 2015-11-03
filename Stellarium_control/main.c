/*
 * main.c
 *
 * Copyright 2014 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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

#include <math.h>
#include <endian.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <time.h>
// for pthread_kill
//#define _XOPEN_SOURCE  666
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "usefull_macros.h"
#include "angle_functions.h"
#include "bta_shdata.h"

// daemon.c
extern void check4running(char **argv, char *pidfilename, void (*iffound)(pid_t pid));

// Max amount of connections
#define BACKLOG     (1)
// port for connections
#define PORT (10000)
// accept only local connections
#define ACCEPT_IP  "127.0.0.1"
#define BUFLEN (1024)

static uint8_t buff[BUFLEN+1];

//glob_pars *Global_parameters = NULL;

static volatile int global_quit = 0;
// quit by signal
void signals(int sig){
	restore_console();
	DBG("Get signal %d, quit.\n", sig);
	global_quit = 1;
	sleep(1);
	exit(sig);
}

// search a first word after needle without spaces
char* stringscan(char *str, char *needle){
	char *a, *e;
	char *end = str + strlen(str);
	a = strstr(str, needle);
	if(!a) return NULL;
	a += strlen(needle);
	while (a < end && (*a == ' ' || *a == '\r' || *a == '\t')) a++;
	if(a >= end) return NULL;
	e = strchr(a, ' ');
	if(e) *e = 0;
	return a;
}

/**
 * Send data to user
 * @param data   - data to send
 * @param dlen   - data length
 * @param sockfd - socket fd for sending data
 * @return 0 if failed
 */
int send_data(uint8_t *data, size_t dlen, int sockfd){
	size_t sent = write(sockfd, data, dlen);
	if(sent != dlen){
		WARN("write()");
		return 0;
	}
	return 1;
}

//read: 0x14 0x0 0x0 0x0 0x5b 0x5a 0x2e 0xc6 0x8c 0x23 0x5 0x0 0x23 0x9 0xe5 0xaf 0x23 0x2e 0x34 0xed
// command: goto 16h29 24.45 -26d25 55.62
/*
 LITTLE-ENDIAN!!!
 from client:
LENGTH (2 bytes, integer): length of the message
TYPE (2 bytes, integer): 0
TIME (8 bytes, integer): current time on the server computer in microseconds
    since 1970.01.01 UT. Currently unused.
RA (4 bytes, unsigned integer): right ascension of the telescope (J2000)
    a value of 0x100000000 = 0x0 means 24h=0h,
    a value of 0x80000000 means 12h
DEC (4 bytes, signed integer): declination of the telescope (J2000)
    a value of -0x40000000 means -90degrees,
    a value of 0x0 means 0degrees,
    a value of 0x40000000 means 90degrees

to client:
LENGTH (2 bytes, integer): length of the message
TYPE (2 bytes, integer): 0
TIME (8 bytes, integer): current time on the server computer in microseconds
    since 1970.01.01 UT. Currently unused.
RA (4 bytes, unsigned integer): right ascension of the telescope (J2000)
    a value of 0x100000000 = 0x0 means 24h=0h,
    a value of 0x80000000 means 12h
DEC (4 bytes, signed integer): declination of the telescope (J2000)
    a value of -0x40000000 means -90degrees,
    a value of 0x0 means 0degrees,
    a value of 0x40000000 means 90degrees
STATUS (4 bytes, signed integer): status of the telescope, currently unused.
    status=0 means ok, status<0 means some error
*/

#define DEG2DEC(degr)  ((int32_t)(degr / 90. * ((double)0x40000000)))
#define HRS2RA(hrs)    ((uint32_t)(hrs / 12. * ((double)0x80000000)))
#define DEC2DEG(i32)   (((double)i32)*90./((double)0x40000000))
#define RA2HRS(u32)    (((double)u32)*12. /((double)0x80000000))

typedef struct __attribute__((__packed__)){
	uint16_t len;
	uint16_t type;
	uint64_t time;
	uint32_t ra;
	int32_t dec;
} indata;

typedef struct __attribute__((__packed__)){
	uint16_t len;
	uint16_t type;
	uint64_t time;
	uint32_t ra;
	int32_t dec;
	int32_t status;
} outdata;

#define ACS_CMD(a)   do{green(#a); printf("\n");}while(0)
/**
 * send input RA/Decl (j2000!) coordinates to tel
 * both coords are in seconds (ra in time, dec in angular)
 */
int setCoords(double ra, double dec){
	double r, d;
	calc_AP(ra, dec, &r, &d);
	DBG("Set RA/Decl to %g, %g", r/3600, d/3600);
	ACS_CMD(SetRADec(r, d));
	int i;
	for(i = 0; i < 10; ++i){
		if(InpAlpha == r && InpDelta == d) break;
		usleep(100000);
	}
	if(InpAlpha != r || InpDelta != d){
		WARNX(_("Can't send data to system!"));
		return 0;
	}
	return 1;
}

int proc_data(uint8_t *data, ssize_t len){
	FNAME();
	if(len != sizeof(indata)){
		WARN("Bad data size: got %zd instead of %zd!", len, sizeof(indata));
		return 0;
	}
	indata *dat = (indata*)data;
	uint16_t L, T;
	//uint64_t tim;
	uint32_t ra;
	int32_t dec;
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	L = le16toh(dat->len); T = le16toh(dat->type);
	//tim = le64toh(dat->time);
	ra = le32toh(dat->ra);
	dec = (int32_t)le32toh((uint32_t)dat->dec);
#else
	L = dat->len; T = dat->type;
	//tim = dat->time;
	ra = dat->ra; dec = dat->dec;
#endif
	WARN("got message with len %u & type %u", L, T);
	double tagRA = RA2HRS(ra), tagDec = DEC2DEG(dec);
	WARN("RA: %u (%g), DEC: %d (%g)", ra, tagRA,
		dec, tagDec);
	if(!setCoords(tagRA, tagDec)) return 0;
	return 1;
/*
	time_t z = time(NULL);
	time_t tm = (time_t)(tim/1000000);
	WARN("time: %ju  (local: %ju)", (uintmax_t)tm, (uintmax_t)z);
	WARN("time: %zd -- %s local: %s", tim, ctime(&tm), ctime(&z));
	*/
}

/**
 * main socket service procedure
 */
void handle_socket(int sock){
	FNAME();
	if(global_quit) return;
	ssize_t readed;
	outdata dout;
	dout.len = htole16(sizeof(outdata));
	dout.type = 0;
	dout.status = 0;
	while(!global_quit){
		double r, d, ca, cd;
		calc_AD(val_A, val_Z, S_time, &ca, &cd); // calculate current telescope polar coordinates
		calc_mean(ca, cd, &r, &d);
		dout.ra = htole32(HRS2RA(r));
		dout.dec = (int32_t)htole32(DEG2DEC(d));
		if(!send_data((uint8_t*)&dout, sizeof(outdata), sock)) break;
		DBG("sent ra = %g, dec = %g", RA2HRS(dout.ra), DEC2DEG(dout.dec));
		fd_set readfds;
		struct timeval timeout;
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
		timeout.tv_sec = 1; // wait not more than 1 second
		timeout.tv_usec = 0;//100000;
		int sel = select(sock + 1 , &readfds , NULL , NULL , &timeout);
		if(sel < 0){
			if(errno != EINTR)
				WARN("select()");
			continue;
		}
		if(!(FD_ISSET(sock, &readfds))) continue;
		// fill incoming buffer
		readed = read(sock, buff, BUFLEN);
		DBG("read %zd", readed);
		if(readed <= 0){ // error or disconnect
			DBG("Nothing to read from fd %d (ret: %zd)", sock, readed);
			break;
		}
		/**************************************
		 *       DO SOMETHING WITH DATA       *
		 **************************************/
		if(!proc_data(buff, readed)) dout.status = -1;
		else dout.status = 0;
	}
	close(sock);
}

typedef struct{
	uint32_t keylev;
	uint32_t codelev;
} passhash;

void get_passhash(passhash *p){
	int i, c, nlev = 0;
	// ask user to enter password
	setup_con(); // hide echo
	for(i = 3; i > 0; --i){ // try 3 times
		char pass[256]; int k = 0;
		printf("Enter password, you have %d tr%s\n", i, (i > 1) ? "ies":"y");
		while ((c = mygetchar()) != '\n' && c != EOF && k < 255){
			if(c == '\b' || c == 127){ // use DEL and BACKSPACE to erase previous symbol
				if(k) --k;
				printf("\b \b");
			}else{
				pass[k++] = c;
				printf("*");
			}
			fflush(stdout);
		}
		pass[k] = 0;
		printf("\n");
		if((nlev = find_lev_passwd(pass, &p->keylev, &p->codelev)))
			break;
		printf(_("No, not this\n"));
	}
	restore_console();
	if(nlev == 0)
		ERRX(_("Tries excess!"));
	set_acckey(p->keylev);
	DBG("OK, level %d", nlev);
}

static inline void main_proc(){
	int sock;
	int reuseaddr = 1;
	// open socket
	struct sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(PORT);
	if(!inet_aton(ACCEPT_IP, (struct in_addr*)&myaddr.sin_addr.s_addr))
		ERR("inet_aton");
	if((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1){
		ERR("socket");
	}
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
		ERR("setsockopt");
	}
	if(-1 == bind(sock, (struct sockaddr*)&myaddr, sizeof(myaddr))){
		close(sock);
		ERR("bind");
	}
	// Listen
	if(listen(sock, BACKLOG) == -1){
		ERR("listen");
	}
	//freeaddrinfo(res);
	// Main loop
	while(!global_quit){
		socklen_t size = sizeof(struct sockaddr_in);
		struct sockaddr_in myaddr;
		int newsock;
		newsock = accept(sock, (struct sockaddr*)&myaddr, &size);
		if(newsock <= 0){
			WARN("accept()");
			continue;
		}
		handle_socket(newsock);
	}
	close(sock);
}

int main(_U_ int argc, char **argv){
	// setup coloured output
	initial_setup();
	check4running(argv, NULL, NULL);
//	Global_parameters = parce_args(argc, argv);
//	assert(Global_parameters != NULL);

	signal(SIGTERM, signals); // kill (-15) - quit
	signal(SIGHUP, SIG_IGN);  // hup - ignore
	signal(SIGINT, signals);  // ctrl+C - quit
	signal(SIGQUIT, signals); // ctrl+\ - quit
	signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
	if(!get_shm_block(&sdat, ClientSide))
		ERRX(_("Can't find shared memory block"));
	if(!check_shm_block(&sdat))
		ERRX(_("There's no connection to BTA!"));
	double last = M_time;
	int i;
	printf(_("Test connection\n"));
	for(i = 0; i < 10 && fabs(M_time - last) < 0.02; ++i){
		printf("."); fflush(stdout);
		sleep(1);
	}
	printf("\n");
	if(fabs(M_time - last) < 0.02)
		ERRX(_("Data stale!"));
	//get_cmd_queue(&ucmd, ClientSide);
	//passhash pass = {0,0};
	//get_passhash(&pass);
	printf(_("All OK, start socket\n"));

/*
#ifndef EBUG // daemonize only in release mode
	if(!Global_parameters->nodaemon){
		if(daemon(1, 0)){
			perror("daemon()");
			exit(1);
		}
	}
#endif // EBUG
*/

	while(1){
		pid_t childpid = fork();
		if(childpid < 0)
			ERR("ERROR on fork");
		if(childpid){
			DBG("Created child with PID %d\n", childpid);
			wait(NULL);
			printf("Child %d died\n", childpid);
		}else{
			prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
			main_proc();
			return 0;
		}
	}

	return 0;
}
