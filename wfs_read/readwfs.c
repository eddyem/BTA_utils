/*
 * readwfs.c
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
#include <time.h>

#include "usefull_macros.h"
#include "readwfs.h"

WFS_header hdr;
void hexdump(int fd, int N);


void signals(int sig){
    exit(sig);
}

void gettime(struct tm *tm, SYSTEMTIME *st){
    if(!tm || !st) return;
    tm->tm_sec = st->wSecond;
    tm->tm_min = st->wMinute;
    tm->tm_hour = st->wHour;
    tm->tm_mday = st->wDay;
    tm->tm_mon = st->wMonth - 1;
    tm->tm_year = st->wYear - 1900;
    tm->tm_isdst = -1;
}

/**
 * read from file fd maxlen bytes into optr with size optrlen
 * return 0 if suxeed
 */
int get_struct(int fd, void *optr, int optrlen, int maxlen){
    if(maxlen < optrlen) return 1;
    if(optrlen != read(fd, optr, optrlen)) return 2;
    if(optrlen != maxlen){
        //printf("seek to %d", maxlen-optrlen);
        lseek(fd, maxlen-optrlen, SEEK_CUR);
    }
    return 0;
}

void show_sparam(int fd){
    Sparam *par = &hdr.sparam;
    if(get_struct(fd, (void*)par, sizeof(Sparam), hdr.Sparam_sz)) ERRX("read sparam");
    //if(get_struct(fd, (void*)par, sizeof(Sparam), sizeof(Sparam))) ERRX("read sparam");
    printf("\nSPARAM:\nInput pupil: %gmm, wavelength: %gnm\n", par->SystemInputPupilM*1e3,
        par->WLength*1e9);
    printf("Focus len: %gmm, refraction idx: %g\n", par->SysFocusLength*1e3,
        par->SysRefraction);
    printf("Pix2WF: %g, pixel size: %gmkm, sensor size(W:H): %d:%d\n", par->Pix2WF, par->Pix2Meter*1e6,
        par->W, par->H);
    printf("Distance between lenses: %g, %spre-estimate\n", par->LensD, par->bPreEstimate ? "" : "not ");
    printf("Pupil shift: %gmm, output pupil: %dm\n", par->dPupilShift*1e3,
        par->SystemOutputPupilP);
    printf("Xdir: %g, Ydir: %g\n", par->XDir, par->YDir);
    printf("Version: %d\n", par->Version);
    printf("Using %d polynomials\n", par->NumberOfPolynomials);
    printf("Lenslet geometry: %s\n", par->iLensletGeometry == 8 ? "square":
        (par->iLensletGeometry == 6 ? "hexagonal" : "rombus"));
    if(par->Afocal) printf("Afocal system\n");
    printf("Lenslet F: %gmm\n", par->LensletFocusLength);
    printf("Scale factor: %g, Well depth: %de\n\n", par->ScaleFactor, par->WellDepth);
}

void show_mparam(int fd){
    Mparam *par = &hdr.mparam;
    if(get_struct(fd, (void*)par, sizeof(Mparam), hdr.Mparam_sz)) ERRX("read mparam");
    //if(get_struct(fd, (void*)par, sizeof(Mparam), sizeof(Mparam))) ERRX("read mparam");
    struct tm tm;
    gettime(&tm, &par->DateTime);
    time_t t = mktime(&tm);
    printf("\nMPARAM:\nMeasurements date/time: %s\n", ctime(&t));
    int i;
    printf("#\tMeasurement id\n");
    for(i = 0; i < 1024; ++i) if(par->MeasurementID[i]) printf("%d\t0x%02X\n", i, par->MeasurementID[i]);
}

/**
 "* Test header of WFS file
 */
