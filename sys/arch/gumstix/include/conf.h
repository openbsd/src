/*	$OpenBSD: conf.h,v 1.2 2008/11/27 20:51:48 miod Exp $	*/
/*	$NetBSD: conf.h,v 1.8 2002/02/10 12:26:03 chris Exp $	*/

#ifndef _GUMSTIX_CONF_H
#define	_GUMSTIX_CONF_H

#include <sys/conf.h>

/*
 * GUMSTIX specific device includes go in here
 */

#define CONF_HAVE_APM
#define	CONF_HAVE_USB
#define	CONF_HAVE_WSCONS

#include <arm/conf.h>

#endif	/* _GUMSTIX_CONF_H */
