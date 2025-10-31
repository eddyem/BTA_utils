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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>

#include "cmdlnopts.h"

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars  G;

//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
    .refresh = 0.5,
    .pidfile = "/tmp/bta_print_header.pid"
};

/*
 * Define command line options by filling structure:
 *  name    has_arg flag    val     type        argptr          help
*/
sl_option_t cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h', arg_none,      APTR(&help),        N_("show this help")},
    {"out",     NEED_ARG,   NULL,   'o', arg_string,    APTR(&G.outfile),   N_("output file name")},
    {"refresh", NEED_ARG,   NULL,   'r', arg_double,    APTR(&G.refresh),   N_("refresh rate (0.1-30s; default: 0.5)")},
    {"pidfile", NEED_ARG,   NULL,   'p', arg_string,    APTR(&G.pidfile),   N_("PID file name")},
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
    // parse arguments
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(help) sl_showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.outfile = strdup(argv[0]);
        if(argc > 1){
            WARNX("%d unused parameters:\n", argc - 1);
            for(int i = 1; i < argc; ++i)
                printf("\t%4d: %s\n", i, argv[i]);
        }
    }
    return &G;
}

