/*	$OpenBSD: conf.h,v 1.2 2004/12/31 14:57:13 drahn Exp $	*/
/*	$NetBSD: conf.h,v 1.8 2002/02/10 12:26:03 chris Exp $	*/

#ifndef _ZAURUS_CONF_H
#define	_ZAURUS_CONF_H

/*
 * ZAURUS specific device includes go in here
 */
#include "fcom.h"

#define	CONF_HAVE_PCI
#define	CONF_HAVE_USB
#define	CONF_HAVE_SCSIPI
#define	CONF_HAVE_WSCONS

#include <arm/conf.h>

#endif	/* _ZAURUS_CONF_H */
