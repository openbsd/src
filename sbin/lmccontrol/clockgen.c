/* $OpenBSD: clockgen.c,v 1.3 2004/08/08 19:04:25 deraadt Exp $ */
/* $Id: clockgen.c,v 1.3 2004/08/08 19:04:25 deraadt Exp $ */

/*-
 * Copyright (c) 1997-1999 LAN Media Corporation (LMC)
 * All rights reserved.  www.lanmedia.com
 *
 * This code is written by Michael Graff (explorer@vix.com) for LMC.
 * The code is derived from permitted modifications to software created
 * by Matt Thomas (matt@3am-software.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All marketing or advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      This product includes software developed by LAN Media Corporation
 *      and its contributors.
 * 4. Neither the name of LAN Media Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAN MEDIA CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE CORPORATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>

typedef struct lmc___ctl lmc_ctl_t;
#include <dev/pci/if_lmcioctl.h>

#define T1_FREF  20000000
#define T1_FMIN  30000000
#define T1_FMAX 330000000

void
lmc_av9110_freq(u_int32_t target, lmc_av9110_t *av)
{
	unsigned int n, m, v, x, r;
	unsigned long f;
	unsigned long iFvco;

	av->n = 0;
	av->m = 0;
	av->v = 0;
	av->x = 0;
	av->r = 0;
	av->f = 0;
	av->exact = 0;

	target *= 16;

	for (n = 3 ; n <= 127 ; n++)
		for (m = 3 ; m <= 127 ; m++)
			for (v = 1 ; v <= 8 ; v *= 8)
				for (x = 1 ; x <= 8 ; x <<= 1)
					for (r = 1 ; r <= 8 ; r <<= 1) {
						iFvco = (T1_FREF / m) * n * v;
						if (iFvco < T1_FMIN || iFvco > T1_FMAX)
							continue;
						f = iFvco / (x * r);
						if (f >= target)
							if ((av->f == 0) || (f - target < av->f - target)) {

							av->n = n;
							av->m = m;
							if (v == 1)
								av->v = 0;
							else
								av->v = 1;
							if (x == 1)
								av->x = 0;
							else if (x == 2)
								av->x = 1;
							else if (x == 4)
								av->x = 2;
							else if (x == 8)
								av->x = 3;
							if (r == 1)
								av->r = 0;
							else if (r == 2)
								av->r = 1;
							else if (r == 4)
								av->r = 2;
							else if (r == 8)
								av->r = 3;
							av->f = f;
							if (f == target) {
								av->exact = 1;
								av->f /= 16;
								return;
							}
						}
					}
	av->f /= 16;
}

