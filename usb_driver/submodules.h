#ifndef __SUBMODULES_H__
#define __SUBMODULES_H__

/*** submodules: EDIT HERE TO ADD SUBMODULE ***/
/* to add a submodule: 
   1. include headers here
   2. add init and exit to arrays here
   3. add the <your_submodule>.o to teensy_mono-objs in the Makefile
   (4. write code to run on the teensy and handle your request ...)
*/
/* NC: I couldn't figure out how to have these in submodule.h (it's
   included in multiple places), so i moved the defs here */
#include "teensy_adc.h"
#include "teensy_mc.h"
static int  (*inits[])(void) __attribute__((unused)) = {
        adc_init,
        mc_init,
};
static void (*exits[])(void) __attribute__((unused)) = {
        adc_exit,
        mc_exit,
};

/* call all inits/exits */
int init_submodules(void);
void exit_submodules(void);
#endif
