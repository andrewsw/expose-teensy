/*
 * teensy.c
 * 
 */
#ifndef TEENSY_H
#include "teensy.h"
#endif


static int probe_teensy (struct usb_interface *intf,
			 const struct usb_device_id *id) 
{
	DPRINT("connect detected\n");

	return 0;
  
}

static void disconnect_teensy(struct usb_interface *intf) 
{

	DPRINT("disconnect detected\n");

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
		
	DPRINT("initialized.\n");
	
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
