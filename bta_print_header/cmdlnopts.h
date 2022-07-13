/*
 * This file is part of the btaprinthdr project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#pragma once
#ifndef __CMDLNOPTS_H__
#define __CMDLNOPTS_H__

#include <usefull_macros.h>

/*
 * here are some typedef's for global data
 */
typedef struct{
    char *outfile;
    char *pidfile;
    double refresh;
} glob_pars;

glob_pars *parse_args(int argc, char **argv);
void verbose(int levl, const char *fmt, ...);
#endif // __CMDLNOPTS_H__
