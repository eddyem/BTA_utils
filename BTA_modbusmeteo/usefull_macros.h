/*
 * usefull_macros.h - a set of usefull macros: memory, color etc
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
#ifndef __USEFULL_MACROS_H__
#define __USEFULL_MACROS_H__

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <locale.h>
#include <stdlib.h>
#include <termios.h>
#include <termio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>


#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif


// unused arguments with -Wall -Werror
#define _U_    __attribute__((__unused__))

/*
 * Coloured messages output
 */
#define RED			"\033[1;31;40m"
#define GREEN		"\033[1;32;40m"
#define OLDCOLOR	"\033[0;0;0m"

/*
 * ERROR/WARNING messages
 */
extern int globErr;
extern void signals(int sig);
#define ERR(...) do{globErr=errno; _WARN(__VA_ARGS__); signals(SIGTERM);}while(0)
#define ERRX(...) do{globErr=0; _WARN(__VA_ARGS__); signals(SIGTERM);}while(0)
#define WARN(...) do{globErr=errno; _WARN(__VA_ARGS__);}while(0)
#define WARNX(...) do{globErr=0; _WARN(__VA_ARGS__);}while(0)

/*
 * print function name, debug messages
 * debug mode, -DEBUG
 */
#ifdef EBUG
    #define FNAME() fprintf(stderr, "\n%s (%s, line %d)\n", __func__, __FILE__, __LINE__)
    #define DBG(...) do{fprintf(stderr, "%s (%s, line %d): ", __func__, __FILE__, __LINE__); \
                    fprintf(stderr, __VA_ARGS__);			\
                    fprintf(stderr, "\n");} while(0)
#else
    #define FNAME()	 do{}while(0)
    #define DBG(...) do{}while(0)
#endif //EBUG

/*
 * Memory allocation
 */
#define ALLOC(type, var, size)  type * var = ((type *)my_alloc(size, sizeof(type)))
#define MALLOC(type, size) ((type *)my_alloc(size, sizeof(type)))
#define FREE(ptr)			do{free(ptr); ptr = NULL;}while(0)
#define LOG(...)    do{Cl_putlogt(__VA_ARGS__); WARNX(__VA_ARGS__);}while(0)

double dtime();

// functions for color output in tty & no-color in pipes
extern int (*red)(const char *fmt, ...);
extern int (*_WARN)(const char *fmt, ...);
extern int (*green)(const char *fmt, ...);
void * my_alloc(size_t N, size_t S);
void initial_setup();

typedef struct{
    char *logpath;          // full path to logfile
    pthread_mutex_t mutex;  // log mutex
} Cl_log;

int Cl_createlog(char *logname);
int Cl_putlogt(const char *fmt, ...);

#endif // __USEFULL_MACROS_H__
