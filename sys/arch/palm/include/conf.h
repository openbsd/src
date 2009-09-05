/*	$OpenBSD: conf.h,v 1.1 2009/09/05 01:22:11 marex Exp $	*/
/*	$NetBSD: conf.h,v 1.8 2002/02/10 12:26:03 chris Exp $	*/

#ifndef _PALM_CONF_H
#define	_PALM_CONF_H

#include <sys/conf.h>

/*
 * PALM specific device includes go in here
 */

#define CONF_HAVE_USB
#define	CONF_HAVE_WSCONS

#include <arm/conf.h>

#endif	/* _PALM_CONF_H */
