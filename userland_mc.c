/* userland test driver for sstore kernel module
 *
 * Run with no arguments for usage.
 *
 * Author: Nathan Collins <nathan.collins@gmail.com> 
 */
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "usb_driver/teensy_mc.h"

void usage(char * argv0) {
  fprintf(stderr, "usage: %s MC SPEED DIRECTION \n\n"
          "where MC = <NUM> specifies mc dev,\n"
          "0 <= SPEED < 256 specifies speed,\n"
          "('f'|'s'|'r') specifies direction.\n",
          argv0);
  exit(2);
}

int main(int argc, char ** argv) {
        int speed, fd, ioc;
        char mc, direction, *mc_file;

        /* check args and init vars */
        if (argc != 4 ||
            sscanf(argv[1], "%c", &mc) != 1 ||
            /* (! (mc == 0 || mc == 1)) || */
            sscanf(argv[2], "%i", &speed)  != 1 ||
            (speed < 0 || 255 < speed) || 
            sscanf(argv[3], "%c", &direction)  != 1 ||
            (direction != 'f' && direction != 's' && direction != 'r'))
          usage(argv[0]);

        /* HACK */
        mc_file = "/dev/mc?";
        mc_file[7] = mc;
        fd = open(mc_file, O_WRONLY);

        switch (direction) {
        case 's':
                ioc = MC_IOC_STOP;
                break;
        case 'f':
                ioc = MC_IOC_FWD;
                break;
        case 'r':
                ioc = MC_IOC_REV;
                break;
        }
        if (ioctl(fd, ioc, speed) < 0) {
                perror("mc: ioctl error");
                exit(1);
        }
}
