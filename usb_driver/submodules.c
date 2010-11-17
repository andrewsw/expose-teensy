#include "submodules.h"
#include <linux/module.h> /* printk */

/*** submodules ***/
/* to add a submodule: 
   1. include headers here
   2. add init and exit to arrays here
   3. add the <your_submodule>.o to teensy_mono-objs in the Makefile 
*/
/* NC: I couldn't figure out how to have these in submodule.h (it's
   included in multiple places), so i moved the defs here */
#include "teensy_adc.h"
static int (*inits[])(void) = {
  adc_init,
};
static void (*exits[])(void) = {
  adc_exit,
};

/*** API ***/
int init_submodules(void) {
  int i, j, ret = 0;
  for (i = 0; i < sizeof(inits)/sizeof(inits[0]); ++i) {
    ret = inits[i]();
    if (ret < 0) {
      printk(KERN_ERR "failed to initialize %dth submodule\n", i);
      /* clean up */
      for (j = 0; j < i; ++j)
        exits[j]();
    }
  }
  return ret;
}

void exit_submodules(void) {
  int i;
  for (i = 0; i < sizeof(exits)/sizeof(exits[0]); ++i)
    exits[i]();
}
