/*	$OpenBSD: conf.h,v 1.1 2002/06/08 15:47:33 miod Exp $	*/

#include <sys/conf.h>

#define	mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);
