/* submodules.h
 *
 *  use this header file to add submodules to the list of those
 *  initialized during a teensy probe.
 *
 *  a bit of code to control the
 *  initializing of sub-modules for the teensy driver.
 * 
 * Copyright (C) 2010 Nathan Collins <nathan.collins@gmail.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301 USA.
 *
 */#ifndef __SUBMODULES_H__
#define __SUBMODULES_H__

/*** submodules: EDIT HERE TO ADD SUBMODULE ***/
/* to add a submodule: 
   1. include headers here
   2. add init and exit to arrays here
   3. add the <your_submodule>.o to teensy_mono-objs in the Makefile
   (4. write code to run on the teensy and handle your request ...)
*/
/* NC: I couldn't figure out how to have these in submodule.h (it's
   included in multiple places), so i moved the defs here */
#include "teensy_adc.h"
#include "teensy_mc.h"
static int  (*inits[])(void) __attribute__((unused)) = {
        adc_init,
        mc_init,
};
static void (*exits[])(void) __attribute__((unused)) = {
        adc_exit,
        mc_exit,
};

/* call all inits/exits */
int init_submodules(void);
void exit_submodules(void);
#endif
