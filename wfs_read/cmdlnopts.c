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
#include <strings.h>
#include "cmdlnopts.h"
#include "usefull_macros.h"
#include "zernike.h" // for DEFAULT_CRD_STEP & DEFAULT_WF_UNIT

/*
 * here are global parameters initialisation
 */
int help;
static glob_pars  G;
//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
     .inwfs = NULL                      // input WFS file name
    ,.indat = NULL                      // input DAT file name
    ,.outname = NULL                    // output file name prefix (default: basename of input)
    ,.step = DEFAULT_CRD_STEP           // coordinate step in wavefront map
    ,.wfunits = DEFAULT_WF_UNIT         // units for wavefront measurement in WF map
    ,.wavelength = DEFAULT_WAVELENGTH   // default wavelength
    ,.zzero = 0                         // amount of Z polynomials to be reset
    ,.rotangle = 0.                     // wavefront rotation angle (rotate matrix to -rotangle after computing)
    ,.tozero = NULL                     // coefficients to be reset (maybe more than one time)
    ,.addcoef = NULL                    // constant to be added (format x=c, where x is number, c is additive constant)
    ,.scale = 1.                        // Zernike coefficients' scaling factor
};

/*
 * Define command line options by filling structure:
 *  name    has_arg flag    val     type        argptr          help
*/
myoption cmdlnopts[] = {
    // set 1 to param despite of its repeating number:
    {"help",        NO_ARGS,  NULL, 'h',    arg_int,    APTR(&help),        _("show this help")},
    // simple integer parameter with obligatory arg:
    {"wfs",         NEED_ARG, NULL, 'w',    arg_string, APTR(&G.inwfs),     _("input WFS file name")},
    {"dat",         NEED_ARG, NULL, 'd',    arg_string, APTR(&G.indat),     _("input DAT file name")},
    {"output",      NEED_ARG, NULL, 'o',    arg_string, APTR(&G.outname),   _("output file name prefix")},
    {"step",        NEED_ARG, NULL, 's',    arg_double, APTR(&G.step),      _("coordinate step in wavefront map (R=1)")},
    {"wfunits",     NEED_ARG, NULL, 'u',    arg_string, APTR(&G.wfunits),   _("units for wavefront measurement in output WF map")},
    {"wavelength",  NEED_ARG, NULL, 'l',    arg_double, APTR(&G.wavelength),_("default wavelength (in meters, microns or nanometers), 101..9999nm")},
    {"zerofirst",   NEED_ARG, NULL, 'z',    arg_int,    APTR(&G.zzero),     _("amount of first Zernike polynomials to be reset to 0")},
    {"rotangle",    NEED_ARG, NULL, 'r',    arg_double, APTR(&G.rotangle),  _("wavefront rotation angle (degrees, CCW)")},
    {"zero",        MULT_PAR, NULL, '0',    arg_int,    APTR(&G.tozero),    _("reset given Znumber to 0")},
    {"addconst",    MULT_PAR, NULL, 'a',    arg_string, APTR(&G.addcoef),   _("add constant to given Znumber (e.g. -a4=10 adds 10 to Znum=4)")},
    {"scale",       NEED_ARG, NULL, 'S',    arg_double, APTR(&G.scale),     _("Zernike coefficients' scaling factor")},
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
    void *ptr = memcpy(&G, &Gdefault, sizeof(G));
    if(!ptr) ERR(_("Can't memcpy"));
    // format of help: "Usage: progname [args]\n"
    change_helpstring(_("Usage: %s [args]\n\n\tWhere args are:\n"));
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
/*
    int i;
    if(argc > 0){
        G.rest_pars_num = argc;
        G.rest_pars = calloc(argc, sizeof(char*));
        for (i = 0; i < argc; i++)
            G.rest_pars[i] = strdup(argv[i]);
    }*/
    return &G;
}

