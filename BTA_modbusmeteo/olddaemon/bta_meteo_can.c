/* Получение метео-данных от PEP-контроллеров по CAN-шине */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termio.h>
#include <sys/file.h>

#include "bta_shdata.h"
#include "CAN/can_io.h"

const double zeroV  = 1.0;
const double scaleT = 50.0/(5.-1.);   /* New: 1:5V -> -20:+30dgr */
const double zeroT2  = -19.1;         /* -20.0 + 0.9 Tind (16.11.2012)*/

static int stop_prog = 0;
static char *myname;

static void print_date(FILE *fdout) {
   static char lastdate[40] = "01.01.2000 00:00:00";
   const char *formdate = "%d.%m.%Y %H:%M:%S";
   time_t      t;
   struct tm  *tm;
   char   currdate[40];
   int len;

   time(&t);
   tm = localtime(&t);
   len = strftime(currdate,40,formdate,tm);
   if(strncmp(lastdate,currdate,11)==0)
      fprintf(fdout,"%s ", &currdate[11]);
   else
      fprintf(fdout,"Date: %s ", currdate);

   strncpy(lastdate,currdate,20);
}

static void   fprtime(FILE *fd) { can_prtime(fd); }

static double dsleep(double dt) {
   struct timespec ts,tsr;
   ts.tv_sec = (time_t)dt;
   ts.tv_nsec = (long)((dt-ts.tv_sec)*1e9);
   nanosleep(&ts,&tsr);
   return((double)ts.tv_sec + (double)ts.tv_nsec/1e9);
}

static double dtime() {
   struct timeval ct;
   struct timezone tz;
   gettimeofday(&ct, &tz);
   return (ct.tv_sec + ct.tv_usec/1e6);
}

static void myabort(int sig) {
    int ret;
    char ss[10], tmp[80];
    signal(sig,SIG_IGN);
    switch (sig) {
    case SIGHUP : strcpy(ss,"SIGHUP");  break;
    case SIGINT : strcpy(ss,"SIGINT");  break;
    case SIGQUIT: strcpy(ss,"SIGQUIT"); break;
    case SIGFPE : strcpy(ss,"SIGFPE");  break;
    case SIGPIPE: strcpy(ss,"SIGPIPE"); break;
    case SIGSEGV: strcpy(ss,"SIGSEGV"); break;
    case SIGTERM: strcpy(ss,"SIGTERM"); break;
    default:  sprintf(ss,"SIG_%d",sig); break;
    }
    print_date(stderr);
    switch (sig) {
    default:

    case SIGHUP :
    case SIGINT :
         fprintf(stderr,"%s: %s - Ignore .....\n",myname,ss);
         fflush(stderr);
         signal(sig, myabort);
         return;
    case SIGPIPE:
    case SIGQUIT:
    case SIGFPE :
    case SIGSEGV:
    case SIGTERM:
         signal(SIGALRM, SIG_IGN);
         fprintf(stderr,"%s: %s - programm stop!\n",myname,ss);
         fflush(stderr);
         stop_prog = sig;
         return;
    }
}

static int rxpnt;
static double rxtime;

