/*	$OpenBSD: conf.h,v 1.8 1998/12/29 09:33:36 deraadt Exp $	*/
/*	$NetBSD: conf.h,v 1.8 1996/12/31 07:12:43 mrg Exp $	*/

/*
 * Copyright (c) 1996 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
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
 */

#define mmread mmrw
#define mmwrite mmrw
cdev_decl(mm);

/* open, close, ioctl */
#define	cdev_openprom_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) nullop, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }

cdev_decl(openprom);

cdev_decl(zs);

bdev_decl(fd);
cdev_decl(fd);

cdev_decl(fb);

/* open, close, read, write, ioctl, select */
#define	cdev_gen_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	0, dev_init(c,n,select), (dev_type_mmap((*))) enodev }

cdev_decl(ms);

cdev_decl(kbd);

cdev_decl(bwtwo);

cdev_decl(cgtwo);

cdev_decl(cgthree);

cdev_decl(cgfour);

cdev_decl(cgsix);

cdev_decl(cgeight);

cdev_decl(tcx);

cdev_decl(cgfourteen);

cdev_decl(ipl);
#ifdef IPFILTER
#define NIPF 1
#else
#define NIPF 0
#endif

bdev_decl(xd);
cdev_decl(xd);

bdev_decl(xy);
cdev_decl(xy);

bdev_decl(sw);
cdev_decl(sw);

bdev_decl(rd);
cdev_decl(rd);

/* open, close, write, ioctl */
#define	cdev_bpp_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	0, seltrue, (dev_type_mmap((*))) enodev }

cdev_decl(bpp);

cdev_decl(mtty);
cdev_decl(mbpp);

cdev_decl(ksyms);
