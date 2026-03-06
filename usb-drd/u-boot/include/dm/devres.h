#ifndef _DM_DEVRES_H
#define _DM_DEVRES_H

#include <stdlib.h>

/* "Device managed" allocations — in bare-metal just malloc/free.
 * Lifetime is the whole application. */
#define devm_kzalloc(dev, size, flags)	calloc(1, size)
#define devm_kfree(dev, ptr)		free(ptr)

#endif /* _DM_DEVRES_H */