int test_wfs_file(int fd){
    if(fd < 1) return 0;
    int L = 3*sizeof(uint32_t);
    if(read(fd, &hdr, L) != L){
        ERR("read");
    }
    if(sizeof(Zhistory) > hdr.Zhistory_sz){
        ERRX("Zhistory size is %d instead of %d\n", hdr.Zhistory_sz, sizeof(Zhistory));
    }
    printf("Zhistory: should be: %ld, in file: %d\n", sizeof(Zhistory), hdr.Zhistory_sz);
    if(sizeof(Hhistory) > hdr.Hhistory_sz){
        ERRX("Hhistory size is %d instead of %d\n", hdr.Hhistory_sz, sizeof(Hhistory));
    }
    printf("Hhistory: should be: %ld, in file: %d\n", sizeof(Hhistory), hdr.Hhistory_sz);
    if(sizeof(Sparam) > hdr.Sparam_sz){
        ERRX("Sparam size is %d instead of %d\n", hdr.Sparam_sz, sizeof(Sparam));
    }
    printf("Sparam: should be: %ld, in file: %d\n", sizeof(Sparam), hdr.Sparam_sz);
    show_sparam(fd);
    if(sizeof(uint32_t) != read(fd, &hdr.Mparam_sz, sizeof(uint32_t))){
        ERR("read");
    }
    printf("Mparam: should be: %ld, in file: %d\n", sizeof(Mparam), hdr.Mparam_sz);
    if(sizeof(Mparam) > hdr.Mparam_sz){
        ERRX("Mparam size is %d instead of %d\n", hdr.Mparam_sz, sizeof(Mparam));
    }
    show_mparam(fd);
    if(sizeof(uint32_t) != read(fd, &hdr.History_len, sizeof(uint32_t))){
        ERR("read");
    }
    printf("\n\n%d records in history\n\n", hdr.History_len);
    return 1;
}

void show_hhistry(Hhistory *h){
    printf("\nHHISTORY:\nN spots: %d\n", h->NSpots);
    printf("Unitary circle center: (%d, %d), radius: %d\n", h->C.ux, h->C.uy, h->C.ur);
    printf("Measurement area center: (%d, %d), ", h->C.ax, h->C.ay);
    if(h->C.ar == 0) printf("rectangle\n");
    else{
        if(h->C.ar < 0) printf("radius: %d (area outside this radius)\n", -h->C.ar);
        else printf("radius: %d\n", h->C.ar);
    }
    printf("Pointers: ");
    int i; for(i = 0; i < 6; ++i) printf("%d, ", h->pointers[i]);
    printf("\nPdiff: ");
    for(i = 1; i < 6; ++i) printf("%d, ", h->pointers[i] - h->pointers[i-1]);
    printf("\n");
}

static off_t zhstart;
Zhistory *show_zhistry(int fd){
    Zhistory *hist = MALLOC(Zhistory, 1);
    zhstart = lseek(fd, 0, SEEK_CUR);
    int L = (void*)&hist->h - (void*)hist;
    if(get_struct(fd, (void*)hist, L, L)) ERRX("read Zhistory header");
    if(get_struct(fd, (void*)&hist->h, sizeof(Hhistory), hdr.Hhistory_sz)) ERRX("read Hhistory");
    L = (void*)&hist->zhend - (void*)&hist->sx;
    int szdiff = (hdr.Zhistory_sz - sizeof(Zhistory)) - (hdr.Hhistory_sz - sizeof(Hhistory));
    // 2 - dirty hack
    if(get_struct(fd, (void*)&hist->sx, L, L+szdiff+2)) ERRX("read remain of Zhistory");
    //printf("diff: %d\n", szdiff);
    printf("\nZHISTORY:\nCurrent number of poly: %d\n", hist->CurrentNumberOfPolynomials);
    if(hist->CurrentNumberOfPolynomials < 37){
        int i;
        printf("#\t polynomial value\n");
        for(i = 0; i < 37; ++i) printf("%d\t%g\n", i, hist->loPolynomials[i]);
    }
    printf("Sphere: %g, Cylinder: %g, Axis: %g\n", hist->Sphere, hist->Cylinder, hist->Axis);
    printf("Circle diameter: %gmm\n", hist->ADiameter * 1e3);
    printf("Time from measurements start: %dms\n", hist->Time);
    printf("Polynomial set: %s\n", hist->PolynomialSet == 0 ? "Fringe" :
        (hist->PolynomialSet == 1 ? "Born & Wolf" :
            (hist->PolynomialSet == 2 ? "OSA" : "Ring")));
    show_hhistry(&hist->h);
    printf("Beam center: (%g, %g)mm, radius: %gmm\n",
        hist->sx*1e3, hist->sy*1e3, hist->sr*1e3);
    printf("Unitary circle diameter: %gmm\n", hist->UDiameter*1e3);
    if(hist->bBad) red("Bad image - not use it!\n");
    if(hist->bZRW) printf("Reconstructed by zonal method\n");
    printf("Chi^2 = %g\n", hist->chi2);
    printf("Time from start %Lgmks\n", hist->time_mcsec);
    printf("\n\nRESERVED:\nReserved3_1: %g, %g, %g\n", hist->Reserved3_1[0], hist->Reserved3_1[1], hist->Reserved3_1[2]);
    printf("Reserved3_2: %g, %g, %g\n", hist->Reserved3_2[0], hist->Reserved3_2[1], hist->Reserved3_2[2]);
    printf("pfReserved3: %d, iReserved4: %d\n", hist->pfReserved3, hist->iReserved4);
    printf("pdReserved6: %d, pReserved7: %d\n", hist->pdReserved6, hist->pReserved7);

    return hist;
}

