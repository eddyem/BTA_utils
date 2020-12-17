
/* Print some BTA NewACS data (or write  to file)
 * Usage:
 *         bta_print [time_step] [file_name]
 * Where:
 *         time_step - writing period in sec., >=1.0
 *                      <1.0 - once and exit (default)
 *         file_name - name of file to write to,
 *                      "-" - stdout (default)
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/times.h>

#include <crypt.h>

//#define SHM_OLD_SIZE
#include "bta_shdata.h"

static double time_step=0.0;

static char *file_name = "-";

char *time_asc(double t, char *lin)
{
    int h, min;
    double sec;
    h   = (int)(t/3600.);
    min = (int)((t - (double)h*3600.)/60.);
    sec = t - (double)h*3600. - (double)min*60.;
    h %= 24;
    if(sec>59.99) sec=59.99;
    sprintf(lin, "%02d:%02d:%05.2f", h,min,sec);
    return lin;
}

char *angle_asc(double a, char *lin)
{
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
    sprintf (lin, "%c%02d:%02d:%04.1f", s,d,min,sec);
    return lin;
}

char *angle_fmt(double a, char *format, char *lin)
{
    char s, *p;
    int d, min, n;
    double sec, msec;
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
       sprintf (lin, format, s,d,min,sec);
    else
       sprintf (lin, format, d,min,sec);
    return lin;
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

static void calc_AZ(double alpha, double delta, double stime, double *az, double *zd)
{
    double sin_t,cos_t, sin_d,cos_d,  cos_z;
    double t, d, z, a, x, y;

    t = (stime - alpha) * 15.;
    if (t < 0.)
       t += S360;      /* +360degr */
    t *= S2R;          /* -> rad */
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

