/*	$OpenBSD: conf.h,v 1.7 2010/11/28 20:49:47 miod Exp $	*/
/*	$NetBSD: conf.h,v 1.8 2002/02/10 12:26:03 chris Exp $	*/

#ifndef _ZAURUS_CONF_H
#define	_ZAURUS_CONF_H

#include <sys/conf.h>

/*
 * ZAURUS specific device includes go in here
 */

#define CONF_HAVE_APM

#include <arm/conf.h>

#endif	/* _ZAURUS_CONF_H */
