/*	$OpenBSD: wsmuxvar.h,v 1.5 2002/03/14 01:27:03 millert Exp $	*/
/*	$NetBSD: wsmuxvar.h,v 1.1 1999/07/29 18:20:43 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct wsdisplay_softc;
struct wsplink;

struct wsmux_softc {
	struct device sc_dv;
	struct wseventvar sc_events;	/* event queue state */
	int sc_flags, sc_mode;		/* open flags */
	struct proc *sc_p;		/* open proc */
	LIST_HEAD(, wsplink) sc_reals;  /* list of real devices */
	struct wsmux_softc *sc_mux;     /* if part of another mux */
	struct device *sc_displaydv;    /* our display if part of one */
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int sc_rawkbd;		        /* A hack to remember the kbd mode */
#endif
};

struct wsmuxops {
	int (*dopen)(dev_t, int, int, struct proc *);
	int (*dclose)(struct device *, int, int, struct proc *);
	int (*dioctl)(struct device *, u_long, caddr_t, int, 
			   struct proc *);
	int (*ddispioctl)(struct device *, u_long, caddr_t, int, 
			       struct proc *);
	int (*dsetdisplay)(struct device *, struct wsmux_softc *);
	int (*dissetdisplay)(struct device *);
};


/*
 * configure defines
 */
#define	WSKBDDEVCF_MUX_DEFAULT		-1
#define WSMOUSEDEVCF_MUX		0
#define WSMOUSEDEVCF_MUX_DEFAULT	-1

struct wsmux_softc *wsmux_create(const char *name, int no);
int	wsmux_attach_sc(
	  struct wsmux_softc *,
	  int, struct device *, struct wseventvar *,
	  struct wsmux_softc **,
	  struct wsmuxops *);
int	wsmux_detach_sc(struct wsmux_softc *, struct device *);
void	wsmux_attach(
	  int, int, struct device *, struct wseventvar *,
	  struct wsmux_softc **,
	  struct wsmuxops *);
void	wsmux_detach(int, struct device *);

int	wsmux_displayioctl(struct device *dev, u_long cmd,
	    caddr_t data, int flag, struct proc *p);

int	wsmuxdoioctl(struct device *, u_long, caddr_t,int,struct proc *);

int	wsmux_add_mux(int, struct wsmux_softc *);
int	wsmux_rem_mux(int, struct wsmux_softc *);
int	wskbd_add_mux(int, struct wsmux_softc *);
int	wskbd_rem_mux(int, struct wsmux_softc *);
int	wsmouse_add_mux(int, struct wsmux_softc *);
int	wsmouse_rem_mux(int, struct wsmux_softc *);
