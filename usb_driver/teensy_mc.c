/* teensy_mc.c
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

#include "teensy_mc.h"
#include "teensy.h"

/* TODO
 *
 */

MODULE_LICENSE("Dual BSD/GPL");

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

#define pk(fmt,args...) printk(KERN_DEBUG "mc: process %i (%s): " fmt, \
                               current->pid, current->comm, ##args)
#define ui (unsigned int)

static struct mc_filp_data * _get_private_data (struct file * filp) {
  return (struct mc_filp_data *)filp->private_data;
}

/*** API ***/

int mc_open (struct inode *inode, struct file *filp) {
  struct mc_filp_data * data;
  struct mc_dev_t * dev;

  pk("open(): iminor=%d, filp=0x%X\n", iminor(inode), ui filp);
  
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

  pk("release(): iminor=%d, filp=0x%X\n", iminor(inode), ui filp);

  kfree(filp->private_data);
  return 0;
}


/* @buf:
 * @count:
 * @return:
 */
ssize_t mc_read (struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
  //struct mc_dev_t * dev = _get_private_data(filp)->mc;
  int ret = 0;
  char * mybuf = kmalloc(count, GFP_KERNEL);
  struct read_request req = {
    .t_dev = 'a',
    .buf   = mybuf,
    .size  = count,
  };

  pk("read(): buf=0x%X, count=%d, *pos=0x%X\n", ui buf, count, ui *pos);

  if (mybuf == NULL)
    return -ENOMEM;

  /* pass request to teensy_read() */
  ret = teensy_read(&req);
  if (ret < 0) {
    pk("read(): error calling teensy_read()\n");
    goto mc_read_cleanup;
  }
  if (ret > count) {
    pk("read(): teeny_read() returned to large\n");
    goto mc_read_cleanup;
  }
  /* copy data to user buf */
  if (copy_to_user(buf, mybuf, ret)) {
    ret = -EFAULT;
    goto mc_read_cleanup;
  }

 mc_read_cleanup:
  kfree(mybuf);
  return ret; /* TODO: is it correct to return ret on success? */
}

/* MAYBE TODO: would be nicer to use /sys instead of ioctls? */
int mc_ioctl (struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg) {
  int speed;
  //struct mc_dev_t * dev = _get_private_data(filp)->mc;

  pk("ioctl(): iminor=%d, filp=0x%X, cmd=0x%X, arg=0x%X\n", iminor(inode), ui filp, ui cmd, ui arg);

  speed = (int) arg;
  switch (cmd) {

  case MC_IOC_STOP:
    speed = 0;
    /* fall through */

  case MC_IOC_FWD:
    /* set speed */
    break;

  case MC_IOC_REV:
    /* set speed */
    break;

  default:
    return -ENOTTY; /* this is the right error code according to ldd3 :P */
  }
  return 0;
}


struct file_operations mc_fops = {
  .owner   = THIS_MODULE,
  .read    = mc_read,
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
