/*	$OpenBSD: iodc.h,v 1.2 1998/08/29 01:33:32 mickey Exp $	*/

/*
 * Copyright (c) 1990 mt Xinu, Inc.  All rights reserved.
 * Copyright (c) 1990,1991,1992,1994 University of Utah.  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * This file may be freely distributed in any form as long as
 * this copyright notice is included.
 * MTXINU, THE UNIVERSITY OF UTAH, AND CSL PROVIDE THIS SOFTWARE ``AS
 * IS'' AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: iodc.h 1.6 94/12/14$
 */

/*
 * Definitions for talking to IODC (I/O Dependent Code).
 *
 * The PDC is used to load I/O Dependent Code from a particular module.
 * I/O Dependent Code is module-type dependent software which provides
 * a uniform way to identify, initialize, and access a module (and in
 * some cases, their devices).
 */

#ifndef	_IODC_
#define _IODC_

/* iodc_type */
#define	IODC_TP_NPROC	 0	/* native processor */
#define	IODC_TP_MEMORY	 1	/* memory */
#define	IODC_TP_B_DMA	 2	/* Type-B DMA (NIO Transit, Parallel, ... ) */
#define	IODC_TP_B_DIRECT 3	/* Type-B Direct */
#define	IODC_TP_A_DMA	 4	/* Type-A DMA (NIO HPIB, LAN, ... ) */
#define	IODC_TP_A_DIRECT 5	/* Type-A Direct (RS232, HIL, ... ) */
#define	IODC_TP_OTHER	 6	/* other */
#define	IODC_TP_BCPORT	 7	/* Bus Converter Port */
#define	IODC_TP_CIO	 8	/* CIO adapter */
#define	IODC_TP_CONSOLE	 9	/* console */
#define	IODC_TP_FIO	10	/* foreign I/O module */
#define	IODC_TP_BA	11	/* bus adaptor */
#define	IODC_TP_MULTI	12	/* Multiple-Type I/O */
#define	IODC_TP_FAULTY	31	/* broken */

/* iodc_sv_model (IODC_TP_MEMORY) */
#define	SVMOD_MEM_ARCH	0x8	/* architected memory module */
#define	SVMOD_MEM_PDEP	0x9	/* processor-dependent memory module */

/* iodc_sv_model (IODC_TP_OTHER) */
#define	SVMOD_O_SPECFB	0x48	/* hp800 Spectograph frame buffer */
#define	SVMOD_O_SPECCTL	0x49	/* hp800 Spectograph control */

/* iodc_sv_model (IODC_TP_BA) */
#define	SVMOD_BA_ASP	0x70	/* hp700 Core Bus Adapter (ASP/Hardball) */
#define	SVMOD_BA_EISA	0x76	/* hp700 EISA Bus Adapter */
#define	SVMOD_BA_VME	0x78	/* hp700 VME Bus Adapter (unsupported) */
#define	SVMOD_BA_LASI	0x81	/* hp700 712 Bus Adapter */
#define	SVMOD_BA_WAX	0x8e	/* hp700 ??? Bus Adapter (unsupported) */

/* iodc_sv_model (IODC_TP_FIO) */
#define	SVMOD_FIO_SCSI	0x71	/* hp700 Core SCSI */
#define	SVMOD_FIO_FWSCSI 0x7c	/* hp700 Core FW SCSI */
#define	SVMOD_FIO_LAN	0x72	/* hp700 Core LAN */
#define	SVMOD_FIO_FDDI	0x7d	/* hp700 Core FDDI (unsupported) */
#define	SVMOD_FIO_HIL	0x73	/* hp700 Core HIL */
#define	SVMOD_FIO_CENT	0x74	/* hp700 Core Centronics */
#define	SVMOD_FIO_RS232	0x75	/* hp700 Core RS-232 */
#define	SVMOD_FIO_SGC	0x77	/* hp700 SGC Graphics */
#define	SVMOD_FIO_A1	0x7a	/* hp700 Core audio (type 1) */
#define	SVMOD_FIO_A1NB	0x7e	/* hp700 Core audio (type 1, no beeper) */
#define	SVMOD_FIO_A2	0x7f	/* hp700 Core audio (type 2) */
#define	SVMOD_FIO_A2NB	0x7b	/* hp700 Core audio (type 2, no beeper) */
#define	SVMOD_FIO_HPIB	0x80	/* hp700 Core HPIB (unsupported) */

#define	SVMOD_FIO_GSCSI	0x82	/* hp712 Core SCSI */
#define	SVMOD_FIO_GPCFD	0x83	/* hp712 PC floppy disk */
#define	SVMOD_FIO_GPCIO	0x84	/* hp712 PC keyboard and mouse */
#define	SVMOD_FIO_GGRF	0x85	/* hp712 SGC Graphics */
#define	SVMOD_FIO_GLAN	0x8a	/* hp712 Core LAN */
#define	SVMOD_FIO_GRS232 0x8c	/* hp712 Core RS232 */
#endif	/* _IODC_ */
