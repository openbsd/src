/*	$OpenBSD: conf.h,v 1.8 2000/03/02 23:01:45 todd Exp $	*/

/*-
 * Copyright (c) 1996 Kenneth Stailey.  All rights reserved.
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
 *	This product includes software developed by Kenneth Stailey.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 */

bdev_decl(rd);
cdev_decl(rd);

/* swap device (required) */
bdev_decl(sw);
cdev_decl(sw);

bdev_decl(xd);
cdev_decl(xd);

#define	NXT 0	/* XXX */
bdev_decl(xt);
cdev_decl(xt);

bdev_decl(xy);
cdev_decl(xy);

/*
 * Devices that have only CHR nodes are declared below.
 */

cdev_decl(fd);

dev_decl(filedesc,open);

cdev_decl(idprom);

#define	mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);

cdev_decl(zs);
cdev_decl(ms);
cdev_decl(kbd);
cdev_decl(kd);

/* frame-buffer devices */
cdev_decl(fb);
#include "bwtwo.h"
cdev_decl(bw2);
#include "cgtwo.h"
cdev_decl(cg2);
#include "cgfour.h"
cdev_decl(cg4);

/* IP packet filtering */
/* open, close, read, ioctl */
cdev_decl(ipl);
#ifdef IPFILTER
#define NIPF 1
#else
#define NIPF 0
#endif

cdev_decl(ksyms);
