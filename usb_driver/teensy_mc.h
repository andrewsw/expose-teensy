/* teensy_mc.h
 *
 *  a character device for driving the pwm-controllers on a teensy to
 *  control a motor.
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
 * based on sstore.c
 */
#include <linux/ioctl.h>

#ifndef __MC_H__
#define __MC_H__

/* ioctls */
/* choosing ioctl numbers:
 * * include/asm-generic/ioctl.h
 * * Documentation/ioctl/ioctl-decoding.txt
 * * Documentation/ioctl-number.txt
 * * ldd3 ch 6
 */

/* MAYBE TODO: would be nicer to use /sys instead of ioctls? */
#define MC_IOC_MAGIC 'M'
#define MC_IOC_STOP _IO(MC_IOC_MAGIC, 42)       /* stop */
#define MC_IOC_FWD  _IOW(MC_IOC_MAGIC, 43, int) /* forward, at given speed */
#define MC_IOC_REV  _IOW(MC_IOC_MAGIC, 44, int) /* reverse, at given speed */

int  mc_init(void);
void mc_exit(void);
#endif
