/*
 * cmdlnopts.h - comand line options for parceargs
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
#ifndef __CMDLNOPTS_H__
#define __CMDLNOPTS_H__

#include "parseargs.h"

#define MSG(lvl, ...) do{if(lvl >= verbose) printf(__VA_ARGS__);}while(0)

/*
 * here are some typedef's for global data
 */
typedef struct{
    double gotoangle; // rotate for given angle
    int absmove;      // absolute move (from zero angle)
} glob_pars;


// default & global parameters
extern glob_pars const Gdefault;
extern int verbose;

glob_pars *parse_args(int argc, char **argv);
#endif // __CMDLNOPTS_H__