int main (int argc, char *argv[])
{
   double tcurr, tlast, tok, twndok;
   char msg[60];
   double t0=0., t;
   int i,m,n, idr, idt, dlen, start=0, imsg;
   unsigned char rdata[8],tdata[8];
   char tmp[100],tmp1[100], pmsg[48], *pmsgp;

   myname = argv[0];
   tzset();
   print_date(stderr);
   sdat.mode |= 0200;
   sdat.atflag = 0;
   get_shm_block( &sdat, ClientSide);
   get_cmd_queue( &ocmd, ClientSide);

   MeteoMode &= ~SENSOR_T2;

   init_can_io();
   can_clean_recv(&rxpnt, &rxtime);
   fprintf(stderr,"\n");

   signal(SIGHUP, myabort);
   signal(SIGINT, myabort);
   signal(SIGQUIT,myabort);
   signal(SIGFPE, myabort);
   signal(SIGPIPE,myabort);
   signal(SIGSEGV,myabort);
   signal(SIGTERM,myabort);

   imsg=0;
   pmsgp=pmsg;

   t0 = tcurr = tlast = tok = twndok = dtime();
   tok -= 600.;

   while (1) {
      char *pep = "RK";
      int nch = 0;
      dsleep(0.2);
      tcurr = dtime();
      if(PEP_A_On && PEP_R_On) {
     if(!stop_prog && !start && tcurr-tok>15.) {
        idt = 0x447;
        dlen=6;
        tdata[0] = 7;
        tdata[2] = 50;                /* 0.5s */
        tdata[3] = 1;
        tdata[4] = 0; tdata[5] = 50;  /* 0.5s */
        for(i=0; i<8; i++) {
           if(i==2||i==3||i==5||i==6) /* T-зеркала, ветер, влажность, T на метео-мачте */
          continue;               /* пока не используются (заменены)     */
           if(i==4) {   /*временно: пока АЦП4 идет с PEP-A (давление)*/
          pep = "A";
          idt = 0x40f;            /* PEP-A */
          tdata[1] = nch = 4;     /* АЦП/4 */
           } else {
          pep = "RK";
          idt = 0x447;            /* PEP-RK */
          tdata[1] =  nch = i;    /* АЦП/i  */
           }
           print_date(stderr);
           if(can_send_frame(idt, dlen, tdata)<=0) {
          fprintf(stderr,"Can't send command \"Start ADC%d\" to PEP-%s!\n",nch,pep);
           } else if(tcurr-tlast<70.)
          fprintf(stderr,"Send command \"Start ADC%d\" to PEP-%s.\n",nch,pep);
           fflush(stderr);
        }
        start=1;
        tok = tcurr;
     }
     if(stop_prog || (start && tcurr-tok>5.)) {
        if(!stop_prog) {
           print_date(stderr);
           fprintf(stderr,"PEP-RK: ADC(0,1,7) (or PEP-A ADC4) timeout!\n");
        }
#if 0
        MeteoMode &= ~SENSOR_T2;
        idt = 0x447;
        dlen=6;
        tdata[0] = 7;
        tdata[2] = tdata[3] = tdata[4] = tdata[5] = 0;
        for(i=0; i<8; i++) {
           if(i==2||i==3||i==5||i==6) /* T-зеркала, ветер, влажность, T на метео-мачте */
          continue;         /* пока не используются (заменены)     */
           if(i==4) {        /*временно: пока АЦП4 идет с PEP-A (давление)*/
          pep = "A";
          idt = 0x40f;            /* PEP-A */
          tdata[1] = nch = 4;     /* АЦП/4 */
           } else {
          pep = "RK";
          idt = 0x447;            /* PEP-RK */
          tdata[1] =  nch = i;    /* АЦП/i  */
           }
           print_date(stderr);
           if(can_send_frame(idt, dlen, tdata)<=0) {
          fprintf(stderr,"Can't send command \"Stop ADC%d\" to PEP-%s!\n",nch,pep);
           } else if(tcurr-tlast<70.)
          fprintf(stderr,"Send command \"Stop ADC%d\" to PEP-%s.\n",nch,pep);
           fflush(stderr);
        }
#endif
        if(stop_prog) can_exit(0);
        start=0;
        tok = tcurr;
     }
      } else {
     if(stop_prog) can_exit(0);
     else {
        static int tpr = 0;
        if(tcurr-tpr>600.) {
           if(PEP_R_Off) {
          print_date(stderr);
          fprintf(stderr,"PEP-RK (ADC0/1/7) turned off!\n");
           }
           if(PEP_A_Off) {
          print_date(stderr);
          fprintf(stderr,"PEP-A (ADC4) turned off!\n");
           }
           tpr=tcurr;
        }
        if(PEP_R_Off) {
           if((MeteoMode & NET_T2)==0)
          MeteoMode &= ~SENSOR_T2;
        }
     }
      }
      while(can_recv_frame(&rxpnt, &rxtime, &idr, &dlen, rdata)) {
     int rcode = 0;
     t = rxtime;
     if(idr==0x447||idr==0x40f) {
        pep = (idr==0x40f)? "A" : "RK";
        if(rdata[0]==7) {
           if(rdata[2] == 0 && rdata[3] == 0)
          fprintf(stderr,"%s PEP-%s: Echo command \"Stop ADC%d\"\n", time2asc(rxtime-timezone),pep,rdata[1]);
           else
          fprintf(stderr,"%s PEP-%s: Echo command \"Start ADC%d\"\n", time2asc(rxtime-timezone),pep,rdata[1]);
        } else if(rdata[0]==8) {
           fprintf(stderr,"%s PEP-%s: Echo command \"Stop all ADC\"\n", time2asc(rxtime-timezone),pep);
           start=0;
        }
        fflush(stderr);
     } else if(idr==0x20c) {   /* временно: код АЦП4 PEP-A - давление */
        if(dlen!=3)
           goto wrong_frame;
        idr = 0x224;           /* временно: имитация  АЦП4 PEP-RK     */
        goto adc_pep_rk;
     } else if((idr&0xff8)==0x220) {   /* код от АЦП PEP-RK */
        static double T2 = 0.;
        static int ctm=0;
        static int terr=0;
        int chan, code;
        double volt,T,b,w,h;
     adc_pep_rk:
        if(dlen!=3) {
           static double last_print = 0.;
        wrong_frame:
           if(tcurr-last_print > 60.) {
          print_date(stderr);
          fprintf(stderr,"Wrong CAN-frame id=0x%x len=%d < ",idr,dlen);
          for(i=0; i<dlen; i++) fprintf(stderr,"%02x ",rdata[i]);
          fprintf(stderr,">\n");
          fflush(stderr);
          last_print = tcurr;
           }
           continue;
        }
        chan = idr&7;
        code = (unsigned int)rdata[0]<<8|rdata[1];
        volt = (double)code/4096.*5.;     /* ADC 12bit 0-5V */
        if(chan == 1){ /* АЦП1 - T-подкупольного */
            ctm |= 2;                    /* АЦП1 - T-подкупольного */
                       T = zeroT2 + (volt-zeroV)*scaleT;
            /*t2=T;*/
                      if(T >= -20. && T <= 30.) {
                         if((MeteoMode & SENSOR_T2) && fabs(T2-T)<5.) {
                        if(fabs(T2-T)>0.1)
                           T2 += (T2>T)? -0.005 : 0.005;
                        else
                           T2 = 0.9*T2 + 0.1*T;
                         } else {
                        T2=T;
                        MeteoMode |= SENSOR_T2;
                         }
                      } else {
                         terr |= 2;
                         if(T<-20.) T2=-20.;
                         else if(T>30.) T2=30.;
                         MeteoMode &= ~SENSOR_T2;
                      }
                      if((MeteoMode & INPUT_T2)== 0 && (MeteoMode & SENSOR_T2))
                         val_T2 = T2;
//printf("Get T=%.1f\n", T2);
                    tok = t;
        }
        if(chan<3 && (ctm&0x93) == 0x93) {  /* АЦП-0,1,4,7 - Ok */
           tok = t;
           ctm=0;
        }
        tlast=t;
     }
     fflush(stdout);
      }
      if(stop_prog) can_exit(0);
   }
}
