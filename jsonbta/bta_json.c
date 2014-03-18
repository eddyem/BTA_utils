/*
 * bta_json.c - create socket and reply bta data
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
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

#include <strings.h>
#include "bta_json.h"

// wait for child to avoid zombies
static void wait_for_child(int sig){
	pid_t child;
	while((child = waitpid(-1, NULL, WNOHANG)) > 0);
}

// search a first word after needle without spaces
char* stringscan(char *str, char *needle){
	char *a, *e;
	char *end = str + strlen(str);
	a = strstr(str, needle);
	if(!a) return NULL;
	a += strlen(needle);
	while (a < end && (*a == ' ' || *a == '\r' || *a == '\t' || *a == '\r')) a++;
	if(a >= end) return NULL;
	e = strchr(a, ' ');
	if(e) *e = 0;
	return a;
}

/**
 * Parce data received from client
 * In case of http request we send all avaiable data
 * In case of request from client socket, parce it
 * Socket's request have structure like "par1<del>par2<del>..."
 * 		where "pars" are names of bta_pars fields
 * 		<del> is delimeter: one of symbols " &\t\n"
 * @param buf - incoming data
 * @param L - length of buf
 * @param par - returned parameters structure
 */
void parce_incoming_buf(char *buf, size_t L, bta_pars *par){
	char *tok, *got;
	memset(par, 0, sizeof(bta_pars));
	DBG("got data: %s", buf);
	// http request - send all if get == "bta_par"
	// if get == "bta_par?a&b&c...", change buf to pars
	if((got = stringscan(buf, "GET"))){
		size_t LR = strlen(RESOURCE);
		if(strcmp(got, RESOURCE) == 0){
			par->ALL = true;
			return;
		}else if(strncmp(got, RESOURCE, LR) == 0) buf = &got[LR+1];
		else exit(-1); // wrong request
	}
	// request from socket -> check all parameters
	tok = strtok(buf, " &\t\n");
	if(!tok) return;
	do{
		#define checkpar(val) if(strcasecmp(tok, val) == 0){par->val = true; continue;}
		checkpar(vel);
		checkpar(diff);
		checkpar(corr);
		checkpar(mtime);
		checkpar(meteo);
		checkpar(target);
		checkpar(p2mode);
		checkpar(eqcoor);
		checkpar(telmode);
		checkpar(sidtime);
		checkpar(horcoor);
		checkpar(valsens);
		checkpar(telfocus);
		#undef checkpar
	}while((tok = strtok(NULL, " &\t\n")));
}

void handle(int newsock){
	size_t buflen = 4095;
	char buffer[buflen + 1];
	bta_pars par;
	do{
		ssize_t readed = recv(newsock, buffer, buflen, 0);
		if(readed < 1) break; // client closed or error
		parce_incoming_buf(buffer, readed, &par);
		#ifdef EBUG
			#define checkpar(val) if(par.val){ fprintf(stderr, "par: %s\n", val); }
			if(par.ALL){ fprintf(stderr, "par: ALL\n"); }
			checkpar(vel);
			checkpar(diff);
			checkpar(corr);
			checkpar(mtime);
			checkpar(sidtime);
			checkpar(meteo);
			checkpar(target);
			checkpar(p2mode);
			checkpar(eqcoor);
			checkpar(telmode);
			checkpar(horcoor);
			checkpar(valsens);
			checkpar(telfocus);
			#undef checkpar
		#endif // EBUG
		make_JSON(newsock, &par);
	}while(!par.ALL);
	close(newsock);
}

int main(int argc, char **argv){
	int sock;
	struct sigaction sa;
	struct addrinfo hints, *res, *p;
	int reuseaddr = 1;
	check4running(argv, PIDFILE, NULL);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if(getaddrinfo(NULL, PORT, &hints, &res) != 0){
		perror("getaddrinfo");
		return 1;
	}
	struct sockaddr_in *ia = (struct sockaddr_in*)res->ai_addr;
	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(ia->sin_addr), str, INET_ADDRSTRLEN);
	printf("port: %u, addr: %s\n", ntohs(ia->sin_port), str);
	// loop through all the results and bind to the first we can
	for(p = res; p != NULL; p = p->ai_next){
		if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("socket");
			continue;
		}
		if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
			perror("setsockopt");
			return -1;
		}
		if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
			close(sock);
			perror("bind");
			continue;
		}
		break; // if we get here, we must have connected successfully
	}
	if(p == NULL){
		// looped off the end of the list with no successful bind
		fprintf(stderr, "failed to bind socket\n");
		exit(2);
	}
	// Listen
	if(listen(sock, BACKLOG) == -1) {
		perror("listen");
		return 1;
	}
	freeaddrinfo(res);
	// Set up the signal handler
	sa.sa_handler = wait_for_child;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		return 1;
	}
	// OK, all done, now we can daemonize
	#ifndef EBUG // daemonize only in release mode
		if(daemon(1, 0)){
			perror("daemon()");
			exit(1);
		}
	#endif // EBUG
	// Main loop
	while(1){
		struct sockaddr_in their_addr;
		socklen_t size = sizeof(struct sockaddr_in);
		int newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
		int pid;
		if(newsock == -1) return 0;
		pid = fork();
		if(pid == 0){
			// Child process
			close(sock);
			handle(newsock);
			return 0;
		}else{
			// Parent process
			if(pid == -1) return 1;
			else{
				close(newsock);
				DBG("Create child: %d", pid);
			}
		}
	}
	close(sock);
	return 0;
}


// конец файла
