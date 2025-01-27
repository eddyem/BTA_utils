/*
 * This file is part of the btaprinthdr project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>, Vladimir S. Shergin <vsher@sao.ru>.
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <usefull_macros.h>
#include <erfa.h>
#include <erfam.h>

#include "bta_print.h"
#include "bta_shdata.h"
#include "bta_site.h"

// rad to time sec
#ifndef ERFA_DR2S
#define ERFA_DR2S 1.3750987083139757010431557155385240879777313391975e4
#endif
// seconds of time in full circle
#define S24  (24.*3600.)

const double longitude = TELLONG * 3600.;
const double Fi = TELLAT * 3600.;

const double cos_fi=0.7235272793;
const double sin_fi=0.6902957888;

/*
const double cos_fi = 0.723527277857;    // Cos of SAO latitude
const double sin_fi = 0.690295790366;    // Sin  ---  ""  -----
*/

/**
 * convert apparent coordinates (nowadays) to mean (JD2000)
 * appRA, appDecl in seconds
 * r, d in seconds
 */
void calc_mean(double appRA, double appDecl, double *r, double *dc){
    double ra=0., dec=0., utc1, utc2, tai1, tai2, tt1, tt2, fd, eo, ri;
    int y, m, d, H, M;
    DBG("appRa: %g'', appDecl'': %g", appRA, appDecl);
    appRA *= ERFA_DS2R;
    appDecl *= ERFA_DAS2R;
#define ERFA(f, ...) do{if(f(__VA_ARGS__)){WARNX("Error in " #f); goto rtn;}}while(0)
    // 1. convert system JDate to UTC
    ERFA(eraJd2cal, JDate, 0., &y, &m, &d, &fd);
    fd *= 24.;
    H = (int)fd;
    fd = (fd - H)*60.;
    M = (int)fd;
    fd = (fd - M)*60.;
    ERFA(eraDtf2d, "UTC", y, m, d, H, M, fd, &utc1, &utc2);
    ERFA(eraUtctai, utc1, utc2, &tai1, &tai2);
    ERFA(eraTaitt, tai1, tai2, &tt1, &tt2);
    eraAtic13(appRA, appDecl, tt1, tt2, &ri, &dec, &eo);
    ra = eraAnp(ri + eo);
    ra *= ERFA_DR2S;
    dec *= ERFA_DR2AS;
#undef ERFA
rtn:
    if(r) *r = ra;
    if(dc) *dc = dec;
}

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
        snprintf(tmp, 80, "%-8s= %-21s / %s", key,  val, cmnt);
    }else{
        snprintf(tmp, 80, "%-8s= %s", key, val);
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

static void calc_AZ(double alpha, double delta, double stime, double *az, double *zd){
    double ha = (stime - alpha) * 15.;
    if(ha < 0.) ha += ERFA_TURNAS;
    ha *= ERFA_DAS2R;
    double A, alt;
    eraHd2ae(ha, delta*ERFA_DAS2R, TELLAT*ERFA_DD2R, &A, &alt);
    if(az) *az = (A - M_PI) * ERFA_DR2AS; // BTA counts Az from south, not north
    if(zd) *zd = (M_PI_2 - alt) * ERFA_DR2AS;
}

extern void calc_airmass(
    // in
    double daynum,	// Day number from beginning of year, 0 = midnight Jan 1
    double relhumid,// Relative humidity in percent
    double p0,		// local pressure in mmHg
    double t0,		// temperature in kelvins
    double z,		// zenith distance in degr
    // out
    double *am,		// AIRMASS
    double *acd,	// column density
    double *wcd,	// water vapor column density
    double *wam		// water vapor airmass
);


#if 0
//double coeff[8] = { -97.4, 13.0, -11.7, -3.6, -6.2, -279.9, -33.3, 8.2};
void pos_corr (double Delta, double A, double Z, double P,
           double *pcA, double *pcZ) {
   double ar, dr, zr, pr;

   dr = Delta;
   ar = A;
   zr = Z;
   pr = P;
double *coeff = (double*)sdt->pc_coeff;
      *pcA = coeff[0] + coeff[1]/tan(zr) + coeff[2]/sin(zr) -
         coeff[3]*sin(ar)/tan(zr) + coeff[4]*cos(dr)*cos(pr)/sin(zr);

      *pcZ = coeff[5] + coeff[6]*sin(zr) + coeff[7]*cos(zr) +
         coeff[3]*cos(ar) + coeff[4]*cos_fi*sin(ar);

}

void calc_AZP(double alpha, double delta, double stime,
          double *az, double *zd, double *pa){
    double sin_t,cos_t, sin_d,cos_d, sin_z,cos_z, sin_p,cos_p;
    double t, d, z, a, p , x, y;

    t = (stime - alpha) * 15.;
    red("st=%g, alpha=%g, delta=%g, t=%g\n", stime/3600., alpha/3600., delta/3600., t/3600./15.);
    if (t < 0.)
       t += ERFA_TURNAS;
    t *= ERFA_DAS2R;
    d = delta * ERFA_DAS2R;
    sincos(t, &sin_t, &cos_t);
    sincos(d, &sin_d, &cos_d);

    cos_z = cos_fi * cos_d * cos_t + sin_fi * sin_d;
    z = acos(cos_z);
    sin_z = sqrt(1. - cos_z*cos_z);

    y = cos_d * sin_t;
    x = cos_d * sin_fi * cos_t - cos_fi * sin_d;
    a = atan2(y, x);

    sin_p = sin_t * cos_fi / sin_z;
    cos_p = (sin_fi * cos_d - sin_d * cos_fi * cos_t) / sin_z;
    p = atan2(sin_p, cos_p);
    if (p < 0.0)
       p += 2.0*M_PI;
double pca, pcz;
pos_corr(d, a, z, p, &pca, &pcz);
red("pca=%g, pcz=%g; tel_cor_A=%g, tel_cor_Z=%g\n", pca, pcz, tel_cor_A, tel_cor_Z);
    *zd = z * ERFA_DR2AS;
    *az = a * ERFA_DR2AS;
    *pa = p * ERFA_DR2AS;
}
#endif

#define WRHDR(k, v, c)  do{if(printhdr(fd, k, v, c)){goto returning;}}while(0)
int print_header(_U_ const char *path){
    int ret = FALSE;
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
    WRHDR("ORIGIN", "'SAO RAS, Russia'", "Organization responsible for the data");
    VALD(TELLAT);
    COMMENT("Telescope lattitude (degr): %s", angle_asc(TELLAT*3600.));
    WRHDR("SITELAT", val, comment);
    VALD(TELLONG);
    COMMENT("Telescope longitude (degr): %s", angle_asc(TELLONG*3600.));
    WRHDR("SITELONG", val, comment);
    VAL("%.1f", TELALT);
    WRHDR("SITEALT", val, "Telescope altitude (m)");
    double sidtm = S_time-EE_time;
    if(sidtm < 0.) sidtm += S24;
    else if(sidtm > S24){
        int x = (int)(sidtm / S24);
        sidtm -= S24 * (double)x;
    }
    COMMENT("Sidereal time, seconds: %s", time_asc(sidtm));
    VALD(sidtm);
    WRHDR("ST", val, comment);
    COMMENT("Universal time, seconds: %s", time_asc(M_time));
    VALD(M_time);
    WRHDR("UT", val, comment);
    VALD(JDate);
    WRHDR("JD", val, "Julian date");
    {time_t t_now = time(NULL);
    struct tm *tm_ut = gmtime(&t_now);
    double l = ERFA_DTY; int y = 1900 + tm_ut->tm_year;
    if((0 == y%400) || ((0 == y%4)&&(0 != y%100))) l += 1.;
    double dtmp = y + (double)tm_ut->tm_yday / l;
    VALD(dtmp);
    WRHDR("EQUINOX", val, "Coordinates epoch");}
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
    char *telmode = "Undefined";
    if(Tel_Hardware == Hard_Off) telmode = "Off";
    else if(Tel_Mode != Automatic) telmode = "Manual";
    else{
        switch (Sys_Mode){
            default:
            case SysStop    :  telmode = "Stopping";  break;
            case SysWait    :  telmode = "Waiting";   break;
            case SysPointAZ :
            case SysPointAD :  telmode = "Pointing";  break;
            case SysTrkStop :
            case SysTrkStart:
            case SysTrkMove :
            case SysTrkSeek :  telmode = "Seeking";   break;
            case SysTrkOk   :  telmode = "Tracking";  break;
            case SysTrkCorr :  telmode = "Correction";break;
            case SysTest    :  telmode = "Testing";   break;
        }
    }
    VALS(telmode);
    WRHDR("TELMODE", val, "Telescope working mode");

    VAL("%.2f", val_F);
    WRHDR("VAL_F", val, "Focus value of telescope (mm)");
    double a2000, d2000;
    calc_mean(InpAlpha, InpDelta, &a2000, &d2000);
    VALD(a2000 * 15. / 3600.);
    COMMENT("Input R.A. for J2000 (deg): %s", time_asc(a2000));
    WRHDR("RA_INP0", val, comment);
    VALD(d2000 / 3600.);
    COMMENT("Input Decl. for J2000 (deg): %s", angle_asc(d2000));
    WRHDR("DEC_INP0", val, comment);
    calc_mean(CurAlpha, CurDelta, &a2000, &d2000);
    VALD(a2000 * 15. / 3600.);
    COMMENT("Telescope R.A. for J2000 (deg): %s", time_asc(a2000));
    WRHDR("RA_0", val, comment);
    VALD(d2000 / 3600.);
    COMMENT("Telescope Decl. for J2000 (deg): %s", angle_asc(d2000));
    WRHDR("DEC_0", val, comment);
#define RA(ra, dec, text, pref) do{VALD(ra*15./3600.); COMMENT(text " R.A. (degr): %s", time_asc(ra)); WRHDR("RA" pref, val, comment); \
        VALD(dec/3600.); COMMENT(text " Decl (degr): %s", angle_asc(dec)); WRHDR("DEC" pref, val, comment);}while(0)
    RA(InpAlpha, InpDelta, "Input", "_INP");
    RA(CurAlpha, CurDelta, "Current object", "_OBJ");
    RA(SrcAlpha, SrcDelta, "Source", "_SRC");
    RA(val_Alp, val_Del, "Telescope", "");
#undef RA
#define AZ(a, z, text, pref) do{VALD(a/3600.); COMMENT(text " Az (degr): %s", angle_asc(a)); WRHDR("A" pref, val, comment); \
    VALD(z/3600.); COMMENT(text " ZD (degr): %s", angle_asc(z)); WRHDR("Z" pref, val, comment);}while(0)
    AZ(InpAzim, InpZdist, "Input", "_INP");
    AZ(tag_A, tag_Z, "Current object", "_OBJ");
    AZ(val_A, val_Z, "Telescope", "");
