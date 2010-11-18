#include "submodules.h"
#include <linux/module.h> /* printk */

/*** private ***/

static int loaded = 0;

/*** API ***/
int init_submodules(void) {
        int i, j, ret = 0;

        printk(KERN_DEBUG "init_submodules()\n");

        if (loaded) {
                printk(KERN_ERR "init_submodules(): someone called us when submodules were already loaded ... FAIL\n");
                return -EINVAL; /* TODO: correct error code ? */
        }

        for (i = 0; i < sizeof(inits)/sizeof(inits[0]); ++i) {
                ret = inits[i]();
                if (ret < 0) {
                        printk(KERN_ERR "init_submodules(): failed to initialize %dth submodule\n", i);
                        /* clean up */
                        for (j = 0; j < i; ++j)
                                exits[j]();
                        return ret;
                }
        }
        loaded = 1;
        return 0;
}

/* MAYBE TODO: should these instead be unloaded in the opposite the
   order they were loaded ??? */
void exit_submodules(void) {
        int i;

        printk(KERN_DEBUG "exit_submodules()\n");

        if (!loaded)
                printk(KERN_ERR "exit_submodules(): someone called us when submodules weren't loaded ... NO-OP\n");
        else {
                for (i = 0; i < sizeof(exits)/sizeof(exits[0]); ++i)
                        exits[i]();
                loaded = 0;
        }
}
