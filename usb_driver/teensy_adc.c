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

/* TODO
 *
 */

MODULE_LICENSE("Dual BSD/GPL");

#define DEVICE_NAME "adc"
#define ADC_NUM_DEVS 2

static struct adc_dev_t {
  struct adc_buf ** store; /* user data */
  struct mutex lock;
  wait_queue_head_t wq;
  int open_count;             /* number of open()s currently active on dev */
  int free_on_last_close;     /* bool controlling blob freeing */
  struct cdev cdev;
} adc_devs[ADC_NUM_DEVS];

/* to put in filp->private_data */
/* make it a struct so i can add more fields later if needed */
struct adc_filp_data {
  struct adc_dev_t * adc;
};

static dev_t adc_dev_number;
struct class * adc_class;
static struct proc_dir_entry * proc_dev_dir, * proc_stats, * proc_data;

/*** params ***/

/* wldd section 3.1 page 27 */
/* to init: `insmod adc blob_size=123 blob_count=456` */
static int blob_count = 64;
static int blob_size = 1 << 20; /* 1MB */
module_param (blob_count, int, S_IRUGO);
module_param (blob_size, int, S_IRUGO);

/*** helpers ***/

#define pk(fmt,args...) printk(KERN_DEBUG "adc: process %i (%s): " fmt, \
                               current->pid, current->comm, ##args)
#define ui (unsigned int)

static struct adc_filp_data * _get_private_data (struct file * filp) {
  return (struct adc_filp_data *)filp->private_data;
}

/* check validity of count and userbuf args to read()/write() */
static int _check_args(int count, const char * userbuf, struct adc_buf * mybuf) {
  if (count != sizeof(struct adc_buf)) {
    pk("write(): invalid count=%d, should be sizeof(struct adc_buf)=%d\n", 
       count, sizeof(struct adc_buf));
    return -EINVAL;
  }
  if (copy_from_user(mybuf, userbuf, sizeof(struct adc_buf))) {
    pk("write(): failed to copy adc_buf from user buf\n");
    return -EINVAL;
  }
  if (mybuf->size < 0 || blob_size < mybuf->size) {
    pk("write(): invalid size=%d (blob_size=%d)\n", mybuf->size, blob_size);
    return -EINVAL;
  }
  if (mybuf->index < 0 || blob_count < mybuf->index) {
    pk("write(): invalid index=%d (blob_count=%d)\n", mybuf->index, blob_count);
    return -EINVAL;
  }
  return 0;
}

/* free all (non-NULL) blobs in dev->store 
 *
 * NB: calls must be protected by dev->lock
 */
static void _free_blobs(struct adc_dev_t * dev) {
  int j;
  struct adc_buf ** store = dev->store;

  pk("_free_blobs(): freeing store\n");

  for (j = 0; j < blob_count; ++j)
    if (store[j] != NULL) {
      if (store[j]->data != NULL)
        kfree(store[j]->data);
      kfree(store[j]);
      store[j] = NULL;
    }
}

/*** proc ***/
/* in seq functions we interpret position as
 * store = *pos / blob_count;
 * index = *pos % blob_count; // which blob in store
 */

static void *proc_data_seq_start(struct seq_file *s, loff_t *pos) {
  if ((((int) *pos) / blob_count) >= ADC_NUM_DEVS)
    return NULL;
  /* print header */
  if (*pos == 0) {
    seq_printf(s, "STORE INDEX SIZE: DATA\n");
    seq_printf(s, "======================\n");
  }
  return pos;
}

static void *proc_data_seq_next(struct seq_file *s, void *v, loff_t *pos) {
  ++(*pos);
  if ((((int) *pos) / blob_count) >= ADC_NUM_DEVS)
    return NULL;
  return pos;
}

/* NB: if you don't put a stop function in the fops you get OOPS! */
static void proc_data_seq_stop(struct seq_file *s, void *v) {}

