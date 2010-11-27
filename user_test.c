/* userland test file that read from an ADC 
 * to set the speed of the motor.
 *
 * Author: Rémi Auduon <remi.auduon@pdx.edu> 
 */
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "usb_driver/teensy_mc.h"

#define DEBUG(x...) /* fprintf(stderr, x) */

void usage(char * argv0) {
  fprintf(stderr, "usage: %s\n",
          argv0);
  exit(2);
}

int main(int argc, char ** argv) {
	/* constant limit values of ADCs and MC speed*/
	const int MinAdc0 = 0;
/*	const int MinAdc1 = 0;*/
	const int MaxAdc0 = 100;
/*	const int MaxAdc1 = 100;*/
	const int MaxMSpeed = 250;
	const int MinMSpeed = 0;
	int value, speed;
        int fdAdc0, fdM0 /*, fdAdc1, fdM1 */;
	char adc_file0[] = "/dev/adc0";
/*	char adc_file1[] = "/dev/adc1"; */
	char mc_file0[] = "/dev/mc0";
/*	char mc_file1[] = "/dev/mc1";*/
	char buf0, buf1;

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
/*	fdAdc1 = open(adc_file1, O_RDONLY);
	if (fdAdc1 < 0) {
                fprintf(stderr, "open(%s): ", adc_file1);
                perror(NULL);
                exit(errno);
        }*/
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
/*		read(fdAdc1, buf1, 1);*/

		/* write the corresponding speed to both mc */
		if (sscanf(&buf0, "%i", &value) != 1) {
			DEBUG("Error reading speed from ADC0\n");
		}
		else {
			DEBUG("mc=0, speed=%i\n", value);
			speed = (value-MinAdc0)/(MaxAdc0-MinAdc0)
					*(MaxMSpeed-MinMSpeed) + MinMSpeed;
			speed<MinMSpeed ? MinMSpeed : speed;
			speed>MaxMSpeed ? MaxMSpeed : speed;
			if (ioctl(fdM0, 
				MC_IOC_FWD,
				speed ) < 0){
				perror("ioctl error");
				exit(errno);
			}
		}
/*		if (sscanf(buf1, "%d", &speed) != 1) {
			DEBUG("Error reading speed from ADC1\n");
		}
		else {
			DEBUG("mc=1, speed=%i\n", value);
			speed = (value-MinAdc1)/(MaxAdc1-MinAdc1)
					*(MaxMSpeed-MinMSpeed) + MinMSpeed;
			speed<MinMSpeed ? MinMSpeed : speed;
			speed>MaxMSpeed ? MaxMSpeed : speed;
			if (ioctl(fdM1, 
				MC_IOC_FWD, 
				speed ) < 0 {
				perror("ioctl error");
				exit(errno);
			}
		}
*/
		/* put the program to sleep to avoid sending to much data */
		sleep(1);
	}
	return 0;
}
