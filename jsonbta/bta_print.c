/*
 * bta_print.c
 *
 * Copyright Vladimir S. Shergin <vsher@sao.ru>
 *
 * some changes (2013) Edward V. Emelianoff <eddy@sao.ru>
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

#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <crypt.h>
#include <assert.h>
#include <slamac.h>  // SLA macros

//#include "sofa.h"

#include "bta_shdata.h"
#define BTA_PRINT_C
#include "bta_json.h"

#define BUFSZ 255
static char buf[BUFSZ+1];

char *double_asc(double d, char *fmt){
	if(!fmt) snprintf(buf, BUFSZ, "%.6f", d);
	else snprintf(buf, BUFSZ, fmt, d);
	return buf;
}

char *time_asc(double t){
	int h, min;
	double sec;
	h   = (int)(t/3600.);
	min = (int)((t - (double)h*3600.)/60.);
	sec = t - (double)h*3600. - (double)min*60.;
	h %= 24;
	if(sec>59.99) sec=59.99;
	snprintf(buf, BUFSZ, "\"%02d:%02d:%05.2f\"", h,min,sec);
	return buf;
}

char *angle_asc(double a){
	char s;
	int d, min;
	double sec;
	if (a >= 0.)
		s = '+';
	else {
		s = '-'; a = -a;
	}
	d   = (int)(a/3600.);
	min = (int)((a - (double)d*3600.)/60.);
	sec = a - (double)d*3600. - (double)min*60.;
	d %= 360;
	if(sec>59.9) sec=59.9;
	snprintf (buf, BUFSZ, "\"%c%02d:%02d:%04.1f\"", s,d,min,sec);
	return buf;
}

char *angle_fmt(double a, char *format){
	char s, *p;
	int d, min, n;
	double sec, msec;
	char *newformat = calloc(strlen(format) + 3, 1);
	assert(newformat);
	sprintf(newformat, "\"%s\"", format);
	if (a >= 0.)
		s = '+';
	else {
		s = '-'; a = -a;
	}
	d   = (int)(a/3600.);
	min = (int)((a - (double)d*3600.)/60.);
	sec = a - (double)d*3600. - (double)min*60.;
	d %= 360;
	if ((p = strchr(format,'.')) == NULL)
		msec=59.;
	else if (*(p+2) == 'f' ) {
		n = *(p+1) - '0';
		msec = 60. - pow(10.,(double)(-n));
	} else
		msec=60.;
	if(sec>msec) sec=msec;
	if (strstr(format,"%c"))
		snprintf(buf, BUFSZ, newformat, s,d,min,sec);
	else
		snprintf(buf, BUFSZ, newformat, d,min,sec);
	free(newformat);
	return buf;
}

#ifndef PI
#define PI 3.14159265358979323846      /* pi */
#endif

#define R2D 180./PI     /* rad. to degr.  */
#define D2R PI/180.     /* degr. to rad. */
#define R2S 648000./PI  /* rad. to sec  */
#define S2R PI/648000.  /* sec. to rad. */
#define S360 1296000.   /* sec in 360degr */

const double longitude=149189.175;   /* SAO longitude 41 26 29.175 (-2:45:45.945)*/
const double Fi=157152.7;            /* SAO latitude 43 39 12.7 */
const double cos_fi=0.7235272793;    /* Cos of SAO latitude     */
const double sin_fi=0.6902957888;    /* Sin  ---  ""  -----     */

static void calc_AZ(double alpha, double delta, double stime, double *az, double *zd){
	double sin_t,cos_t, sin_d,cos_d,  cos_z;
	double t, d, z, a, x, y;

	t = (stime - alpha) * 15.;
	if (t < 0.)
		t += S360;      // +360degr
	t *= S2R;          // -> rad
	d = delta * S2R;
	sin_t = sin(t);
	cos_t = cos(t);
	sin_d = sin(d);
	cos_d = cos(d);

	cos_z = cos_fi * cos_d * cos_t + sin_fi * sin_d;
	z = acos(cos_z);

	y = cos_d * sin_t;
	x = cos_d * sin_fi * cos_t - cos_fi * sin_d;
	a = atan2(y, x);

	*zd = z * R2S;
	*az = a * R2S;
}

