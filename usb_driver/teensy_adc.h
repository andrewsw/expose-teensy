/* Author: Nathan Collins <nathan.collins@gmail.com> */
#include <linux/ioctl.h>

#ifndef __ADC_H__
#define __ADC_H__

#define ADC_FREE_ON_LAST_CLOSE 1 /* initial value for sstore_dev_t.free_on_last_close */

/* ioctls */
/* choosing ioctl numbers:
 * * include/asm-generic/ioctl.h
 * * Documentation/ioctl/ioctl-decoding.txt
 * * Documentation/ioctl-number.txt
 * * ldd3 ch 6
 */
#define ADC_IOC_MAGIC 'S'
#define ADC_IOC_DEL _IOW(ADC_IOC_MAGIC, 42, int) /* delete blob at given index */

/* for *reading* and writing data */
struct adc_buf {
  int size;
  int index;
  char * data;
};

int  adc_init(void);
void adc_exit(void);
#endif
