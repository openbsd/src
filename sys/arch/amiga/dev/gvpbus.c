/*	$OpenBSD: gvpbus.c,v 1.3 1996/05/02 06:44:00 niklas Exp $	*/
/*	$NetBSD: gvpbus.c,v 1.11 1996/04/21 21:11:36 veego Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <amiga/amiga/device.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/gvpbusvar.h>

void gvpbusattach __P((struct device *, struct device *, void *));
int gvpbusmatch __P((struct device *, void *, void *));
int gvpbusprint __P((void *auxp, char *));

extern int sbic_no_dma;		/* Kludge for A1291 - mlh */

struct cfattach gvpbus_ca = {
	sizeof(struct device), gvpbusmatch, gvpbusattach
};

struct cfdriver gvpbus_cd = {
	NULL, "gvpbus", DV_DULL, NULL, 0
};

int
gvpbusmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct zbus_args *zap;

	zap = auxp;

	/*
	 * Check manufacturer and product id.
	 */
#if 0
	if (zap->manid == 2017 && (zap->prodid == 11 || zap->prodid == 2))
#else
	if (zap->manid == 2017 && zap->prodid == 11)
#endif
		return(1);
	return(0);
}

void
gvpbusattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct zbus_args *zap;
	struct gvpbus_args ga;

	zap = auxp;
	bcopy(zap, &ga.zargs, sizeof(struct zbus_args));
	ga.flags = 0;
	
	/*
	 * grab secondary type (or fake it if we have a series I)
	 */
	if (zap->prodid != 9) {
		ga.prod = *((u_char *)zap->va + 0x8001) & 0xf8;
		if (*((u_char *)zap->va + 0x8001) & 0x01)
			ga.flags |= GVP_14MHZ;
	printf(": subprod %02x flags %02x", *((u_char *)zap->va + 0x8001), ga.flags);
#if 0
	} else {
		ga.prod = GVP_SERIESII;		/* really a series I */
		ga.flags |= GVP_NOBANK;
#endif
	}
	

	switch (ga.prod) {
	/* no scsi */
	case GVP_GFORCE_040:
	case GVP_GFORCE_030:
		ga.flags = GVP_IO;
		/*FALLTHROUGH*/
	case GVP_COMBO_R4:
	case GVP_COMBO_R3:
		ga.flags |= GVP_ACCEL;
		break;
	/* scsi */
	case GVP_A1291_SCSI:
		ga.flags |= GVP_SCSI | GVP_ACCEL;
		sbic_no_dma = 1;	/* Kludge !!!!!!! mlh */
		break;
	case GVP_GFORCE_040_SCSI:
		ga.flags |= GVP_SCSI | GVP_IO | GVP_ACCEL;
		break;
	case GVP_GFORCE_030_SCSI:
		ga.flags |= GVP_SCSI | GVP_IO | GVP_ACCEL | GVP_25BITDMA;
		break;
	case GVP_A530_SCSI:
	case GVP_COMBO_R4_SCSI:
		ga.flags |= GVP_SCSI | GVP_ACCEL | GVP_25BITDMA;
		if (ga.prod == GVP_COMBO_R4_SCSI)
			ga.flags ^= GVP_14MHZ;
		break;
	case GVP_COMBO_R3_SCSI:
		ga.flags |= GVP_SCSI | GVP_ACCEL | GVP_24BITDMA;
		ga.flags ^= GVP_14MHZ;
		break;
	case GVP_SERIESII:
		ga.flags |= GVP_SCSI | GVP_24BITDMA;
		break;
	/* misc */
	case GVP_IOEXTEND:
		ga.flags |= GVP_IO;
		break;
	default:
		printf(": unknown Series II %x", ga.prod);
	}
	printf("\n");
	/*
	 * attempt to configure the board.
	 */
	config_found(dp, &ga, gvpbusprint);
	/*
	 * eventually when io support is added we need to 
	 * configure that too.
	 */
}

int
gvpbusprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	struct gvpbus_args *gap;

	gap = auxp;
	if (pnp == NULL)
		return(QUIET);
	/*
	 * doesn't support io yet.
	 */
	if (gap->prod == GVP_IOEXTEND) 
		printf("gio at %s", pnp);
	else
		printf("gtsc at %s", pnp);
	return(UNCONF);
}