#undef AZ
/*
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
    */
    double t = sidtm * ERFA_DS2R - val_Alp * ERFA_DS2R;
    if(t < 0) t += 2*M_PI;
    double P = ERFA_DR2D * eraHd2pa(t, val_Del * ERFA_DAS2R, TELLAT * ERFA_DD2R);
    if(P < 0.) P += 360.;
    VALD(P);
    COMMENT("Parallactic angle (degr): %s", angle_asc(P*3600.));
    WRHDR("PARANGLE", val, comment);
    VALD(tag_P/3600.);
    COMMENT("Target par. angle (degr): %s", angle_asc(tag_P));
    WRHDR("TAGANGLE", val, comment);
    VALD(val_P/3600.);
    COMMENT("Current P2 rot. angle (degr): %s", angle_asc(val_P));
    WRHDR("ROTANGLE", val, comment);
    VALD(val_D/3600.);
    COMMENT("Dome Az (degr): %s", angle_asc(val_D));
    WRHDR("DOME_A", val, comment);
    if(Sys_Mode==SysTrkSeek||Sys_Mode==SysTrkOk||Sys_Mode==SysTrkCorr||Sys_Mode==SysTrkStart||Sys_Mode==SysTrkMove){
        double curA,curZ,srcA,srcZ;
        double corAlp,corDel,corA,corZ;
        corAlp = CurAlpha-SrcAlpha;
        corDel = CurDelta-SrcDelta;
        if(corAlp >  23*3600.) corAlp -= 24*3600.;
        if(corAlp < -23*3600.) corAlp += 24*3600.;
        calc_AZ(SrcAlpha, SrcDelta, S_time, &srcA, &srcZ);
        calc_AZ(CurAlpha, CurDelta, S_time, &curA, &curZ);
        corA = curA-srcA;
        corZ = curZ-srcZ;
        VALD(corAlp);
        WRHDR("RACORR", val, "RA correction (current - source)");
        VALD(corDel);
        WRHDR("DECCORR", val, "DEC correction (current - source)");
        VALD(corA);
        WRHDR("ACORR", val, "A correction (current - source)");
        VALD(corZ);
        WRHDR("ZCORR", val, "Z correction (current - source)");
    }
    VALD(DUT1);
    WRHDR("DUT1", val, "DUT1 = UT1 - UTC");
