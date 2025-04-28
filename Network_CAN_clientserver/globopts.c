/*
 * This file is part of the canserver project.
 * Copyright 2025 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include "globopts.h"
#include <usefull_macros.h>

globopts UserOpts = {
    .maxclients = 10,
};

static sl_option_t cmdlnopts[] = {
    {"help",        NO_ARGS,    NULL,   'h',    arg_int,    APTR(&UserOpts.help),       "show this help"},
    {"verbose",     NO_ARGS,    NULL,   'v',    arg_none,   APTR(&UserOpts.verbose),    "verbose level (each -v adds 1)"},
    {"logfile",     NEED_ARG,   NULL,   'l',    arg_string, APTR(&UserOpts.logfile),    "log file name"},
    {"srvnode",     NEED_ARG,   NULL,   'N',    arg_string, APTR(&UserOpts.srvnode),    "server node \"port\", \"name:port\" or path (could be \"\\0path\" for anonymous UNIX-socket)"},
    {"srvunix",     NO_ARGS,    NULL,   'U',    arg_int,    APTR(&UserOpts.srvunix),    "server use UNIX socket instead of INET"},
    {"maxclients",  NEED_ARG,   NULL,   'm',    arg_int,    APTR(&UserOpts.maxclients), "max amount of clients connected to server (default: 2)"},
    {"cltnode",     NEED_ARG,   NULL,   'n',    arg_string, APTR(&UserOpts.cltnode),    "serial client node \"IP\", \"name:IP\" or path (could be \"\\0path\" for anonymous UNIX-socket)"},
    {"cltunix",     NO_ARGS,    NULL,   'u',    arg_int,    APTR(&UserOpts.cltunix),    "serial client use UNIX socket instead of INET"},
    end_option
};

void parseargs(int argc, char **argv){
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(UserOpts.help) sl_showhelp(-1, cmdlnopts);
}
