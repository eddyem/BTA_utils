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
#define _BSD_SOURCE
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

// daemon.c
extern void check4running(char **argv, char *pidfilename, void (*iffound)(pid_t pid));

// Max amount of connections
#define BACKLOG     (1)

#define PORT ("10000")
#define BUFLEN (1024)

static uint8_t buff[BUFLEN+1];

//glob_pars *Global_parameters = NULL;

static volatile int global_quit = 0;
// quit by signal
static void signals(int sig){
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
 */
void send_data(uint8_t *data, size_t dlen, int sockfd){
	/*char buf[1024];
	if(!strip){
		if(imtype == IMTYPE_RAW)
			L = snprintf(buf, 255, "%s\n%dx%d\n", imsuffixes[imtype], w, h);
		else
			L = snprintf(buf, 255, "%s\n%zd\n", imsuffixes[imtype], buflen);
	}else{
		L = snprintf(buf, 1023, "HTTP/2.0 200 OK\r\nContent-type: image/%s\r\n"
			"Content-Length: %zd\r\n\r\n", mimetypes[imtype], buflen);
	}
	buff = MALLOC(uint8_t, L + buflen);
	memcpy(buff, buf, L);
	memcpy(buff+L, imagedata, buflen);
	FREE(imagedata);
	buflen += L;*/
	size_t sent = write(sockfd, data, dlen);
	if(sent != dlen) WARN("write()");
	//FREE(buff);
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

static double tagRA = -1., tagDec = -100.;

void proc_data(uint8_t *data, ssize_t len){
	FNAME();
	if(len != sizeof(indata)){
		WARN("Bad data size: got %zd instead of %zd!", len, sizeof(indata));
		return;
	}
	indata *dat = (indata*)data;
	uint16_t L, T;
	uint64_t tim;
	uint32_t ra;
	int32_t dec;
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	L = le16toh(dat->len); T = le16toh(dat->type);
	tim = le64toh(dat->time);
	ra = le32toh(dat->ra);
	dec = (int32_t)le32toh((uint32_t)dat->dec);
#else
	L = dat->len; T = dat->type;
	tim = dat->time;
	ra = dat->ra; dec = dat->dec;
#endif
	WARN("got message with len %u & type %u", L, T);
	tagRA = RA2HRS(ra); tagDec = DEC2DEG(dec);
	WARN("RA: %u (%g), DEC: %d (%g)", ra, tagRA,
		dec, tagDec);
	time_t z = time(NULL);
	time_t tm = (time_t)(tim/1000000);
	WARN("time: %ju  (local: %ju)", (uintmax_t)tm, (uintmax_t)z);
	WARN("time: %zd -- %s local: %s", tim, ctime(&tm), ctime(&z));
/*	memmove(buff, data, sizeof(indata));
	outdata *dout = (outdata*) buff;
	dout->ra = 0; dout->dec = 0x40000000;
	dout->status = 0;
	send_data(data, sizeof(outdata), sock);*/
}

/**
 * main socket service procedure
 */
void handle_socket(int sock){
	FNAME();
	if(global_quit) return;
	ssize_t readed;
	outdata dout;
	uint32_t oldra;
	int32_t olddec;
	dout.len = sizeof(outdata);
	dout.type = 0;
	dout.status = 0;
	dout.ra = (tagRA < -0.1) ? 0 : HRS2RA(tagRA);
	dout.dec = (tagDec < -91.) ? DEG2DEC(80.) : DEG2DEC(tagDec);
	oldra = dout.ra; olddec = dout.dec;
	while(!global_quit){
		//dout.ra += 0xF5555555;
		if(tagRA < -0.1) dout.ra += HRS2RA(0.33);
		else dout.ra = HRS2RA(tagRA);
		if(tagDec > -91.) dout.dec = DEG2DEC(tagDec);
		if(dout.ra != oldra || dout.dec != olddec){
			send_data((uint8_t*)&dout, sizeof(outdata), sock);
			DBG("sent ra = %g (%g), dec = %g (%g)", RA2HRS(dout.ra), tagRA, DEC2DEG(dout.dec), tagDec);
			oldra = (dout.ra+oldra)/2; olddec = (dout.dec+olddec)/2;
		}
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
		proc_data(buff, readed);
		//send_data(...);
	}
	close(sock);
}

static inline void main_proc(){
	int sock;
	struct addrinfo hints, *res, *p;
	int reuseaddr = 1;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if(getaddrinfo(NULL, PORT, &hints, &res) != 0){
		ERR("getaddrinfo");
	}
	struct sockaddr_in *ia = (struct sockaddr_in*)res->ai_addr;
	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(ia->sin_addr), str, INET_ADDRSTRLEN);
	DBG("port: %u, addr: %s\n", ntohs(ia->sin_port), str);
	// loop through all the results and bind to the first we can
	for(p = res; p != NULL; p = p->ai_next){
		if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			WARN("socket");
			continue;
		}
		if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
			ERR("setsockopt");
		}
		if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
			close(sock);
			WARN("bind");
			continue;
		}
		break; // if we get here, we have a successfull connection
	}
	if(p == NULL){
		// looped off the end of the list with no successful bind
		ERRX("failed to bind socket");
	}
	// Listen
	if(listen(sock, BACKLOG) == -1){
		ERR("listen");
	}
	freeaddrinfo(res);
	// Main loop
	while(!global_quit){
//		fd_set readfds;
//		struct timeval timeout;
		socklen_t size = sizeof(struct sockaddr_in);
		struct sockaddr_in their_addr;
		int newsock;
/*		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
		timeout.tv_sec = 0; // wait not more than 10 milliseconds
		timeout.tv_usec = 10000;
		int sel = select(sock + 1 , &readfds , NULL , NULL , &timeout);
		if(sel < 0){
			if(errno != EINTR)
				WARN("select()");
			continue;
		}
		if(!(FD_ISSET(sock, &readfds))) continue;*/
	//	DBG("accept");
		newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
	//	printf("got addr %ul\n", their_addr.sin_addr.s_addr);
		if(newsock <= 0){
			WARN("accept()");
			continue;
		}
		pid_t pid = fork();
		if(pid < 0)
			ERR("ERROR on fork");
		if(pid == 0){
			close(sock);
			handle_socket(newsock);
			exit(0);
		}else
			close(newsock);
	}

	// wait for thread ends before closing videodev
//	pthread_join(readout_thread, NULL);
//	pthread_mutex_unlock(&readout_mutex);
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
/*
	while(1){
		pid_t childpid = fork();
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
	*/
	main_proc();
	return 0;
}
