/*
 * zernike.c
 *
 * Copyright 2016 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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

#include <strings.h>
#include "zernike.h"
#include "usefull_macros.h"

// coordinate step on a grid
static double coord_step = DEFAULT_CRD_STEP;
// default wavelength for wavefront (650nm) in meters
static double wavelength = DEFAULT_WAVELENGTH;
// default coefficient to transform vawefront from wavelengths into user value
static double wf_coeff = 1.;

/**
 * Set default coordinate grid step on an unity circle
 * @param step - new step
 * @return 0 if all OK, -1 or 1 if `step` bad
 */
int z_set_step(double step){
    if(step < DBL_EPSILON) return -1;
    if(step > 1.) return 1;
    coord_step = step;
    return 0;
}

double z_get_step(){
    return coord_step;
}

/**
 * Set value of default wavelength
 * @param w - new wavelength (from 100nm to 10um) in meters, microns or nanometers
 * @return 0 if all OK
 */
int z_set_wavelength(double w){
    if(w > 1e-7 && w < 1e-5) // meters
        wavelength = w;
    else if(w > 0.1 && w < 10.) // micron
        wavelength = w * 1e-6;
    else if(w > 100. && w < 10000.) // nanometer
        wavelength = w * 1e-9;
    else return 1;
    return 0;
}

double z_get_wavelength(){
    return wavelength;
}

// thanks to http://stackoverflow.com/a/3875555/1965803
typedef struct{
    double wf_coeff;
    const char * const *units;
} wf_units;

wf_units wfunits[] = {
    { 1.    , (const char * const []){"meter", "m", NULL}},
    {1e-3   , (const char * const []){"millimeter", "mm", NULL}},
    {1e-6   , (const char * const []){"micrometer", "um", "u", NULL}},
    {1e-9   , (const char * const []){"nanometer", "nm", "n", NULL}},
    {-1.    , (const char * const []){"wavelength", "wave", "lambda", "w", "l", NULL}},
    {0.     , (const char * const []){NULL}}
};

/**
 * Set coefficient `wf_coeff` to user defined unit
 */
int z_set_wfunit(char *U){
    wf_units *u = wfunits;
    while(u->units[0]){
        const char * const*unit = u->units;
        while(*unit){
            if(strcasecmp(*unit, U) == 0){
                wf_coeff = u->wf_coeff;
                if(wf_coeff < 0.){ // wavelengths
                    wf_coeff = 1.; // in wavelengths
                }else{ // meters etc
                    wf_coeff = wavelength / wf_coeff;
                }
                printf("wf_coeff = %g\n", wf_coeff);
                return 0;
            }
            ++unit;
        }
        ++u;
    }
    return 1;
}

double z_get_wfcoeff(){
    return wf_coeff;
}

/**
 * Print all wf_units available
 */
void z_print_wfunits(){
    wf_units *u = wfunits;
    printf(_("Unit (meters)\tAvailable values\n"));
    do{
        const char * const*unit = u->units;
        double val = u->wf_coeff;
        if(val > 0.)
            printf("%-8g\t", val);
        else
            printf("(wavelength)\t");
        do{
            printf("%s  ", *unit);
        }while(*(++unit));
        printf("\n");
    }while((++u)->units[0]);
    printf("\n");
}
