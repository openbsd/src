/*	$OpenBSD: radio_if.h,v 1.1 2001/10/04 19:17:59 gluk Exp $	*/
/* $RuOBSD: radio_if.h,v 1.5 2001/09/29 20:33:02 gluk Exp $ */

/*
 * Copyright (c) 2001 Maxim Tsyplakov <tm@oganer.net>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_DEV_RADIO_IF_H
#define _SYS_DEV_RADIO_IF_H

/*
 * Generic interface to hardware driver
 */

#define RADIOUNIT(x)	(minor(x))

struct radio_hw_if {
	/* open hardware */
	int	(*open)(dev_t, int, int, struct proc *);	

	/* close hardware */
	int	(*close)(dev_t, int, int, struct proc *);

	/* ioctl hardware */
	int	(*ioctl)(dev_t, u_long, caddr_t, int, struct proc*);
};

struct radio_attach_args {
	struct radio_hw_if *hwif;
	void    *hdl;
};

struct device  *radio_attach_mi(struct radio_hw_if *, void *, struct device *);

#endif /* _SYS_DEV_RADIO_IF_H */