static double calc_PA(double alpha, double delta, double stime){
	double sin_t,cos_t, sin_d,cos_d;
	double t, d, p, sp, cp;

	t = (stime - alpha) * 15.;
	if (t < 0.)
		t += S360;      // +360degr
	t *= S2R;          // -> rad
	d = delta * S2R;
	sin_t = sin(t);
	cos_t = cos(t);
	sin_d = sin(d);
	cos_d = cos(d);

	sp = sin_t * cos_fi;
	cp = sin_fi * cos_d - sin_d * cos_fi * cos_t;
	p = atan2(sp, cp);
	if (p < 0.0)
		p += 2.0*PI;

	return(p * R2S);
}

/*
void calc2000(double *ra, double *dec){
	double elong, phi, utc1, utc2;
	struct tm tms;
	time_t t = time(NULL);
	gmtime_r(&t, &tms);
	int y, m, d;
	y = 1900 + tms.tm_year;
	m = tms.tm_mon + 1;
	d = tms.tm_mday;
	iauDtf2d("UTC", y, m, d, tms.tm_hour, tms.tm_min, tms.tm_sec, &utc1, &utc2);
	iauAf2a ( '+', 41, 26, 29.175, &elong );
	iauAf2a ( '+', 43, 39, 12.69, &phi );
	//iauAtoc13("R", val_Alp * DS2R, val_Del * DAS2R, utc1, utc2, DUT1,
	iauAtoc13("R", InpAlpha * DS2R, InpDelta * DAS2R, utc1, utc2, DUT1,
		elong, phi, 2070.0, polarX, polarY, Pressure/0.76, Temper,
		val_Hmd/100., 0.55, ra, dec);
	int i[4];
	char pm;
	iauA2tf ( 7, *ra, &pm, i );
	printf ( " %2.2d %2.2d %2.2d.%7.7d", i[0],i[1],i[2],i[3] );
	iauA2af ( 6, *dec, &pm, i );
	printf ( " %c%2.2d %2.2d %2.2d.%6.6d\n", pm, i[0],i[1],i[2],i[3] );
	printf("DUT: %g, x:%g, y:%g, pres: %g, temp: %g, hum: %g%%\n", DUT1,
		 polarX, polarY, Pressure/0.76, Temper, val_Hmd/100.);
}*/

extern void sla_amp(double*, double*, double*, double*, double*, double*);

void slaamp(double ra, double da, double date, double eq, double *rm, double *dm ){
	double r = ra, d = da, mjd = date, equi = eq;
	sla_amp(&r, &d, &mjd, &equi, rm, dm);
}
const double jd0 = 2400000.5; // JD for MJD==0
/**
 * convert apparent coordinates (nowadays) to mean (JD2000)
 * appRA, appDecl in seconds
 * r, d in seconds
 */
void calc_mean(double appRA, double appDecl, double *r, double *d){
	double ra, dec;
	appRA *= DS2R;
	appDecl *= DAS2R;
	DBG("appRa: %g, appDecl: %g", appRA, appDecl);
	double mjd = JDate - jd0;
	slaamp(appRA, appDecl, mjd, 2000.0, &ra, &dec);
	ra *= DR2S;
	dec *= DR2AS;
	if(r) *r = ra;
	if(d) *d = dec;
}

