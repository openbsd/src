/*	$OpenBSD: spkr.h,v 1.2 2000/08/05 22:07:32 niklas Exp $	*/
/*	$NetBSD: spkr.h,v 1.3 1994/10/27 04:16:27 cgd Exp $	*/

/*
 * spkr.h -- interface definitions for speaker ioctl()
 */

#ifndef _I386_SPKR_H_
#define _I386_SPKR_H_

#include <sys/ioctl.h>

#define SPKRTONE        _IOW('S', 1, tone_t)    /* emit tone */
#define SPKRTUNE        _IO('S', 2)             /* emit tone sequence */

typedef struct {
	int	frequency;	/* in hertz */
	int	duration;	/* in 1/100ths of a second */
} tone_t;

#endif /* _I386_SPKR_H_ */
