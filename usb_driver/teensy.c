/*
 * teensy.c
 * 
 */
#include "teensy.h"
#include "submodules.h"

/*** usb structs ***/

/* NC: moved these defs and decls here, from teensy.h, to so that i
 * could #include teensy.h in teensy_adc.c ... they are static and so

 * are, presumably, not intended for inclusion in other files (and
 * they caused warnings and errors)
 */

static struct usb_device_id teensy_table [] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ } /* terminating entry */
};

MODULE_DEVICE_TABLE(usb, teensy_table);

static int probe_teensy (struct usb_interface *intf,
			 const struct usb_device_id *id);
static void disconnect_teensy(struct usb_interface *intf);
static struct usb_driver teensy_driver = {
	.name =         "teensy",
	.probe =        probe_teensy,
	.disconnect =   disconnect_teensy,
	.id_table =     teensy_table
};

/*
 * module-wide data structures
 *
 */

/* list, lock and wait_queue for sleeping readers */
static LIST_HEAD(readers_list);
spinlock_t readers_lock;

wait_queue_head_t readers_queue;

struct usb_teensy *teensy_dev;

/* data pack/unpack */

/* frees req->buf and allocates new req->buf with packed data
 *
 * NOT INTERRUPT MODE SAFE!
 *
 * NOT SAFE TO FREE req->buf IF THIS RETURNS ERROR
 *
 * @return: < 0 on failure; 0 o/w
 */
int pack(struct read_request * req) {
    char * packed;
    /* packed data layout:
     *
     * [destination]: 1 byte
     * [size]:        1 byte // BREAKS if RAWHID_RX_SIZE gets large
     * [payload]:     N bytes
     * [padding]:     RAWHID_RX_SIZE - 1 - 1 - N bytes
     */

    /* validate input */
    if (req->size + 1+1 > RAWHID_RX_SIZE
        || req->size >= 1 << 8) { /* too big for uint_8 */
      printk(KERN_ERR "pack(): req->size too large: %i\n", req->size);
      return -EINVAL;
    }

    packed = kzalloc(RAWHID_RX_SIZE,GFP_KERNEL);
    if (!packed)
      return -ENOMEM;

    /* pack data */
    packed[0] = req->t_dev;
    packed[1] = (uint8_t) req->size;
    memcpy(packed+1+1,req->buf,req->size);
    /* zalloc already made pad bytes zero */

    /* free old buf; insert new buf and updated size */
    kfree(req->buf);
    req->buf = packed;
    req->size = RAWHID_RX_SIZE;

    return 0;
}

/* frees req->buf and allocates new req->buf with unpacked data
 *
 * INTERRUPT MODE SAFE
 *
 * @return: < 0 on failure; 0 o/w
 */
