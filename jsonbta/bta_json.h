/*
 * bta_json.h
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

#pragma once
#ifndef __BTA_JSON_H__
#define __BTA_JSON_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <stdbool.h>

#define RESOURCE "/bta_par" // resource to request in http
#define PORT    "12345" // Port to listen on
#define BACKLOG     10  // Passed to listen()
#define PIDFILE "/tmp/btajson.pid" // PID file

#ifdef EBUG // debug mode
	#define DBG(...)  do{fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");}while(0)
#else
	#define DBG(...)
#endif

typedef struct{
	bool ALL;
	bool corr;
	bool diff;
	bool eqcoor;
	bool horcoor;
	bool meteo;
	bool mtime;
	bool p2mode;
	bool sidtime;
	bool target;
	bool telfocus;
	bool telmode;
	bool valsens;
	bool vel;
} bta_pars;

// named parameters
#ifndef BTA_PRINT_C
	#define defpar(val)  const char* val = #val
#else
	#define defpar(val)  extern const char* val
#endif
defpar(mtime);
defpar(sidtime);
defpar(telmode);
defpar(telfocus);
defpar(target);
defpar(p2mode);
defpar(eqcoor);
defpar(horcoor);
defpar(valsens);
defpar(diff);
defpar(vel);
defpar(corr);
defpar(meteo);
#undef defpar

void make_JSON(int sock, bta_pars *par); // bta_print.c
void check4running(char **argv, char *pidfilename, void (*iffound)(pid_t pid)); // daemon.h

#endif // __BTA_JSON_H__

// конец файла
