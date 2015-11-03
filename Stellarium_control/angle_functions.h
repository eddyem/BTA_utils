/*
 * angle_functions.h
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
#pragma once
#ifndef __ANGLE_FUNCTIONS_H__
#define __ANGLE_FUNCTIONS_H__

double sec_to_degr(double sec);
void calc_AP(double r, double d, double *appRA, double *appDecl);
void calc_mean(double appRA, double appDecl, double *r, double *d);
void calc_AD(double az, double zd, double stime, double *alpha, double *delta);
#endif // __ANGLE_FUNCTIONS_H__
