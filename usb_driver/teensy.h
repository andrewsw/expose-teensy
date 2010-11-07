#ifndef TEENSY_H
#define TEENSY_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/usb.h>
#include <linux/device.h>

#define TEENSY_DEBUG 

#define VENDOR_ID        0x16C0
#define PRODUCT_ID        0x0480

/* a debug printk */
#ifdef TEENSY_DEBUG
#define DPRINT(msg)  printk(KERN_DEBUG "teensy: " msg)
#else
#define DPRINT(msg)
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

#endif /* TEENSY_H */
