/*
 * teensy.c
 * 
 */
#ifndef TEENSY_H
#include "teensy.h"
#endif



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
								
	printk(KERN_INFO "teensy: initialized\n");
	
	return 0;
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
	printk (KERN_INFO "teensy: Removing teenst\n");

	printk (KERN_INFO "teensy: removal complete.\n");
	
	return;
	
}

module_init(teensy_init);
module_exit(teensy_exit);
