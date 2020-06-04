/*	$OpenBSD: conf.c,v 1.8 2020/05/26 14:00:42 deraadt Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stand.h>

const char version[] = "1.12";

extern void	nullsys();
extern int	nodev();
extern int	noioctl();

int	diostrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int	dioopen(struct open_file *, ...);
int	dioclose(struct open_file *);
#define	dioioctl	noioctl

int	netstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int	netopen(struct open_file *, ...);
int	netclose(struct open_file *);
#define	netioctl	noioctl

struct devsw devsw[] = {
	{ "scsi",	diostrategy, dioopen, dioclose,	dioioctl },
	{ "bootp",	netstrategy, netopen, netclose,	netioctl }
};

int	ndevs = (sizeof(devsw)/sizeof(devsw[0]));
