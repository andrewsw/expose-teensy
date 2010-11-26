/* userland_mc.c
 *
 * userland test driver for sstore kernel module
 *
 * Run with no arguments for usage.
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
 */
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
  fprintf(stderr, "usage: %s MC SPEED DIRECTION \n\n"
          "where MC = <NUM> specifies mc dev,\n"
          "0 <= SPEED < 256 specifies speed,\n"
          "('f'|'s'|'r') specifies direction.\n",
          argv0);
  exit(2);
}

int main(int argc, char ** argv) {
        int speed, fd, ioc;
        char mc, direction, mc_file[] = "/dev/mc?";

        if (argc >= 4)
                DEBUG("v0=%s, v1=%s, v2=%s, v3=%s\n", argv[0], argv[1], argv[2], argv[3]);

        if (argc >= 4) {
                mc = argv[1][0];
                direction = argv[3][0];
        }

        /* check args and init vars */
        if (argc != 4 ||
            /* (! (mc == 0 || mc == 1)) || */
            sscanf(argv[2], "%i", &speed)  != 1 ||
            (speed < 0 || 255 < speed) ||
            (direction != 'f' && direction != 's' && direction != 'r'))
          usage(argv[0]);

        DEBUG("mc=%c, speed=%i, direction=%c\n", mc, speed, direction);

        /* HACK: choose mc dev */
        //mc_file = "/dev/mc?";
        DEBUG("mc file is %s\n", mc_file);
        mc_file[7] = mc;
        DEBUG("mc file is %s\n", mc_file);
        fd = open(mc_file, O_WRONLY);
        if (fd < 0) {
                fprintf(stderr, "open(%s): ", mc_file);
                perror(NULL);
                exit(errno);
        }

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
                perror("ioctl error");
                exit(errno);
        }
}
