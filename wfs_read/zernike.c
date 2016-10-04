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


#define _GNU_SOURCE  (1)  // for math.h
#include <math.h>
#include <strings.h>
#include "zernike.h"
#include "usefull_macros.h"

#ifndef iabs
#define iabs(a)  (((a)<(0)) ? (-a) : (a))
#endif

// coordinate step on a grid
static double coord_step = DEFAULT_CRD_STEP;
// default wavelength for wavefront (650nm) in meters
static double wavelength = DEFAULT_WAVELENGTH;
// default coefficient to transform vawefront from wavelengths into user value
static double wf_coeff = 1.;
// array of factorials 1..100
static double *FK = NULL;
// unit for WF measurement
static char *outpunit = DEFAULT_WF_UNIT;

/**
 * Set default coordinate grid step on an unity circle
 * @param step - new step
 * @return 0 if all OK, -1 or 1 if `step` bad
 */
int z_set_step(double step){
    printf("set to %g\n", step);
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

// for `const char * const *units` thanks to http://stackoverflow.com/a/3875555/1965803
typedef struct{
    double wf_coeff;            // multiplier for wavefront units (in .dat files coefficients are in meters)
    const char * const *units;  // symbol units' names
} wf_units;

wf_units wfunits[] = {
    { 1.    , (const char * const []){"meter", "m", NULL}},
    {1e3    , (const char * const []){"millimeter", "mm", NULL}},
    {1e6    , (const char * const []){"micrometer", "um", "u", NULL}},
    {1e9    , (const char * const []){"nanometer", "nm", "n", NULL}},
    {-1.    , (const char * const []){"wavelength", "wave", "lambda", "w", "l", NULL}},
    {0.     , (const char * const []){NULL}}
};

/**
 * Set coefficient `wf_coeff` to user defined unit
 */
int z_set_wfunit(char *U){
    wf_units *u = wfunits;
    while(u->units[0]){
        const char * const * unit = u->units;
        while(*unit){
            if(strcasecmp(*unit, U) == 0){
                wf_coeff = u->wf_coeff;
                if(wf_coeff < 0.){ // wavelengths
                    wf_coeff = 1./wavelength; // in wavelengths
                }
                outpunit = (char*)u->units[0];
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
        double val = 1./u->wf_coeff;
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

/**
 * Convert polynomial order in Noll notation into n/m
 * @param p (i) - order of Zernike polynomial in Noll notation
 * @param N (o) - order of polynomial
 * @param M (o) - angular parameter
 */
void convert_Zidx(int p, int *N, int *M){
    int n = (int) floor((-1.+sqrt(1.+8.*p)) / 2.);
    if(M) *M = (int)(2.0*(p - n*(n+1.)/2. - 0.5*(double)n));
    if(N) *N = n;
}

/**
 * Generate polar coordinates for grid [-1..1] by both coordinates
 * with default step
 * @param len (o) - size of array
 * @return array of coordinates
 */
polar *gen_coords(int *len){
    int WH = 1 + (int)(2. / coord_step), max_sz = WH * WH, L = 0;
    polar *coordinates = malloc(max_sz * sizeof(polar)), *cptr = coordinates;
    if(!cptr) return NULL;
    double x, y;
    for(y = -1.; y < 1.; y += coord_step){
        for(x = -1.; x < 1.; x += coord_step){
            double R = sqrt(x*x + y*y);
            if(R > 1.) continue;
            cptr->r = R;
            cptr->theta = atan2(y, x);
            ++cptr;
            ++L;
        }
    }
    printf("%d points outside circle (ratio = %g, ideal = %g)\n", max_sz - L, ((double)L)/max_sz, M_PI/4.);
    if(len) *len = L;
    return coordinates;
}

/**
 * Build pre-computed array of factorials from 1 to 100
 */
void build_factorial(){
    double F = 1.;
    int i;
    if(FK) return;
    FK = MALLOC(double, ZERNIKE_MAX_POWER);
    FK[0] = 1.;
    for(i = 1; i < ZERNIKE_MAX_POWER; i++)
        FK[i] = (F *= (double)i);
}

/**
 * Validation check of zernfun parameters
 * return 1 in case of error
 */
int check_parameters(int n, int m, int Sz, polar *P){
    if(Sz < 3 || !P){
        WARNX(_("Size of matrix must be > 2!"));
        return 1;
    }
    if(n > ZERNIKE_MAX_POWER){
        WARNX(_("Order of Zernike polynomial must be <= 100!"));
        return 1;
    }
    int erparm = 0;
    if(n < 0) erparm = 1;
    if(n < iabs(m)) erparm = 1; // |m| must be <= n
    if((n - m) % 2) erparm = 1; // n-m must differ by a prod of 2
    if(erparm)
        WARNX(_("Wrong parameters of Zernike polynomial (%d, %d)"), n, m);
    else
        if(!FK) build_factorial();
    return erparm;
}

/**
 * Build array with R powers (from 0 to n inclusive)
 * @param n     - power of Zernike polinomial (array size = n+1)
 * @param Sz    - size of P array
 * @param P (i) - polar coordinates of points
 */
double **build_rpow(int n, int Sz, polar *P){
    int i, j, N = n + 1;
    double **Rpow = MALLOC(double*, N);
    Rpow[0] = MALLOC(double, Sz);
    for(i = 0; i < Sz; i++) Rpow[0][i] = 1.; // zero's power
    for(i = 1; i < N; i++){ // Rpow - is quater I of cartesian coordinates ('cause other are fully simmetrical)
        Rpow[i] = MALLOC(double, Sz);
        double *rp = Rpow[i], *rpo = Rpow[i-1];
        polar *p = P;
        for(j = 0; j < Sz; j++, rp++, rpo++, p++){
            *rp = (*rpo) * p->r;
        }
    }
    return Rpow;
}

/**
 * Free array of R powers with power n
 * @param Rpow (i) - array to free
 * @param n        - power of Zernike polinomial for that array (array size = n+1)
 */
void free_rpow(double ***Rpow, int n){
	int i, N = n+1;
	for(i = 0; i < N; i++) FREE((*Rpow)[i]);
	FREE(*Rpow);
}

/**
 * Zernike function for scattering data
 * @param n,m     - orders of polynomial
 * @param Sz      - number of points
 * @param P(i)    - array with points coordinates (polar, r<=1)
 * @param norm(o) - (optional) norm coefficient
 * @return dynamically allocated array with Z(n,m) for given array P
 */
double *zernfun(int n, int m, int Sz, polar *P, double *norm){
    if(check_parameters(n, m, Sz, P)) return NULL;
    int j, k, m_abs = iabs(m), iup = (n-m_abs)/2;
    double **Rpow = build_rpow(n, Sz, P);
    double ZSum = 0.;
    // now fill output matrix
    double *Zarr = MALLOC(double, Sz); // output matrix
    double *Zptr = Zarr;
    polar *p = P;
    for(j = 0; j < Sz; j++, p++, Zptr++){
        double Z = 0.;
        if(p->r > 1.) continue; // throw out points with R>1
        // calculate R_n^m
        for(k = 0; k <= iup; k++){ // Sum
            double z = (1. - 2. * (k % 2)) * FK[n - k]        //       (-1)^k * (n-k)!
                /(//----------------------------------- -----   -------------------------------
                    FK[k]*FK[(n+m_abs)/2-k]*FK[(n-m_abs)/2-k] // k!((n+|m|)/2-k)!((n-|m|)/2-k)!
                );
            Z += z * Rpow[n-2*k][j];  // *R^{n-2k}
        }
        // normalize
        double eps_m = (m) ? 1. : 2.;
        Z *= sqrt(2.*(n+1.) / M_PI / eps_m  );
        double m_theta = (double)m_abs * p->theta;
        // multiply to angular function:
        if(m){
            if(m > 0)
                Z *= cos(m_theta);
            else
                Z *= sin(m_theta);
        }
        *Zptr = Z;
        ZSum += Z*Z;
    }
    if(norm) *norm = ZSum;
    // free unneeded memory
    free_rpow(&Rpow, n);
    return Zarr;
}

/**
 * Restoration of image in points P by Zernike polynomials' coefficients
 * @param Zsz  (i) - number of actual elements in coefficients array
 * @param Zidxs(i) - array with Zernike coefficients
 * @param Sz, P(i) - number (Sz) of points (P)
 * @return restored image
 */
double *Zcompose(int Zsz, double *Zidxs, int Sz, polar *P){
    int i;
    double *image = MALLOC(double, Sz);
    for(i = 0; i < Zsz; i++){ // now we fill array
        double K = Zidxs[i];
        if(fabs(K) < DBL_EPSILON) continue; // 0.0
        int n, m;
        convert_Zidx(i, &n, &m);
        double *Zcoeff = zernfun(n, m, Sz, P, NULL);
        int j;
        double *iptr = image, *zptr = Zcoeff;
        for(j = 0; j < Sz; j++, iptr++, zptr++)
            *iptr += K * (*zptr); // add next Zernike polynomial
        FREE(Zcoeff);
    }
    return image;
}

/**
 * Save restored wavefront into file `filename`
 * @param Sz    - size of `P`
 * @param P (i) - points coordinates
 * @param Z (i) - wavefront shift (in lambdas)
 * @param filename (i) - name of output file
 * @return 1 if failed
 */
int z_save_wavefront(int Sz, polar *P, double *Z, char *filename){
    if(!P || !Z || Sz < 0 || !filename) return 1;
    FILE *f = fopen(filename, "w");
    if(!f) return 1;
    fprintf(f, "# X (-1..1)\tY (-1..1)\tZ (%ss)\n", outpunit);
    int i;
    for(i = 0; i < Sz; ++i, ++P, ++Z){
        double x, y, s, c, r = P->r;
        sincos(P->theta, &s, &c);
        x = r * c, y = r * s;
        fprintf(f, "%g\t%g\t%g\n", x, y, (*Z) * wf_coeff);
    }
    fclose(f);
    return 0;
}
