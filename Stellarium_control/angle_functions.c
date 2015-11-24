/*
 * angle_functions.c - different functions for angles/times processing in different format
 *
 * Copyright 2015 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <slamac.h>  // SLA macros
#include "bta_shdata.h"
#include "angle_functions.h"
#include "usefull_macros.h"

extern void sla_caldj(int*, int*, int*, double*, int*);
extern void sla_amp(double*, double*, double*, double*, double*, double*);
extern void sla_map(double*, double*, double*, double*, double*,double*, double*, double*, double*, double*);
void slacaldj(int y, int m, int d, double *djm, int *j){
	int iy = y, im = m, id = d;
	sla_caldj(&iy, &im, &id, djm, j);
}
void slaamp(double ra, double da, double date, double eq, double *rm, double *dm ){
	double r = ra, d = da, mjd = date, equi = eq;
	sla_amp(&r, &d, &mjd, &equi, rm, dm);
}
void slamap(double rm, double dm, double pr, double pd,
             double px, double rv, double eq, double date,
             double *ra, double *da){
	double r = rm, d = dm, p1 = pr, p2 = pd, ppx = px, prv = rv, equi = eq, dd = date;
	sla_map(&r, &d, &p1, &p2, &ppx, &prv, &equi, &dd, ra, da);
}

/**
 *  convert angle in seconds into degrees
 * @return angle in range [0..360]
 */
double sec_to_degr(double sec){
	double sign = 1.;
	if(sec < 0.){
		sign = -1.;
		sec = -sec;
	}
	int d = ((int)sec / 3600.);
	sec -= ((double)d) * 3600.;
	d %= 360;
	double angle = sign * (((double)d) + sec / 3600.);
	if(angle < 0.) angle += 360.;
	return (angle);
}

const double jd0 = 2400000.5; // JD for MJD==0

/**
 * Calculate apparent place for given coordinates
 * @param r,d (i)            - RA/Decl for JD2000.0 (RA in hours, Decl in degrees)
 * @param appRA, appDecl (o) - calculated apparent place (in seconds)
 */
void calc_AP(double r, double d, double *appRA, double *appDecl){
	double mjd = 51544.5, pmra = 0., pmdecl = 0.; // mjd2000
	// convert to radians
	r *= DH2R;
	d *= DD2R;
/*	double ra2000, decl2000; // coordinates for 2000.0
	DBG("slaamp(%g, %g, %g, 2000.0, ra, dec)", r,d,mjd);
	slaamp(r, d, mjd, 2000.0, &ra2000, &decl2000);
	DBG("2000: %g, %g", ra2000*DR2H, decl2000*DR2D);
	// proper motion on  R.A./Decl (mas/year)
	double pmra = GP->pmra/1000.*DAS2R, pmdecl = GP->pmdecl/1000.*DAS2R;*/
	mjd = JDate - jd0;
	slamap(r, d, pmra, pmdecl, 0., 0., 2000.0, mjd, &r, &d);
	DBG("APP: %g, %g", r*DR2H, d*DR2D);
	r *= DR2S;
	d *= DR2AS;
	if(appRA) *appRA = r;
	if(appDecl) *appDecl = d;
}

/**
 * convert apparent coordinates (nowadays) to mean (JD2000)
 * appRA, appDecl in seconds
 * r, d in hours & degrees
 */
void calc_mean(double appRA, double appDecl, double *r, double *d){
	double ra, dec;
	appRA *= DS2R;
	appDecl *= DAS2R;
	DBG("appRa: %g, appDecl: %g", appRA, appDecl);
	double mjd = JDate - jd0;
	slaamp(appRA, appDecl, mjd, 2000.0, &ra, &dec);
	ra *= DR2H;
	dec *= DR2D;
	if(r) *r = ra;
	if(d) *d = dec;
}

/**
 * calculate polar coordinates alpha, delta
 * from horizontal az, zd for given siderial time stime
 * ALL IN SECONDS!
 */
void calc_AD(double az, double zd, double stime, double *alpha, double *delta){
	double sin_d, sin_a, cos_a, sin_z, cos_z;
	double t, d, x, y;
	const double cos_fi = 0.7235272793;    /* Cos of SAO latitude     */
	const double sin_fi = 0.6902957888;    /* Sin  ---  ""  -----     */
	DBG("AZ: %g, ZD: %g", az, zd);
	az *= DAS2R;
	zd *= DAS2R;
	sincos(az, &sin_a, &cos_a);
	sincos(zd, &sin_z, &cos_z);

	y = sin_z * sin_a;
	x = cos_a * sin_fi * sin_z + cos_fi * cos_z;
	t = atan2(y, x);
	if (t < 0.0)
		t += 2.0*M_PI;

	sin_d = sin_fi * cos_z - cos_fi * cos_a * sin_z;
	d = asin(sin_d);

	*delta = d * DR2AS;
	*alpha = (stime - t * DR2S);
	if (*alpha < 0.0)
		*alpha += 86400.;      // +24h
	DBG("A: %g, Z: %g, alp: %g, del: %g", az, zd, *alpha, *delta);
}


