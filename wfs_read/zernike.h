/*
 * zernike.h
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
#pragma once
#ifndef __ZERNIKE_H__
#define __ZERNIKE_H__

// default step in coordinate grid
#define DEFAULT_CRD_STEP  (0.05)
// default wavefront unit: lambda
#define DEFAULT_WF_UNIT "meter"
// default wavelength
#define DEFAULT_WAVELENGTH (0.65e-6)
// max power of Zernike polynomial
#define ZERNIKE_MAX_POWER  (100)


typedef struct{
    double r,theta;
} polar;

int z_set_step(double step);
double z_get_step();

int z_set_wavelength(double w);
double z_get_wavelength();

int z_set_wfunit(char *u);
double z_get_wfcoeff();
void z_print_wfunits();

void convert_Zidx(int p, int *N, int *M);

polar *gen_coords(int *len);

double *Zcompose(int Zsz, double *Zidxs, int Sz, polar *P);

int z_save_wavefront(int Sz, polar *P, double *Z, char *filename);
#endif // __ZERNIKE_H__
