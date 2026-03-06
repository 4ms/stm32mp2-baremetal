#ifndef _LINUX_BUG_H
#define _LINUX_BUG_H

#include <stdio.h>

#define WARN_ON(cond)		((void)(cond))
#define WARN_ON_ONCE(cond)	((void)(cond))

#define BUG_ON(cond) \
	do { \
		if (cond) { \
			printf("BUG_ON at %s:%d\n", __FILE__, __LINE__); \
			while (1) ; \
		} \
	} while (0)

#endif /* _LINUX_BUG_H */
