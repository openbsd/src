/*	$NetBSD: hpdefs.h,v 1.1 1995/02/13 00:44:00 ragge Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

 /* All bugs are subject to removal without further notice */
		


/* hpdefs.h - 940325/ragge */

#define HPCS1_DVA	0x800	/* Drive avail, in dual-port config */
#define HPDS_VV		0x40	/* Volume valid, not changed */
#define HPDS_DRY	0x80	/* Drive ready to accept commands */
#define HPDS_DPR	0x100	/* Drive present */
#define HPDS_PGM	0x200	/* Programmable in dual-port config */
#define HPDS_WRL	0x800	/* Write locked media */
#define HPDS_MOL	0x1000	/* Medium on-line */
#define HPDT_DRQ	0x800	/* Dual-port disk */

/*
 * Drive status, per drive
 */
struct hp_info {
        daddr_t hp_dsize;       /* size in sectors */
/*      u_long  hp_type;        /* drive type */
        u_long  hp_mediaid;     /* media id */
        int     hp_state;       /* open/closed state */
        struct  hp_geom {       /* geometry information */
                u_short rg_nsectors;    /* sectors/track */
                u_short rg_ngroups;     /* track groups */
                u_short rg_ngpc;        /* groups/cylinder */
                u_short rg_ntracks;     /* ngroups*ngpc */
                u_short rg_ncyl;        /* ra_dsize/ntracks/nsectors */
#ifdef notyet
                u_short rg_rctsize;     /* size of rct */
                u_short rg_rbns;        /* replacement blocks per track */
                u_short rg_nrct;        /* number of rct copies */
#endif
        } hp_geom;
        int     hp_wlabel;      /* label sector is currently writable */
        u_long  hp_openpart;    /* partitions open */
        u_long  hp_bopenpart;   /* block partitions open */
        u_long  hp_copenpart;   /* character partitions open */
};

/*
 * Device to unit number and partition and back
 */
#define UNITSHIFT       3
#define UNITMASK        7
#define hpunit(dev)    (minor(dev) >> UNITSHIFT)
#define hppart(dev)    (minor(dev) & UNITMASK)
#define hpminor(u, p)  (((u) << UNITSHIFT) | (p))

