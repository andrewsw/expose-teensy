/* teensy_adc.c
 *
 * Author: Nathan Collins <nathan.collins@gmail.com>
 *
 * based on sstore.c
 */

#include <linux/module.h>
#include <linux/init.h>   
#include <linux/uaccess.h>    /* copy_*_user */ 
#include <linux/device.h>     /* create_class and friends ? */
#include <linux/cdev.h>   
#include <linux/sched.h>      /* current */
#include <linux/kernel.h>     /* printk() */
#include <linux/fs.h>
#include <linux/types.h>      /* size_t */
#include <linux/capability.h> /* access controls */
#include <linux/mutex.h>
#include <linux/wait.h>       /* sleep */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>   /* large proc read() */

#include "teensy_adc.h"
#include "teensy.h"

/* TODO
 *
 */

MODULE_LICENSE("Dual BSD/GPL");

#define DEVICE_NAME "adc"
#define ADC_NUM_DEVS 2

static struct adc_dev_t {
        struct cdev cdev;
} adc_devs[ADC_NUM_DEVS];

/* to put in filp->private_data */
/* make it a struct so i can add more fields later if needed */
struct adc_filp_data {
        struct adc_dev_t * adc;
};

static dev_t adc_dev_number;
struct class * adc_class;

/*** params ***/

/*** helpers ***/

#define pk(fmt,args...) printk(KERN_DEBUG "adc: process %i (%s): " fmt, \
                               current->pid, current->comm, ##args)
#define ui (unsigned int)

static struct adc_filp_data * _get_private_data (struct file * filp) __attribute__((unused)); /* prevent "usused" compile warning */
static struct adc_filp_data * _get_private_data (struct file * filp) {
        return (struct adc_filp_data *)filp->private_data;
}

/*** API ***/

int adc_open (struct inode *inode, struct file *filp) {
        struct adc_filp_data * data;
        struct adc_dev_t * dev;

        pk("open(): iminor=%d, filp=0x%X\n", iminor(inode), ui filp);
  
        /* filp->private_data */
        dev = &adc_devs[iminor(inode)];
        data = kmalloc(sizeof(struct adc_filp_data), GFP_KERNEL);
        if (!data)
                return -ENOMEM;
        data->adc = dev;
        filp->private_data = data;

        return 0;
}

int adc_release (struct inode * inode, struct file * filp) {
        //struct adc_dev_t * dev = _get_private_data(filp)->adc;

        pk("release(): iminor=%d, filp=0x%X\n", iminor(inode), ui filp);

        kfree(filp->private_data);
        return 0;
}


/* @buf:
 * @count:
 * @return:
 */
ssize_t adc_read (struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
        //struct adc_dev_t * dev = _get_private_data(filp)->adc;
        int ret = 0;
        char * mybuf = kmalloc(1, GFP_KERNEL);
        struct read_request req = {
                .buf   = mybuf,
                .size  = 1,
        };

        pk("read(): buf=0x%X, count=%d, *pos=0x%X\n", ui buf, count, ui *pos);

        if (mybuf == NULL) {
                pk("adc_read(): no mem when allocing zero bytes\n");
                return -ENOMEM;
        }

        req.buf[0] = 'a';
        
        /* pass request to teensy_read() */
        /* teensy_read() returns a DIFFERENT buf in req->buf, so we must
           free that; our mybuf was already free'd in teensy_read().
        */
        ret = teensy_read(&req);
        if (ret < 0) {
                pk("adc_read(): error calling teensy_read()\n");
                return ret;
        }
        printk(KERN_DEBUG "adc_read(): read %i bytes from teensy\n", req.size);

        /* copy data to user buf */
        ret = req.size < count ? req.size : count; /* min */
        if (copy_to_user(buf, req.buf, ret)) {
                ret = -EFAULT;
                pk("adc_read(): copy_to_user() failed\n");
                return ret;
        }
        printk(KERN_DEBUG "adc_read(): copied %i bytes to userbuf\n", ret);

        kfree(req.buf); /* free the NEW buf */

        return ret;
}

struct file_operations adc_fops = {
        .owner   = THIS_MODULE,
        .read    = adc_read,
        .open    = adc_open,
        .release = adc_release,
};

/*** setup/teardown ***/

int adc_init(void)
{
        struct adc_dev_t * dev;
        int result, i;

        pk ("init():\n");

        /*
         * Register major, and accept a dynamic number
         */
        result = alloc_chrdev_region(&adc_dev_number, 0, ADC_NUM_DEVS, DEVICE_NAME);
        if (result < 0)
                return result;

        /* sysfs */
        adc_class = class_create(THIS_MODULE, DEVICE_NAME);

        /* MAYBE FIXME: memory leak on failure? (if module load is unlike userland, where memory is freed on exit ...) */
        for (i = 0; i < ADC_NUM_DEVS; ++i) {
                dev = &adc_devs[i];

                /* cdev */ /* mostly copying ELDD cmos from here on ... */
                cdev_init(&dev->cdev, &adc_fops);
                dev->cdev.owner = THIS_MODULE;
                result = cdev_add(&dev->cdev, MKDEV(MAJOR(adc_dev_number), i), 1);
                if (result < 0)
                        return result;

                /* udev /dev node creation */ /* returns pointer to /sys entry as well */
                /* http://www.gnugeneration.com/books/linux/2.6.20/kernel-api/re694.html */
                device_create(adc_class, NULL, MKDEV(MAJOR(adc_dev_number), i), DEVICE_NAME "%d", i);
        }
        return 0;
}

void adc_exit(void)
{
        int i;
        struct adc_dev_t * dev;

        for (i = 0; i < ADC_NUM_DEVS; ++i) {
                dev = &adc_devs[i];

                /* sysfs and udev */
                device_destroy(adc_class, MKDEV(MAJOR(adc_dev_number), i));
                /* cdev */
                cdev_del(&dev->cdev);
        }

        unregister_chrdev_region(adc_dev_number, ADC_NUM_DEVS);
        class_destroy(adc_class);

        pk("cleanup(): module cleaned up successfully\n");
}

/* thoght these were ignored by the teeny_mono setup in Makefile ... */
//module_init(adc_init);
//module_exit(adc_exit);
