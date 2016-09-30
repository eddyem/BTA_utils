/*
 * wfsparams.h
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
#ifndef __WFSPARAMS_H__
#define __WFSPARAMS_H__

#include <stdint.h>

#pragma pack(push, 4)
typedef struct{
    double SystemInputPupilM;
    double WLength; // wavelength in meters
    double SysFocusLength;
    double SysRefraction;
    double Pix2WF;  // coefficient of spots' shift reduction into wavefront tilts
    double Pix2Meter; // sensor's pixel size in meters
    double LensD;   // distance between lenses in lenslet (pixels)
    int32_t W, H;       // sensor's size (pixels)
    int32_t bPreEstimate;
    double dPupilShift;
    int32_t iReserved2;
    int32_t SystemOutputPupilP;  // in meters
    double XDir;    // camera mirrored by X? (1 or -1)
    uint32_t Version;
    double YDir;     // camera mirrored by Y? (1 or -1)
    int32_t iReserved3_3[3];
    double dReserved4;
    int32_t iReserved4_2[2];
    int32_t NumberOfPolynomials;
    int32_t iLensletGeometry; // lenslet geometry: 8 - square, 6 - hexagonal, 4 - rombus
    int32_t Afocal;     // TRUE for afocal system
    double LensletFocusLength; // in meters
    int32_t iTRelay;     // WTF?
    double ScaleFactor;
    int32_t WellDepth;   // in electrons
    int8_t reserved_[20]; // added for compatibility with M$W
} Sparam;

enum eDummy {eDummyNone = 0};

typedef struct _SYSTEMTIME {
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
} SYSTEMTIME;

typedef struct {
    int8_t MeasurementID[1024];
    uint8_t Dummy;
    double Dummy1;
    double Dummy2;
    double Dummy3;
    int8_t Reserved1[2048];
    double Dummy4;
    int8_t Dummy5;
    int32_t Reserved2_2[2];
    double Dummy6;
    double Reserved3;
    int8_t bReserved4;
    double Reserved5;
    SYSTEMTIME DateTime;
    double Dummy7;
    double Dummy8;
    int8_t Dummy9;
    int64_t Reserved6;
} Mparam;

typedef struct {
    int32_t NSpots;
    union {
        struct {
            int32_t ux, uy, ur; // unitary circle center & radius
            int32_t ax, ay, ar; // measurement area -//-  ar == 0 - rectangle, ar < 0 - inverted (outside this radius)
        } C;
        struct {
            int32_t x, y, a, w, h; // rectangle area center, rotation angle, width and height
        } R;
    };
    int32_t pointers[6]; //uint8_t *ucpReserved2;
    uint32_t ucpReserved2;
    int8_t bReserved3;
    //int8_t Reserved4[256];
    int8_t Reserved4[258];
} Hhistory;

typedef struct{
    double loPolynomials[37]; // if amount of poly <37, they are stored here
    int32_t CurrentNumberOfPolynomials;
    double Sphere, Cylinder, Axis; // approximate sphere & astigmatism in dptr/degrees
    double ADiameter; // measurement circle diameter in meters
    int32_t Time; // time from measurement starts (ms)
    double Reserved3_1[3];
    uint8_t PolynomialSet; // 0 - Fringe, 1 - Born/Wolf, 2 - OSA , 4 - Ring
    double Reserved3_2[3];
    Hhistory h;
    double sx, sy, sr; // center & beam radius calculated by S-H (meters)
    double UDiameter; // diameter of unitary circle (meters)
    int32_t pfReserved3; // float*
    int32_t iReserved4;
    uint8_t bReserved5_6[6];
    int32_t pdReserved6; // double*
    int8_t bBad; // == true if current image bad
    int8_t bZRW; // == true if wavefront was reconstructed by zonal method
    int32_t pReserved7; // float*
    double chi2;
    long double time_mcsec; // time from beginning of measurement, mks
    int8_t reserved_[74]; // added for compatibility with M$W
    uint8_t zhend[0];
} Zhistory;

typedef struct{
    uint32_t Zhistory_sz;
    uint32_t Hhistory_sz;
    uint32_t Sparam_sz;
    Sparam sparam;
    uint32_t Mparam_sz;
    Mparam mparam;
    int32_t History_len;
} WFS_header;
#pragma pack(pop)

int test_wfs_file(int fd);
void print_table(Zhistory *hist, int fd);
Zhistory *show_zhistry(int fd);

#endif // __WFSPARAMS_H__
