/*	$OpenBSD: hilvar.h,v 1.2 2003/02/15 23:38:46 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: hilvar.h 1.3 92/01/21$
 *
 *	@(#)hilvar.h	8.1 (Berkeley) 6/10/93
 */

#define NHILD		8		/* 7 actual + loop pseudo (dev 0) */

struct hil_cb {
	void (*cb_fn)(void *, u_int, u_int8_t *);
	void *cb_arg;
};

struct hil_softc {
	struct device	sc_dev;
	bus_space_handle_t sc_bsh;
	bus_space_tag_t	sc_bst;

	int		sc_cmddone;
	int		sc_cmdending;
	u_int		sc_actdev;	/* current input device */
	u_int		sc_cmddev;	/* device to perform command on */
	u_int8_t	sc_pollbuf[HILBUFSIZE];	/* interrupt time input buf */
	u_int8_t	sc_cmdbuf[HILBUFSIZE];
	u_int8_t	*sc_pollbp;	/* pointer into sc_pollbuf */
	u_int8_t	*sc_cmdbp;	/* pointer into sc_cmdbuf */

	u_int		sc_maxdev;	/* number of devices on loop */
	u_int		sc_kbddev;		/* keyboard device id */
	struct hil_cb	sc_cb[NHILD];	/* interrupt dispatcher */
};

#ifdef _KERNEL

void	send_hil_cmd(struct hil_softc *, u_int, u_int8_t *, u_int, u_int8_t *);
void	send_hildev_cmd(struct hil_softc *, u_int, u_int, u_int8_t *, u_int *);
void	hil_set_poll(struct hil_softc *, int);
int	hil_poll_data(struct hil_softc *, u_int8_t *, u_int8_t *);

void	hil_attach(struct hil_softc *);
void	hil_attach_deferred(void *);
void	hil_callback_register(struct hil_softc *, u_int,
    void (*)(void *, u_int, u_int8_t *), void *);
int	hil_intr(void *);
int	hildevprint(void *, const char *);

#endif /* _KERNEL */
