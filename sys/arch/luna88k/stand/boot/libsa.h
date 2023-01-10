/*	$OpenBSD: libsa.h,v 1.1 2023/01/10 17:10:57 miod Exp $	*/

/* public domain */

#include <lib/libsa/stand.h>

#define DEFAULT_KERNEL_ADDRESS 0

void devboot(dev_t, char *);
void machdep(void);
void run_loadfile(uint64_t *, int);