/* prints blob stats if the blob is non-NULL */
static int proc_data_seq_show(struct seq_file *s, void *v) {
  int pos = *((int *) v), store = pos / blob_count, index = pos % blob_count;
  struct adc_dev_t dev = adc_devs[store];
  struct adc_buf * blob = dev.store[index];
  int i;

  if (mutex_lock_interruptible(&dev.lock))
    return -ERESTARTSYS;
  if (blob != NULL) {
    seq_printf(s, "%i %i %i: ", store, index, blob->size);
    for (i = 0; i < blob->size; ++i)
      seq_printf(s, "%.2X", blob->data[i]);
    seq_putc(s, '\n');
  }
  mutex_unlock(&dev.lock);
  return 0;
}

static struct seq_operations proc_data_seq_ops = {
  .start = proc_data_seq_start,
  .next = proc_data_seq_next,
  .stop = proc_data_seq_stop,
  .show = proc_data_seq_show
};

static int proc_data_seq_open(struct inode *inode, struct file *file) {   
  return seq_open (file, &proc_data_seq_ops);
}

static const struct file_operations proc_data_ops = {
  .open = proc_data_seq_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = seq_release
};

/** end proc_data_, begin proc_stats_ **/

/* all printing happens here */
static void *proc_stats_seq_start(struct seq_file *s, loff_t *pos) {
  struct adc_dev_t dev;
  int i, j, count, sum;

  pk("proc_stats_seq_start(): \n");

  /* HACK: if seq_start always returns NULL then nothing is printed
   * ??? so, return non-NULL the first time */
  if (*pos > 0) return NULL;

  /* acquire all locks */
  for (i = 0; i < ADC_NUM_DEVS; ++i)
    if (mutex_lock_interruptible(&adc_devs[i].lock)) {
      for (j = 0; j < i; ++j)
        mutex_unlock(&adc_devs[j].lock);
      pk("proc_stats_seq_start(): interrupted, but can't return -ERESTARTSYS here!\n");
      return NULL;
    }
  /* summarize each store */
  seq_printf(s, "STORE #OPENS #BLOBS #BYTES\n");
  seq_printf(s, "==========================\n");
  for (i = 0; i < ADC_NUM_DEVS; ++i) {
    dev = adc_devs[i];
    count = sum = 0;
    for (j = 0; j < blob_count; ++j)
      if (dev.store[j] != NULL) {
        ++count;
        sum += dev.store[j]->size;
      }
    seq_printf(s, "%i\t%i\t%i\t%i\n", i, dev.open_count, count, sum);
  }
  /* unlock */
  for (i = 0; i < ADC_NUM_DEVS; ++i)
    mutex_unlock(&adc_devs[i].lock);
  return pos;
}

static void *proc_stats_seq_next(struct seq_file *s, void *v, loff_t *pos) {
  return NULL;
}

/* NB: if you don't put a stop function in the fops you get OOPS! */
static void proc_stats_seq_stop(struct seq_file *s, void *v) {}

/* prints blob stats if the blob is non-NULL */
static int proc_stats_seq_show(struct seq_file *s, void *v) {
  return 0;
}

static struct seq_operations proc_stats_seq_ops = {
  .start = proc_stats_seq_start,
  .next = proc_stats_seq_next,
  .stop = proc_stats_seq_stop,
  .show = proc_stats_seq_show
};

static int proc_stats_seq_open(struct inode *inode, struct file *file) {   
  return seq_open (file, &proc_stats_seq_ops);
}

static const struct file_operations proc_stats_ops = {
  .open = proc_stats_seq_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = seq_release
};

/*** API ***/

int adc_open (struct inode *inode, struct file *filp) {
  struct adc_filp_data * data;
  struct adc_dev_t * dev;

  pk("open(): iminor=%d, filp=0x%X\n", iminor(inode), ui filp);
  
  /* only allow root to open device */
  if (!capable(CAP_SYS_ADMIN)) {
    pk("open(): non-CAP_SYS_ADMIN user attempted to open()\n");
    return -EACCES; /* or is -EPERM better ? */
  }

  /* filp->private_data */
  dev = &adc_devs[iminor(inode)];
  data = kmalloc(sizeof(struct adc_filp_data), GFP_KERNEL);
  if (!data)
    return -ENOMEM;
  data->adc = dev;
  filp->private_data = data;

  /* ref count */
  mutex_lock_interruptible(&dev->lock);
  ++dev->open_count;
  mutex_unlock(&dev->lock);

  return 0;
}

