#ifndef _MALLOC_H
#define _MALLOC_H

#include <stdlib.h>
#include <stddef.h>

/* memalign — provided by newlib.
 * Declare here so driver files don't need U-Boot's malloc.h. */
void *memalign(size_t alignment, size_t size);

#define kmalloc_array(n, size, flags)	calloc((n), (size))

#endif /* _MALLOC_H */
