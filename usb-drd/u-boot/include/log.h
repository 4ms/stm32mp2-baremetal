#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

#define pr_debug(fmt, ...)	printf("[DWC3 DBG] " fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)	printf("[DWC3 INF] " fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)	printf("[DWC3 WRN] " fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)	printf("[DWC3 ERR] " fmt, ##__VA_ARGS__)

/* U-Boot's debug() macro — used by linux-compat.h's dev_WARN */
#define debug(fmt, ...)		printf("[DWC3 DBG] " fmt, ##__VA_ARGS__)

#endif /* _LOG_H */
