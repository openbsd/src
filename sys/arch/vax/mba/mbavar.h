/*	$NetBSD: mbavar.h,v 1.1 1995/02/13 00:44:04 ragge Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden
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
 *      This product includes software developed at Ludd, University of Lule}.
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

/* Mba n}nting... ragge 940311 */

#define MBCR_INIT	1
#define	MBCR_IE		(1<<2)
#define	MBDS_DPR	(1<<8)
#define	MBSR_NED	(1<<18)
#define	MBDT_MOH	(1<<13)
#define	MBDT_TYPE	511
#define MBDT_TAP	(1<<14)

#define	CLOSED		0
#define	WANTOPEN	1
#define	RDLABEL		2
#define	OPEN		3
#define	OPENRAW		4

struct mba_ctrl {
	struct mba_regs *mba_regs;
	struct mba_device *mba_device[8];
};

struct mba_device {
	struct mba_driver *driver;
	int unit;
	int mbanum;
	int drive;
	int dk;
	int alive;
	int type;
	struct mba_regs *mi_mba;
	struct mba_hd *hd;
	int drv;
	int device;
};

struct mba_slave {
	struct mba_driver *driver;
	int ctlr;
	int unit;
	int slave;
	int alive;
};

struct mba_driver {
	int (*slave)();
	char *sname;
	char *dname;
	short *type;
	int (*attach)();
	struct mba_device **info;
};

struct mba_hd {
	struct mba_device *device[8]; /* XXX - Var tidigare mh_mbip */
	int ndrive;
	int mh_active;
	struct mba_regs *mh_mba;
	struct mba_regs *mh_physmba;
	struct mba_device *mh_actf;
	struct mba_device *mh_actl;
};
