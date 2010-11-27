/*
 *  user_test.c  
 *  userland test file that read from an ADC 
 *  to set the speed of the motor.
 * 
 *  This is a linux module to expose portions of the linux hardware to
 *  userspace as character devices.
 *
 *  Copyright (C) 2010  Rémi Auduon <remi.auduon@pdx.edu> 
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
 * 
 */
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#include "usb_driver/teensy_mc.h"

#define DEBUG(x...)  fprintf(stderr, x) 

void usage(char * argv0) {
  fprintf(stderr, "usage: %s\n",
          argv0);
  exit(2);
}

int main(int argc, char ** argv) {
	/* constant limit values of ADCs and MC speed */
	const int MinAdc0 = 0;
	const int MinAdc1 = 0x0100; /* experimental min from light sensor */
	const int MaxAdc0 = 100;
	const int MaxAdc1 = 0x03ff; /* experimental maxish */
	const int MaxMSpeed = 190;
	const int MinMSpeed = 120;
	int value, speed;
        int fdAdc0, fdM0, fdAdc1, fdM1;
	char adc_file0[] = "/dev/adc0";
	char adc_file1[] = "/dev/adc1"; 
	char mc_file0[] = "/dev/mc0";
	char mc_file1[] = "/dev/mc1";
	uint8_t buf0[2], buf1[2];

        /* check args */
        if (argc != 1)
          usage(argv[0]);

	/* open all needed files */
	fdAdc0 = open(adc_file0, O_RDONLY);
	if (fdAdc0 < 0) {
                fprintf(stderr, "open(%s): ", adc_file0);
                perror(NULL);
                exit(errno);
        }
	fdAdc1 = open(adc_file1, O_RDONLY);
	if (fdAdc1 < 0) {
                fprintf(stderr, "open(%s): ", adc_file1);
                perror(NULL);
                exit(errno);
        }
	fdM0 = open(mc_file0, O_WRONLY);
	if (fdM0 < 0) {
                fprintf(stderr, "open(%s): ", mc_file0);
                perror(NULL);
                exit(errno);
        }
/*	fdM1 = open(mc_file1, O_WRONLY);
	if (fdM1 < 0) {
                fprintf(stderr, "open(%s): ", mc_file1);
                perror(NULL);
                exit(errno);
        }*/

	while(1)
	{
		/* read 1 byte from both adc */
		read(fdAdc0, buf0, 1);
		read(fdAdc1, buf1, 2);

		/* write the corresponding speed to both mc */
		DEBUG("buf[0]=%x, buf1[1]=%x\n",buf1[0], buf1[1]);
		
		value = (buf1[0] << 8) +  buf1[1];
		DEBUG("mc=0, value=%x\n", value);
		speed = ((float)value-MinAdc1)/(MaxAdc1-MinAdc1)
				*(MaxMSpeed-MinMSpeed) + MinMSpeed;
		speed = speed<MinMSpeed + 10 ? 0 : speed;
		speed = speed>MaxMSpeed ? MaxMSpeed : speed;
		DEBUG("speed=%d\n", speed);
		
		if (ioctl(fdM0, MC_IOC_FWD, speed ) < 0){
			perror("ioctl error");
			exit(errno);
		}
/*		value = (int) buf1;
		DEBUG("mc=1, speed=%i\n", value);
		speed = ((float)value-MinAdc1)/(MaxAdc1-MinAdc1)
				*(MaxMSpeed-MinMSpeed) + MinMSpeed;
		speed<MinMSpeed ? MinMSpeed : speed;
		speed>MaxMSpeed ? MaxMSpeed : speed;
		if (ioctl(fdM1, MC_IOC_FWD, speed ) < 0) {
			perror("ioctl error");
			exit(errno);
		}*/

		/* put the program to sleep to avoid sending to much data */
		//sleep(1);
	}
	return 0;
}