/*
    double az,zd;//,pa;
    calc_AZ(val_Alp, val_Del, sidtm, &az, &zd);
    green("calc_AZ: AZ=%g, ZD=%g\n", az/3600., zd/3600.);
    calc_AZx(val_Alp, val_Del, sidtm, &az, &zd);
    green("calc_AZx: AZ=%g, ZD=%g\n", az/3600., zd/3600.);
    calc_AZP(val_Alp, val_Del, sidtm, &az, &zd, &pa);
    green("AZ=%g, ZD=%g, PA=%g; sidtm=%g\n", az/3600.,zd/3600.,pa/3600., sidtm/3600.);
*/
    if(UsePCorr) VALS("true"); else VALS("false");
    WRHDR("USEPCORR", val, "P.corr.sys.: K0..K7 (real = measured - PCS)");
    if(UsePCorr){
        char kx[3];
        const char *descr[8] = {
            "A0 - PCS Azimuth zero",
            "L - PCS horizontal axe inclination",
            "k - PCS collimation error",
            "F - PCS lattitude error of vert. axe",
            "dS - PCS time error",
            "Z0 - PCS zenith zero",
            "d - PCS tube bend [sin(Z)]",
            "d1 - PCS tube bend [cos(Z)]"
        };
        for(int i = 0; i < 8; ++i){
            VAL("%.2f", PosCor_Coeff[i]);
            snprintf(kx, 3, "K%d", i);
            WRHDR(kx, val, descr[i]);
        }
#if 0
        // SKN:
        // dA = K0 + K1/tg(Z) + K2/sin(Z) - K3*sin(A)/tg(Z) + K4 *cos(delta)*cos(P)/sin(Z)
        // dZ = K5 + K6*sin(Z) + K7*cos(Z) + K3*cos(A) + K4*cos(phi)*sin(A)
        double A = val_A * ERFA_DAS2R, Z = val_Z * ERFA_DAS2R;
        double sinZ, cosZ, sinA, cosA, tgZ = tan(Z);
        sincos(Z, &sinZ, &cosZ);
        sincos(A, &sinA, &cosA);
        double dA = PosCor_Coeff[0] + PosCor_Coeff[1]/tgZ + PosCor_Coeff[2]/sinZ -
                    PosCor_Coeff[3]*sinA/tgZ +
                    PosCor_Coeff[4]*cos(CurDelta*ERFA_DAS2R)*cos(P*ERFA_DD2R)/sinZ;
        double dZ = PosCor_Coeff[5] + PosCor_Coeff[6]*sinZ + PosCor_Coeff[7]*cosZ +
                    PosCor_Coeff[3]*cosA + PosCor_Coeff[4]*cos_fi*sinA;
        red("dA=%g, dZ=%g; tel_cor_A=%g, tel_cor_Z=%g\n", dA, dZ, tel_cor_A, tel_cor_Z);
#endif
        VAL("%.1f", pos_cor_A);
        WRHDR("PCSDA_O", val, "Pointing correction for object A (arcsec)");
        VAL("%.1f", pos_cor_Z);
        WRHDR("PCSDZ_O", val, "Pointing correction for object Z (arcsec)");
        VAL("%.1f", tel_cor_A);
        WRHDR("PCSDA_T", val, "Pointing correction for telescope A (arcsec)");
        VAL("%.1f", tel_cor_Z);
        WRHDR("PCSDZ_T", val, "Pointing correction for telescope Z (arcsec)");
    }
    VAL("%.1f", refract_Z);
    WRHDR("REFR_O", val, "Refraction for object position (arcsec)");
    VAL("%.1f", tel_ref_Z);
    WRHDR("REFR_T", val, "Refraction for telescope position (arcsec)");
    {double refa, refb, tz = tan(val_Z*ERFA_DAS2R);
    eraRefco(val_B*1.33322, val_T1, val_Hmd/100., 0.45, &refa, &refb);
    double dz = (refa*tz + refb*tz*tz*tz)*ERFA_DR2AS;
    VAL("%.2f", dz);
    WRHDR("REFR_T_E", val, "RREFR_T by ERFA eraRefco()");}