int unpack(struct read_request * req) {
    char * unpacked;
    uint8_t size;

    /* received format assumed same as setup in pack() */

    /* validate input */
    /* ASSUME RAWHID_RX_SIZE == RAWHID_TX_SIZE (true by default) */
    if (req->size > RAWHID_RX_SIZE) {
      printk(KERN_WARNING "unpack(): req->size too large: %i\n", req->size);
      /* return -EINVAL; */ // not an actual error, but very suspicious
    }
    /* does buf contain enough data to encode destination and size ? */
    if (req->size < 1+1) {
      printk(KERN_ERR "unpack(): req->size too small: %i\n", req->size);
      return -EINVAL;
    }
    /* does buf correspond to req ? */
    if (req->buf[0] != req->t_dev) {
      printk(KERN_ERR "unpack(): req->buf not for req->dev_t: %c != %c\n",
             req->buf[0], req->t_dev);
      return -EINVAL;
    }

    /* unpack data */
    size = (uint8_t) req->buf[1];
    printk(KERN_DEBUG "unpack(): coded rx packet size is: %i\n", size);
    unpacked = kzalloc(size, GFP_ATOMIC);
    if (!unpacked)
      return -ENOMEM;
    memcpy(unpacked,req->buf+1+1,size);

    /* free old buf; insert new buf and updated size */
    kfree(req->buf);
    req->buf = unpacked;
    req->size = size;

    return 0;
}

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
	struct usb_teensy *dev;
	int status;
	int i;
    int ret;
	char data[RAWHID_RX_SIZE+1]; /* TODO: why +1 ??? */
	struct list_head *curr;
	char packet_id;
	struct read_request *req = NULL;
				
	//DPRINT("interrupt_in callback called\n");

	if (!urb)
		goto reset;
	
	dev = urb->context;
	status = urb->status;
	
	if (status != 0){
		if ((status != -ENOENT) && (status != -ECONNRESET) &&
		    (status != -ESHUTDOWN)) {
			printk(KERN_ERR "teensy: input callback nonzero status received: %d\n", status);
			
		}
	}

	if (urb->actual_length > 0 && dev->in_buf) {
		
		/* examine the first byte */
        packet_id = dev->in_buf[0] & 0x0ff; /* TODO: make this a function */
		
		/* lock the list!!! */
		spin_lock(&readers_lock);

		/* search readers list for match, if no match, just
		 * drop the packet, snoozers are loozers */
		if (!list_empty(&readers_list)) {
			list_for_each(curr, &readers_list){
				
				struct read_request *temp = list_entry(curr, struct read_request, list);
				if (temp) {
					if (temp->t_dev == packet_id && !temp->complete) {
						req = temp;
						break;
					}
					
				}
			}
			
		} else {
            //DPRINT("no readers! Dropping packet!\n");
			goto reset;
			
		}

		if (req) {

			/* set read_request to completed so no other thread grabs it, lock?  */
		 	req->complete = true; 

			/* copy the received data into the req and upack */
            kfree(req->buf); /* free old buf: we're making new buf */
            req->buf = kmalloc(urb->actual_length, GFP_ATOMIC);
            if(!req->buf) /* TODO: what do we do ??? */
              printk(KERN_ERR "teensy_interrupt_in_callback(): "
                "failed atomic kmalloc: we're hosed!\n"); //return -ENOMEM;
            req->size = urb->actual_length;
			memcpy(req->buf, dev->in_buf, urb->actual_length);
            if ((ret = unpack(req)) < 0) /* TODO: what do we do ??? */
              printk(KERN_ERR "teensy_interrupt_in_callback(): "
                "failed unpack(): we're hosed!\n"); //return -EOHNO;

			/* wakeup the readers wait_queue */
		 	wake_up(&readers_queue); 
		}
	}
reset:
	spin_unlock(&readers_lock);
	
	usb_submit_urb(urb, GFP_ATOMIC);

	//DPRINT ("in URB RE-submitted\n");
	
}

/*
 * teensy_interrupt_out_callback
 *
 * TODO: document
 *
 * callback function to handle data coming on from teensy
 *
 * note, this runs in interrupt context, play nice!
 *
 */
