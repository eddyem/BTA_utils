/*
 * Read WCS FITS headers from files given in list and calculate median values of
 * CROTA2 and CDELT1/CDELT2
 * main.c
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

#include "usefull_macros.h"
#include <math.h>

#ifndef DBL_EPSILON
#define DBL_EPSILON    (2.2204460492503131e-16)
#endif

void signals(int sig){ exit(sig); }

/**
 * return pointer to first symbol after '=' of key found or NULL
 */
char* get_key(double *keyval, char *keyname, char *hdu){
	char *found = strstr(hdu, keyname);
	if(!found) return NULL;
	if(found[8] != '=') return NULL;
	found += 9;
	if(!keyval) return found;
	char *eptr;
	*keyval = strtod(found, &eptr);
	if(eptr == found) return NULL;
	return found;
}

/* convert angle ang to [0..360] */
double rev360(double ang){
	while(ang < 0.) ang += 360.;
	while(ang > 360.) ang -= 360.;
	return ang;
}

/* convert angle ang to [-180..180] */
double rev180(double ang){
	while(ang < -180.) ang += 360.;
	while(ang > 180.) ang -= 360.;
	return ang;
}

typedef double Item;

#define ELEM_SWAP(a, b) {register Item t = a; a = b; b = t;}
#define PIX_SORT(a, b)  {if (a > b) ELEM_SWAP(a, b);}
/**
 * quick select - algo for approximate median calculation for array idata of size n
 */
Item quick_select(Item *idata, int n){
	int low, high;
	int median;
	int middle, ll, hh;
	Item *arr = MALLOC(Item, n);
	memcpy(arr, idata, n*sizeof(Item));
	low = 0 ; high = n-1 ; median = (low + high) / 2;
	for(;;){
		if(high <= low) // One element only
			break;
		if(high == low + 1){ // Two elements only
			PIX_SORT(arr[low], arr[high]) ;
			break;
		}
		// Find median of low, middle and high items; swap into position low
		middle = (low + high) / 2;
		PIX_SORT(arr[middle], arr[high]) ;
		PIX_SORT(arr[low], arr[high]) ;
		PIX_SORT(arr[middle], arr[low]) ;
		// Swap low item (now in position middle) into position (low+1)
		ELEM_SWAP(arr[middle], arr[low+1]) ;
		// Nibble from each end towards middle, swapping items when stuck
		ll = low + 1;
		hh = high;
		for(;;){
			do ll++; while (arr[low] > arr[ll]);
			do hh--; while (arr[hh] > arr[low]);
			if(hh < ll) break;
			ELEM_SWAP(arr[ll], arr[hh]) ;
		}
		// Swap middle item (in position low) back into correct position
		ELEM_SWAP(arr[low], arr[hh]) ;
		// Re-set active partition
		if (hh <= median) low = ll;
		if (hh >= median) high = hh - 1;
	}
	Item ret = arr[median];
	FREE(arr);
	return ret;
}
#undef PIX_SORT
#undef ELEM_SWAP

// calculate standard deviation between array `arr` and value `val`, N - length of `arr`
double get_std(double *arr, double val, int N){
	double diff = 0.;
	int i;
	for(i = 0; i < N; ++i, ++arr){
		double d = *arr - val;
		diff += d*d; // SUM(X - val)^2
	}
	DBG("diff: %g", diff);
	return sqrt(diff/N); // AVER[(X - val)^2]
}

/**
 * Remove data outliers (>3RMS) & do correction for angles near 360degr
 * @param buf (io) - buffer with data
 * @param N (io)   - data length
 * @param med      - median data value
 * @param std      - RMS of data values
 */
void rm_outliers(double **buf, int *N, double med, double std){
	if(std > med/2.) std = med/2.; // standard deviation can't be so large
	std *= 3.;
	int L = 0, i, oN = *N;
	double *ret = MALLOC(double, oN), *rptr = ret, *iptr = *buf;
	for(i = 0; i < oN; ++i, ++iptr){
		double d = *iptr;
		if(fabs(d - med) > std){
			if(fabs(d - 360.) < std && med < std) // angle near 360 and median near 0
				d -= 360.;
			else if(d < std && fabs(med - 360.) < std) // vice versa
				d += 360.;
		}
		if(fabs(d - med) < std){
			*rptr++ = d;
			++L;
		}else printf("Outlier: %g (median=%g, 3RMS=%g)\n", d, med, std);
	}
	if(L != *N) printf("%d outliers removed (from %d)\n\n", *N-L, *N);
	free(*buf);
	*buf = ret;
	*N = L;
}

