/*	$OpenBSD: conf.h,v 1.4 2022/06/28 14:43:50 visa Exp $	*/
/*	$NetBSD: conf.h,v 1.2 1996/05/05 19:28:34 christos Exp $	*/

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

#ifndef _MACHINE_CONF_H_
#define _MACHINE_CONF_H_

#include <sys/conf.h>

#define	mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);

/* open, close, ioctl */
#define cdev_openprom_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) nullop, 0, \
	(dev_type_mmap((*))) enodev }

cdev_decl(openprom);

/* open, close, write, ioctl, kqueue */
#define cdev_acpiapm_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, \
	(dev_type_mmap((*))) enodev, 0, 0, dev_init(c,n,kqfilter) }

cdev_decl(apm);
cdev_decl(acpiapm);

/*
 * These numbers have to be in sync with bdevsw/cdevsw.
 */

#define BMAJ_WD		0
#define BMAJ_SW		1
#define BMAJ_SD		4
#define BMAJ_ST		5

#define CMAJ_MM		2
#define CMAJ_PTS	5
#define CMAJ_PTC	6
#define CMAJ_COM	8
#define CMAJ_WSDISPLAY	12
#define CMAJ_ST		14
#define CMAJ_LPT	16
#define CMAJ_CH		17
#define CMAJ_UK		20
#define CMAJ_BPF	23
#define CMAJ_TUN	40
#define CMAJ_AUDIO	42
#define CMAJ_VIDEO	44
#define CMAJ_BKTR	49
#define CMAJ_MIDI	52
#define CMAJ_USB	61
#define CMAJ_UHID	62
#define CMAJ_UGEN	63
#define CMAJ_ULPT	64
#define CMAJ_UCOM	66
#define CMAJ_WSKBD	67
#define CMAJ_WSMOUSE	68
#ifdef USER_PCICONF
#define CMAJ_PCI	72
#endif
#define CMAJ_RADIO	76
#define CMAJ_DRM	87
#define CMAJ_GPIO	88
#define CMAJ_VSCSI	89

#endif	/* _MACHINE_CONF_H_ */