#define T(hdr, t, text) do{VAL("%.1f", t); COMMENT(text " temperature (degC)"); WRHDR(hdr, val, comment);}while(0)
    T("OUTTEMP", val_T1, "Outern");
    T("DOMETEMP", val_T2, "In-dome");
    T("MIRRTEMP", val_T3, "Mirror");
#undef T
    VAL("%.1f", val_B);
    WRHDR("PRESSURE", val, "Atm. pressure (mmHg)");
    VAL("%.1f", val_Wnd);
    WRHDR("WIND", val, "Wind speed (m/s)");
    VAL("%.1f", val_Hmd);
    WRHDR("HUMIDITY", val, "Relative humidity (%)");
    /*
     * Airmass calculation
     * by Reed D. Meyer
     */
    {double am, acd, wcd, wam;
    time_t t_now = time(NULL);
    struct tm *tm_loc;
    tm_loc = localtime(&t_now);
    calc_airmass(tm_loc->tm_yday, val_Hmd, val_B, val_T1+273.15, val_Z/3600., &am, &acd, &wcd, &wam);
    VALD(am);
    WRHDR("AIRMASS", val, "Air mass by Reed D. Meyer");
    VALD(wam);
    WRHDR("WVAM", val, "Water vapour air mass by Reed D. Meyer");
    VALD(acd);
    WRHDR("ATMDENS", val, "Atm. column density by Reed D. Meyer (g/cm^2)");
    VALD(wcd);
    WRHDR("WVDENS", val, "WV column density by Reed D. Meyer (g/cm^2)");}
    ret = TRUE;
returning:
    close(fd);
    rename(aname, path);
    FREE(aname);
    return ret;
}
