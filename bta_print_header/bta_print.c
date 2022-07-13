/*
 * This file is part of the btaprinthdr project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <usefull_macros.h>

#include "bta_print.h"
#include "bta_shdata.h"


static char buf[1024];
char *time_asc(double t){
	int h, m;
	double s;
	h   = (int)(t/3600.);
	m = (int)((t - (double)h*3600.)/60.);
	s = t - (double)h*3600. - (double)m*60.;
	h %= 24;
	if(s>59) s=59;
	sprintf(buf, "%d:%02d:%04.1f", h,m,s);
	return buf;
}

char *angle_asc(double a){
	char s;
	int d, min;
	double sec;
	if (a >= 0.)
		s = '+';
	else{
		s = '-'; a = -a;
	}
	d   = (int)(a/3600.);
	min = (int)((a - (double)d*3600.)/60.);
	sec = a - (double)d*3600. - (double)min*60.;
	d %= 360;
	if(sec>59.9) sec=59.9;
	sprintf(buf, "%c%d:%02d:%04.1f", s,d,min,sec);
	return buf;
}

/**
 * @brief printhdr - write FITS record into output file
 * @param fd   - fd to write
 * @param key  - key
 * @param val  - value
 * @param cmnt - comment
 * @return 0 if all OK
 */
static int printhdr(int fd, const char *key, const char *val, const char *cmnt){
    char tmp[81];
    char tk[9];
    if(strlen(key) > 8){
        strncpy(tk, key, 8);
        key = tk;
    }
    if(cmnt){
        snprintf(tmp, 81, "%-8s= %-21s / %s", key,  val, cmnt);
    }else{
        snprintf(tmp, 81, "%-8s= %s", key, val);
    }
    size_t l = strlen(tmp);
    tmp[l] = '\n';
    ++l;
    if(write(fd, tmp, l) != (ssize_t)l){
        WARN("write()");
        return 1;
    }
    return 0;
}

#define WRHDR(k, v, c)  do{if(printhdr(fd, k, v, c)){goto returning;}}while(0)
int print_header(_U_ const char *path){
    int ret = FALSE;
    double dtmp;
    char val[23], comment[71];
#define COMMENT(...) do{snprintf(comment, 70, __VA_ARGS__);}while(0)
#define VAL(fmt, x) do{snprintf(val, 22, fmt, x);}while(0)
#define VALD(x) VAL("%.10f", x)
#define VALS(x) VAL("'%s'", x)
    int l = strlen(path) + 7;
    char *aname = MALLOC(char, l);
    snprintf(aname, l, "%sXXXXXX", path);
    int fd = mkstemp(aname);
    if(fd < 0){
        WARN("Can't write header file, mkstemp()");
        FREE(aname);
        return FALSE;
    }
    fchmod(fd, 0644);
    WRHDR("TELESCOP", "'BTA 6m telescope'", "Telescope name");
    WRHDR("ORIGIN", "'SAO RAS'", "Organization responsible for the data");
    dtmp = S_time-EE_time;
    COMMENT("Sidereal time, seconds: %s", time_asc(dtmp));
    VALD(dtmp);
    WRHDR("ST", val, comment);
    COMMENT("Universal time, seconds: %s", time_asc(M_time));
    VALD(M_time);
    WRHDR("UT", val, comment);
    VALD(JDate);
    WRHDR("JD", val, "Julian date");
    switch(Tel_Focus){
		default:
		case Prime:
			VALS("Prime"); break;
		case Nasmyth1:
            VALS("Nasmyth1");  break;
		case Nasmyth2:
            VALS("Nasmyth2");  break;
	}
    WRHDR("FOCUS", val, "Observation focus");
    VAL("%.2f", val_F);
    WRHDR("VAL_F", val, "Focus value of telescope (mm)");
#define RA(ra, dec, text, pref) do{VALD(ra*15./3600.); COMMENT(text " R.A. (degr): %s", angle_asc(15.*ra)); WRHDR(pref "RA", val, comment); \
        VALD(dec/3600.); COMMENT(text " Decl (degr): %s", angle_asc(dec)); WRHDR(pref "DEC", val, comment);}while(0)
    RA(CurAlpha, CurDelta, "Current object", "");
    RA(SrcAlpha, SrcDelta, "Source", "S_");
    RA(val_Alp, val_Del, "Telescope", "T_");
    VALD(tag_A/3600.);
    COMMENT("Target Az (degr): %s", angle_asc(tag_A));
    WRHDR("TARG_A", val, comment);
    VALD(tag_Z/3600.);
    COMMENT("Target ZD (degr): %s", angle_asc(tag_Z));
    WRHDR("TARG_Z", val, comment);
    VALD(val_A/3600.);
    COMMENT("Telescope Az (degr): %s", angle_asc(val_A));
    WRHDR("A", val, comment);
    VALD(val_Z/3600.);
    COMMENT("Telescope ZD (degr): %s", angle_asc(val_Z));
    WRHDR("Z", val, comment);
    VALD(tag_P/3600.);
    COMMENT("Target rotation angle (degr): %s", angle_asc(tag_P));
    WRHDR("TARG_R", val, comment);
    VALD(val_P/3600.);
    COMMENT("Telescope rotation angle (degr): %s", angle_asc(val_P));
    WRHDR("ROTANGLE", val, comment);
    VALD(val_D/3600.);
    COMMENT("Dome Az (degr): %s", angle_asc(val_D));
    WRHDR("DOME_A", val, comment);
#define T(hdr, t, text) do{VAL("%.1f", t); COMMENT(text " temperature (degC)"); WRHDR(hdr, val, comment);}while(0)
    T("OUTTEMP", val_T1, "Outern");
    T("DOMETEMP", val_T2, "In-dome");
    T("MIRRTEMP", val_T3, "Mirror");
    VAL("%.1f", val_B);
    WRHDR("PRESSURE", val, "Atm. pressure (mmHg)");
    VAL("%.1f", val_Wnd);
    WRHDR("WIND", val, "Wind speed (m/s)");
    VAL("%.0f", val_Hmd);
    WRHDR("HUMIDITY", val, "Relative humidity (%)");
    ret = TRUE;
returning:
    close(fd);
    rename(aname, path);
    FREE(aname);
    return ret;
}