static double calc_PA(double alpha, double delta, double stime)
{
    double sin_t,cos_t, sin_d,cos_d;
    double t, d, p, sp, cp;

    t = (stime - alpha) * 15.;
    if (t < 0.)
       t += S360;      /* +360degr */
    t *= S2R;          /* -> rad */
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

#if 0
static void calc_AD(double az, double zd, double stime, double *alpha, double *delta)
{
    double sin_d, sin_a,cos_a, sin_z,cos_z;
    double t, d, z, a, x, y;
    a = az * S2R;
    z = zd * S2R;
    sin_a = sin(a);
    cos_a = cos(a);
    sin_z = sin(z);
    cos_z = cos(z);

    y = sin_z * sin_a;
    x = cos_a * sin_fi * sin_z + cos_fi * cos_z;
    t = atan2(y, x);
    if (t < 0.0)
       t += 2.0*PI;

    sin_d = sin_fi * cos_z - cos_fi * cos_a * sin_z;
    d = asin(sin_d);

    *delta = d * R2S;
    *alpha = (stime - t * R2S / 15.);
    if (*alpha < 0.0)
       *alpha += S360/15.;      /* +24h */
}
#endif

static void my_sleep(double dt)
{
  int nfd;
  struct timeval tv;
   tv.tv_sec = (int)dt;
   tv.tv_usec = (int)((dt - tv.tv_sec)*1000000.);
slipping:
   nfd = select(0, (fd_set *)NULL,(fd_set *)NULL,(fd_set *)NULL, &tv);
   if(nfd < 0) {
      if(errno == EINTR)
    /*On  Linux,  timeout  is  modified to reflect the amount of
     time not slept; most other implementations DO NOT do this!*/
     goto slipping;
      fprintf(stderr,"Error in mydelay(){ select() }. %s\n",strerror(errno));
   }
}

int main (int argc, char *argv[])
{
    FILE *fd;
    double last;
    int i,acs_bta;
    char tmp[80], *value;

    if(argc>1) {
       if(isdigit(argv[1][0])||argv[1][0]=='.') time_step=atof(argv[1]);
       else file_name = argv[1];
       if(argc>2) {
      if(isdigit(argv[2][0])||argv[2][0]=='.') time_step=atof(argv[2]);
      else file_name = argv[2];
       }
    }
    if(strcmp(file_name,"-")==0) {
    fd = stdout;
    } else if((fd=fopen(file_name,"w"))==NULL) {
    fprintf(stderr,"Can't write BTA data to file: %s\n",file_name);
    exit(1);
    }
    if(!get_shm_block( &sdat, ClientSide)) return 1;
    last = M_time;
    for(i=0;i<50 && fabs(M_time-last)<0.01; i++)
       my_sleep(0.02);

    do {
      if(fd != stdout)
     if((fd=freopen(file_name,"w",fd))==NULL) {
        fprintf(stderr,"Can't write BTA data to file: %s\n",file_name);
        exit(1);
     }

      acs_bta = ( check_shm_block(&sdat) && fabs(M_time-last)>0.01);
      value = (acs_bta)? "On" : "Off";
      fprintf(fd,"ACS_BTA=\"%s\"\n",value);
      /* Mean Solar Time */
      fprintf(fd,"M_time=\"%s\"\n",time_asc(M_time+DUT1,tmp));
      /* Mean Sidereal Time */
#ifdef EE_time
      fprintf(fd,"S_time=\"%s\"\n",time_asc(S_time-EE_time,tmp));
      //fprintf(fd,"JDate=\"%13.5f\"\n",JDate);
      fprintf(fd,"JDate=\"%g\"\n",JDate);
#else
      fprintf(fd,"S_time=\"%s\"\n",time_asc(S_time,tmp));
#endif
      if(!acs_bta || Tel_Hardware == Hard_Off) value = "Off";
      else if(Tel_Mode != Automatic)   value = "Manual";
      else {
    switch (Sys_Mode) {
      default:
      case SysStop    :  value = "Stopping";  break;
      case SysWait    :  value = "Waiting";   break;
      case SysPointAZ :
      case SysPointAD :  value = "Pointing";  break;
      case SysTrkStop :
      case SysTrkStart:
      case SysTrkMove :
      case SysTrkSeek :  value = "Seeking";   break;
      case SysTrkOk   :  value = "Tracking";  break;
      case SysTrkCorr :  value = "Correction";break;
      case SysTest    :  value = "Testing";   break;
    }
      }
      fprintf(fd,"Tel_Mode=\"%s\"\n",value);

      switch (Tel_Focus) {
    default:
    case Prime    :  value = "Prime";     break;
    case Nasmyth1 :  value = "Nasmyth1";  break;
    case Nasmyth2 :  value = "Nasmyth2";  break;
      }
      fprintf(fd,"Tel_Focus=\"%s\"\n",value);

      switch (Sys_Target) {
    default:
    case TagObject   :  value = "Object";   break;
    case TagPosition :  value = "A/Z-Pos."; break;
    case TagNest     :  value = "Nest";     break;
    case TagZenith   :  value = "Zenith";   break;
    case TagHorizon  :  value = "Horizon";  break;
      }
      fprintf(fd,"Tel_Taget=\"%s\"\n",value);

      if(acs_bta && Tel_Hardware == Hard_On)
    switch (P2_State) {
      default:
      case P2_Off   :  value = "Stop";    break;
      case P2_On    :  value = "Track";   break;
      case P2_Plus  :  value = "Move+";   break;
      case P2_Minus :  value = "Move-";   break;
    }
      else value = "Off";
      fprintf(fd,"P2_Mode=\"%s\"\n",value);
      fprintf(fd,"code_KOST=\"0x%04X\"\n",code_KOST);

      fprintf(fd,"CurAlpha=\"%s\"\n", time_asc(CurAlpha,tmp));
      fprintf(fd,"CurDelta=\"%s\"\n",angle_asc(CurDelta,tmp));
      fprintf(fd,"SrcAlpha=\"%s\"\n", time_asc(SrcAlpha,tmp));
      fprintf(fd,"SrcDelta=\"%s\"\n",angle_asc(SrcDelta,tmp));
      fprintf(fd,"InpAlpha=\"%s\"\n", time_asc(InpAlpha,tmp));
      fprintf(fd,"InpDelta=\"%s\"\n",angle_asc(InpDelta,tmp));
      fprintf(fd,"TelAlpha=\"%s\"\n", time_asc(val_Alp,tmp));
      fprintf(fd,"TelDelta=\"%s\"\n",angle_asc(val_Del,tmp));

      fprintf(fd,"InpAzim=\"%s\"\n",angle_fmt(InpAzim,"%c%03d:%02d:%04.1f",tmp));
      fprintf(fd,"InpZenD=\"%s\"\n",angle_fmt(InpZdist,"%02d:%02d:%04.1f",tmp));
      fprintf(fd,"CurAzim=\"%s\"\n",angle_fmt(tag_A,"%c%03d:%02d:%04.1f",tmp));
      fprintf(fd,"CurZenD=\"%s\"\n",angle_fmt(tag_Z,"%02d:%02d:%04.1f",tmp));
      fprintf(fd,"CurPA=\"%s\"\n",angle_fmt(tag_P,"%03d:%02d:%04.1f",tmp));
      fprintf(fd,"SrcPA=\"%s\"\n",angle_fmt(calc_PA(SrcAlpha,SrcDelta,S_time),"%03d:%02d:%04.1f",tmp));
      fprintf(fd,"InpPA=\"%s\"\n",angle_fmt(calc_PA(InpAlpha,InpDelta,S_time),"%03d:%02d:%04.1f",tmp));
      fprintf(fd,"TelPA=\"%s\"\n",angle_fmt(calc_PA(val_Alp, val_Del, S_time),"%03d:%02d:%04.1f",tmp));

      fprintf(fd,"ValAzim=\"%s\"\n",angle_fmt(val_A,"%c%03d:%02d:%04.1f",tmp));
      fprintf(fd,"ValZenD=\"%s\"\n",angle_fmt(val_Z,"%02d:%02d:%04.1f",tmp));
      fprintf(fd,"ValP2=\"%s\"\n",angle_fmt(val_P,"%03d:%02d:%04.1f",tmp));
      fprintf(fd,"ValDome=\"%s\"\n", angle_fmt(val_D,"%c%03d:%02d:%04.1f",tmp));

      fprintf(fd,"DiffAzim=\"%s\"\n",angle_fmt(Diff_A,"%c%03d:%02d:%04.1f",tmp));
      fprintf(fd,"DiffZenD=\"%s\"\n",angle_fmt(Diff_Z,"%c%02d:%02d:%04.1f",tmp));
      fprintf(fd,"DiffP2=\"%s\"\n",angle_fmt(Diff_P,"%c%03d:%02d:%04.1f",tmp));
      fprintf(fd,"DiffDome=\"%s\"\n",angle_fmt(val_A-val_D,"%c%03d:%02d:%04.1f",tmp));

      fprintf(fd,"VelAzim=\"%s\"\n",angle_fmt(vel_A,"%c%02d:%02d:%04.1f",tmp));
      fprintf(fd,"VelZenD=\"%s\"\n",angle_fmt(vel_Z,"%c%02d:%02d:%04.1f",tmp));
      fprintf(fd,"VelP2=\"%s\"\n",angle_fmt(vel_P,"%c%02d:%02d:%04.1f",tmp));
      fprintf(fd,"VelPA=\"%s\"\n",angle_fmt(vel_objP,"%c%02d:%02d:%04.1f",tmp));
      fprintf(fd,"VelDome=\"%s\"\n",angle_fmt(vel_D,"%c%02d:%02d:%04.1f",tmp));

      if(Sys_Mode==SysTrkSeek||Sys_Mode==SysTrkOk||Sys_Mode==SysTrkCorr) {
    double curA,curZ,srcA,srcZ;
    double corAlp,corDel,corA,corZ;
    corAlp = CurAlpha-SrcAlpha;
    corDel = CurDelta-SrcDelta;
    if(corAlp >  23*3600.) corAlp -= 24*3600.;
    if(corAlp < -23*3600.) corAlp += 24*3600.;
    calc_AZ(SrcAlpha, SrcDelta, S_time, &srcA, &srcZ);
    calc_AZ(CurAlpha, CurDelta, S_time, &curA, &curZ);
    corA=curA-srcA;
    corZ=curZ-srcZ;
    fprintf(fd,"CorrAlpha=\"%s\"\n",angle_fmt(corAlp,"%c%01d:%02d:%05.2f",tmp));
    fprintf(fd,"CorrDelta=\"%s\"\n",angle_fmt(corDel,"%c%01d:%02d:%04.1f",tmp));
    fprintf(fd,"CorrAzim=\"%s\"\n",angle_fmt(corA,"%c%01d:%02d:%04.1f",tmp));
    fprintf(fd,"CorrZenD=\"%s\"\n",angle_fmt(corZ,"%c%01d:%02d:%04.1f",tmp));
      } else {
    fprintf(fd,"CorrAlpha=\"+0:00:00.00\"\n");
    fprintf(fd,"CorrDelta=\"+0:00:00.0\"\n");
    fprintf(fd,"CorrAzim=\"+0:00:00.0\"\n");
    fprintf(fd,"CorrZenD=\"+0:00:00.0\"\n");
      }
      fprintf(fd,"ValFoc=\"%0.2f\"\n",val_F);
      fprintf(fd,"ValTout=\"%+05.1f\"\n",val_T1);
      fprintf(fd,"ValTind=\"%+05.1f\"\n",val_T2);
      fprintf(fd,"ValTmir=\"%+05.1f\"\n",val_T3);
      fprintf(fd,"ValPres=\"%05.1f\"\n",val_B);
      fprintf(fd,"ValWind=\"%04.1f\"\n",val_Wnd);
      if(Wnd10_time>0.1 && Wnd10_time<=M_time /*&& M_time-Wnd10_time<24*3600.*/) {
    fprintf(fd,"Blast10=\"%.1f\"\n",(M_time-Wnd10_time)/60);
    fprintf(fd,"Blast15=\"%.1f\"\n",(M_time-Wnd15_time)/60);
      } else {
    fprintf(fd,"Blast10=\" \"\n");
    fprintf(fd,"Blast15=\" \"\n");
      }
      fprintf(fd,"ValHumd=\"%04.1f\"\n",val_Hmd);
      if(Precip_time>0.1 && Precip_time<=M_time /*&& M_time-Precip_time<24*3600.*/)
    fprintf(fd,"Precipt=\"%.1f\"\n",(M_time-Precip_time)/60);
      else
    fprintf(fd,"Precipt=\" \"\n");
      fflush(fd);

      last = M_time;
      if(time_step>0.9) my_sleep(time_step);

    } while (time_step>0.9);    /* else only once */

    exit(0);
}
