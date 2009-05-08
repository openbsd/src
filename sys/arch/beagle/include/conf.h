/*	$OpenBSD: conf.h,v 1.1 2009/05/08 03:13:26 drahn Exp $	*/
/*	$NetBSD: conf.h,v 1.8 2002/02/10 12:26:03 chris Exp $	*/

#ifndef _BEAGLEBOARD_CONF_H
#define	_BEAGLEBOARD_CONF_H

#include <sys/conf.h>

/*
 * BEAGLEBOARD specific device includes go in here
 */

#define	CONF_HAVE_USB
//#define	CONF_HAVE_GPIO

#include <arm/conf.h>

#endif	/* _BEAGLEBOARD_CONF_H */