int adc_release (struct inode * inode, struct file * filp) {
  struct adc_dev_t * dev = _get_private_data(filp)->adc;

  pk("release(): iminor=%d, filp=0x%X\n", iminor(inode), ui filp);

  /* ref count and maybe free */
  mutex_lock_interruptible(&dev->lock);
  if (--dev->open_count == 0 && dev->free_on_last_close)
    _free_blobs(dev);
  mutex_unlock(&dev->lock);

  kfree(filp->private_data);
  return 0;
}

int adc_ioctl (struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg) {
  int i;
  struct adc_dev_t * dev;

  pk("ioctl(): iminor=%d, filp=0x%X, cmd=0x%X, arg=0x%X\n", iminor(inode), ui filp, ui cmd, ui arg);

  dev = _get_private_data(filp)->adc;
  switch (cmd) {

  case ADC_IOC_DEL:
    i = (int) arg;
    if (i < 0 || blob_count < i) {
      pk("ioctl(): invalid blob index in DEL ioctl: arg=%d, blob_count=%d\n", i, blob_count);
      return -EINVAL;
    }
    if (mutex_lock_interruptible(&dev->lock))
      return -ERESTARTSYS;
    if (dev->store[i]) {
      pk("ioctl(): freeing non-NULL blob at index=%d\n", i);
      kfree(dev->store[i]->data);
      kfree(dev->store[i]);
      dev->store[i] = NULL;
    } else {
      pk("ioctl(): NOOP: freeing NULL blob at index=%d\n", i);
    }
    mutex_unlock(&dev->lock);
    break;

  default:
    return -ENOTTY; /* this is the right error code according to ldd3 :P */
  }
  return 0;
}

/* @buf: pointer to struct adc_buf.
 * @count: == sizeof(struct adc_buf), a sanity check.
 * @return: count on success.
 *
 * the actual data to return is copied to buf->data if buf->index and
 * buf->size are valid, and buf->size is updated to size of the
 * returned buf->data.
 *
 * buf->index in [0,blob_count) is valid.
 *
 * buf->size in [(adc_buf at buf->index).size, blob_size] is valid.
 * 
 * the suggested way to call read(), when the caller is unsure of the
 * size of the blob they're requesting, is to pass a buf with
 * buf->size = blob_size, and then realloc the returned buf->data to
 * the actual size (now in buf->size) if memory is a big concern.
 */
ssize_t adc_read (struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
  struct adc_buf mybuf;
  struct adc_dev_t * dev = _get_private_data(filp)->adc;
  int ret;

  pk("read(): buf=0x%X, count=%d, *pos=0x%X\n", ui buf, count, ui *pos);

  /* check user args */
  ret = _check_args(count, buf, &mybuf);
  if (ret < 0)
    return ret;
  if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;
  /* sleep if no data available */
  while (dev->store[mybuf.index] == NULL) {
    pk("read(): requested index=%i is empty, sleeping ...\n", mybuf.index);
    mutex_unlock(&dev->lock);
    if (wait_event_interruptible(dev->wq, dev->store[mybuf.index] != NULL) < 0) {
      pk("read(): ... interrupted while sleeping\n");
      return -ERESTARTSYS;
    }
    pk("read(): ... woke up\n");
    mutex_lock(&dev->lock);
  }

  /* copy data to user buf */
  if (copy_to_user(mybuf.data, dev->store[mybuf.index]->data, dev->store[mybuf.index]->size)) {
    pk("read(): failed to copy blob to user\n");
    mutex_unlock(&dev->lock);
    return -EFAULT;
  }
  if (put_user(dev->store[mybuf.index]->size, &((struct adc_buf *) buf)->size)) {
    pk("read(): failed to update size in user buf\n");
    mutex_unlock(&dev->lock);
    return -EFAULT;
  }
  mutex_unlock(&dev->lock);
  return count;
}

/* @buf: pointer to struct adc_buf.
 * @count: == sizeof(struct adc_buf), a sanity check.
 * @return: count on success.
 *
 * the actual data to store is copied from buf->data if buf->index and
 * buf->size are valid.
 *
 * buf->size in [0,blob_size] is valid.
 *
 * buf->index in [0,blob_count) is valid.
 */
