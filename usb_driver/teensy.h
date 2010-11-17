#ifndef TEENSY_H
#define TEENSY_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/usb.h>
#include <linux/device.h>

#define TEENSY_DEBUG 

#define VENDOR_ID        0x16C0
#define PRODUCT_ID        0x0FFF

/* a debug printk */
#ifdef TEENSY_DEBUG
#define DPRINT(msg...)  printk(KERN_DEBUG "teensy: " msg)
#else
#define DPRINT(msg...)
#endif

MODULE_AUTHOR("Andrew Sackville-West"); 
MODULE_LICENSE("GPL");

/*
 * module parameters
 * 
 */

/*
 * device structs
 * 
 */

struct usb_teensy {
	struct usb_device *udev;          /* the usb device for this device */
	struct usb_interface *interface;  /* the interface for this device */
	unsigned char *in_buf;            /* the buffer for receiving data */
	size_t in_size;                   /* the size of the buffer */
	__u8 in_endpoint;                 /* the device endpoint for incoming packets */
	__u8 out_endpoint;                /* the device endpoint for outgoing packets */
	struct urb *in_urb;               /* our input urb */
	int in_interval;                  /* the polling interval of the input endpoint */
	int out_interval;                 /* the polling interval of the output endpoint */
};

/* 
 * Andrew says:
 *   I've pushed a branch called "readers" that has the code I'm working on
 *   for the read operation. Look in teensy.h for a struct read_request
 *   object. It is not actually used yet, but contains what I think I need
 *   to make it work. the client code should populate the t_dev field with
 *   a device number (to be determined still), the *buf field with the
 *   address of a buffer to be filled and the size paramater with the size
 *   of the buffer (or of the data desired, less than buffer size,
 *   obviously). Ignore the other fields, they're used internally.
 */
struct read_request {

	struct list_head list; /* we're a linked list */
	char t_dev;            /* teensy device */
	char *buf;             /* buffer to store the read data in */
	size_t size;           /* the size of the request */
	bool complete;         /* the status of the request */
	
};
int teensy_read(struct read_request *);

#endif /* TEENSY_H */
