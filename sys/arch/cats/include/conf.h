/*	$OpenBSD: conf.h,v 1.1 2004/02/01 05:12:54 drahn Exp $	*/
/*	$NetBSD: conf.h,v 1.8 2002/02/10 12:26:03 chris Exp $	*/

#ifndef _CATS_CONF_H
#define	_CATS_CONF_H

/*
 * CATS specific device includes go in here
 */
#include "fcom.h"

#define	CONF_HAVE_PCI
#define	CONF_HAVE_USB
#define	CONF_HAVE_SCSIPI
#define	CONF_HAVE_WSCONS

#include <arm/conf.h>

#endif	/* _CATS_CONF_H */
