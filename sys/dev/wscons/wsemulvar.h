/* $OpenBSD: wsemulvar.h,v 1.11 2009/09/05 14:30:24 miod Exp $ */
/* $NetBSD: wsemulvar.h,v 1.6 1999/01/17 15:46:15 drochner Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

struct device;
struct wsdisplay_emulops;

enum wsemul_resetops {
	WSEMUL_RESET,
	WSEMUL_SYNCFONT,
	WSEMUL_CLEARSCREEN,
	WSEMUL_CLEARCURSOR
};

struct wsemul_ops {
	char name[WSEMUL_NAME_SIZE];

	void	*(*cnattach)(const struct wsscreen_descr *, void *,
				  int, int, long);
	void	*(*attach)(int, const struct wsscreen_descr *, void *,
				int, int, void *, long);
	u_int	(*output)(void *, const u_char *, u_int, int);
	int	(*translate)(void *, keysym_t, const char **);
	void	(*detach)(void *, u_int *, u_int *);
	void    (*reset)(void *, enum wsemul_resetops);
};

extern const struct wsemul_ops wsemul_dumb_ops;
extern const struct wsemul_ops wsemul_sun_ops;
extern const struct wsemul_ops wsemul_vt100_ops;

const struct wsemul_ops *wsemul_pick(const char *);

/*
 * Callbacks from the emulation code to the display interface driver.
 */
void	wsdisplay_emulbell(void *v);
void	wsdisplay_emulinput(void *v, const u_char *, u_int);
