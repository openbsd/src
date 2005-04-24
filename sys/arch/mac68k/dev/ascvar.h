/*	$OpenBSD: ascvar.h,v 1.7 2005/04/24 22:25:13 martin Exp $	*/
/*	$NetBSD: ascvar.h,v 1.3 1997/02/24 05:47:34 scottr Exp $	*/

/*
 * Copyright (C) 1997 Scott Reynolds.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#define ASCUNIT(d)	((d) & 0x7)

struct asc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_tag;
	bus_space_handle_t	sc_handle;
	int			sc_open;
	int			sc_ringing;
	struct timeout		sc_bell_tmo;
};

int	ascopen(dev_t dev, int flag, int mode, struct proc *p);
int	ascclose(dev_t dev, int flag, int mode, struct proc *p);
int	ascread(dev_t, struct uio *, int);
int	ascwrite(dev_t, struct uio *, int);
int	ascioctl(dev_t, int, caddr_t, int, struct proc *p);
int	ascpoll(dev_t dev, int rw, struct proc *p);
paddr_t	ascmmap(dev_t dev, off_t off, int prot);
