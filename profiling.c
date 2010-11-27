/*
 * profiling.c
 *
 * test code to demo controlling slot cars to show cpu load
 * 
 *  Copyright (C) 2010  Xingzhi Pan <pan.xingzhi@gmail.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define String_startsWith(s, match) (strstr((s), (match)) == (s))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

int countProcessors()
{
    FILE *fp = fopen("/proc/stat", "r");
    char buffer[256];
    int procs = -1, processorCount, i;
    do {
        procs++;
        fgets(buffer, 255, fp);
    } while (String_startsWith(buffer, "cpu"));

    processorCount = procs - 1;
    fclose(fp);
    return processorCount;
}

int main(int argc, char *argv[])
{

    unsigned long long int prevusertime, prevnicetime, prevSystemTime,
                  previdletime, prevIoWait, previrq, prevSoftIrq, prevsteal;
    unsigned long long int usertime, nicetime, systemtime, idletime, ioWait,
                  irq, softIrq, steal;
    unsigned long long int prevSystemAllTime, prevTotalTime, prevIdleAllTime;
    /*int i, processorCount = countProcessors();*/
    char buffer[256];

    // initialize
    FILE *fp = fopen("/proc/stat", "r");
    fgets(buffer, 255, fp);
    sscanf(buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
            &prevusertime, &prevnicetime, &prevSystemTime, &previdletime,
            &prevIoWait, &previrq, &prevSoftIrq, &prevsteal);
    prevSystemAllTime = prevSystemTime + previrq + prevSoftIrq + prevsteal;
    prevIdleAllTime = previdletime + prevIoWait;
    prevTotalTime = prevusertime + prevnicetime + prevSystemAllTime + prevIdleAllTime;
    printf("prevnicetime %d prevusertime %d prevSystemTime %d\n",
            prevnicetime, prevusertime, prevSystemTime);
    fclose(fp);

    while (1) {
        sleep(2);
        fp = fopen("/proc/stat", "r");
        fgets(buffer, 255, fp);
        sscanf(buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
                        &usertime, &nicetime, &systemtime, &idletime,
                        &ioWait, &irq, &softIrq, &steal);

        unsigned long long int nicePeriod, userPeriod, systemAllPeriod,
                      idlealltime, systemAllTime, totaltime;
        double totalPeriod;

        nicePeriod = nicetime - prevnicetime;
        userPeriod = usertime - prevusertime;
        systemAllTime = systemtime + irq + softIrq + steal;
        systemAllPeriod = systemAllTime - prevSystemAllTime;
        idlealltime = idletime + ioWait;
        totaltime = usertime + nicetime + systemAllTime + idlealltime;
        totalPeriod = totaltime - prevTotalTime;

        prevnicetime = nicetime;
        prevusertime = usertime;
        prevSystemAllTime = systemAllTime;
        prevTotalTime = totaltime;

        printf("%s\n", buffer);
        printf("nicetime %d usertime %d systemtime %d\n",
                nicetime, usertime, systemtime);
        printf("nicePeriod %d userPeriod %d systemAllPeriod %d\n",
                nicePeriod, userPeriod, systemAllPeriod);
        // CPUMeter.c
        double cpu = MIN(100.0, MAX(0.0, ((nicePeriod + userPeriod + systemAllPeriod) / totalPeriod * 100.0)));
        printf("%f\n", cpu);
        fclose(fp);
    }
}