int main(int argc, char **argv){
	initial_setup();
	if(argc == 1)
		ERRX("Usage: %s <filename[s]>\n", argv[0]);
	int c, N = 0;
	double *arr = MALLOC(double, argc-1), *curdata = arr;
	double *scales = MALLOC(double, argc-1), *curscale = scales;
	for(c = 1; c < argc; ++c){
		green("\nFile: %s\n", argv[c]);
		mmapbuf *map = My_mmap(argv[c]);
		if(!map) goto unm;
		char *hdr = map->data;
		char hdrend[81] = {'E', 'N', 'D'};
		int i,j;
		for(i = 3; i < 81; ++i) hdrend[i] = ' ';
		hdrend[80] = 0;
		char *e = strstr(hdr, hdrend);
		if(!e){
			WARNX("Can't find HDU end");
			goto unm;
		}
		size_t sz = e - hdr + 1;
		DBG("len = %zd", sz);
		char *header = MALLOC(char, sz);
		memcpy(header, hdr, sz);
		double parangle, rotangle, CD[3][3];
		if(get_key(&parangle, "PARANGLE", header))
			printf("PARANGLE=%g\n", parangle);
		else goto unm;
		if((header = get_key(&rotangle, "ROTANGLE", header))){
			double r = rotangle;
			if((get_key(&rotangle, "ROTANGLE", header))){
				printf("ROTANGLE=%g\n", rotangle);
			}else{
				if(fabs(r-parangle) > DBL_EPSILON){
					rotangle = r;
					printf("ROTANGLE=%g\n", rotangle);
				}
				else goto unm;
			}
		}else goto unm;
		for(i = 1; i < 3; ++i) for(j = 1; j < 3; ++j){
			char val[9];
			snprintf(val, 8, "CD%d_%d", i,j);
			if(!get_key(&CD[i][j], val, header)) goto unm;
			printf("CD[%d][%d]=%g\n", i,j, CD[i][j]);
		}
		double angle = atan2(CD[2][1], CD[2][2])*180/M_PI;
		int dir = 1;
		if(CD[1][1]*CD[2][2] < 0. || CD[1][2] * CD[2][1] > 0.) dir = -1; // left-sided
		// shortest transit from DEC axis to RA axis: counter-clock wise or clock-wise
		printf("angle: %g, dir: %s\n", angle, dir > 0 ? "CW" : "CCW");
		double R0;
		if(dir < 0){
			R0 = rev360(angle + rotangle - parangle);
			printf("angle + ROTANGLE - PARANGLE = %g\n", R0);
		}else{
			R0 = rev360(angle - rotangle + parangle);
			printf("angle - ROTANGLE + PARANGLE = %g\n", R0);
		}
		*curdata++ = R0;
		++N;
		double sc0;
		// who is nearest to 1: sin or cos?
		double an = fabs(rev180(angle));
		if(an > 135. || an < 45.){ // sin - CD[1][2]
			sc0 = fabs(CD[1][2]*3600./sin(angle*M_PI/180));
			DBG("sin");
		}else{ // cos - CD[1][1]
			sc0 = fabs(CD[1][1]*3600./cos(angle*M_PI/180));
			DBG("cos");
		}
		printf("Scale: %g''/pix\n", sc0);
		*curscale++ = sc0;
		unm:
		My_munmap(map);
	}
	if(N > 1){
		printf("\n\n");
		double R0 = 1., scale = 1., Rstd = 0., scstd = 0.;
		int badsR = 0, badssc = 0, k, Nsc = N, Nr = N;
		for(k = 0; k < 3; ++k){ // twice remove outliers
			if(badsR || Rstd > 0.05*R0){
				rm_outliers(&arr, &Nr, R0, Rstd);
				badsR = 0;
			}
			if(badssc || scstd > 0.05*scale){
				rm_outliers(&scales, &Nsc, scale, scstd);
				badssc = 0;
			}
			R0 = quick_select(arr, Nr);
			scale = quick_select(scales, Nsc);
			curdata = arr; curscale = scales;
			Rstd = get_std(arr, R0, Nr);
			scstd = 3.*get_std(scales, scale, Nsc);
			double _3Rstd = 3.*Rstd, _3scstd = 3.*scstd;
			for(c = 0; c < Nr; ++c, ++curdata)
				if(fabs(*curdata - R0) > _3Rstd){
					++badsR;
				}
			for(c = 0; c < Nsc; ++c, ++curscale){
				if(fabs(*curscale - scale) > _3scstd) ++badssc;
			}
		}
		green("Median values: ROT0=%g (%d values > 3std), std=%g), CDELT1/CDELT2=%g (%d values > 3std), std=%g\n", R0, badsR,
			Rstd, scale, badssc, scstd);
		if(badsR > Nr/2) WARNX("Very bad statistics for ROT0, not safe value!");
		if(badssc > Nsc/2) WARNX("Very bad statistics for CDELT, not safe value!");
	}
	return 0;
}
