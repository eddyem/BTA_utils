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

/*
 * here are some typedef's for global data
 */
typedef struct{
    char *inwfs;        // input WFS file name
    char *indat;        // input DAT file name
    char *outname;      // output file name prefix
    double step;        // coordinate step in wavefront map
    char *wfunits;      // units for wavefront measurement in WF map
    double wavelength;  // default wavelength
    int zzero;          // amount of Z polynomials to be reset
    int **tozero;       // coefficients to be reset (maybe more than one time)
    char **addcoef;     // constant to be added (format x=c, where x is number, c is additive constant)
    double rotangle;    // wavefront rotation angle (rotate matrix to -rotangle after computing)
    double scale;       // Zernike coefficients' scaling factor
} glob_pars;


// default & global parameters
extern glob_pars const Gdefault;

glob_pars *parse_args(int argc, char **argv);
#endif // __CMDLNOPTS_H__