float *getflt(int fd, int sz){
    int L = sizeof(float)*sz;
    float *flt = MALLOC(float, sz);
    if(L != read(fd, flt, L)) ERR("read");
    return flt;
}
uint8_t *get_uint8(int fd, int sz){
    uint8_t *u = MALLOC(uint8_t, sz);
    if(sz != read(fd, u, sz)) ERR("read");
    return u;
}
void hexdump(int fd, int N){
    uint8_t *hx = MALLOC(uint8_t, N);
    int i, L = read(fd, hx, N);
    for(i = 0; i < L; ++i){
        printf("0x%02X ", hx[i]);
        if(i%16 == 15) printf("\n");
    }
    printf("\n");
    FREE(hx);
}

/**
 * print table of parameters for given "history"
 */
void print_table(Zhistory *hist, int fd){
    int Nspots = hist->h.NSpots, Npoly = hist->CurrentNumberOfPolynomials, i, zon = hist->bZRW;
    float *XR, *YR, *XC, *YC, *W, *D,  *poly = NULL, *Z=NULL, *I;
    uint8_t *flag;
    off_t blkstart = lseek(fd, 0, SEEK_CUR);
    XR = getflt(fd, Nspots);
    YR = getflt(fd, Nspots);
    XC = getflt(fd, Nspots);
    YC = getflt(fd, Nspots);
    W = getflt(fd, Nspots);
    D = getflt(fd, Nspots);
    flag = MALLOC(uint8_t, Nspots);
    I = getflt(fd, Nspots);
    if(Npoly > 37) poly = getflt(fd, Npoly);
    printf("\nTable:\n# Arrays of data for frame\n");
    printf("#\tXR\tYR\tXC\tYC\tWeight\tDispersion\tFlag\tIntensity");
    if(zon){
        printf("\tZonal");
        Z = getflt(fd, Nspots);
    }
    printf("\n");
    for(i = 0; i < Nspots; ++i){
        printf("%d\t%f\t%f\t%f\t%f\t%f\t%f\t%d\t%f", i,XR[i], YR[i], XC[i], YC[i],
                W[i], D[i], flag[i], I[i]);
        if(zon) printf("\t%f", Z[i]);
        printf("\n");
    }
    if(poly){
        printf("\nPolynomial coefficients\n#\tcoeff\n");
        for(i = 0; i < Npoly; ++i) printf("%d\t%f\n", i, poly[i]);
    }
    FREE(XR); FREE(YR); FREE(XC); FREE(YC); FREE(W); FREE(D); FREE(flag);
    FREE(I);
    FREE(Z); FREE(poly);
    //printf("next 4474:\n");
    //hexdump(fd, 4474);
    printf("next 1346:\n");
    hexdump(fd, 1346);
    off_t blkend = lseek(fd, 0, SEEK_CUR);
    printf("Block length: %zd, ZH length: %zd\n", blkend - blkstart, blkend - zhstart);
/*  printf("starting of next ZHistory\nzeros: \n");
    hexdump(fd, 37*8);
    printf("CurrentNumberOfPolynomials: ");
    hexdump(fd, 4);*/
}
