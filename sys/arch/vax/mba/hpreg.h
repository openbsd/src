/*	$NetBSD: hpreg.h,v 1.2 1995/06/16 15:20:11 ragge Exp $ */
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
		


struct hp_regs {
        int     hp_cs1;
        int     hp_ds;
        int     hp_er1;
        int     hp_mr1;
        int     hp_as;
        int     hp_da;
        int     hp_dt;
        int     hp_la;
        int     hp_sn;
        int     hp_of;
        int     hp_dc;
        int     hp_hr;
        int     hp_mr2;
        int     hp_er2;
        int     hp_ec1;
        int     hp_ec2;
        int     utrymme[16];
};

#define	hp_drv	hp_regs

#define	HPCS_PA		0x13	/* Pack acknowledge */
#define	HPCS_SEEK	0x5
#define	HPCS_READ	0x39
#define HPCS_DVA       0x800   /* Drive avail, in dual-port config */

#define HPDS_VV         0x40    /* Volume valid, not changed */
#define HPDS_DRY        0x80    /* Drive ready to accept commands */
#define HPDS_DPR        0x100   /* Drive present */
#define HPDS_PGM        0x200   /* Programmable in dual-port config */
#define HPDS_WRL        0x800   /* Write locked media */
#define HPDS_MOL        0x1000  /* Medium on-line */

#define HPDT_DRQ        0x800   /* Dual-port disk */

#define	HPOF_FMT	0x1000	/* 16/18 bit data */

#if 0
#define	HPCS_
#define	HPCS_
#define	HPCS_
#define	HPCS_
#define	HPCS_
#define	HPCS_
#define	HPCS_
#define	HPCS_
#endif



