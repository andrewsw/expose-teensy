/*
 * teensy.c
 * 
 */
#ifndef TEENSY_H
#include "teensy.h"
#endif



/*
 * teensy_interrupt_in_callback
 *
 * callback function to handle data coming on from teensy
 *
 * note, this runs in interrupt context, play nice!
 *
 */
static void teensy_interrupt_in_callback (struct urb *urb) 
{
	struct usb_teensy *dev = urb->context;
	int status = urb->status;
	int i;
	char data[65];
	

	DPRINT("interrupt_in callback called\n");

	if (status != 0){
		if ((status != -ENOENT) && (status != -ECONNRESET) &&
		    (status != -ESHUTDOWN)) {
			printk(KERN_ERR "teensy: input callback nonzero status received: %d\n", status);
			
		}
	}

	if (urb->actual_length > 0) {
		DPRINT("actual_length: %d\n", urb->actual_length);
		
		for (i=0; i < urb->actual_length;i++)
			data[i]=dev->in_buf[i];
		
		data[64]=0x00;
		DPRINT("data: %s\n", data);		
			
	}

	usb_submit_urb(urb, GFP_ATOMIC);

	DPRINT ("in URB RE-submitted\n");
	
}


/*
 * init_reader
 *
 * this function sets up the reader URB and submits it
 * this enables interrupt driven reading of any packets from teensy
 *
 * struct usb_interface *intf  -- the interface to read from
 *
 */
void init_reader (struct usb_interface *intf) 
{

	struct usb_teensy *dev = usb_get_intfdata(intf);

	dev->in_urb = usb_alloc_urb(0, GFP_KERNEL);

	usb_fill_int_urb (dev->in_urb,
			  dev->udev,
			  usb_rcvintpipe(dev->udev,
					 dev->in_endpoint),
			  dev->in_buf,
			  dev->in_size,
			  teensy_interrupt_in_callback, dev,
			  dev->in_interval);

	usb_submit_urb(dev->in_urb, GFP_KERNEL);

	DPRINT("in URB submitted\n");
	
	
}

	


static int probe_teensy (struct usb_interface *intf,
			 const struct usb_device_id *id) 
{
	struct usb_teensy *dev = NULL;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;
		
	DPRINT("connect detected\n");

	/* allocate for our device structure */
	dev = kmalloc(sizeof(struct usb_teensy), GFP_KERNEL);
	if(!dev) {
		printk(KERN_ERR "teensy: failed to allocate device memory!\n");
		return -ENOMEM;
	}

	memset(dev, 0x00, sizeof(*dev));

	/* connect the device and interface to our dev structure */
	dev->udev = usb_get_dev(interface_to_usbdev(intf));
	dev->interface = intf;

	/* traverse to find our endpoints */
	iface_desc = intf->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {

		DPRINT ("-- probing endpoint %d\n", i);
		

		/* store the current endpoint */
		endpoint = &iface_desc->endpoint[i].desc;

		/* check for an input endpoint, if not already found */
		if (!dev->in_endpoint &&
		    (endpoint->bEndpointAddress & USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
		     == USB_ENDPOINT_XFER_INT)) {

			DPRINT ("-- detected IN endpoint\n");
			
			/* this is an input of interrupt type */
			/* connect it up... */ 
			dev->in_endpoint = endpoint->bEndpointAddress;
			dev->in_size = endpoint->wMaxPacketSize;
			dev->in_interval = endpoint->bInterval;
			dev->in_buf = kmalloc(dev->in_size, GFP_KERNEL);

			DPRINT("--IN endpoint: %d, size: %d, interval: %d\n",
			       dev->in_endpoint,
			       dev->in_size,
			       dev->in_interval);
			

			if (!dev->in_buf) {
				kfree(dev);
				printk(KERN_ERR "teensy: failed to allocate buffer!\n");
				return -ENOMEM;
			}
					
		}

		

		/* check for an out endpoint, if not already found */
		if (!dev->out_endpoint &&
		    !(endpoint->bEndpointAddress & USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
		     == USB_ENDPOINT_XFER_INT)) {

			DPRINT("-- detected OUT endpoint\n");
			
			/* this is an output endpoint */
			dev->out_endpoint = endpoint->bEndpointAddress;

		}
		
	}

	if (!(dev->in_endpoint && dev->out_endpoint)) {
		printk(KERN_ERR "teensy: Failed to find both in and out endpoints\n");
		if (!dev->in_buf) {
			kfree(dev->in_buf);
		}
		if(!dev) {
			kfree(dev);
		}
		return -ENOMEM;
	}

	DPRINT ("successful probe.\n");
	
	/* save the data pointer in the interface */
	usb_set_intfdata (intf, dev);

	/* TODO! Here we should call code to register devices in /dev
	 *
	 * examples would be /dev/adc0 for an analog-to-digital device
	 *
	 * Also we should set up a URB for recieving data from the
	 * device.  That URB should call a completion function (which
	 * runs in interrupt mode) to handle data coming back from the
	 * device.  That completion function should also resubmit the
	 * URB to allow another packet to come in. Finally, (but not
	 * last...) the completion function should take the buffer
	 * that comes in (dev->in_buf) and peek at the first few bytes
	 * to figure out what it is and copy it to a ring-buffer for
	 * the appropriate device and perhaps wakeup the appropriate
	 * sleeping handler to handle that data
	 * 
	 */

	init_reader(intf);
	

	return 0;
  
}

static void disconnect_teensy(struct usb_interface *intf) 
{
	struct usb_teensy *dev;
	
	DPRINT("disconnect detected\n");

	dev = (struct usb_teensy *)usb_get_intfdata(intf);
		
	if(dev) {

		/* kill our URB synchronously... kill it DEAD */
		if(dev->in_urb) {
			usb_kill_urb(dev->in_urb);
			usb_free_urb(dev->in_urb);
		}
		
		if(dev->in_buf) {
			kfree(dev->in_buf);
		}

		kfree(dev);

	}
	DPRINT("completed disconnect\n");
	
}


/*
 *
 * teensy_init
 *
 * the init function... set up this puppy
 *
 */
int __init
teensy_init(void)
{
	int result;
	
	DPRINT("initializing...\n");

	result = usb_register (&teensy_driver);

	if (result) {
		printk(KERN_ERR "teensy: failed to register usb device! error code: %d", result);
	} else {		
		DPRINT("initialized.\n");
	}
		
	return result;
}



/*
 * teensy_exit
 *
 * tear it all down
 *
 */
void __exit
teensy_exit(void)
{
	DPRINT("teensy: Removing teensy\n");

	usb_deregister(&teensy_driver);
	
	DPRINT("removal complete.\n");
	
}

module_init(teensy_init);
module_exit(teensy_exit);
