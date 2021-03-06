/*
 * teensy.h
 *
 *  This is a linux module to expose portions of the linux hardware to
 *  userspace as character devices.
 *
 *  Copyright (C) 2010  Andrew Sackville-West <Andrew@swclan.homelinux.org>
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

#ifndef TEENSY_H
#define TEENSY_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/usb.h>
#include <linux/device.h>

#define TEENSY_DEBUG 

#define VENDOR_ID        0x16C0
#define PRODUCT_ID        0x0FFF

/* MUST BE THE SAME AS IN ../lighty_usb_teensy/usb_rawhid.c */
#define RAWHID_RX_SIZE 64 /* usb buffer packet size */

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
 *   for the read operation. Look in teensy.h for a struct teensy_request
 *   object. It is not actually used yet, but contains what I think I need
 *   to make it work. the client code should populate the packet_id field with
 *   a device number (to be determined still), the *buf field with the
 *   address of a buffer to be filled and the size paramater with the size
 *   of the buffer (or of the data desired, less than buffer size,
 *   obviously). Ignore the other fields, they're used internally.
 */
struct teensy_request {

        struct list_head list; /* we're a linked list */
        char packet_id;        /* packet id for this request */
        char *buf;             /* buffer to store the read data in */
        size_t size;           /* the size of the request */
        bool complete;         /* the status of the request */
        
};
int teensy_send(struct teensy_request *);

#endif /* TEENSY_H */
