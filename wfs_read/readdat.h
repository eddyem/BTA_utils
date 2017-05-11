/*
 * readdat.h
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
#ifndef __READDAT_H__
#define __READDAT_H__

// allocate memory with quantum of this
#define Z_REALLOC_STEP  (10)

typedef struct{
    mmapbuf *buf;   // mmaped buffer
    char *curptr;   // pointer to current symbol
    char *eptr;     // pointer to end of file
    int firstcolumn;// first column with Zernike coefficients
    double **Rpow;  // powers of R
} datfile;

double *dat_read_next_line(datfile *dat, int *sz);
datfile *open_dat_file(char *fname);
void close_dat_file(datfile *dat);

#endif // __READDAT_H__
