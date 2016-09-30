/*
 * readdat.c
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
#include <ctype.h>    // isspace
#include <strings.h>  // strncasecmp
#include "usefull_macros.h"
#include "readdat.h"

/**
 * Get next text line ending with '\n' or '\n\r'
 * @param start (i) - text in current line
 * @param end   (i) - end of text
 * @return            pointer to next line first character (spaces are omit) or NULL
 */
char *nextline(char *start, char* end){
    if(!start || !end) return NULL;
    while(start < end) // get next newline symbol
        if(*start++ == '\n') break;
    while(start < end){ // now skip all spaces, '\r' etc
        if(isspace(*start)) ++start;
        else break;
    }
    if(start == end) return NULL;
    return start;
}

/**
 * Read next double value from .dat file
 * @param begin   (i) - beginning of data portion
 * @param end     (i) - data end
 * @param num     (o) - number read
 * @return              pointer to next data item if line isn't end after current
 *                          or pointer to next '\n' symbol (or `end`) if this value was last in string;
 *                      NULL if there was an error during strtod
 */
char *read_double(char *begin, char *end, double *num){
    char *nextchar;
    if(!num || !begin|| !end || begin >= end) return NULL;
    *num = strtod(begin, &nextchar);
    if(!nextchar) // end of data
        return end;
    if(begin == nextchar || !isspace(*nextchar)){
        char buf[10];
        snprintf(buf, 10, "%s", begin);
        WARNX(_("Not a number %s..."), buf);
        return NULL;
    }
    while(nextchar < end){
        char c = *nextchar;
        if(isspace(c) && c != '\n') ++nextchar;
        else break;
    }
    return nextchar;
}

/**
 * Find in .dat file neader column number named `val`
 * @param val   (i) - name of column to search
 * @param dat   (i) - header start
 * @param end   (i) - data end
 * @return            number of column found or -1
 */
int get_hdrval(char *val, char *dat, char *end){
    size_t l = strlen(val);
    int i = 0;
    while(dat < end){
        char c = 0;
        while(dat < end){ // skip spaces
            c = *dat;
            if(isspace(c) && c != '\n') ++dat;
            else break;
        }
        if(c == '\n' || dat == end) return -1; // end of line or end of data reached
        if(!strncasecmp(dat, val, l)) return i;
        while(dat < end){ // skip non-spaces
            if(!isspace(*dat)) ++dat;
            else break;
        }
        ++i;
    }
    return -1;
}

/**
 * Read .dat file and get Zernike coefficients from it
 * @param fname (i) - .dat file name
 * @param sz    (o) - size of coefficients' array
 * @return            dynamically allocated array or NULL in case of error
 */
double *read_dat_file(char *fname, int *sz){
    if(!fname) return NULL;
    mmapbuf *dbuf = My_mmap(fname);
    if(!dbuf) return NULL;
    char *dat = dbuf->data, *eptr = dat + dbuf->len;
    if(strncasecmp(dat, "time", 4)){
        WARNX(_("Bad header"));
        return NULL;
    }
    int rd = 0, skipfst = get_hdrval("piston", dat, eptr), L = Z_REALLOC_STEP;
    if(skipfst < 0){
        WARNX(_("Dat file don't have OSA Zernike coefficients"));
        return NULL;
    }
    dat = nextline(dat, eptr);
    double *zern = MALLOC(double, Z_REALLOC_STEP);
    while(dat < eptr){
        double d;
        char *next = read_double(dat, eptr, &d);
        if(!next) break;
        dat = next;
        if(skipfst > 0){ // skip this value
            --skipfst;
            continue;
        }
        if(++rd > L){
            L += Z_REALLOC_STEP;
            double *z_realloc = realloc(zern, L * sizeof(double));
            if(!z_realloc){
                WARN(_("Reallocation of memory failed"));
                break;
            }
            zern = z_realloc;
        }
        zern[rd - 1] = d;
        if(*next == '\n') break;
    }
    if(sz) *sz = rd;
    return zern;
}
