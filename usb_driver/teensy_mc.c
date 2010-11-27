/* teensy_mc.c
 *
 *  a character device for driving the pwm-controllers on a teensy to
 *  control a motor.
 * 
 * Copyright (C) 2010 Nathan Collins <nathan.collins@gmail.com>,
 *                    Andrew Sackville-West <andrew@swclan.homelinux.org>
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

#include "teensy_mc.h"
#include "teensy.h"

/* TODO
 *
 */

MODULE_LICENSE("GPL");

#define DEVICE_NAME "mc"
#define MC_NUM_DEVS 2

static struct mc_dev_t {
        struct cdev cdev;
} mc_devs[MC_NUM_DEVS];

/* to put in filp->private_data */
/* make it a struct so i can add more fields later if needed */
struct mc_filp_data {
        struct mc_dev_t * mc;
};

static dev_t mc_dev_number;
struct class * mc_class;

/*** params ***/

/*** helpers ***/

#define pk(fmt,args...) printk(KERN_DEBUG "mc: process %i (%s): " fmt,  \
                               current->pid, current->comm, ##args)
#define ui (unsigned int)

static struct mc_filp_data * _get_private_data (struct file * filp) __attribute__((unused)); /* prevent "usused" compile warning */
static struct mc_filp_data * _get_private_data (struct file * filp) {
        return (struct mc_filp_data *)filp->private_data;
}

/*** API ***/

int mc_open (struct inode *inode, struct file *filp) {
        struct mc_filp_data * data;
        struct mc_dev_t * dev;

        pk("open(): iminor=%d, filp=%p\n", iminor(inode), filp);
  
        /* filp->private_data */
        dev = &mc_devs[iminor(inode)];
        data = kmalloc(sizeof(struct mc_filp_data), GFP_KERNEL);
        if (!data)
                return -ENOMEM;
        data->mc = dev;
        filp->private_data = data;

        return 0;
}

int mc_release (struct inode * inode, struct file * filp) {
        //struct mc_dev_t * dev = _get_private_data(filp)->mc;

        pk("release(): iminor=%d, filp=%p\n", iminor(inode), filp);

        kfree(filp->private_data);
        return 0;
}

/* MAYBE TODO: would be nicer to use /sys instead of ioctls? */
int mc_ioctl (struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg) {
        uint8_t speed;
        char direction;
        /* send msg */
        /* buf format is
         *
         * [device]    		: 1 byte 
         * [minor device]  	: 1 byte 
         * [speed]     		: 1 byte
         * [direction] 		: 1 byte
         */
        int ret = 0;
        struct teensy_request req = {
                /* TODO: use correct adc code */
                .buf   = kmalloc(1+1+1+1, GFP_KERNEL),
                .size  = 1+1+1+1,
        };

        pk("mc_ioctl(): iminor=%d, filp=%p, cmd=0x%X, arg=0x%X\n",
           iminor(inode), filp, ui cmd, ui arg);

        

        if (req.buf == NULL) {
                pk("mc_ioctl(): no mem when allocing zero bytes\n");
                return -ENOMEM;
        }

        /* compute msg params */
        speed = (uint8_t) (int) arg; /* NC: paranoid intermediate cast ... */
        switch (cmd) {

        case MC_IOC_STOP:
                direction = 's'; /* arbitrary */
                speed = 0;
                break;

        case MC_IOC_FWD:
                direction = 'f';
                break;

        case MC_IOC_REV:
                direction = 'r';
                /* set speed */
                break;

        default:
                return -ENOTTY; /* this is the right error code according to ldd3 :P */
        }

        /* pack msg */
        req.buf[0] = 'm';
        req.buf[1] = (uint8_t)iminor(inode);
        req.buf[2] = speed;
        req.buf[3] = direction;

        /* pass request to teensy_send() */
        /* teensy_send() returns a DIFFERENT buf in req->buf, so we must
           free that; our mybuf was already free'd in teensy_send().
        */
        ret = teensy_send(&req);
        if (ret < 0) {
                pk("mc_ioctl(): error calling teensy_send()\n");
                return ret;
        }

        /* force string */
        if (req.size > 0)
                req.buf[req.size-1] = '\0';
        printk(KERN_DEBUG "mc_ioctl(): read %zu bytes [%s] from teensy\n",
               req.size, req.buf);

        kfree(req.buf); /* free the NEW buf */
        return 0;
}

struct file_operations mc_fops = {
        .owner   = THIS_MODULE,
        .open    = mc_open,
        .release = mc_release,
        .ioctl   = mc_ioctl,
};

/*** setup/teardown ***/

int mc_init(void)
{
        struct mc_dev_t * dev;
        int result, i;

        pk ("init():\n");

        /*
         * Register major, and accept a dynamic number
         */
        result = alloc_chrdev_region(&mc_dev_number, 0, MC_NUM_DEVS, DEVICE_NAME);
        if (result < 0)
                return result;

        /* sysfs */
        mc_class = class_create(THIS_MODULE, DEVICE_NAME);

        /* MAYBE FIXME: memory leak on failure? (if module load is unlike userland, where memory is freed on exit ...) */
        for (i = 0; i < MC_NUM_DEVS; ++i) {
                dev = &mc_devs[i];

                /* cdev */ /* mostly copying ELDD cmos from here on ... */
                cdev_init(&dev->cdev, &mc_fops);
                dev->cdev.owner = THIS_MODULE;
                result = cdev_add(&dev->cdev, MKDEV(MAJOR(mc_dev_number), i), 1);
                if (result < 0)
                        return result;

                /* udev /dev node creation */ /* returns pointer to /sys entry as well */
                /* http://www.gnugeneration.com/books/linux/2.6.20/kernel-api/re694.html */
                device_create(mc_class, NULL, MKDEV(MAJOR(mc_dev_number), i), DEVICE_NAME "%d", i);
        }
        return 0;
}

void mc_exit(void)
{
        int i;
        struct mc_dev_t * dev;

        for (i = 0; i < MC_NUM_DEVS; ++i) {
                dev = &mc_devs[i];

                /* sysfs and udev */
                device_destroy(mc_class, MKDEV(MAJOR(mc_dev_number), i));
                /* cdev */
                cdev_del(&dev->cdev);
        }

        unregister_chrdev_region(mc_dev_number, MC_NUM_DEVS);
        class_destroy(mc_class);

        pk("cleanup(): module cleaned up successfully\n");
}

/* thoght these were ignored by the teeny_mono setup in Makefile ... */
//module_init(mc_init);
//module_exit(mc_exit);
