/* $OpenBSD: param.h,v 1.3 2011/06/25 19:38:47 miod Exp $ */
/* public domain */

#ifndef _MACHINE_PARAM_H_
#define _MACHINE_PARAM_H_

#define	MACHINE		"octeon"
#define	_MACHINE	octeon
#define MACHINE_ARCH	"mips64"
#define _MACHINE_ARCH	mips64

#define MID_MACHINE	MID_MIPS64

#define	PAGE_SHIFT	14

#include <mips64/param.h>

#endif	/* _MACHINE_PARAM_H_ */
