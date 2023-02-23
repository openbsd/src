/*	$OpenBSD: libsa.h,v 1.2 2023/02/23 19:48:22 miod Exp $	*/

/* public domain */

#include <lib/libsa/stand.h>

void devboot(dev_t, char *);
void machdep(void);
void run_loadfile(uint64_t *, int);