ssize_t adc_write (struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
  struct adc_buf mybuf;
  char * data;
  struct adc_dev_t * dev = _get_private_data(filp)->adc;
  int ret;

  pk("write(): buf=0x%X, count=%d, *pos=0x%X\n", ui buf, count, ui *pos);

  ret = _check_args(count, buf, &mybuf);
  if (ret < 0)
    return ret;

  /* copy data from user buf */
  if (!(data = kmalloc(mybuf.size, GFP_KERNEL))) {
    pk("write(): failed to kmalloc() for blob\n");
    return -ENOMEM;
  }
  if (copy_from_user(data, mybuf.data, mybuf.size)) {
    pk("write(): failed to copy blob from user\n");
    kfree(data);
    return -EFAULT;
  }

  /* store new data, freeing old if any */
  if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;
  if (dev->store[mybuf.index] != NULL)
    kfree(dev->store[mybuf.index]->data);
  else if (!(dev->store[mybuf.index] = kmalloc(sizeof(struct adc_buf), GFP_KERNEL))) {
    pk("write(): failed to kmalloc for adc_buf\n");
    kfree(data);
    mutex_unlock(&dev->lock);
    return -ENOMEM;
  }
  mybuf.data = data;
  * (dev->store[mybuf.index]) = mybuf;
  mutex_unlock(&dev->lock);
  wake_up_interruptible(&dev->wq);
  return count;
}

struct file_operations adc_fops = {
  .owner   = THIS_MODULE,
  .read    = adc_read,
  .write   = adc_write,
  .open    = adc_open,
  .release = adc_release,
  .ioctl   = adc_ioctl,
};

/*** setup/teardown ***/

int adc_init(void)
{
  struct adc_dev_t * dev;
  int result, i;

  pk ("init(): blob_size=%d, blob_count=%d\n", blob_size, blob_count);

  /*
   * Register your major, and accept a dynamic number
   */
  result = alloc_chrdev_region(&adc_dev_number, 0, ADC_NUM_DEVS, DEVICE_NAME);
  if (result < 0)
    return result;

  /* proc */ /* readable by root only */
  proc_dev_dir = proc_mkdir(DEVICE_NAME, NULL);
  proc_stats = create_proc_entry("stats", 0400, proc_dev_dir);
  proc_stats->proc_fops = &proc_stats_ops;
  proc_data = create_proc_entry("data", 0400, proc_dev_dir);
  proc_data->proc_fops = &proc_data_ops;

  /* sysfs */
  adc_class = class_create(THIS_MODULE, DEVICE_NAME);

  /* MAYBE FIXME: memory leak on failure? (if module load is unlike userland, where memory is freed on exit ...) */
  for (i = 0; i < ADC_NUM_DEVS; ++i) {
    dev = &adc_devs[i];
    /* store */ 
    if (!(dev->store = kcalloc(blob_count, sizeof(struct adc_buf *), GFP_KERNEL)))
      return -ENOMEM;

    dev->open_count = 0;
    dev->free_on_last_close = ADC_FREE_ON_LAST_CLOSE;
    mutex_init(&dev->lock);
    init_waitqueue_head(&dev->wq);

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
    /* store */ /* this is pointless if memory is automatically freed on unload ... */
    mutex_lock_interruptible(&dev->lock);
    _free_blobs(dev);
    mutex_unlock(&dev->lock);
    kfree(dev->store);

    /* proc */
    remove_proc_entry("data", proc_dev_dir);
    remove_proc_entry("stats", proc_dev_dir);
    remove_proc_entry(DEVICE_NAME, NULL);
    /* sysfs and udev */
    device_destroy(adc_class, MKDEV(MAJOR(adc_dev_number), i));
    /* cdev */
    cdev_del(&dev->cdev);
  }

  unregister_chrdev_region(adc_dev_number, ADC_NUM_DEVS);
  class_destroy(adc_class);

  pk("cleanup(): module cleaned up successfully\n");
}

//module_init(adc_init);
//module_exit(adc_exit);
