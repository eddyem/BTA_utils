/*
 *                                                                                                  geany_encoding=koi8-r
 * config.h
 *
 * Copyright 2018 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
 *
 */
#pragma once
#ifndef __CONFIG_H__
#define __CONFIG_H__

// amount of microsteps for each step
#define USTEPS          (8.)
// gear ratio
#define GEARRAT         (8.)
// steps per one revolution
#define ONETURN_STEPS   (200.)

/*
 * Pins definition (used BROADCOM GPIO pins numbering)
 */

// End-switch - GPIO25 (leg22)
#define ESW_PIN         (25)
// Stepper: STEP - GPIO18 (leg12), DIR - GPIO23 (leg16), EN - GPIO24 (leg18)
#define DIR_PIN         (23)
#define STEP_PIN        (18)
#define EN_PIN          (24)

// active/passive levels (depending on stepper driver connection schematic)
#define PIN_ACTIVE      (0)
#define PIN_PASSIVE     (1)

// active level on end-switch: 0 - normally opened, 1 - normally closed
#define ESW_ACTIVE      (0)

// positive & negative directions (signal @ DIR pin)
#define DIR_POSITIVE    (1)
#define DIR_NEGATIVE    (0)

// maximum 200 steps per second
#define MAX_SPEED   (200)
#define USTEP_DELAY (1./MAX_SPEED/USTEPS/2)

// Position angle calculation (val_Alp, val_Del, S_time - for real work)
#define CALC_PA()       calc_PA(SrcAlpha, SrcDelta, S_time)

// PA value for zero end-switch (add this value to desired PA)
#define PA_ZEROVAL      (0.)


// microsteps per one revolution
#define ONETURN_USTEPS  (ONETURN_STEPS * USTEPS * GEARRAT * 2)
// initial (wrong) value of microsteps counter
#define USTEPSBAD       (2*ONETURN_USTEPS)
// minimal PA delta
#define PA_MINSTEP      (360. / ONETURN_USTEPS)

#endif // __CONFIG_H__