static void teensy_interrupt_out_callback (struct urb *urb) 
{
	DPRINT("interrupt_out callback called\n");
    usb_free_urb(urb);
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

/*
 * teensy_read
 *
 * this function takes a read_request from a client, and cues it on
 * the readers_list for servicing by the reader callback routine
 *
 * a submission here will block, but waking is probably
 * non-deterministic. If there are multiple reads queued for the same
 * teensy device, it is very possible that they will be serviced out
 * of order.
 * 
 * @req: req->buf must be kfree()able pointer; caller is expected to
 * free req->buf after return; req->buf WILL NOT be the same pointer
 * as was passed! req->buf and req->size are modified and contain the
 * result on return.
 * 
 * NOTE: if you have a zero byte payload, then do req->buf =
 * kmalloc(0,...): this gives a free()able pointer.
 */
int teensy_read(struct read_request *req)
{
    int ret;
    struct usb_teensy * dev = teensy_dev;
    struct urb * out_urb;
	struct read_request* temp;
	struct list_head *curr;
	
	DPRINT("teensy_read()\n");
	/* check the request for validity (no nullptrs etc) */
	/* set request completed to FALSE */

	if (!req) {
      printk(KERN_ERR "teensy_read(): NULL req, bailing\n");
      return -EINVAL;
    }
    /*
    if (!req->size) {
      DPRINT("teensy_read(): emtpy req->buf, bailing\n");
      return -EINVAL;
    }
    */
    if (!dev) {
      DPRINT("teensy_read(): NULL dev, bailing\n");
      return -EINVAL;
    }

	DPRINT("req size: %d, t_dev: %x, buffer add: %p\n", req->size, req->t_dev, req->buf);
		
	req->complete = false;
	
	/* LOCK the LIST!! */
	spin_lock(&readers_lock);

	DPRINT ("got reader lock\n");
	
	/* put the request on the tail of the list */
	list_add_tail(&req->list, &readers_list); 

	/* UNLOCK THE LIST!! */
	spin_unlock(&readers_lock);
	
	/* send packet to teensy */
	/* TODO -- right now teensy just sends data on its own, need
	 * to implement a protocol for requesting the right kind of
	 * data */

    if ((ret = pack(req)) < 0) {
      printk(KERN_ERR "teensy_read(): pack() failed\n");
      return ret;
    }
    DPRINT("teensy_read(): 1 \n");
	out_urb = usb_alloc_urb(0, GFP_KERNEL);
    DPRINT("teensy_read(): 2 \n");
	usb_fill_int_urb (out_urb,
			  dev->udev,
			  usb_sndintpipe(dev->udev,
					 dev->out_endpoint),

			  req->buf,
			  req->size,

			  teensy_interrupt_out_callback, dev,
			  dev->out_interval);
    DPRINT("teensy_read(): 3 \n");
	usb_submit_urb(out_urb, GFP_KERNEL);
    DPRINT("teensy_read(): 4 \n");	
	/* wait_event completed == TRUE */
	wait_event(readers_queue, (req->complete));
    DPRINT("teensy_read(): 5 \n");
	/* back from blocked read, get the hell off the readers list
	 * because we'll be freed soon...*/
	spin_lock(&readers_lock);
	list_del(&req->list);
	spin_unlock(&readers_lock);

	return req->size;
}

static int probe_teensy (struct usb_interface *intf,
			 const struct usb_device_id *id) 
{
    struct usb_teensy * dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i, result;
		
	DPRINT("connect detected\n");

	/* allocate for our device structure */
	teensy_dev = kmalloc(sizeof(struct usb_teensy), GFP_KERNEL);
    dev = teensy_dev;
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
            dev->out_interval = endpoint->bInterval;
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

	/* additional setup stuff */
	spin_lock_init (&readers_lock);
	init_waitqueue_head(&readers_queue);
	
	/* sub-module-specific init */
	result = init_submodules();
	if (result)
		printk(KERN_ERR "teensy: failed to load adc");

	/* now start our interrupt driven reader... all other setup is done */
	init_reader(intf);
	

	return 0; /* TODO, really? */
  
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

	/* sub-module-specific cleanup */
    exit_submodules();

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

	/* generic init */
	result = usb_register (&teensy_driver);
	if (result) {
		printk(KERN_ERR "teensy: failed to register usb device! error code: %d", result);
	} else {		
		DPRINT("initialized.\n");
	}

#ifdef TEENSY_DEBUG_NO_HW
    /* DEBUG: check module insertion w/o teensy present */
    init_submodules();
#endif

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

#ifdef TEENSY_DEBUG_NO_HW
    /* DEBUG: check module cleanup w/o teensy present */
    exit_submodules();
#endif

	/* generic cleanup */
	usb_deregister(&teensy_driver);
	
	DPRINT("removal complete.\n");
}

module_init(teensy_init);
module_exit(teensy_exit);
