/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ex_version.c	10.27 (Berkeley) 6/9/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"
#include "version.h"

#define	BSDI_ADVERT \
"\nBerkeley Software Design, Inc. (BSDI) is the commercial supplier of \
the state-of-the-art BSD operating system, networking and Internet \
technologies originally developed by the Computer Systems Research \
Group (CSRG) at the University of California at Berkeley.  BSDI's \
BSD/OS represents over twenty years of development by the worldwide \
BSD community.  BSD technology is known for its flexible and portable \
architecture and advanced development environments.  Today, BSDI is \
recognized for the strength of BSDI-powered systems in demanding \
business and technical computing environments, the worldwide customer \
acceptance of BSD-based technology, the know-how of BSDI's leading \
computer scientists, and BSDI's focus on delivering and supporting \
industrial-strength software for computing platforms.  BSDI may be \
contacted at info@bsdi.com or 1-800-800-4273."

/*
 * ex_version -- :version
 *	Display the program version and a shameless plug for BSDI.
 *
 * PUBLIC: int ex_version __P((SCR *, EXCMD *));
 */
int
ex_version(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	msgq(sp, M_INFO, VI_VERSION);
	msgq(sp, M_INFO, BSDI_ADVERT);
	return (0);
}
