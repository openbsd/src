/*	$OpenBSD: imsg_subr.h,v 1.1 2015/07/21 04:06:04 yasuoka Exp $	*/

#ifndef _IMSG_SUBR_H
#define _IMSG_SUBR_H

struct imsgbuf;

#include <sys/cdefs.h>
__BEGIN_DECLS

int	 imsg_sync_read(struct imsgbuf *, int);
int	 imsg_sync_flush(struct imsgbuf *, int);

__END_DECLS

#endif
