/*	$NetBSD: conf.h,v 1.2 1996/04/14 00:56:59 jonathan Exp $	*/


/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * This file contributed by Jonathan Stone.
 */

#define mmread mmrw
#define mmwrite mmrw
cdev_decl(mm);


cdev_decl(scc);		/* pmax (also alpha) m-d z8530 SCC */
cdev_decl(dc);		/* dc7085 dz11-on-a-chip */

bdev_decl(rz);		/* antique 4.4bsd/pmax SCSI disk */
cdev_decl(rz);

bdev_decl(tz);		/* antique 4.4bsd/pmax SCSI tape driver */
cdev_decl(tz);

cdev_decl(dtop);	/* Personal Decstation (MAXINE) desktop bus */
cdev_decl(fb);		/* generic framebuffer pseudo-device */
cdev_decl(rcons);	/* framebuffer-based raster console pseudo-device */

/* TTTTT - stuff from NetBSD mips conf.h */
cdev_decl(pms);

bdev_decl(fd);
cdev_decl(fd);
/* TTTTT - end of stuff from NetBSD mips conf.h */