void make_JSON(int sock, bta_pars *par){
	bool ALL = par->ALL;
	// print next JSON pair; par, val - strings
	void sendstr(char *str){
		size_t L = strlen(str);
		if(send(sock, str, L, 0) != L) exit(-1);
	}
	int json_send(char *par, char *val){
		char buf[256];
		int L = snprintf(buf, 255, ",\n\"%s\": %s", par, val);
		if(send(sock, buf, L, 0) != L) return -1;
		return 0;
	}
	int json_send_s(char *par, char *val){
		char buf[256];
		int L = snprintf(buf, 255, ",\n\"%s\": \"%s\"", par, val);
		if(send(sock, buf, L, 0) != L) return -1;
		return 0;
	}
	#define JSON(p, val) do{if(json_send(p, val)) exit(-1);} while(0)
	#define JSONSTR(p, val) do{if(json_send_s(p, val)) exit(-1);} while(0)
	get_shm_block( &sdat, ClientSide);
	char *str;
	if(!check_shm_block(&sdat)) exit(-1);
	// beginning of json object
	sendstr("{\n\"ACS_BTA\": true");

	// mean local time
	if(ALL || par->mtime)
		JSON("M_time", time_asc(M_time+DUT1));
	// Mean Sidereal Time
	if(ALL || par->sidtime){
		#ifdef EE_time
			JSON("JDate", double_asc(JDate, NULL));
			str = time_asc(S_time-EE_time);
		#else
			str = time_asc(S_time);
		#endif
		JSON("S_time", str);
	}
	// Telecope mode
	if(ALL || par->telmode){
		if(Tel_Hardware == Hard_Off) str = "Off";
		else if(Tel_Mode != Automatic) str = "Manual";
		else{
			switch (Sys_Mode){
				default:
				case SysStop    :  str = "Stopping";  break;
				case SysWait    :  str = "Waiting";   break;
				case SysPointAZ :
				case SysPointAD :  str = "Pointing";  break;
				case SysTrkStop :
				case SysTrkStart:
				case SysTrkMove :
				case SysTrkSeek :  str = "Seeking";   break;
				case SysTrkOk   :  str = "Tracking";  break;
				case SysTrkCorr :  str = "Correction";break;
				case SysTest    :  str = "Testing";   break;
			}
		}
		JSONSTR("Tel_Mode", str);
	}
	// Telescope focus
	if(ALL || par->telfocus){
		switch (Tel_Focus){
			default:
			case Prime    : str = "Prime";     break;
			case Nasmyth1 : str = "Nasmyth1";  break;
			case Nasmyth2 : str = "Nasmyth2";  break;
		}
		JSONSTR("Tel_Focus", str);
		JSON("ValFoc", double_asc(val_F, "%0.2f"));
	}
	// Telescope target
	if(ALL || par->target){
		switch (Sys_Target) {
			default:
			case TagObject   : str = "Object";   break;
			case TagPosition : str = "A/Z-Pos."; break;
			case TagNest     : str = "Nest";     break;
			case TagZenith   : str = "Zenith";   break;
			case TagHorizon  : str = "Horizon";  break;
		}
		JSONSTR("Tel_Taget", str);
	}
	// Mode of P2
	if(ALL || par->p2mode){
		if(Tel_Hardware == Hard_On){
			switch (P2_State) {
				default:
				case P2_Off   : str = "Stop";  break;
				case P2_On    : str = "Track"; break;
				case P2_Plus  : str = "Move+"; break;
				case P2_Minus : str = "Move-"; break;
			}
		} else str = "Off";
		JSONSTR("P2_Mode", str);
	}
	// Equatorial coordinates
	if(ALL || par->eqcoor){
		JSON("CurAlpha", time_asc(CurAlpha));
		JSON("CurDelta", angle_asc(CurDelta));
		JSON("SrcAlpha", time_asc(SrcAlpha));
		JSON("SrcDelta", angle_asc(SrcDelta));
		JSON("InpAlpha", time_asc(InpAlpha));
		JSON("InpDelta", angle_asc(InpDelta));
		JSON("TelAlpha", time_asc(val_Alp));
		JSON("TelDelta", angle_asc(val_Del));
		double a2000, d2000;
		calc_mean(InpAlpha, InpDelta, &a2000, &d2000);
		JSON("InpRA2000", time_asc(a2000));
		JSON("InpDec2000", angle_asc(d2000));
		calc_mean(CurAlpha, CurDelta, &a2000, &d2000);
		JSON("CurRA2000", time_asc(a2000));
		JSON("CurDec2000", angle_asc(d2000));
	}
	// Horizontal coordinates
	if(ALL || par->horcoor){
		JSON("InpAzim", angle_fmt(InpAzim,"%c%03d:%02d:%04.1f"));
		JSON("InpZenD", angle_fmt(InpZdist,"%02d:%02d:%04.1f"));
		JSON("CurAzim", angle_fmt(tag_A,"%c%03d:%02d:%04.1f"));
		JSON("CurZenD", angle_fmt(tag_Z,"%02d:%02d:%04.1f"));
		JSON("CurPA", angle_fmt(tag_P,"%03d:%02d:%04.1f"));
		JSON("SrcPA", angle_fmt(calc_PA(SrcAlpha,SrcDelta,S_time),"%03d:%02d:%04.1f"));
		JSON("InpPA", angle_fmt(calc_PA(InpAlpha,InpDelta,S_time),"%03d:%02d:%04.1f"));
		JSON("TelPA", angle_fmt(calc_PA(val_Alp, val_Del, S_time),"%03d:%02d:%04.1f"));
	}
	// Values from sensors
	if(ALL || par->valsens){
		JSON("ValAzim", angle_fmt(val_A,"%c%03d:%02d:%04.1f"));
		JSON("ValZenD", angle_fmt(val_Z,"%02d:%02d:%04.1f"));
		JSON("ValP2", angle_fmt(val_P,"%03d:%02d:%04.1f"));
		JSON("ValDome", angle_fmt(val_D,"%c%03d:%02d:%04.1f"));
	}
	// Differences
	if(ALL || par->diff){
		JSON("DiffAzim", angle_fmt(Diff_A,"%c%03d:%02d:%04.1f"));
		JSON("DiffZenD", angle_fmt(Diff_Z,"%c%02d:%02d:%04.1f"));
		JSON("DiffP2", angle_fmt(Diff_P,"%c%03d:%02d:%04.1f"));
		JSON("DiffDome", angle_fmt(val_A-val_D,"%c%03d:%02d:%04.1f"));
	}
	// Velocities
	if(ALL || par->vel){
		JSON("VelAzim", angle_fmt(vel_A,"%c%02d:%02d:%04.1f"));
		JSON("VelZenD", angle_fmt(vel_Z,"%c%02d:%02d:%04.1f"));
		JSON("VelP2", angle_fmt(vel_P,"%c%02d:%02d:%04.1f"));
		JSON("VelPA", angle_fmt(vel_objP,"%c%02d:%02d:%04.1f"));
		JSON("VelDome", angle_fmt(vel_D,"%c%02d:%02d:%04.1f"));
	}
	// Correction
	if(ALL || par->corr){
		double curA,curZ,srcA,srcZ;
		double corAlp,corDel,corA,corZ;
		if(Sys_Mode==SysTrkSeek||Sys_Mode==SysTrkOk||Sys_Mode==SysTrkCorr){
			corAlp = CurAlpha-SrcAlpha;
			corDel = CurDelta-SrcDelta;
			if(corAlp >  23*3600.) corAlp -= 24*3600.;
			if(corAlp < -23*3600.) corAlp += 24*3600.;
			calc_AZ(SrcAlpha, SrcDelta, S_time, &srcA, &srcZ);
			calc_AZ(CurAlpha, CurDelta, S_time, &curA, &curZ);
			corA=curA-srcA;
			corZ=curZ-srcZ;
		}else{
			corAlp = corDel = corA = corZ = 0.;
		}
		JSON("CorrAlpha", angle_fmt(corAlp,"%c%01d:%02d:%05.2f"));
		JSON("CorrDelta", angle_fmt(corDel,"%c%01d:%02d:%04.1f"));
		JSON("CorrAzim", angle_fmt(corA,"%c%01d:%02d:%04.1f"));
		JSON("CorrZenD", angle_fmt(corZ,"%c%01d:%02d:%04.1f"));
	}
	// meteo
	if(ALL || par->meteo){
		JSON("ValTout", double_asc(val_T1, "%05.1f"));
		JSON("ValTind", double_asc(val_T2, "%05.1f"));
		JSON("ValTmir", double_asc(val_T3, "%05.1f"));
		JSON("ValPres", double_asc(val_B, "%05.1f"));
		JSON("ValWind", double_asc(val_Wnd, "%04.1f"));
		if(Wnd10_time>0.1 && Wnd10_time<=M_time) {
			JSON("Blast10", double_asc((M_time-Wnd10_time)/60, "%.1f"));
			JSON("Blast15", double_asc((M_time-Wnd15_time)/60, "%.1f"));
		}
		JSON("ValHumd", double_asc(val_Hmd, "%04.1f"));
		if(Precip_time>0.1 && Precip_time<=M_time)
			JSON("Precipt", double_asc((M_time-Precip_time)/60, "%.1f"));
	}
	// end of json onject
	sendstr("\n}\n");
}

