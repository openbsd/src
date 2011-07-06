/*	$OpenBSD: vaxstand.h,v 1.3 2011/07/06 18:32:59 miod Exp $ */
/*	$NetBSD: vaxstand.h,v 1.5 2000/06/15 19:53:23 ragge Exp $ */
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

/* Variables used in autoconf */
extern int askname;
extern struct rpb bootrpb;
extern int csrbase, nexaddr;

/* devsw type definitions, used in bootxx and conf */
#define SADEV(name,strategy,open,close,ioctl) \
	{ (char *)name, \
	 (int(*)(void *, int, daddr32_t, size_t, void *, size_t *))strategy, \
	 (int(*)(struct open_file *, ...))open, \
	 (int(*)(struct open_file *))close, \
	 (int(*)(struct open_file *,u_long, void *))ioctl}

#define SDELAY(count) {volatile int i; for (i = count; i; i--);}
/*
 * Easy-to-use definitions
 */
#ifndef min
#define min(x,y) (x < y ? x : y)
#endif /* min */

struct netif_driver;

char *index(char *, int);
int net_devinit(struct open_file *f, struct netif_driver *drv, u_char *eaddr);

/* device calls */
int	raopen(struct open_file *, int, int, int, int),
	    rastrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int	romopen(struct open_file *, int, int, int, int),
	    romstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int	mfmopen(struct open_file *, int, int, int, int),
	    mfmstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int	sdopen(struct open_file *),
	    sdstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int	leopen(struct open_file *, int, int, int, int),
	    leclose(struct open_file *);
int	qeopen(struct open_file *, int, int, int, int),
	    qeclose(struct open_file *);
int	zeopen(struct open_file *, int, int, int, int),
	    zeclose(struct open_file *);
int	deopen(struct open_file *, int, int, int, int),
	    declose(struct open_file *);
int	netopen(struct open_file *), netclose(struct open_file *);

