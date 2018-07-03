/*
 * stepper.h
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
#ifndef __STEPPER_H__
#define __STEPPER_H__

double getcurpos();
void setup_pins();
int gotozero();
void stop_motor();
int gotoangle(double pa);
double corrpa(double newval);
double getpval();
void mk_pause(double dt);

void stepper_process();

//void stop_motor();

//void *steppers_thread(void *buf);
//void Xmove(int dir, unsigned int Nsteps);

/*
int get_rest_steps();
int get_direction();
int get_endsw();
*/

#endif // __STEPPER_H__
