/*
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
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
#include <assert.h>
#include "cmdlnopts.h"
#include "usefull_macros.h"

#define RAD 57.2957795130823
#define D2R(x) ((x) / RAD)
#define R2D(x) ((x) * RAD)

/*
 * here are global parameters initialisation
 */
int help;
glob_pars  G;

int verbose = 0; // each -v increments this value, e.g. -vvv sets it to 3
//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
    .gotoangle = 400.,
};

/*
 * Define command line options by filling structure:
 *  name    has_arg flag    val     type        argptr          help
*/
myoption cmdlnopts[] = {
    // set 1 to param despite of its repeating number:
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    // change p3 position, don't run guiding
    {"goto",    NEED_ARG,   NULL,   'g',    arg_double, APTR(&G.gotoangle), _("rotate for given angle")},
    // goto 0, don't run guiding
    {"absmove", NO_ARGS,    NULL,   'a',    arg_none,   APTR(&G.absmove),   _("rotate to given absolute angle (or zero without -g)")},
    // incremented parameter without args (any -v will increment value of "verbose")
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&verbose),     _("verbose level (each -v increase it)")},
    end_option
};


/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    // format of help: "Usage: progname [args]\n"
    change_helpstring("Usage: %s [args]\n\n\tWhere args are:\n");
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help || argc > 0) showhelp(-1, cmdlnopts);
    return &G;
}

