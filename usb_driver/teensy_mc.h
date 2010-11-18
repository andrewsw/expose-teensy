/* Author: Nathan Collins <nathan.collins@gmail.com> */
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
