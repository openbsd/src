/*	$OpenBSD: lk201var.h,v 1.7 2006/08/05 22:05:55 miod Exp $	*/
/* $NetBSD: lk201var.h,v 1.2 1998/10/22 17:55:20 drochner Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
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

struct lk201_attachment {
	int (*sendchar)(void *, int);
	void *cookie;
};

struct lk201_state {
	struct	device *device;
	struct	lk201_attachment attmt;

	struct timeout probetmo;
	volatile int waitack;
	int	ackdata;

	int	kbdtype;
#define	KBD_NONE	0x00
#define	KBD_LK201	0x01
#define	KBD_LK401	0x02

#define LK_KLL 8
	int	down_keys_list[LK_KLL];

	int	bellvol;
	int	leds_state;
	int	kcvol;
};

void	lk201_bell(struct lk201_state *, struct wskbd_bell_data *);
int	lk201_decode(struct lk201_state *, int, int, int, u_int *, int *);
int	lk201_get_leds(struct lk201_state *);
int	lk201_get_type(struct lk201_state *);
void	lk201_init(struct lk201_state *);
void	lk201_set_keyclick(struct lk201_state *, int);
void	lk201_set_leds(struct lk201_state *, int);

/* Values returned by lk201_decode */
#define	LKD_NODATA	0x00
#define	LKD_COMPLETE	0x01
#define	LKD_MORE	0x02
