/*	$NetBSD: autoconf.c,v 1.1 1996/09/30 16:34:39 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <machine/powerpc.h>

extern int cold;

void configure __P((void));
void setroot __P((void));
void swapconf __P((void));


/*
 * Determine device configuration for a machine.
 */
void
configure()
{
	ofrootfound();
	(void)spl0();

	/*
	 * Setup root device.
	 * Configure swap area.
	 */
	setroot();
	swapconf();
	cold = 0;
}

/*
 * Try to find the device we were booted from to set rootdev.
 */
void
setroot()
{
	char *cp;
	
	if (mountroot) {
		/*
		 * rootdev/swdevt/mountroot etc. already setup by config.
		 */
		return;
	}
	
	/*
	 * Try to find the device where we were booted from.
	 */
	for (cp = bootpath + strlen(bootpath); --cp >= bootpath;) {
		if (*cp == '/') {
			*cp = '\0';
			if (!dk_match(bootpath)) {
				*cp = '/';
				break;
			}
			*cp = '/';
		}
	}
	if (cp < bootpath || boothowto & RB_ASKNAME) {
		/* Insert -a processing here				XXX */
		panic("Cannot find root device");
	}
	dk_cleanup();
}

/*
 * Configure swap space
 */
void
swapconf()
{
	struct swdevt *swp;
	int nblks;
	
	for (swp = swdevt; swp->sw_dev != NODEV; swp++)
		if (bdevsw[major(swp->sw_dev)].d_psize) {
			nblks = (*bdevsw[major(swp->sw_dev)].d_psize)(swp->sw_dev);
			if (nblks != -1
			    && (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	dumpconf();
}
