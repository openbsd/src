/* $OpenBSD: wsemulvar.h,v 1.5 2002/01/03 21:58:59 jason Exp $ */
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
	WSEMUL_CLEARSCREEN
};

struct wsemul_ops {
	char name[WSEMUL_NAME_SIZE];

	void	*(*cnattach) __P((const struct wsscreen_descr *, void *,
				  int, int, long));
	void	*(*attach) __P((int console, const struct wsscreen_descr *, void *,
				int, int, void *, long));
	void	(*output) __P((void *cookie, const u_char *data, u_int count,
			       int));
	int	(*translate) __P((void *, keysym_t, char **));
	void	(*detach) __P((void *cookie, u_int *crow, u_int *ccol));
	void    (*reset) __P((void *, enum wsemul_resetops));
};

#ifdef WSEMUL_DUMB
extern const struct wsemul_ops wsemul_dumb_ops;
#endif
#if defined(WSEMUL_SUN) || NWSEMUL_SUN > 0
extern const struct wsemul_ops wsemul_sun_ops;
#endif
#ifndef WSEMUL_NO_VT100
extern const struct wsemul_ops wsemul_vt100_ops;
#endif

const struct wsemul_ops *wsemul_pick __P((const char *));

/* 
 * Callbacks from the emulation code to the display interface driver.
 */     
void	wsdisplay_emulbell __P((void *v));
void	wsdisplay_emulinput __P((void *v, const u_char *, u_int));
