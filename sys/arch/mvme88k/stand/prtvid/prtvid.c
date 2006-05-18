/*	$OpenBSD: prtvid.c,v 1.5 2006/05/18 06:11:15 miod Exp $ */

/*
 * Copyright (c) 1995 Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>

#include "vid.h"

void	swabcfg(struct cfg *);
void	swabvid(struct vid *);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct vid *pvid;
	struct cfg *pcfg;

	pvid = (struct vid *) malloc(sizeof (struct vid));

	fread(pvid, sizeof(struct vid), 1, stdin);

	if (BYTE_ORDER != BIG_ENDIAN)
		swabvid(pvid);

	printf("vid_id		%s	%lx\n", pvid->vid_id,
	    (char *)&(pvid->vid_id[4]) - (char *)pvid);
	printf("vid_oss		%x	%lx\n", pvid->vid_oss,
	    (char *)&(pvid->vid_oss) - (char *)pvid);
	printf("vid_osl		%x	%lx\n", pvid->vid_osl,
	    (char *)&(pvid->vid_osl) - (char *)pvid);
	printf("vid_osa_u	%x	%lx\n", pvid->vid_osa_u,
	    (char *)&(pvid->vid_osa_u) - (char *)pvid);
	printf("vid_osa_l	%x	%lx\n", pvid->vid_osa_l,
	    (char *)&(pvid->vid_osa_l) - (char *)pvid);
	printf("vid_vd 		%s	%lx\n", pvid->vid_vd,
	    (char *)&(pvid->vid_vd) - (char *)pvid);
	printf("vid_cas		%x	%lx\n", pvid->vid_cas,
	    (char *)&(pvid->vid_cas) - (char *)pvid);
	printf("vid_cal		%x	%lx\n", pvid->vid_cal,
	    (char *)&(pvid->vid_cal) - (char *)pvid);
	printf("vid_moto	%s	%lx\n", pvid->vid_mot,
	    (char *)&(pvid->vid_mot[0]) - (char *)pvid);

	free(pvid);

	pcfg = (struct cfg *) malloc(sizeof(struct cfg));

	fread(pcfg, sizeof(struct cfg), 1, stdin);

	if (BYTE_ORDER != BIG_ENDIAN)
		swabcfg(pcfg);

	printf("cfg_atm		%x	%lx\n", pcfg->cfg_atm,
	    (char *)&(pcfg->cfg_atm) - (char *)(pcfg));
	printf("cfg_prm		%x	%lx\n", pcfg->cfg_prm,
	    (char *)&(pcfg->cfg_prm) - (char *)(pcfg));
	printf("cfg_atw		%x	%lx\n", pcfg->cfg_atw,
	    (char *)&(pcfg->cfg_atw) - (char *)(pcfg));
	printf("cfg_rec		%x	%lx\n",(int)pcfg->cfg_rec,
	    (char *)&(pcfg->cfg_rec) - (char *)(pcfg));
	printf("cfg_spt		%x	%lx\n", pcfg->cfg_spt,
	    (char *)&(pcfg->cfg_spt) - (char *)(pcfg));
	printf("cfg_hds		%x	%lx\n", pcfg->cfg_hds,
	    (char *)&(pcfg->cfg_hds) - (char *)(pcfg));
	printf("cfg_trk		%x	%lx\n", pcfg->cfg_trk,
	    (char *)&(pcfg->cfg_trk) - (char *)(pcfg));
	printf("cfg_ilv		%x	%lx\n", pcfg->cfg_ilv,
	    (char *)&(pcfg->cfg_ilv) - (char *)(pcfg));
	printf("cfg_sof		%x	%lx\n", pcfg->cfg_sof,
	    (char *)&(pcfg->cfg_sof) - (char *)(pcfg));
	printf("cfg_psm		%x	%lx\n", pcfg->cfg_psm,
	    (char *)&(pcfg->cfg_psm) - (char *)(pcfg));
	printf("cfg_shd		%x	%lx\n", pcfg->cfg_shd,
	    (char *)&(pcfg->cfg_shd) - (char *)(pcfg));
	printf("cfg_pcom	%x	%lx\n", pcfg->cfg_pcom,
	    (char *)&(pcfg->cfg_pcom) - (char *)(pcfg));
	printf("cfg_ssr 	%x	%lx\n", pcfg->cfg_ssr,
	    (char *)&(pcfg->cfg_ssr) - (char *)(pcfg));
	printf("cfg_rwcc	%x	%lx\n", pcfg->cfg_rwcc,
	    (char *)&(pcfg->cfg_rwcc) - (char *)(pcfg));
	printf("cfg_ecc 	%x	%lx\n", pcfg->cfg_ecc,
	    (char *)&(pcfg->cfg_ecc) - (char *)(pcfg));
	printf("cfg_eatm	%x	%lx\n", pcfg->cfg_eatm,
	    (char *)&(pcfg->cfg_eatm) - (char *)(pcfg));
	printf("cfg_eprm	%x	%lx\n", pcfg->cfg_eprm,
	    (char *)&(pcfg->cfg_eprm) - (char *)(pcfg));
	printf("cfg_eatw	%x	%lx\n", pcfg->cfg_eatw,
	    (char *)&(pcfg->cfg_eatw) - (char *)(pcfg));
	printf("cfg_gpb1	%x	%lx\n", pcfg->cfg_gpb1,
	    (char *)&(pcfg->cfg_gpb1) - (char *)(pcfg));
	printf("cfg_gpb2	%x	%lx\n", pcfg->cfg_gpb2,
	    (char *)&(pcfg->cfg_gpb2) - (char *)(pcfg));
	printf("cfg_gpb3	%x	%lx\n", pcfg->cfg_gpb3,
	    (char *)&(pcfg->cfg_gpb3) - (char *)(pcfg));
	printf("cfg_gpb4	%x	%lx\n", pcfg->cfg_gpb4,
	    (char *)&(pcfg->cfg_gpb4) - (char *)(pcfg));
	printf("cfg_ssc		%x	%lx\n", pcfg->cfg_ssc,
	    (char *)&(pcfg->cfg_ssc) - (char *)(pcfg));
	printf("cfg_runit	%x	%lx\n", pcfg->cfg_runit,
	    (char *)&(pcfg->cfg_runit) - (char *)(pcfg));
	printf("cfg_rsvc1	%x	%lx\n", pcfg->cfg_rsvc1,
	    (char *)&(pcfg->cfg_rsvc1) - (char *)(pcfg));
	printf("cfg_rsvc2	%x	%lx\n", pcfg->cfg_rsvc2,
	    (char *)&(pcfg->cfg_rsvc2) - (char *)(pcfg));

	return (0);
}

void
swabvid(pvid)
	struct vid *pvid;
{
	swap32(pvid->vid_oss);
	swap16(pvid->vid_osl);
	swap16(pvid->vid_osa_u);
	swap16(pvid->vid_osa_l);
	swap32(pvid->vid_cas);
}

void
swabcfg(pcfg)
	struct cfg *pcfg;
{
	printf("swapping cfg\n");

	swap16(pcfg->cfg_atm);
	swap16(pcfg->cfg_prm);
	swap16(pcfg->cfg_atm);
	swap16(pcfg->cfg_rec);
	swap16(pcfg->cfg_trk);
	swap16(pcfg->cfg_psm);
	swap16(pcfg->cfg_shd);
	swap16(pcfg->cfg_pcom);
	swap16(pcfg->cfg_rwcc);
	swap16(pcfg->cfg_ecc);
	swap16(pcfg->cfg_eatm);
	swap16(pcfg->cfg_eprm);
	swap16(pcfg->cfg_eatw);
	swap16(pcfg->cfg_rsvc1);
	swap16(pcfg->cfg_rsvc2);
}
