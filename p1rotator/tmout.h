/*
 *                                                                                                  geany_encoding=koi8-r
 * tmout.h
 *
 * Copyright 2018 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
 *
 */
#pragma once
#ifndef __TMOUT_H__
#define __TMOUT_H__

#include <unistd.h>
extern int verbose;
extern char indi[];

#define PRINT(...) do{if(verbose) printf(__VA_ARGS__);}while(0)

#define WAIT_EVENT(evt, max_delay)  do{int __ = 0; set_timeout(max_delay); \
        char *iptr = indi; PRINT(" "); while(!tmout && !(evt)){ \
        usleep(100000); if(!*(++iptr)) iptr = indi; if(++__%10==0) PRINT("\b. "); \
        PRINT("\b%c", *iptr);}; PRINT("\n");}while(0)

void set_timeout(double delay);
extern volatile int tmout;

#endif // __TMOUT_H__
