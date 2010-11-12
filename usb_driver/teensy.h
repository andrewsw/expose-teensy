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

/* function protoypes */
static int probe_teensy (struct usb_interface *intf,
			 const struct usb_device_id *id);
static void disconnect_teensy(struct usb_interface *intf);

/*
 * device structs
 * 
 */
static struct usb_device_id teensy_table [] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ } /* terminating entry */
};

MODULE_DEVICE_TABLE(usb, teensy_table);

static struct usb_driver teensy_driver = {
	.name =         "teensy",
	.probe =        probe_teensy,
	.disconnect =   disconnect_teensy,
	.id_table =     teensy_table
};

struct usb_teensy {
	struct usb_device *udev;          /* the usb device for this device */
	struct usb_interface *interface;  /* the interface for this device */
	unsigned char *in_buf;            /* the buffer for receiving data */
	size_t in_size;                   /* the size of the buffer */
	__u8 in_endpoint;                 /* the device endpoint for incoming packets */
	__u8 out_endpoint;                /* the device endpoint for outgoing packets */
	struct urb *in_urb;               /* our input urb */
	int in_interval;                  /* the polling interval of the input endpoint */
	
	
};

struct read_request {

	struct list_head list; /* we're a linked list */
	char t_dev;            /* teensy device */
	char *buf;             /* buffer to store the read data in */
	size_t size;           /* the size of the request */
	bool complete;         /* the status of the request */
	
};


/*
 * module-wide data structures
 *
 */
static LIST_HEAD(readers_list);

#endif /* TEENSY_H */
