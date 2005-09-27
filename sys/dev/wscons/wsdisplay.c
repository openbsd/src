/* $OpenBSD: wsdisplay.c,v 1.64 2005/09/27 21:45:20 miod Exp $ */
/* $NetBSD: wsdisplay.c,v 1.82 2005/02/27 00:27:52 perry Exp $ */

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

#ifndef	SMALL_KERNEL
#define WSMOUSED_SUPPORT
#define	BURNER_SUPPORT
#define	SCROLLBACK_SUPPORT
#endif

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/timeout.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsemulvar.h>
#include <dev/wscons/wscons_callbacks.h>
#include <dev/cons.h>

#include <dev/ic/pcdisplay.h>

#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"
#include "wsmux.h"

#if NWSKBD > 0
#include <dev/wscons/wseventvar.h>
#include <dev/wscons/wsmuxvar.h>
#endif

#if NWSMOUSE > 0
#include <dev/wscons/wsmousevar.h>
#endif

#include "wsmoused.h"

#if NWSMOUSE > 0
extern struct cfdriver wsmouse_cd;
#endif /* NWSMOUSE > 0 */

struct wsscreen_internal {
	const struct wsdisplay_emulops *emulops;
	void	*emulcookie;

	const struct wsscreen_descr *scrdata;

	const struct wsemul_ops *wsemul;
	void	*wsemulcookie;
};

struct wsscreen {
	struct wsscreen_internal *scr_dconf;

	struct tty *scr_tty;
	int	scr_hold_screen;		/* hold tty output */

	int scr_flags;
#define SCR_OPEN 1		/* is it open? */
#define SCR_WAITACTIVE 2	/* someone waiting on activation */
#define SCR_GRAPHICS 4		/* graphics mode, no text (emulation) output */
#define	SCR_DUMBFB 8		/* in use as dumb fb (iff SCR_GRAPHICS) */
	const struct wscons_syncops *scr_syncops;
	void *scr_synccookie;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	int scr_rawkbd;
#endif

	struct wsdisplay_softc *sc;

#ifdef WSMOUSED_SUPPORT
	/* mouse console support via wsmoused(8) */
	unsigned short mouse;		/* mouse cursor position */
	unsigned short cursor;		/* selection cursor position (if
					different from mouse cursor pos) */
	unsigned short cpy_start;	/* position of the copy start mark*/
	unsigned short cpy_end;		/* position of the copy end mark */
	unsigned short orig_start;	/* position of the original sel. start*/
	unsigned short orig_end;	/* position of the original sel. end */
#define MOUSE_VISIBLE	(1 << 0)	/* flag, the mouse cursor is visible */
#define SEL_EXISTS	(1 << 1)	/* flag, a selection exists */
#define SEL_IN_PROGRESS (1 << 2)	/* flag, a selection is in progress */
#define SEL_EXT_AFTER	(1 << 3)	/* flag, selection is extended after */
#define BLANK_TO_EOL	(1 << 4)	/* flag, there are only blanks
					   characters to eol */
#define SEL_BY_CHAR	(1 << 5)	/* flag, select character by character*/
#define SEL_BY_WORD	(1 << 6)	/* flag, select word by word */
#define SEL_BY_LINE	(1 << 7)	/* flag, select line by line */

#define IS_MOUSE_VISIBLE(ws) ((ws)->mouse_flags & MOUSE_VISIBLE)
#define IS_SEL_EXISTS(ws) ((ws)->mouse_flags & SEL_EXISTS)
#define IS_SEL_IN_PROGRESS(ws) ((ws)->mouse_flags & SEL_IN_PROGRESS)
#define IS_SEL_EXT_AFTER(ws) ((ws)->mouse_flags & SEL_EXT_AFTER)
#define IS_BLANK_TO_EOL(ws) ((ws)->mouse_flags & BLANK_TO_EOL)
#define IS_SEL_BY_CHAR(ws) ((ws)->mouse_flags & SEL_BY_CHAR)
#define IS_SEL_BY_WORD(ws) ((ws)->mouse_flags & SEL_BY_WORD)
#define IS_SEL_BY_LINE(ws) ((ws)->mouse_flags & SEL_BY_LINE)
	unsigned char mouse_flags;	/* flags, status of the mouse */
#endif	/* WSMOUSED_SUPPORT */
};

struct wsscreen *wsscreen_attach(struct wsdisplay_softc *, int, const char *,
	    const struct wsscreen_descr *, void *, int, int, long);
void	wsscreen_detach(struct wsscreen *);
int	wsdisplay_addscreen(struct wsdisplay_softc *, int, const char *,
	    const char *);
int	wsdisplay_getscreen(struct wsdisplay_softc *,
	    struct wsdisplay_addscreendata *);
void	wsdisplay_shutdownhook(void *);
void	wsdisplay_addscreen_print(struct wsdisplay_softc *, int, int);
void	wsdisplay_closescreen(struct wsdisplay_softc *, struct wsscreen *);
int	wsdisplay_delscreen(struct wsdisplay_softc *, int, int);
void	wsdisplay_burner(void *v);

struct wsdisplay_softc {
	struct device sc_dv;

	const struct wsdisplay_accessops *sc_accessops;
	void	*sc_accesscookie;

	const struct wsscreen_list *sc_scrdata;

	struct wsscreen *sc_scr[WSDISPLAY_MAXSCREEN];
	int sc_focusidx;	/* available only if sc_focus isn't null */
	struct wsscreen *sc_focus;

#ifdef BURNER_SUPPORT
	struct timeout sc_burner;
	int	sc_burnoutintvl;
	int	sc_burninintvl;
	int	sc_burnout;
	int	sc_burnman;
	int	sc_burnflags;
#endif

	struct wsdisplay_font sc_fonts[WSDISPLAY_MAXFONT];

	int	sc_isconsole;

	int sc_flags;
#define SC_SWITCHPENDING 1
	int sc_screenwanted, sc_oldscreen; /* valid with SC_SWITCHPENDING */

#if NWSKBD > 0
	struct wsevsrc *sc_input;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int sc_rawkbd;
#endif
#endif /* NWSKBD > 0 */

#ifdef WSMOUSED_SUPPORT
	dev_t wsmoused_dev; /* device opened by wsmoused(8), when active */
	int wsmoused_sleep; /* true when wsmoused(8) is sleeping */
#endif
};

extern struct cfdriver wsdisplay_cd;

/* Autoconfiguration definitions. */
int	wsdisplay_emul_match(struct device *, void *, void *);
void	wsdisplay_emul_attach(struct device *, struct device *, void *);

struct cfdriver wsdisplay_cd = {
	NULL, "wsdisplay", DV_TTY
};

struct cfattach wsdisplay_emul_ca = {
	sizeof(struct wsdisplay_softc), wsdisplay_emul_match,
	    wsdisplay_emul_attach,
};

void	wsdisplaystart(struct tty *);
int	wsdisplayparam(struct tty *, struct termios *);

/* Internal macros, functions, and variables. */
#define	WSDISPLAYUNIT(dev)		(minor(dev) >> 8)
#define	WSDISPLAYSCREEN(dev)		(minor(dev) & 0xff)
#define ISWSDISPLAYCTL(dev)		(WSDISPLAYSCREEN(dev) == 255)
#define WSDISPLAYMINOR(unit, screen)	(((unit) << 8) | (screen))

#define	WSSCREEN_HAS_TTY(scr)		((scr)->scr_tty != NULL)

void	wsdisplay_common_attach(struct wsdisplay_softc *sc,
	    int console, int mux, const struct wsscreen_list *,
	    const struct wsdisplay_accessops *accessops,
	    void *accesscookie);

#ifdef WSDISPLAY_COMPAT_RAWKBD
int	wsdisplay_update_rawkbd(struct wsdisplay_softc *, struct wsscreen *);
#endif

int	wsdisplay_console_initted;
struct wsdisplay_softc *wsdisplay_console_device;
struct wsscreen_internal wsdisplay_console_conf;

int	wsdisplay_getc_dummy(dev_t);
void	wsdisplay_pollc(dev_t, int);

int	wsdisplay_cons_pollmode;
void	(*wsdisplay_cons_kbd_pollc)(dev_t, int);

struct consdev wsdisplay_cons = {
	NULL, NULL, wsdisplay_getc_dummy, wsdisplay_cnputc,
	    wsdisplay_pollc, NULL, NODEV, CN_NORMAL
};

#ifndef WSDISPLAY_DEFAULTSCREENS
#define WSDISPLAY_DEFAULTSCREENS	1
#endif
int	wsdisplay_defaultscreens = WSDISPLAY_DEFAULTSCREENS;

int	wsdisplay_switch1(void *, int, int);
int	wsdisplay_switch2(void *, int, int);
int	wsdisplay_switch3(void *, int, int);

int	wsdisplay_clearonclose;

#ifdef WSMOUSED_SUPPORT
char *Copybuffer;
u_int Copybuffer_size;
char Paste_avail;
#endif

struct wsscreen *
wsscreen_attach(struct wsdisplay_softc *sc, int console, const char *emul,
    const struct wsscreen_descr *type, void *cookie, int ccol, int crow,
    long defattr)
{
	struct wsscreen_internal *dconf;
	struct wsscreen *scr;

	scr = malloc(sizeof(struct wsscreen), M_DEVBUF, M_NOWAIT);
	if (!scr)
		return (NULL);

	if (console) {
		dconf = &wsdisplay_console_conf;
		/*
		 * Tell the emulation about the callback argument.
		 * The other stuff is already there.
		 */
		(*dconf->wsemul->attach)(1, 0, 0, 0, 0, scr, 0);
	} else { /* not console */
		dconf = malloc(sizeof(struct wsscreen_internal),
		    M_DEVBUF, M_NOWAIT);
		if (dconf == NULL) {
			free(scr, M_DEVBUF);
			return (NULL);
		}
		dconf->emulops = type->textops;
		dconf->emulcookie = cookie;
		if (dconf->emulops != NULL &&
		    (dconf->wsemul = wsemul_pick(emul)) != NULL) {
			dconf->wsemulcookie =
			    (*dconf->wsemul->attach)(0, type, cookie,
				ccol, crow, scr, defattr);
		} else {
			free(dconf, M_DEVBUF);
			free(scr, M_DEVBUF);
			return (NULL);
		}
		dconf->scrdata = type;
	}

	scr->scr_dconf = dconf;

	scr->scr_tty = ttymalloc();
	scr->scr_hold_screen = 0;
	scr->scr_flags = 0;

	scr->scr_syncops = 0;
	scr->sc = sc;
#ifdef WSMOUSED_SUPPORT
	scr->mouse_flags = 0;
#endif
#ifdef WSDISPLAY_COMPAT_RAWKBD
	scr->scr_rawkbd = 0;
#endif
	return (scr);
}

void
wsscreen_detach(struct wsscreen *scr)
{
	int ccol, crow; /* XXX */

	if (WSSCREEN_HAS_TTY(scr)) {
		timeout_del(&scr->scr_tty->t_rstrt_to);
		ttyfree(scr->scr_tty);
	}
	(*scr->scr_dconf->wsemul->detach)(scr->scr_dconf->wsemulcookie,
	    &ccol, &crow);
	free(scr->scr_dconf, M_DEVBUF);
	free(scr, M_DEVBUF);
}

const struct wsscreen_descr *
wsdisplay_screentype_pick(const struct wsscreen_list *scrdata, const char *name)
{
	int i;
	const struct wsscreen_descr *scr;

	KASSERT(scrdata->nscreens > 0);

	if (name == NULL || *name == '\0')
		return (scrdata->screens[0]);

	for (i = 0; i < scrdata->nscreens; i++) {
		scr = scrdata->screens[i];
		if (!strncmp(name, scr->name, WSSCREEN_NAME_SIZE))
			return (scr);
	}

	return (0);
}

/*
 * print info about attached screen
 */
void
wsdisplay_addscreen_print(struct wsdisplay_softc *sc, int idx, int count)
{
	printf("%s: screen %d", sc->sc_dv.dv_xname, idx);
	if (count > 1)
		printf("-%d", idx + (count-1));
	printf(" added (%s, %s emulation)\n",
	    sc->sc_scr[idx]->scr_dconf->scrdata->name,
	    sc->sc_scr[idx]->scr_dconf->wsemul->name);
}

int
wsdisplay_addscreen(struct wsdisplay_softc *sc, int idx,
    const char *screentype, const char *emul)
{
	const struct wsscreen_descr *scrdesc;
	int error;
	void *cookie;
	int ccol, crow;
	long defattr;
	struct wsscreen *scr;
	int s;

	if (idx < 0 || idx >= WSDISPLAY_MAXSCREEN)
		return (EINVAL);
	if (sc->sc_scr[idx] != NULL)
		return (EBUSY);

	scrdesc = wsdisplay_screentype_pick(sc->sc_scrdata, screentype);
	if (!scrdesc)
		return (ENXIO);
	error = (*sc->sc_accessops->alloc_screen)(sc->sc_accesscookie,
	    scrdesc, &cookie, &ccol, &crow, &defattr);
	if (error)
		return (error);

	scr = wsscreen_attach(sc, 0, emul, scrdesc,
	    cookie, ccol, crow, defattr);
	if (scr == NULL) {
		(*sc->sc_accessops->free_screen)(sc->sc_accesscookie, cookie);
		return (ENXIO);
	}

	sc->sc_scr[idx] = scr;

	/* if no screen has focus yet, activate the first we get */
	s = spltty();
	if (!sc->sc_focus) {
		(*sc->sc_accessops->show_screen)(sc->sc_accesscookie,
		    scr->scr_dconf->emulcookie, 0, 0, 0);
		sc->sc_focusidx = idx;
		sc->sc_focus = scr;
	}
	splx(s);

#ifdef WSMOUSED_SUPPORT
	allocate_copybuffer(sc); /* enlarge the copy buffer is necessary */
#endif
	return (0);
}

int
wsdisplay_getscreen(struct wsdisplay_softc *sc,
    struct wsdisplay_addscreendata *sd)
{
	struct wsscreen *scr;

	if (sd->idx < 0 && sc->sc_focus)
		sd->idx = sc->sc_focusidx;

	if (sd->idx < 0 || sd->idx >= WSDISPLAY_MAXSCREEN)
		return (EINVAL);

	scr = sc->sc_scr[sd->idx];
	if (scr == NULL)
		return (ENXIO);

	strncpy(sd->screentype, scr->scr_dconf->scrdata->name,
	    WSSCREEN_NAME_SIZE);
	strncpy(sd->emul, scr->scr_dconf->wsemul->name, WSEMUL_NAME_SIZE);

	return (0);
}

void
wsdisplay_closescreen(struct wsdisplay_softc *sc, struct wsscreen *scr)
{
	int maj, mn, idx;

	/* hangup */
	if (WSSCREEN_HAS_TTY(scr)) {
		struct tty *tp = scr->scr_tty;
		(*linesw[tp->t_line].l_modem)(tp, 0);
	}

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;
	/* locate the screen index */
	for (idx = 0; idx < WSDISPLAY_MAXSCREEN; idx++)
		if (scr == sc->sc_scr[idx])
			break;
#ifdef DIAGNOSTIC
	if (idx == WSDISPLAY_MAXSCREEN)
		panic("wsdisplay_forceclose: bad screen");
#endif

	/* nuke the vnodes */
	mn = WSDISPLAYMINOR(sc->sc_dv.dv_unit, idx);
	vdevgone(maj, mn, mn, VCHR);
}

int
wsdisplay_delscreen(struct wsdisplay_softc *sc, int idx, int flags)
{
	struct wsscreen *scr;
	int s;
	void *cookie;

	if (idx < 0 || idx >= WSDISPLAY_MAXSCREEN)
		return (EINVAL);
	if ((scr = sc->sc_scr[idx]) == NULL)
		return (ENXIO);

	if (scr->scr_dconf == &wsdisplay_console_conf ||
	    scr->scr_syncops ||
	    ((scr->scr_flags & SCR_OPEN) && !(flags & WSDISPLAY_DELSCR_FORCE)))
		return(EBUSY);

	wsdisplay_closescreen(sc, scr);

	/*
	 * delete pointers, so neither device entries
	 * nor keyboard input can reference it anymore
	 */
	s = spltty();
	if (sc->sc_focus == scr) {
		sc->sc_focus = 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
		wsdisplay_update_rawkbd(sc, 0);
#endif
	}
	sc->sc_scr[idx] = 0;
	splx(s);

	/*
	 * Wake up processes waiting for the screen to
	 * be activated. Sleepers must check whether
	 * the screen still exists.
	 */
	if (scr->scr_flags & SCR_WAITACTIVE)
		wakeup(scr);

	/* save a reference to the graphics screen */
	cookie = scr->scr_dconf->emulcookie;

	wsscreen_detach(scr);

	(*sc->sc_accessops->free_screen)(sc->sc_accesscookie, cookie);

	printf("%s: screen %d deleted\n", sc->sc_dv.dv_xname, idx);
	return (0);
}

/*
 * Autoconfiguration functions.
 */
int
wsdisplay_emul_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct wsemuldisplaydev_attach_args *ap = aux;

	if (cf->wsemuldisplaydevcf_console != WSEMULDISPLAYDEVCF_CONSOLE_UNK) {
		/*
		 * If console-ness of device specified, either match
		 * exactly (at high priority), or fail.
		 */
		if (cf->wsemuldisplaydevcf_console != 0 && ap->console != 0)
			return (10);
		else
			return (0);
	}

	/* If console-ness unspecified, it wins. */
	return (1);
}

void
wsdisplay_emul_attach(struct device *parent, struct device *self, void *aux)
{
	struct wsdisplay_softc *sc = (struct wsdisplay_softc *)self;
	struct wsemuldisplaydev_attach_args *ap = aux;

	wsdisplay_common_attach(sc, ap->console,
	    sc->sc_dv.dv_cfdata->wsemuldisplaydevcf_mux, ap->scrdata,
	    ap->accessops, ap->accesscookie);

	if (ap->console && cn_tab == &wsdisplay_cons) {
		int maj;

		/* locate the major number */
		for (maj = 0; maj < nchrdev; maj++)
			if (cdevsw[maj].d_open == wsdisplayopen)
				break;

		cn_tab->cn_dev = makedev(maj, WSDISPLAYMINOR(self->dv_unit, 0));
	}
}

/* Print function (for parent devices). */
int
wsemuldisplaydevprint(void *aux, const char *pnp)
{
#if 0 /* -Wunused */
	struct wsemuldisplaydev_attach_args *ap = aux;
#endif

	if (pnp)
		printf("wsdisplay at %s", pnp);
#if 0 /* don't bother; it's ugly */
	printf(" console %d", ap->console);
#endif

	return (UNCONF);
}

void
wsdisplay_common_attach(struct wsdisplay_softc *sc, int console, int kbdmux,
    const struct wsscreen_list *scrdata,
    const struct wsdisplay_accessops *accessops, void *accesscookie)
{
	static int hookset = 0;
	int i, start = 0;
#if NWSKBD > 0
	struct wsevsrc *kme;
#if NWSMUX > 0
	struct wsmux_softc *mux;

	if (kbdmux >= 0)
		mux = wsmux_getmux(kbdmux);
	else
		mux = wsmux_create("dmux", sc->sc_dv.dv_unit);
	/* XXX panic()ing isn't nice, but attach cannot fail */
	if (mux == NULL)
		panic("wsdisplay_common_attach: no memory");
	sc->sc_input = &mux->sc_base;
	mux->sc_displaydv = &sc->sc_dv;
	if (kbdmux >= 0)
		printf(" mux %d", kbdmux);
#else
#if 0	/* not worth keeping, especially since the default value is not -1... */
	if (kbdmux >= 0)
		printf(" (mux ignored)");
#endif
#endif	/* NWSMUX > 0 */
#endif	/* NWSKBD > 0 */

	sc->sc_isconsole = console;

	if (console) {
		KASSERT(wsdisplay_console_initted);
		KASSERT(wsdisplay_console_device == NULL);

		sc->sc_scr[0] = wsscreen_attach(sc, 1, 0, 0, 0, 0, 0, 0);
		if (sc->sc_scr[0] == NULL)
			return;
		wsdisplay_console_device = sc;

		printf(": console (%s, %s emulation)",
		       wsdisplay_console_conf.scrdata->name,
		       wsdisplay_console_conf.wsemul->name);

#if NWSKBD > 0
		kme = wskbd_set_console_display(&sc->sc_dv, sc->sc_input);
		if (kme != NULL)
			printf(", using %s", kme->me_dv.dv_xname);
#if NWSMUX == 0
		sc->sc_input = kme;
#endif
#endif

		sc->sc_focusidx = 0;
		sc->sc_focus = sc->sc_scr[0];
		start = 1;
	}
	printf("\n");

#if NWSKBD > 0 && NWSMUX > 0
	wsmux_set_display(mux, &sc->sc_dv);
#endif

	sc->sc_accessops = accessops;
	sc->sc_accesscookie = accesscookie;
	sc->sc_scrdata = scrdata;

	/*
	 * Set up a number of virtual screens if wanted. The
	 * WSDISPLAYIO_ADDSCREEN ioctl is more flexible, so this code
	 * is for special cases like installation kernels.
	 */
	for (i = start; i < wsdisplay_defaultscreens; i++) {
		if (wsdisplay_addscreen(sc, i, 0, 0))
			break;
	}

	if (i > start)
		wsdisplay_addscreen_print(sc, start, i-start);

#ifdef BURNER_SUPPORT
	sc->sc_burnoutintvl = (hz * WSDISPLAY_DEFBURNOUT) / 1000;
	sc->sc_burninintvl = (hz * WSDISPLAY_DEFBURNIN ) / 1000;
	sc->sc_burnflags = 0;	/* off by default */
	timeout_set(&sc->sc_burner, wsdisplay_burner, sc);
	sc->sc_burnout = sc->sc_burnoutintvl;
	wsdisplay_burn(sc, sc->sc_burnflags);
#endif

	if (hookset == 0)
		shutdownhook_establish(wsdisplay_shutdownhook, NULL);
	hookset = 1;
}

void
wsdisplay_cnattach(const struct wsscreen_descr *type, void *cookie, int ccol,
    int crow, long defattr)
{
	const struct wsemul_ops *wsemul;

	KASSERT(!wsdisplay_console_initted);
	KASSERT(type->nrows > 0);
	KASSERT(type->ncols > 0);
	KASSERT(crow < type->nrows);
	KASSERT(ccol < type->ncols);

	wsdisplay_console_conf.emulops = type->textops;
	wsdisplay_console_conf.emulcookie = cookie;
	wsdisplay_console_conf.scrdata = type;

	wsemul = wsemul_pick(""); /* default */
	wsdisplay_console_conf.wsemul = wsemul;
	wsdisplay_console_conf.wsemulcookie =
	    (*wsemul->cnattach)(type, cookie, ccol, crow, defattr);

	cn_tab = &wsdisplay_cons;

	wsdisplay_console_initted = 1;
}

/*
 * Tty and cdevsw functions.
 */
int
wsdisplayopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	int unit, newopen, error;
	struct wsscreen *scr;

	unit = WSDISPLAYUNIT(dev);
	if (unit >= wsdisplay_cd.cd_ndevs ||	/* make sure it was attached */
	    (sc = wsdisplay_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if (WSDISPLAYSCREEN(dev) >= WSDISPLAY_MAXSCREEN)
		return (ENXIO);
	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (WSSCREEN_HAS_TTY(scr)) {
		tp = scr->scr_tty;
		tp->t_oproc = wsdisplaystart;
		tp->t_param = wsdisplayparam;
		tp->t_dev = dev;
		newopen = (tp->t_state & TS_ISOPEN) == 0;
		if (newopen) {
			ttychars(tp);
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
			wsdisplayparam(tp, &tp->t_termios);
			ttsetwater(tp);
		} else if ((tp->t_state & TS_XCLUDE) != 0 &&
			   p->p_ucred->cr_uid != 0)
			return (EBUSY);
		tp->t_state |= TS_CARR_ON;

		error = ((*linesw[tp->t_line].l_open)(dev, tp));
		if (error)
			return (error);

		if (newopen) {
			/* set window sizes as appropriate, and reset
			   the emulation */
			tp->t_winsize.ws_row = scr->scr_dconf->scrdata->nrows;
			tp->t_winsize.ws_col = scr->scr_dconf->scrdata->ncols;
		}
	}

	scr->scr_flags |= SCR_OPEN;
	return (0);
}

int
wsdisplayclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	int unit;
	struct wsscreen *scr;

	unit = WSDISPLAYUNIT(dev);
	sc = wsdisplay_cd.cd_devs[unit];

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (WSSCREEN_HAS_TTY(scr)) {
		if (scr->scr_hold_screen) {
			int s;

			/* XXX RESET KEYBOARD LEDS, etc. */
			s = spltty();	/* avoid conflict with keyboard */
			wsdisplay_kbdholdscreen((struct device *)sc, 0);
			splx(s);
		}
		tp = scr->scr_tty;
		(*linesw[tp->t_line].l_close)(tp, flag);
		ttyclose(tp);
	}

	if (scr->scr_syncops)
		(*scr->scr_syncops->destroy)(scr->scr_synccookie);

	scr->scr_flags &= ~SCR_GRAPHICS;
	(*scr->scr_dconf->wsemul->reset)(scr->scr_dconf->wsemulcookie,
					 WSEMUL_RESET);
	if (wsdisplay_clearonclose)
		(*scr->scr_dconf->wsemul->reset)
			(scr->scr_dconf->wsemulcookie, WSEMUL_CLEARSCREEN);

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (scr->scr_rawkbd) {
		int kbmode = WSKBD_TRANSLATED;
		(void) wsdisplay_internal_ioctl(sc, scr, WSKBDIO_SETMODE,
		    (caddr_t)&kbmode, FWRITE, p);
	}
#endif

	scr->scr_flags &= ~SCR_OPEN;

#ifdef WSMOUSED_SUPPORT
	/* remove the selection at logout */
	if (Copybuffer)
		bzero(Copybuffer, Copybuffer_size);
	Paste_avail = 0;
#endif

	return (0);
}

int
wsdisplayread(dev_t dev, struct uio *uio, int flag)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	int unit;
	struct wsscreen *scr;

	unit = WSDISPLAYUNIT(dev);
	sc = wsdisplay_cd.cd_devs[unit];

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (!WSSCREEN_HAS_TTY(scr))
		return (ENODEV);

	tp = scr->scr_tty;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
wsdisplaywrite(dev_t dev, struct uio *uio, int flag)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	int unit;
	struct wsscreen *scr;

	unit = WSDISPLAYUNIT(dev);
	sc = wsdisplay_cd.cd_devs[unit];

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (!WSSCREEN_HAS_TTY(scr))
		return (ENODEV);

	tp = scr->scr_tty;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
wsdisplaytty(dev_t dev)
{
	struct wsdisplay_softc *sc;
	int unit;
	struct wsscreen *scr;

	unit = WSDISPLAYUNIT(dev);
	sc = wsdisplay_cd.cd_devs[unit];

	if (ISWSDISPLAYCTL(dev))
		panic("wsdisplaytty() on ctl device");

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (NULL);

	return (scr->scr_tty);
}

int
wsdisplayioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct wsdisplay_softc *sc;
	struct tty *tp;
	int unit, error;
	struct wsscreen *scr;

	unit = WSDISPLAYUNIT(dev);
	sc = wsdisplay_cd.cd_devs[unit];

#ifdef WSDISPLAY_COMPAT_USL
	error = wsdisplay_usl_ioctl1(sc, cmd, data, flag, p);
	if (error >= 0)
		return (error);
#endif

	if (ISWSDISPLAYCTL(dev))
		return (wsdisplay_cfg_ioctl(sc, cmd, data, flag, p));

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (WSSCREEN_HAS_TTY(scr)) {
		tp = scr->scr_tty;

/* printf("disc\n"); */
		/* do the line discipline ioctls first */
		error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
		if (error >= 0)
			return (error);

/* printf("tty\n"); */
		/* then the tty ioctls */
		error = ttioctl(tp, cmd, data, flag, p);
		if (error >= 0)
			return (error);
	}

#ifdef WSDISPLAY_COMPAT_USL
	error = wsdisplay_usl_ioctl2(sc, scr, cmd, data, flag, p);
	if (error >= 0)
		return (error);
#endif

	error = wsdisplay_internal_ioctl(sc, scr, cmd, data, flag, p);
	return (error != -1 ? error : ENOTTY);
}

int
wsdisplay_param(struct device *dev, u_long cmd, struct wsdisplay_param *dp)
{
	struct wsdisplay_softc *sc = (struct wsdisplay_softc *)dev;

	return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie, cmd,
	    (caddr_t)dp, 0, NULL));
}

int
wsdisplay_internal_ioctl(struct wsdisplay_softc *sc, struct wsscreen *scr,
    u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int error;

#if NWSKBD > 0
	struct wsevsrc *inp;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	switch (cmd) {
	case WSKBDIO_SETMODE:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		scr->scr_rawkbd = (*(int *)data == WSKBD_RAW);
		return (wsdisplay_update_rawkbd(sc, scr));
	case WSKBDIO_GETMODE:
		*(int *)data = (scr->scr_rawkbd ?
				WSKBD_RAW : WSKBD_TRANSLATED);
		return (0);
	}
#endif
	inp = sc->sc_input;
	if (inp != NULL) {
		error = wsevsrc_display_ioctl(inp, cmd, data, flag, p);
		if (error >= 0)
			return (error);
	}
#endif /* NWSKBD > 0 */

	switch (cmd) {
	case WSDISPLAYIO_SMODE:
	case WSDISPLAYIO_USEFONT:
#ifdef BURNER_SUPPORT
	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_SBURNER:
#endif
	case WSDISPLAYIO_SETSCREEN:
		if ((flag & FWRITE) == 0)
			return (EACCES);
	}

	switch (cmd) {
	case WSDISPLAYIO_GMODE:
		if (scr->scr_flags & SCR_GRAPHICS) {
			if (scr->scr_flags & SCR_DUMBFB)
				*(u_int *)data = WSDISPLAYIO_MODE_DUMBFB;
			else
				*(u_int *)data = WSDISPLAYIO_MODE_MAPPED;
		} else
			*(u_int *)data = WSDISPLAYIO_MODE_EMUL;
		return (0);

	case WSDISPLAYIO_SMODE:
#define d (*(int *)data)
		if (d != WSDISPLAYIO_MODE_EMUL &&
		    d != WSDISPLAYIO_MODE_MAPPED &&
		    d != WSDISPLAYIO_MODE_DUMBFB)
			return (EINVAL);

		scr->scr_flags &= ~SCR_GRAPHICS;
		if (d == WSDISPLAYIO_MODE_MAPPED ||
		    d == WSDISPLAYIO_MODE_DUMBFB) {
			scr->scr_flags |= SCR_GRAPHICS |
			    ((d == WSDISPLAYIO_MODE_DUMBFB) ?  SCR_DUMBFB : 0);

#ifdef WSMOUSED_SUPPORT
			/*
			 * wsmoused cohabitation with X-Window support
			 * X-Window is starting
			 */
			wsmoused_release(sc);
#endif

#ifdef BURNER_SUPPORT
			/* disable the burner while X is running */
			if (sc->sc_burnout)
				timeout_del(&sc->sc_burner);
#endif
		} else {
#ifdef BURNER_SUPPORT
			/* reenable the burner after exiting from X */
			if (!sc->sc_burnman)
				wsdisplay_burn(sc, sc->sc_burnflags);
#endif

#ifdef WSMOUSED_SUPPORT
			/*
			 * wsmoused cohabitation with X-Window support
			 * X-Window is ending
			 */
			wsmoused_wakeup(sc);
#endif
		}

		(void)(*sc->sc_accessops->ioctl)(sc->sc_accesscookie, cmd, data,
		    flag, p);

		return (0);
#undef d

	case WSDISPLAYIO_USEFONT:
#define d ((struct wsdisplay_font *)data)
		if (!sc->sc_accessops->load_font)
			return (EINVAL);
		d->data = 0;
		error = (*sc->sc_accessops->load_font)(sc->sc_accesscookie,
		    scr->scr_dconf->emulcookie, d);
		if (!error)
			(*scr->scr_dconf->wsemul->reset)
			    (scr->scr_dconf->wsemulcookie, WSEMUL_SYNCFONT);
		return (error);
#undef d
#ifdef BURNER_SUPPORT
	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = !sc->sc_burnman;
		break;

	case WSDISPLAYIO_SVIDEO:
		if (*(u_int *)data != WSDISPLAYIO_VIDEO_OFF &&
		    *(u_int *)data != WSDISPLAYIO_VIDEO_ON)
			return (EINVAL);
		if (sc->sc_accessops->burn_screen == NULL)
			return (EOPNOTSUPP);
		(*sc->sc_accessops->burn_screen)(sc->sc_accesscookie,
		     *(u_int *)data, sc->sc_burnflags);
		break;

	case WSDISPLAYIO_GBURNER:
#define d ((struct wsdisplay_burner *)data)
		d->on  = sc->sc_burninintvl  * 1000 / hz;
		d->off = sc->sc_burnoutintvl * 1000 / hz;
		d->flags = sc->sc_burnflags;
		return (0);

	case WSDISPLAYIO_SBURNER:
		if (d->flags & ~(WSDISPLAY_BURN_VBLANK | WSDISPLAY_BURN_KBD |
		    WSDISPLAY_BURN_MOUSE | WSDISPLAY_BURN_OUTPUT))
			error = EINVAL;
		else {
			error = 0;
			sc->sc_burnflags = d->flags;
			/* disable timeout if necessary */
			if ((sc->sc_burnflags & (WSDISPLAY_BURN_OUTPUT |
			    WSDISPLAY_BURN_KBD | WSDISPLAY_BURN_MOUSE)) == 0) {
				if (sc->sc_burnout)
					timeout_del(&sc->sc_burner);
			}
		}
		if (d->on) {
			error = 0;
			sc->sc_burninintvl = hz * d->on / 1000;
			if (sc->sc_burnman)
				sc->sc_burnout = sc->sc_burninintvl;
		}
		if (d->off) {
			error = 0;
			sc->sc_burnoutintvl = hz * d->off / 1000;
			if (!sc->sc_burnman) {
				sc->sc_burnout = sc->sc_burnoutintvl;
				/* reinit timeout if changed */
				if ((scr->scr_flags & SCR_GRAPHICS) == 0)
					wsdisplay_burn(sc, sc->sc_burnflags);
			}
		}
		return (error);
#undef d
#endif	/* BURNER_SUPPORT */
	case WSDISPLAYIO_GETSCREEN:
		return (wsdisplay_getscreen(sc,
		    (struct wsdisplay_addscreendata *)data));

	case WSDISPLAYIO_SETSCREEN:
		return (wsdisplay_switch((void *)sc, *(int *)data, 1));
	}

	/* check ioctls for display */
	return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie, cmd, data,
	    flag, p));
}

int
wsdisplay_cfg_ioctl(struct wsdisplay_softc *sc, u_long cmd, caddr_t data,
    int flag, struct proc *p)
{
	int error;
	void *buf;
	size_t fontsz;
#if NWSKBD > 0
	struct wsevsrc *inp;
#endif

	switch (cmd) {
#ifdef WSMOUSED_SUPPORT
	case WSDISPLAYIO_WSMOUSED:
		error = wsmoused(sc, cmd, data, flag, p);
		return (error);
#endif
	case WSDISPLAYIO_ADDSCREEN:
#define d ((struct wsdisplay_addscreendata *)data)
		if ((error = wsdisplay_addscreen(sc, d->idx,
		    d->screentype, d->emul)) == 0)
			wsdisplay_addscreen_print(sc, d->idx, 0);
		return (error);
#undef d
	case WSDISPLAYIO_DELSCREEN:
#define d ((struct wsdisplay_delscreendata *)data)
		return (wsdisplay_delscreen(sc, d->idx, d->flags));
#undef d
	case WSDISPLAYIO_GETSCREEN:
		return (wsdisplay_getscreen(sc,
		    (struct wsdisplay_addscreendata *)data));
	case WSDISPLAYIO_SETSCREEN:
		return (wsdisplay_switch((void *)sc, *(int *)data, 1));
	case WSDISPLAYIO_LDFONT:
#define d ((struct wsdisplay_font *)data)
		if (!sc->sc_accessops->load_font)
			return (EINVAL);
		if (d->index >= WSDISPLAY_MAXFONT)
			return (EINVAL);
		fontsz = d->fontheight * d->stride * d->numchars;
		if (fontsz > WSDISPLAY_MAXFONTSZ)
			return (EINVAL);

		buf = malloc(fontsz, M_DEVBUF, M_WAITOK);
		error = copyin(d->data, buf, fontsz);
		if (error) {
			free(buf, M_DEVBUF);
			return (error);
		}
		d->data = buf;
		error =
		  (*sc->sc_accessops->load_font)(sc->sc_accesscookie, 0, d);
		if (error)
			free(buf, M_DEVBUF);
		else if (d->index >= 0 || d->index < WSDISPLAY_MAXFONT)
			sc->sc_fonts[d->index] = *d;
		return (error);

	case WSDISPLAYIO_LSFONT:
		if (d->index < 0 || d->index >= WSDISPLAY_MAXFONT)
			return (EINVAL);
		*d = sc->sc_fonts[d->index];
		return (0);

	case WSDISPLAYIO_DELFONT:
		return (EINVAL);
#undef d

#if NWSKBD > 0
	case WSMUXIO_ADD_DEVICE:
#define d ((struct wsmux_device *)data)
		if (d->idx == -1 && d->type == WSMUX_KBD)
			d->idx = wskbd_pickfree();
#undef d
		/* fall into */
	case WSMUXIO_INJECTEVENT:
	case WSMUXIO_REMOVE_DEVICE:
	case WSMUXIO_LIST_DEVICES:
		inp = sc->sc_input;
		if (inp == NULL)
			return (ENXIO);
		return (wsevsrc_ioctl(inp, cmd, data, flag,p));
#endif /* NWSKBD > 0 */

	}
	return (EINVAL);
}

paddr_t
wsdisplaymmap(dev_t dev, off_t offset, int prot)
{
	struct wsdisplay_softc *sc = wsdisplay_cd.cd_devs[WSDISPLAYUNIT(dev)];
	struct wsscreen *scr;

	if (ISWSDISPLAYCTL(dev))
		return (-1);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (-1);

	if (!(scr->scr_flags & SCR_GRAPHICS))
		return (-1);

	/* pass mmap to display */
	return ((*sc->sc_accessops->mmap)(sc->sc_accesscookie, offset, prot));
}

int
wsdisplaypoll(dev_t dev, int events, struct proc *p)
{
	struct wsdisplay_softc *sc = wsdisplay_cd.cd_devs[WSDISPLAYUNIT(dev)];
	struct wsscreen *scr;

	if (ISWSDISPLAYCTL(dev))
		return (0);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (ENXIO);

	if (!WSSCREEN_HAS_TTY(scr))
		return (ENODEV);

	return (ttpoll(dev, events, p));
}

int
wsdisplaykqfilter(dev_t dev, struct knote *kn)
{
	struct wsdisplay_softc *sc = wsdisplay_cd.cd_devs[WSDISPLAYUNIT(dev)];
	struct wsscreen *scr;

	if (ISWSDISPLAYCTL(dev))
		return (1);

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(dev)]) == NULL)
		return (1);

	if (WSSCREEN_HAS_TTY(scr))
		return (ttkqfilter(dev, kn));
	else
		return (1);
}

void
wsdisplaystart(struct tty *tp)
{
	struct wsdisplay_softc *sc;
	struct wsscreen *scr;
	int s, n, unit;
	u_char *buf;

	unit = WSDISPLAYUNIT(tp->t_dev);
	if (unit >= wsdisplay_cd.cd_ndevs ||
	    (sc = wsdisplay_cd.cd_devs[unit]) == NULL)
		return;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	if (tp->t_outq.c_cc == 0 && tp->t_wsel.si_selpid == 0)
		goto low;

	if ((scr = sc->sc_scr[WSDISPLAYSCREEN(tp->t_dev)]) == NULL) {
		splx(s);
		return;
	}
	if (scr->scr_hold_screen) {
		tp->t_state |= TS_TIMEOUT;
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	splx(s);

	/*
	 * Drain output from ring buffer.
	 * The output will normally be in one contiguous chunk, but when the
	 * ring wraps, it will be in two pieces.. one at the end of the ring,
	 * the other at the start.  For performance, rather than loop here,
	 * we output one chunk, see if there's another one, and if so, output
	 * it too.
	 */

	n = ndqb(&tp->t_outq, 0);
	buf = tp->t_outq.c_cf;

	if (!(scr->scr_flags & SCR_GRAPHICS)) {
#ifdef BURNER_SUPPORT
		wsdisplay_burn(sc, WSDISPLAY_BURN_OUTPUT);
#endif
#ifdef WSMOUSED_SUPPORT
		if (scr == sc->sc_focus) {
			if (IS_SEL_EXISTS(sc->sc_focus))
				/* hide a potential selection */
				remove_selection(sc);
			/* hide a potential mouse cursor */
			mouse_hide(sc);
		}
#endif
		(*scr->scr_dconf->wsemul->output)(scr->scr_dconf->wsemulcookie,
		    buf, n, 0);
	}
	ndflush(&tp->t_outq, n);

	if ((n = ndqb(&tp->t_outq, 0)) > 0) {
		buf = tp->t_outq.c_cf;

		if (!(scr->scr_flags & SCR_GRAPHICS)) {
#ifdef BURNER_SUPPORT
			wsdisplay_burn(sc, WSDISPLAY_BURN_OUTPUT);
#endif
			(*scr->scr_dconf->wsemul->output)
			    (scr->scr_dconf->wsemulcookie, buf, n, 0);
		}
		ndflush(&tp->t_outq, n);
	}

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	/* Come back if there's more to do */
	if (tp->t_outq.c_cc) {
		tp->t_state |= TS_TIMEOUT;
		timeout_add(&tp->t_rstrt_to, (hz > 128) ? (hz / 128) : 1);
	}
	if (tp->t_outq.c_cc <= tp->t_lowat) {
low:
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	splx(s);
}

int
wsdisplaystop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY))
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	splx(s);

	return (0);
}

/* Set line parameters. */
int
wsdisplayparam(struct tty *tp, struct termios *t)
{

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return (0);
}

/*
 * Callbacks for the emulation code.
 */
void
wsdisplay_emulbell(void *v)
{
	struct wsscreen *scr = v;

	if (scr == NULL)		/* console, before real attach */
		return;

	if (scr->scr_flags & SCR_GRAPHICS) /* can this happen? */
		return;

	(void) wsdisplay_internal_ioctl(scr->sc, scr, WSKBDIO_BELL, NULL,
	    FWRITE, NULL);
}

void
wsdisplay_emulinput(void *v, const u_char *data, u_int count)
{
	struct wsscreen *scr = v;
	struct tty *tp;

	if (v == NULL)			/* console, before real attach */
		return;

	if (scr->scr_flags & SCR_GRAPHICS) /* XXX can't happen */
		return;
	if (!WSSCREEN_HAS_TTY(scr))
		return;

	tp = scr->scr_tty;
	while (count-- > 0)
		(*linesw[tp->t_line].l_rint)(*data++, tp);
}

/*
 * Calls from the keyboard interface.
 */
void
wsdisplay_kbdinput(struct device *dev, keysym_t ks)
{
	struct wsdisplay_softc *sc = (struct wsdisplay_softc *)dev;
	struct wsscreen *scr;
	char *dp;
	int count;
	struct tty *tp;

	KASSERT(sc != NULL);

	scr = sc->sc_focus;

	if (!scr || !WSSCREEN_HAS_TTY(scr))
		return;

	tp = scr->scr_tty;

	if (KS_GROUP(ks) == KS_GROUP_Ascii)
		(*linesw[tp->t_line].l_rint)(KS_VALUE(ks), tp);
	else {
		count = (*scr->scr_dconf->wsemul->translate)
		    (scr->scr_dconf->wsemulcookie, ks, &dp);
		while (count-- > 0)
			(*linesw[tp->t_line].l_rint)(*dp++, tp);
	}
}

#ifdef WSDISPLAY_COMPAT_RAWKBD
int
wsdisplay_update_rawkbd(struct wsdisplay_softc *sc, struct wsscreen *scr)
{
#if NWSKBD > 0
	int s, raw, data, error;
	struct wsevsrc *inp;

	s = spltty();

	raw = (scr ? scr->scr_rawkbd : 0);

	if (scr != sc->sc_focus || sc->sc_rawkbd == raw) {
		splx(s);
		return (0);
	}

	data = raw ? WSKBD_RAW : WSKBD_TRANSLATED;
	inp = sc->sc_input;
	if (inp == NULL) {
		splx(s);
		return (ENXIO);
	}
	error = wsevsrc_display_ioctl(inp, WSKBDIO_SETMODE, &data, FWRITE, 0);
	if (!error)
		sc->sc_rawkbd = raw;
	splx(s);
	return (error);
#else
	return (0);
#endif
}
#endif

int
wsdisplay_switch3(void *arg, int error, int waitok)
{
	struct wsdisplay_softc *sc = arg;
	int no;
	struct wsscreen *scr;

	if (!(sc->sc_flags & SC_SWITCHPENDING)) {
		printf("wsdisplay_switch3: not switching\n");
		return (EINVAL);
	}

	no = sc->sc_screenwanted;
	if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
		panic("wsdisplay_switch3: invalid screen %d", no);
	scr = sc->sc_scr[no];
	if (!scr) {
		printf("wsdisplay_switch3: screen %d disappeared\n", no);
		error = ENXIO;
	}

	if (error) {
		/* try to recover, avoid recursion */

		if (sc->sc_oldscreen == WSDISPLAY_NULLSCREEN) {
			printf("wsdisplay_switch3: giving up\n");
			sc->sc_focus = 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
			wsdisplay_update_rawkbd(sc, 0);
#endif
			sc->sc_flags &= ~SC_SWITCHPENDING;
			return (error);
		}

		sc->sc_screenwanted = sc->sc_oldscreen;
		sc->sc_oldscreen = WSDISPLAY_NULLSCREEN;
		return (wsdisplay_switch1(arg, 0, waitok));
	}

	sc->sc_flags &= ~SC_SWITCHPENDING;

	if (!error && (scr->scr_flags & SCR_WAITACTIVE))
		wakeup(scr);
	return (error);
}

int
wsdisplay_switch2(void *arg, int error, int waitok)
{
	struct wsdisplay_softc *sc = arg;
	int no;
	struct wsscreen *scr;

	if (!(sc->sc_flags & SC_SWITCHPENDING)) {
		printf("wsdisplay_switch2: not switching\n");
		return (EINVAL);
	}

	no = sc->sc_screenwanted;
	if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
		panic("wsdisplay_switch2: invalid screen %d", no);
	scr = sc->sc_scr[no];
	if (!scr) {
		printf("wsdisplay_switch2: screen %d disappeared\n", no);
		error = ENXIO;
	}

	if (error) {
		/* try to recover, avoid recursion */

		if (sc->sc_oldscreen == WSDISPLAY_NULLSCREEN) {
			printf("wsdisplay_switch2: giving up\n");
			sc->sc_focus = 0;
			sc->sc_flags &= ~SC_SWITCHPENDING;
			return (error);
		}

		sc->sc_screenwanted = sc->sc_oldscreen;
		sc->sc_oldscreen = WSDISPLAY_NULLSCREEN;
		return (wsdisplay_switch1(arg, 0, waitok));
	}

	sc->sc_focusidx = no;
	sc->sc_focus = scr;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	(void) wsdisplay_update_rawkbd(sc, scr);
#endif
	/* keyboard map??? */

#define wsswitch_cb3 ((void (*)(void *, int, int))wsdisplay_switch3)
	if (scr->scr_syncops) {
		error = (*scr->scr_syncops->attach)(scr->scr_synccookie, waitok,
		    sc->sc_isconsole && wsdisplay_cons_pollmode ?
		      0 : wsswitch_cb3, sc);
		if (error == EAGAIN) {
			/* switch will be done asynchronously */
			return (0);
		}
	}

	return (wsdisplay_switch3(sc, error, waitok));
}

int
wsdisplay_switch1(void *arg, int error, int waitok)
{
	struct wsdisplay_softc *sc = arg;
	int no;
	struct wsscreen *scr;

	if (!(sc->sc_flags & SC_SWITCHPENDING)) {
		printf("wsdisplay_switch1: not switching\n");
		return (EINVAL);
	}

	no = sc->sc_screenwanted;
	if (no == WSDISPLAY_NULLSCREEN) {
		sc->sc_flags &= ~SC_SWITCHPENDING;
		if (!error) {
			sc->sc_focus = 0;
		}
		wakeup(sc);
		return (error);
	}
	if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
		panic("wsdisplay_switch1: invalid screen %d", no);
	scr = sc->sc_scr[no];
	if (!scr) {
		printf("wsdisplay_switch1: screen %d disappeared\n", no);
		error = ENXIO;
	}

	if (error) {
		sc->sc_flags &= ~SC_SWITCHPENDING;
		return (error);
	}

#define wsswitch_cb2 ((void (*)(void *, int, int))wsdisplay_switch2)
	error = (*sc->sc_accessops->show_screen)(sc->sc_accesscookie,
	    scr->scr_dconf->emulcookie, waitok,
	    sc->sc_isconsole && wsdisplay_cons_pollmode ? 0 : wsswitch_cb2, sc);
	if (error == EAGAIN) {
		/* switch will be done asynchronously */
		return (0);
	}

	return (wsdisplay_switch2(sc, error, waitok));
}

int
wsdisplay_switch(struct device *dev, int no, int waitok)
{
	struct wsdisplay_softc *sc = (struct wsdisplay_softc *)dev;
	int s, res = 0;
	struct wsscreen *scr;

	if (no != WSDISPLAY_NULLSCREEN) {
		if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
			return (EINVAL);
		if (sc->sc_scr[no] == NULL)
			return (ENXIO);
	}

	s = spltty();

	if ((sc->sc_focus && no == sc->sc_focusidx) ||
	    (sc->sc_focus == NULL && no == WSDISPLAY_NULLSCREEN)) {
		splx(s);
		return (0);
	}

	if (sc->sc_flags & SC_SWITCHPENDING) {
		splx(s);
		return (EBUSY);
	}

	sc->sc_flags |= SC_SWITCHPENDING;
	sc->sc_screenwanted = no;

	splx(s);

	scr = sc->sc_focus;
	if (!scr) {
		sc->sc_oldscreen = WSDISPLAY_NULLSCREEN;
		return (wsdisplay_switch1(sc, 0, waitok));
	} else
		sc->sc_oldscreen = sc->sc_focusidx;


#ifdef WSMOUSED_SUPPORT
	/*
	 *  wsmoused cohabitation with X-Window support
	 *
	 *  Detect switch from a graphic to text console and vice-versa
	 *  This only happen when switching from X-Window to text mode and
	 *  switching back from text mode to X-Window.
	 *
	 *  scr_flags is not yet flagged with SCR_GRAPHICS when X-Window starts
	 *  (KD_GRAPHICS ioctl happens after VT_ACTIVATE ioctl in
	 *  xf86OpenPcvt()). Conversely, scr_flags is no longer flagged with
	 *  SCR_GRAPHICS when X-Window stops. In this case, the first of the
	 *  three following 'if' statements is evaluated.
	 *  We handle wsmoused(8) events the WSDISPLAYIO_SMODE ioctl.
	 */

	if (!(scr->scr_flags & SCR_GRAPHICS) &&
	    (!(sc->sc_scr[no]->scr_flags & SCR_GRAPHICS))) {
		/* switching from a text console to another text console */
		/* XXX evaluated when the X-server starts or stops, see above */

		/* remove a potential wsmoused(8) selection */
		mouse_remove(sc);
	}

	if (!(scr->scr_flags & SCR_GRAPHICS) &&
	    (sc->sc_scr[no]->scr_flags & SCR_GRAPHICS)) {
		/* switching from a text console to a graphic console */
	
		/* remote a potential wsmoused(8) selection */
		mouse_remove(sc);
		wsmoused_release(sc);
	}
	
	if ((scr->scr_flags & SCR_GRAPHICS) &&
	    !(sc->sc_scr[no]->scr_flags & SCR_GRAPHICS)) {
		/* switching from a graphic console to a text console */

		wsmoused_wakeup(sc);
	}
#endif	/* WSMOUSED_SUPPORT */

#define wsswitch_cb1 ((void (*)(void *, int, int))wsdisplay_switch1)
	if (scr->scr_syncops) {
		res = (*scr->scr_syncops->detach)(scr->scr_synccookie, waitok,
		    sc->sc_isconsole && wsdisplay_cons_pollmode ?
		      0 : wsswitch_cb1, sc);
		if (res == EAGAIN) {
			/* switch will be done asynchronously */
			return (0);
		}
	} else if (scr->scr_flags & SCR_GRAPHICS) {
		/* no way to save state */
		res = EBUSY;
	}

	return (wsdisplay_switch1(sc, res, waitok));
}

void
wsdisplay_reset(struct device *dev, enum wsdisplay_resetops op)
{
	struct wsdisplay_softc *sc = (struct wsdisplay_softc *)dev;
	struct wsscreen *scr;

	KASSERT(sc != NULL);
	scr = sc->sc_focus;

	if (!scr)
		return;

	switch (op) {
	case WSDISPLAY_RESETEMUL:
		(*scr->scr_dconf->wsemul->reset)(scr->scr_dconf->wsemulcookie,
		    WSEMUL_RESET);
		break;
	case WSDISPLAY_RESETCLOSE:
		wsdisplay_closescreen(sc, scr);
		break;
	}
}

/*
 * Interface for (external) VT switch / process synchronization code
 */
int
wsscreen_attach_sync(struct wsscreen *scr, const struct wscons_syncops *ops,
    void *cookie)
{
	if (scr->scr_syncops) {
		/*
		 * The screen is already claimed.
		 * Check if the owner is still alive.
		 */
		if ((*scr->scr_syncops->check)(scr->scr_synccookie))
			return (EBUSY);
	}
	scr->scr_syncops = ops;
	scr->scr_synccookie = cookie;
	return (0);
}

int
wsscreen_detach_sync(struct wsscreen *scr)
{
	if (!scr->scr_syncops)
		return (EINVAL);
	scr->scr_syncops = 0;
	return (0);
}

int
wsscreen_lookup_sync(struct wsscreen *scr,
    const struct wscons_syncops *ops, /* used as ID */
    void **cookiep)
{
	if (!scr->scr_syncops || ops != scr->scr_syncops)
		return (EINVAL);
	*cookiep = scr->scr_synccookie;
	return (0);
}

/*
 * Interface to virtual screen stuff
 */
int
wsdisplay_maxscreenidx(struct wsdisplay_softc *sc)
{
	return (WSDISPLAY_MAXSCREEN - 1);
}

int
wsdisplay_screenstate(struct wsdisplay_softc *sc, int idx)
{
	if (idx < 0 || idx >= WSDISPLAY_MAXSCREEN)
		return (EINVAL);
	if (!sc->sc_scr[idx])
		return (ENXIO);
	return ((sc->sc_scr[idx]->scr_flags & SCR_OPEN) ? EBUSY : 0);
}

int
wsdisplay_getactivescreen(struct wsdisplay_softc *sc)
{
	return (sc->sc_focus ? sc->sc_focusidx : WSDISPLAY_NULLSCREEN);
}

int
wsscreen_switchwait(struct wsdisplay_softc *sc, int no)
{
	struct wsscreen *scr;
	int s, res = 0;

	if (no == WSDISPLAY_NULLSCREEN) {
		s = spltty();
		while (sc->sc_focus && res == 0) {
			res = tsleep(sc, PCATCH, "wswait", 0);
		}
		splx(s);
		return (res);
	}

	if (no < 0 || no >= WSDISPLAY_MAXSCREEN)
		return (ENXIO);
	scr = sc->sc_scr[no];
	if (!scr)
		return (ENXIO);

	s = spltty();
	if (scr != sc->sc_focus) {
		scr->scr_flags |= SCR_WAITACTIVE;
		res = tsleep(scr, PCATCH, "wswait", 0);
		if (scr != sc->sc_scr[no])
			res = ENXIO; /* disappeared in the meantime */
		else
			scr->scr_flags &= ~SCR_WAITACTIVE;
	}
	splx(s);
	return (res);
}

void
wsdisplay_kbdholdscreen(struct device *dev, int hold)
{
	struct wsdisplay_softc *sc = (struct wsdisplay_softc *)dev;
	struct wsscreen *scr;

	scr = sc->sc_focus;

	if (hold)
		scr->scr_hold_screen = 1;
	else {
		scr->scr_hold_screen = 0;
		timeout_add(&scr->scr_tty->t_rstrt_to, 0); /* "immediate" */
	}
}

#if NWSKBD > 0
void
wsdisplay_set_console_kbd(struct wsevsrc *src)
{
	if (wsdisplay_console_device == NULL) {
		src->me_dispdv = NULL;
		return;
	}
#if NWSMUX > 0
	if (wsmux_attach_sc((struct wsmux_softc *)
			    wsdisplay_console_device->sc_input, src)) {
		src->me_dispdv = NULL;
		return;
	}
#else
	wsdisplay_console_device->sc_input = src;
#endif
	src->me_dispdv = &wsdisplay_console_device->sc_dv;
}
#endif /* NWSKBD > 0 */

/*
 * Console interface.
 */
void
wsdisplay_cnputc(dev_t dev, int i)
{
	struct wsscreen_internal *dc;
	char c = i;

	if (!wsdisplay_console_initted)
		return;

	if (wsdisplay_console_device != NULL &&
	    (wsdisplay_console_device->sc_scr[0] != NULL) &&
	    (wsdisplay_console_device->sc_scr[0]->scr_flags & SCR_GRAPHICS))
		return;

	dc = &wsdisplay_console_conf;
#ifdef BURNER_SUPPORT
	/*wsdisplay_burn(wsdisplay_console_device, WSDISPLAY_BURN_OUTPUT);*/
#endif
	(*dc->wsemul->output)(dc->wsemulcookie, &c, 1, 1);
}

int
wsdisplay_getc_dummy(dev_t dev)
{
	/* panic? */
	return (0);
}

void
wsdisplay_pollc(dev_t dev, int on)
{

	wsdisplay_cons_pollmode = on;

	/* notify to fb drivers */
	if (wsdisplay_console_device != NULL &&
	    wsdisplay_console_device->sc_accessops->pollc != NULL)
		(*wsdisplay_console_device->sc_accessops->pollc)
		    (wsdisplay_console_device->sc_accesscookie, on);

	/* notify to kbd drivers */
	if (wsdisplay_cons_kbd_pollc)
		(*wsdisplay_cons_kbd_pollc)(dev, on);
}

void
wsdisplay_set_cons_kbd(int (*get)(dev_t), void (*poll)(dev_t, int),
    void (*bell)(dev_t, u_int, u_int, u_int))
{
	wsdisplay_cons.cn_getc = get;
	wsdisplay_cons.cn_bell = bell;
	wsdisplay_cons_kbd_pollc = poll;
}

void
wsdisplay_unset_cons_kbd()
{
	wsdisplay_cons.cn_getc = wsdisplay_getc_dummy;
	wsdisplay_cons.cn_bell = NULL;
	wsdisplay_cons_kbd_pollc = 0;
}

/*
 * Switch the console display to it's first screen.
 */
void
wsdisplay_switchtoconsole()
{
	struct wsdisplay_softc *sc;
	struct wsscreen *scr;

	if (wsdisplay_console_device != NULL) {
		sc = wsdisplay_console_device;
		if ((scr = sc->sc_scr[0]) == NULL)
			return;
		(*sc->sc_accessops->show_screen)(sc->sc_accesscookie,
		    scr->scr_dconf->emulcookie, 0, NULL, NULL);
	}
}

#ifdef SCROLLBACK_SUPPORT
void
wsscrollback(void *arg, int op)
{
	struct wsdisplay_softc *sc = arg;
	int lines;

	if (op == WSDISPLAY_SCROLL_RESET)
		lines = 0;
	else {
		lines = sc->sc_focus->scr_dconf->scrdata->nrows - 1;
		if (op == WSDISPLAY_SCROLL_BACKWARD)
			lines = -lines;
	}

	if (sc->sc_accessops->scrollback) {
		(*sc->sc_accessops->scrollback)(sc->sc_accesscookie,
		    sc->sc_focus->scr_dconf->emulcookie, lines);
	}
}
#endif

#ifdef BURNER_SUPPORT
void
wsdisplay_burn(void *v, u_int flags)
{
	struct wsdisplay_softc *sc = v;

	if ((flags & sc->sc_burnflags & (WSDISPLAY_BURN_OUTPUT |
	    WSDISPLAY_BURN_KBD | WSDISPLAY_BURN_MOUSE)) &&
	    sc->sc_accessops->burn_screen) {
		if (sc->sc_burnout)
			timeout_add(&sc->sc_burner, sc->sc_burnout);
		if (sc->sc_burnman)
			sc->sc_burnout = 0;
	}
}

void
wsdisplay_burner(void *v)
{
	struct wsdisplay_softc *sc = v;
	int s;

	if (sc->sc_accessops->burn_screen) {
		(*sc->sc_accessops->burn_screen)(sc->sc_accesscookie,
		    sc->sc_burnman, sc->sc_burnflags);
		s = spltty();
		if (sc->sc_burnman) {
			sc->sc_burnout = sc->sc_burnoutintvl;
			timeout_add(&sc->sc_burner, sc->sc_burnout);
		} else
			sc->sc_burnout = sc->sc_burninintvl;
		sc->sc_burnman = !sc->sc_burnman;
		splx(s);
	}
}
#endif

/*
 * Switch the console at shutdown.
 */
void
wsdisplay_shutdownhook(void *arg)
{
	wsdisplay_switchtoconsole();
}

#ifdef WSMOUSED_SUPPORT
/*
 * wsmoused(8) support functions
 */

/* pointer to the current screen wsdisplay_softc structure */
static struct wsdisplay_softc *sc = NULL;

/*
 * Main function, called from wsdisplay_cfg_ioctl.
 */
int
wsmoused(struct wsdisplay_softc *ws_sc, u_long cmd, caddr_t data,
    int flag, struct proc *p)
{
	int error = -1;
	struct wscons_event mouse_event = *(struct wscons_event *)data;

	if (cmd == WSDISPLAYIO_WSMOUSED) {
		if (IS_MOTION_EVENT(mouse_event.type)) {
			motion_event(mouse_event.type, mouse_event.value);
			return (0);
		}
		if (IS_BUTTON_EVENT(mouse_event.type)) {
			/* XXX tv_sec contains the number of clicks */
			if (mouse_event.type == WSCONS_EVENT_MOUSE_DOWN) {
				button_event(mouse_event.value,
				    mouse_event.time.tv_sec);
			} else
				button_event(mouse_event.value, 0);
			return (0);
		}
		if (IS_CTRL_EVENT(mouse_event.type)) {
			return (ctrl_event(mouse_event.type, mouse_event.value,
			    ws_sc, p));
		}
	}
	return (error);
}

/*
 * Mouse motion events
 */
void
motion_event(u_int type, int value)
{
	switch (type) {
		case WSCONS_EVENT_MOUSE_DELTA_X:
			mouse_moverel(value, 0);
			break;
		case WSCONS_EVENT_MOUSE_DELTA_Y:
			mouse_moverel(0, 0 - value);
			break;
		case WSCONS_EVENT_MOUSE_DELTA_Z:
			mouse_zaxis(value);
			break;
		default:
			break;
	}
}

/*
 * Button clicks events
 */
void
button_event(int button, int clicks)
{
	switch (button) {
	case MOUSE_COPY_BUTTON:
		switch (clicks % 4) {
		case 0: /* button is up */
			mouse_copy_end();
			mouse_copy_selection();
			break;
		case 1: /* single click */
			mouse_copy_start();
			mouse_copy_selection();
			break;
		case 2: /* double click */
			mouse_copy_word();
			mouse_copy_selection();
			break;
		case 3: /* triple click */
			mouse_copy_line();
			mouse_copy_selection();
			break;
		default:
			break;
		}
		break;

	case MOUSE_PASTE_BUTTON:
		switch (clicks) {
		case 0: /* button is up */
			break;
		default: /* paste */
			mouse_paste();
			break;
		}
		break;

	case MOUSE_EXTEND_BUTTON:
		switch (clicks) {
		case 0: /* button is up */
			break;
		default: /* extend the selection */
			mouse_copy_extend_after();
			break;
		}
		break;

	default:
		break;
	}
}

/*
 * Control events
 */
int
ctrl_event(u_int type, int value, struct wsdisplay_softc *ws_sc, struct proc *p)
{
	int i, error;

	if (type == WSCONS_EVENT_WSMOUSED_ON) {
		if (!ws_sc->sc_accessops->getchar)
			/* no wsmoused(8) support in the display driver */
			return (1);
		/* initialization of globals */
		sc = ws_sc;
		allocate_copybuffer(sc);
		Paste_avail = 0;
		ws_sc->wsmoused_dev = value;
	}
	if (type == WSCONS_EVENT_WSMOUSED_OFF) {
		Paste_avail = 0;
		ws_sc->wsmoused_dev = 0;
		return (0);
	}
	if (type == WSCONS_EVENT_WSMOUSED_SLEEP) {
		/* sleeping until next switch to text mode */
		ws_sc->wsmoused_sleep = 1;
		error = 0;
		while (ws_sc->wsmoused_sleep && error == 0)
			error = tsleep(&ws_sc->wsmoused_sleep, PPAUSE,
			    "wsmoused_sleep", 0);
		return (error);
	}
	for (i = 0 ; i < WSDISPLAY_DEFAULTSCREENS ; i++)
		if (sc->sc_scr[i]) {
			sc->sc_scr[i]->mouse =
				((WS_NCOLS(sc->sc_scr[i]) *
				  WS_NROWS(sc->sc_scr[i])) / 2);
			sc->sc_scr[i]->cursor = sc->sc_scr[i]->mouse;
			sc->sc_scr[i]->cpy_start = 0;
			sc->sc_scr[i]->cpy_end = 0;
			sc->sc_scr[i]->orig_start = 0;
			sc->sc_scr[i]->orig_end = 0;
			sc->sc_scr[i]->mouse_flags = 0;
		}
	return (0);
}

void
mouse_moverel(char dx, char dy)
{
	unsigned short old_mouse = MOUSE;
	unsigned char mouse_col = (MOUSE % N_COLS);
	unsigned char mouse_row = (MOUSE / N_COLS);

	/* wscons has support for screen saver via the WSDISPLAYIO_{G,S}VIDEO
	   with WSDISPLAY_VIDEO_OFF and WSDISPLAY_VIDEO_ON values.
	   However, none of the pc display driver (pcdisplay.c or vga.c)
	   support this ioctl. Only the alpha display driver (tga.c) support it.

	   When screen saver support is available, /usr/sbin/screenblank can be
	   used with the -m option, so that mice movements stop the screen
	   saver.
	 */

	/* update position */

	if (mouse_col + dx >= MAXCOL)
		mouse_col = MAXCOL;
	else {
		if (mouse_col + dx <= 0)
			mouse_col = 0;
		else
			mouse_col += dx;
	}
	if (mouse_row + dy >= MAXROW)
		mouse_row = MAXROW;
	else {
		if (mouse_row + dy <= 0)
			mouse_row = 0;
		else
			mouse_row += dy;
	}
	MOUSE = XY_TO_POS(mouse_col, mouse_row);
	/* if we have moved */
	if (old_mouse != MOUSE) {
		if (IS_SEL_IN_PROGRESS(sc->sc_focus)) {
			/* selection in progress */
			mouse_copy_extend();
		} else {
			inverse_char(MOUSE);
			if (IS_MOUSE_VISIBLE(sc->sc_focus))
				inverse_char(old_mouse);
			else
				MOUSE_FLAGS |= MOUSE_VISIBLE;
		}
	}
}

void
inverse_char(unsigned short pos)
{
	u_int16_t uc;
	u_int16_t attr;

	uc = GET_FULLCHAR(pos);
	attr = uc;

	if ((attr >> 8) == 0)
		attr = (FG_LIGHTGREY << 8);

	attr = (((attr >> 8) & 0x88) | ((((attr >> 8) >> 4) |
		((attr >> 8) << 4)) & 0x77)) ;
	PUTCHAR(pos, (u_int) (uc & 0x00FF), (long) attr);
}

void
inverse_region(unsigned short start, unsigned short end)
{
	unsigned short current_pos;
	unsigned short abs_end;

	/* sanity check, useful because 'end' can be (0 - 1) = 65535 */
	abs_end = N_COLS * N_ROWS;
	if (end > abs_end)
		return;
	current_pos = start;
	while (current_pos <= end)
		inverse_char(current_pos++);
}

/*
 * Return the number of contiguous blank characters between the right margin
 * if border == 1 or between the next non-blank character and the current mouse
 * cursor if border == 0
 */
unsigned char
skip_spc_right(char border)
{
	unsigned short current = CPY_END;
	unsigned short mouse_col = (CPY_END % N_COLS);
	unsigned short limit = current + (N_COLS - mouse_col - 1);
	unsigned char res = 0;

	while ((GETCHAR(current) == ' ') && (current <= limit)) {
		current++;
		res++;
	}
	if (border == BORDER) {
		if (current > limit)
			return (res - 1);
		else
			return (0);
	} else {
		if (res)
			return (res - 1);
		else
			return (res);
	}
}

/*
 * Return the number of contiguous blank characters between the first of the
 * contiguous blank characters and the current mouse cursor
 */
unsigned char
skip_spc_left(void)
{
	short current = CPY_START;
	unsigned short mouse_col = (MOUSE % N_COLS);
	unsigned short limit = current - mouse_col;
	unsigned char res = 0;

	while ((GETCHAR(current) == ' ') && (current >= limit)) {
		current--;
		res++;
	}
	if (res)
		res--;
	return (res);
}

/*
 * Class of characters
 * Stolen from xterm sources of the Xfree project (see cvs tag below)
 * $TOG: button.c /main/76 1997/07/30 16:56:19 kaleb $
 */
static const int charClass[256] = {
/* NUL  SOH  STX  ETX  EOT  ENQ  ACK  BEL */
    32,   1,   1,   1,   1,   1,   1,   1,
/*  BS   HT   NL   VT   NP   CR   SO   SI */
     1,  32,   1,   1,   1,   1,   1,   1,
/* DLE  DC1  DC2  DC3  DC4  NAK  SYN  ETB */
     1,   1,   1,   1,   1,   1,   1,   1,
/* CAN   EM  SUB  ESC   FS   GS   RS   US */
     1,   1,   1,   1,   1,   1,   1,   1,
/*  SP    !    "    #    $    %    &    ' */
    32,  33,  34,  35,  36,  37,  38,  39,
/*   (    )    *    +    ,    -    .    / */
    40,  41,  42,  43,  44,  45,  46,  47,
/*   0    1    2    3    4    5    6    7 */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   8    9    :    ;    <    =    >    ? */
    48,  48,  58,  59,  60,  61,  62,  63,
/*   @    A    B    C    D    E    F    G */
    64,  48,  48,  48,  48,  48,  48,  48,
/*   H    I    J    K    L    M    N    O */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   P    Q    R    S    T    U    V    W */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   X    Y    Z    [    \    ]    ^    _ */
    48,  48,  48,  91,  92,  93,  94,  48,
/*   `    a    b    c    d    e    f    g */
    96,  48,  48,  48,  48,  48,  48,  48,
/*   h    i    j    k    l    m    n    o */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   p    q    r    s    t    u    v    w */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   x    y    z    {    |    }    ~  DEL */
    48,  48,  48, 123, 124, 125, 126,   1,
/* x80  x81  x82  x83  IND  NEL  SSA  ESA */
     1,   1,   1,   1,   1,   1,   1,   1,
/* HTS  HTJ  VTS  PLD  PLU   RI  SS2  SS3 */
     1,   1,   1,   1,   1,   1,   1,   1,
/* DCS  PU1  PU2  STS  CCH   MW  SPA  EPA */
     1,   1,   1,   1,   1,   1,   1,   1,
/* x98  x99  x9A  CSI   ST  OSC   PM  APC */
     1,   1,   1,   1,   1,   1,   1,   1,
/*   -    i   c/    L   ox   Y-    |   So */
   160, 161, 162, 163, 164, 165, 166, 167,
/*  ..   c0   ip   <<    _        R0    - */
   168, 169, 170, 171, 172, 173, 174, 175,
/*   o   +-    2    3    '    u   q|    . */
   176, 177, 178, 179, 180, 181, 182, 183,
/*   ,    1    2   >>  1/4  1/2  3/4    ? */
   184, 185, 186, 187, 188, 189, 190, 191,
/*  A`   A'   A^   A~   A:   Ao   AE   C, */
    48,  48,  48,  48,  48,  48,  48,  48,
/*  E`   E'   E^   E:   I`   I'   I^   I: */
    48,  48,  48,  48,  48,  48,  48,  48,
/*  D-   N~   O`   O'   O^   O~   O:    X */
    48,  48,  48,  48,  48,  48,  48, 216,
/*  O/   U`   U'   U^   U:   Y'    P    B */
    48,  48,  48,  48,  48,  48,  48,  48,
/*  a`   a'   a^   a~   a:   ao   ae   c, */
    48,  48,  48,  48,  48,  48,  48,  48,
/*  e`   e'   e^   e:    i`  i'   i^   i: */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   d   n~   o`   o'   o^   o~   o:   -: */
    48,  48,  48,  48,  48,  48,  48,  248,
/*  o/   u`   u'   u^   u:   y'    P   y: */
    48,  48,  48,  48,  48,  48,  48,  48
};

/*
 * Find the first blank beginning after the current cursor position
 */
unsigned char
skip_char_right(unsigned short offset)
{
	unsigned short current = offset;
	unsigned short limit = current + (N_COLS - (MOUSE % N_COLS) - 1);
	unsigned char class = charClass[GETCHAR(current)];
	unsigned char res = 0;

	while ((charClass[GETCHAR(current)] == class)
		&& (current <= limit)) {
		current++;
		res++;
	}
	if (res)
		res--;
	return (res);
}

/*
 * Find the first non-blank character before the cursor position
 */
unsigned char
skip_char_left(unsigned short offset)
{
	short current = offset;
	unsigned short limit = current - (MOUSE % N_COLS);
	unsigned char class = charClass[GETCHAR(current)];
	unsigned char res = 0;

	while ((charClass[GETCHAR(current)] == class) && (current >= limit)) {
		current--;
		res++;
	}
	if (res)
		res--;
	return (res);
}

/*
 * Compare character classes
 */
unsigned char
class_cmp(unsigned short first, unsigned short second)
{
	unsigned char first_class;
	unsigned char second_class;

	first_class = charClass[GETCHAR(first)];
	second_class = charClass[GETCHAR(second)];

	if (first_class != second_class)
		return (1);
	else
		return (0);
}

/*
 * Beginning of a copy operation
 */
void
mouse_copy_start(void)
{
	unsigned char right;
	/* if no selection, then that's the first one */

	if (!Paste_avail)
		Paste_avail = 1;

	/* remove the previous selection */

	if (IS_SEL_EXISTS(sc->sc_focus))
		remove_selection(sc);

	/* initial show of the cursor */
	if (!IS_MOUSE_VISIBLE(sc->sc_focus))
		inverse_char(MOUSE);

	CPY_START = MOUSE;
	CPY_END = MOUSE;
	ORIG_START = CPY_START;
	ORIG_END = CPY_END;
	CURSOR = CPY_END + 1; /* init value */

	right = skip_spc_right(BORDER); /* useful later, in mouse_copy_extend */
	if (right)
		MOUSE_FLAGS |= BLANK_TO_EOL;

	MOUSE_FLAGS |= SEL_IN_PROGRESS;
	MOUSE_FLAGS |= SEL_EXISTS;
	MOUSE_FLAGS |= SEL_BY_CHAR; /* select by char */
	MOUSE_FLAGS &= ~SEL_BY_WORD;
	MOUSE_FLAGS &= ~SEL_BY_LINE;
	MOUSE_FLAGS &= ~MOUSE_VISIBLE; /* cursor hidden in selection */
}

/*
 * Copy of the word under the cursor
 */
void
mouse_copy_word()
{
	unsigned char right;
	unsigned char left;

	if (IS_SEL_EXISTS(sc->sc_focus))
		remove_selection(sc);

	if (IS_MOUSE_VISIBLE(sc->sc_focus))
		inverse_char(MOUSE);

	CPY_START = MOUSE;
	CPY_END = MOUSE;

	if (IS_ALPHANUM(MOUSE)) {
		right = skip_char_right(CPY_END);
		left = skip_char_left(CPY_START);
	} else {
		right = skip_spc_right(NO_BORDER);
		left = skip_spc_left();
	}

	CPY_START -= left;
	CPY_END += right;
	ORIG_START = CPY_START;
	ORIG_END = CPY_END;
	CURSOR = CPY_END + 1; /* init value, never happen */
	inverse_region(CPY_START, CPY_END);

	MOUSE_FLAGS |= SEL_IN_PROGRESS;
	MOUSE_FLAGS |= SEL_EXISTS;
	MOUSE_FLAGS &= ~SEL_BY_CHAR;
	MOUSE_FLAGS |= SEL_BY_WORD;
	MOUSE_FLAGS &= ~SEL_BY_LINE;

	/* mouse cursor hidden in the selection */
	MOUSE_FLAGS &= ~BLANK_TO_EOL;
	MOUSE_FLAGS &= ~MOUSE_VISIBLE;
}

/*
 * Copy of the current line
 */
void
mouse_copy_line(void)
{
	unsigned char row = MOUSE / N_COLS;

	if (IS_SEL_EXISTS(sc->sc_focus))
		remove_selection(sc);

	if (IS_MOUSE_VISIBLE(sc->sc_focus))
		inverse_char(MOUSE);

	CPY_START = row * N_COLS;
	CPY_END = CPY_START + (N_COLS - 1);
	ORIG_START = CPY_START;
	ORIG_END = CPY_END;
	CURSOR = CPY_END + 1;
	inverse_region(CPY_START, CPY_END);

	MOUSE_FLAGS |= SEL_IN_PROGRESS;
	MOUSE_FLAGS |= SEL_EXISTS;
	MOUSE_FLAGS &= ~SEL_BY_CHAR;
	MOUSE_FLAGS &= ~SEL_BY_WORD;
	MOUSE_FLAGS |= SEL_BY_LINE;

	/* mouse cursor hidden in the selection */
	MOUSE_FLAGS &= ~BLANK_TO_EOL;
	MOUSE_FLAGS &= ~MOUSE_VISIBLE;
}

/*
 * End of a copy operation
 */
void
mouse_copy_end(void)
{
	MOUSE_FLAGS &= ~(SEL_IN_PROGRESS);
	if (IS_SEL_BY_WORD(sc->sc_focus) || IS_SEL_BY_LINE(sc->sc_focus)) {
		if (CURSOR != (CPY_END + 1))
			inverse_char(CURSOR);
		CURSOR = CPY_END + 1;
	}
}


/*
 * Generic selection extend function
 */
void
mouse_copy_extend(void)
{
	if (IS_SEL_BY_CHAR(sc->sc_focus))
		mouse_copy_extend_char();
	if (IS_SEL_BY_WORD(sc->sc_focus))
		mouse_copy_extend_word();
	if (IS_SEL_BY_LINE(sc->sc_focus))
		mouse_copy_extend_line();
}

/*
 * Extend a selected region, character by character
 */
void
mouse_copy_extend_char()
{
	unsigned char right;

	if (!IS_SEL_EXT_AFTER(sc->sc_focus)) {

		if (IS_BLANK_TO_EOL(sc->sc_focus)) {
			/*
			 * First extension of selection. We handle special
			 * cases of blank characters to eol
			 */

			right = skip_spc_right(BORDER);
			if (MOUSE > ORIG_START) {
				/* the selection goes to the lower part of
				   the screen */

				/* remove the previous cursor, start of
				   selection is now next line */
				inverse_char(CPY_START);
				CPY_START += (right + 1);
				CPY_END = CPY_START;
				ORIG_START = CPY_START;
				/* simulate the initial mark */
				inverse_char(CPY_START);
			} else {
				/* the selection goes to the upper part
				   of the screen */
				/* remove the previous cursor, start of
				   selection is now at the eol */
				inverse_char(CPY_START);
				ORIG_START += (right + 1);
				CPY_START = ORIG_START - 1;
				CPY_END = ORIG_START - 1;
				/* simulate the initial mark */
				inverse_char(CPY_START);
			}
			MOUSE_FLAGS &= ~ BLANK_TO_EOL;
		}

		if (MOUSE < ORIG_START && CPY_END >= ORIG_START) {
			/* we go to the upper part of the screen */

			/* reverse the old selection region */
			remove_selection(sc);
			CPY_END = ORIG_START - 1;
			CPY_START = ORIG_START;
		}
		if (CPY_START < ORIG_START && MOUSE >= ORIG_START) {
			/* we go to the lower part of the screen */

			/* reverse the old selection region */

			remove_selection(sc);
			CPY_START = ORIG_START;
			CPY_END = ORIG_START - 1;
		}
		/* restore flags cleared in remove_selection() */
		MOUSE_FLAGS |= SEL_IN_PROGRESS;
		MOUSE_FLAGS |= SEL_EXISTS;
	}
	/* beginning of common part */

	if (MOUSE >= ORIG_START) {

		/* lower part of the screen */
		if (MOUSE > CPY_END) {
			/* extending selection */
			inverse_region(CPY_END + 1, MOUSE);
		} else {
			/* reducing selection */
			inverse_region(MOUSE + 1, CPY_END);
		}
		CPY_END = MOUSE;
	} else {
		/* upper part of the screen */
		if (MOUSE < CPY_START) {
			/* extending selection */
			inverse_region(MOUSE,CPY_START - 1);
		} else {
			/* reducing selection */
			inverse_region(CPY_START,MOUSE - 1);
		}
		CPY_START = MOUSE;
	}
	/* end of common part */
}

/*
 * Extend a selected region, word by word
 */
void
mouse_copy_extend_word(void)
{
	unsigned short old_cpy_end;
	unsigned short old_cpy_start;

	if (!IS_SEL_EXT_AFTER(sc->sc_focus)) {

		/* remove cursor in selection (black one) */

		if (CURSOR != (CPY_END + 1))
			inverse_char(CURSOR);

		/* now, switch between lower and upper part of the screen */

		if (MOUSE < ORIG_START && CPY_END >= ORIG_START) {
			/* going to the upper part of the screen */
			inverse_region(ORIG_END + 1, CPY_END);
			CPY_END = ORIG_END;
		}

		if (MOUSE > ORIG_END && CPY_START <= ORIG_START) {
			/* going to the lower part of the screen */
			inverse_region(CPY_START, ORIG_START - 1);
			CPY_START = ORIG_START;
		}
	}

	if (MOUSE >= ORIG_START) {
		/* lower part of the screen */

		if (MOUSE > CPY_END) {
			/* extending selection */

			old_cpy_end = CPY_END;
			CPY_END = MOUSE + skip_char_right(MOUSE);
			inverse_region(old_cpy_end + 1, CPY_END);
		} else {
			if (class_cmp(MOUSE, MOUSE + 1)) {
				/* reducing selection (remove last word) */
				old_cpy_end = CPY_END;
				CPY_END = MOUSE;
				inverse_region(CPY_END + 1, old_cpy_end);
			} else {
				old_cpy_end = CPY_END;
				CPY_END = MOUSE + skip_char_right(MOUSE);
				if (CPY_END != old_cpy_end) {
					/* reducing selection, from the end of
					 * next word */
					inverse_region(CPY_END + 1,
					    old_cpy_end);
				}
			}
		}
	} else {
		/* upper part of the screen */
		if (MOUSE < CPY_START) {
			/* extending selection */
			old_cpy_start = CPY_START;
			CPY_START = MOUSE - skip_char_left(MOUSE);
			inverse_region(CPY_START, old_cpy_start - 1);
		} else {
			if (class_cmp(MOUSE - 1, MOUSE)) {
				/* reducing selection (remove last word) */
				old_cpy_start = CPY_START;
				CPY_START = MOUSE;
				inverse_region(old_cpy_start,
				    CPY_START - 1);
			} else {
				old_cpy_start = CPY_START;
				CPY_START = MOUSE - skip_char_left(MOUSE);
				if (CPY_START != old_cpy_start) {
					inverse_region(old_cpy_start,
					    CPY_START - 1);
				}
			}
		}
	}

	if (!IS_SEL_EXT_AFTER(sc->sc_focus)) {
		/* display new cursor */
		CURSOR = MOUSE;
		inverse_char(CURSOR);
	}
}

/*
 * Extend a selected region, line by line
 */
void
mouse_copy_extend_line(void)
{
	unsigned short old_row;
	unsigned short new_row;
	unsigned short old_cpy_start;
	unsigned short old_cpy_end;

	if (!IS_SEL_EXT_AFTER(sc->sc_focus)) {
		/* remove cursor in selection (black one) */

		if (CURSOR != (CPY_END + 1))
			inverse_char(CURSOR);

		/* now, switch between lower and upper part of the screen */

		if (MOUSE < ORIG_START && CPY_END >= ORIG_START) {
			/* going to the upper part of the screen */
			inverse_region(ORIG_END + 1, CPY_END);
			CPY_END = ORIG_END;
		}

		if (MOUSE > ORIG_END && CPY_START <= ORIG_START) {
			/* going to the lower part of the screen */
			inverse_region(CPY_START, ORIG_START - 1);
			CPY_START = ORIG_START;
		}
	}

	if (MOUSE >= ORIG_START) {
		/* lower part of the screen */
		if (CURSOR == (CPY_END + 1))
			CURSOR = CPY_END;
		old_row = CURSOR / N_COLS;
		new_row = MOUSE / N_COLS;
		old_cpy_end = CPY_END;
		CPY_END = (new_row * N_COLS) + MAXCOL;
		if (new_row > old_row)
			inverse_region(old_cpy_end + 1, CPY_END);
		else if (new_row < old_row)
			inverse_region(CPY_END + 1, old_cpy_end);
	} else {
		/* upper part of the screen */
		old_row = CURSOR / N_COLS;
		new_row = MOUSE / N_COLS;
		old_cpy_start = CPY_START;
		CPY_START = new_row * N_COLS;
		if (new_row < old_row)
			inverse_region(CPY_START, old_cpy_start - 1);
		else if (new_row > old_row)
			inverse_region(old_cpy_start, CPY_START - 1);
	}

	if (!IS_SEL_EXT_AFTER(sc->sc_focus)) {
		/* display new cursor */
		CURSOR = MOUSE;
		inverse_char(CURSOR);
	}
}

void
mouse_hide(struct wsdisplay_softc *sc)
{
	if (IS_MOUSE_VISIBLE(sc->sc_focus)) {
		inverse_char(MOUSE);
		MOUSE_FLAGS &= ~MOUSE_VISIBLE;
	}
}

/*
 * Add an extension to a selected region, word by word
 */
void
mouse_copy_extend_after(void)
{
	unsigned short start_dist;
	unsigned short end_dist;

	if (IS_SEL_EXISTS(sc->sc_focus)) {
		MOUSE_FLAGS |= SEL_EXT_AFTER;
		mouse_hide(sc); /* hide current cursor */

		if (CPY_START > MOUSE)
			start_dist = CPY_START - MOUSE;
		else
			start_dist = MOUSE - CPY_START;
		if (MOUSE > CPY_END)
			end_dist = MOUSE - CPY_END;
		else
			end_dist = CPY_END - MOUSE;
		if (start_dist < end_dist) {
			/* upper part of the screen*/
			ORIG_START = MOUSE + 1;
			/* only used in mouse_copy_extend_line() */
			CURSOR = CPY_START;
		} else {
			/* lower part of the screen */
			ORIG_START = MOUSE;
			/* only used in mouse_copy_extend_line() */
			CURSOR = CPY_END;
		}
		if (IS_SEL_BY_CHAR(sc->sc_focus))
			mouse_copy_extend_char();
		if (IS_SEL_BY_WORD(sc->sc_focus))
			mouse_copy_extend_word();
		if (IS_SEL_BY_LINE(sc->sc_focus))
			mouse_copy_extend_line();
		mouse_copy_selection();
	}
}

/*
 * Remove a previously selected region
 */
void
remove_selection(struct wsdisplay_softc *sc)
{
	if (IS_SEL_EXT_AFTER(sc->sc_focus)) {
		/* reset the flag indicating an extension of selection */
		MOUSE_FLAGS &= ~SEL_EXT_AFTER;
	}
	inverse_region(CPY_START, CPY_END);
	MOUSE_FLAGS &= ~SEL_IN_PROGRESS;
	MOUSE_FLAGS &= ~SEL_EXISTS;
}

/*
 * Put the current visual selection in the selection buffer
 */
void
mouse_copy_selection(void)
{
	unsigned short current = 0;
	unsigned short blank = current;
	unsigned short buf_end = ((N_COLS + 1) * N_ROWS);
	unsigned short sel_cur;
	unsigned short sel_end;

	sel_cur = CPY_START;
	sel_end = CPY_END;

	while (sel_cur <= sel_end && current < buf_end - 1) {
		Copybuffer[current] = (GETCHAR(sel_cur));
		if (!IS_SPACE(Copybuffer[current]))
			blank = current + 1; /* first blank after non-blank */
		current++;
		if (POS_TO_X(sel_cur) == MAXCOL) {
			/* we are on the last col of the screen */
			Copybuffer[blank] = '\r'; /* carriage return */
			current = blank + 1; /* restart just after the carriage
					       return in the buffer */
			blank = current;
		}
		sel_cur++;
	}

	Copybuffer[current] = '\0';
}

/*
 * Paste the current selection
 */
void
mouse_paste(void)
{
	unsigned short len;
	unsigned char *current = Copybuffer;

	if (Paste_avail) {
		for (len = strlen(Copybuffer) ; len > 0; len--) {
			(*linesw[sc->sc_focus->scr_tty->t_line].l_rint)
			    (*current++, sc->sc_focus->scr_tty);
		}
	}
}

/*
 * Handle the z axis.
 * The z axis (roller or wheel) is mapped by default to scrollback.
 */
void
mouse_zaxis(int z)
{
	if (z < 0)
		wsscrollback(sc, WSDISPLAY_SCROLL_BACKWARD);
	else
		wsscrollback(sc, WSDISPLAY_SCROLL_FORWARD);
}

/*
 * Allocate the copy buffer. The size is:
 * (cols + 1) * (rows)
 * (+1 for '\n' at the end of lines),
 * where cols and rows are the maximum of column and rows of all screens.
 */
void
allocate_copybuffer(struct wsdisplay_softc *sc)
{
	int nscreens = sc->sc_scrdata->nscreens;
	int i,s;
	const struct wsscreen_descr **screens_list = sc->sc_scrdata->screens;
	const struct wsscreen_descr *current;
	unsigned short size = Copybuffer_size;

	s = spltty();
	for (i = 0; i < nscreens; i++) {
		current = *screens_list;
		if (( (current->ncols + 1) * current->nrows) > size)
			size = ((current->ncols + 1) * current->nrows);
			screens_list++;
	}
	if ((size != Copybuffer_size) && (Copybuffer_size != 0)) {
		bzero(Copybuffer, Copybuffer_size);
		free(Copybuffer, M_DEVBUF);
	}
	if ((Copybuffer = (char *)malloc(size, M_DEVBUF, M_NOWAIT)) == NULL) {
		printf("wscons: copybuffer memory malloc failed\n");
		Copybuffer_size = 0;
	}
	Copybuffer_size = size;
	splx(s);
}


/* Remove selection and cursor on current screen */
void
mouse_remove(struct wsdisplay_softc *sc)
{
	if (IS_SEL_EXISTS(sc->sc_focus))
		remove_selection(sc);

	mouse_hide(sc);
}

/* Send a wscons event to notify wsmoused(8) to release the mouse device */
void
wsmoused_release(struct wsdisplay_softc *sc)
{
#if NWSMOUSE > 0
	struct device *wsms_dev = NULL;
	struct device **wsms_dev_list;
	int is_wsmouse = 0;
#if NWSMUX > 0
	int is_wsmux = 0;
#endif /* NWSMUX > 0 */

	if (sc->wsmoused_dev) {
		/* wsmoused(8) is running */

		wsms_dev_list = (struct device **) wsmouse_cd.cd_devs;
		if (!wsms_dev_list)
			/* no wsmouse device exists */
			return ;

		/* test whether device opened by wsmoused(8) is a wsmux device
		 * (/dev/wsmouse) or a wsmouse device (/dev/wsmouse{0..n} */

#if NWSMUX > 0
		/* obtain major of /dev/wsmouse multiplexor device */
		/* XXX first member of wsmux_softc is of type struct device */
		if (cdevsw[major(sc->wsmoused_dev)].d_open == wsmuxopen)
			is_wsmux = 1;

		if (is_wsmux && (minor(sc->wsmoused_dev) == WSMOUSEDEVCF_MUX)) {
			/* /dev/wsmouse case */
			/* XXX at least, wsmouse0 exist */
			wsms_dev = wsms_dev_list[0];
		}
#endif /* NWSMUX > 0 */

		/* obtain major of /dev/wsmouse{0..n} devices */
		if (wsmouse_cd.cd_ndevs > 0) {
			if (cdevsw[major(sc->wsmoused_dev)].d_open ==
			     wsmouseopen)
				is_wsmouse = 1;
		}

		if (is_wsmouse && (minor(sc->wsmoused_dev) <= NWSMOUSE)) {
			/* /dev/wsmouseX case */
			if (minor(sc->wsmoused_dev) <= wsmouse_cd.cd_ndevs) {
				wsms_dev =
				    wsms_dev_list[minor(sc->wsmoused_dev)];
			}
			else
				/* no corresponding /dev/wsmouseX device */
				return;
		}

		/* inject event to notify wsmoused(8) to close mouse device */
		if (wsms_dev != NULL) 
			wsmouse_input(wsms_dev, 0, 0, 0, 0,
				      WSMOUSE_INPUT_WSMOUSED_CLOSE);
		
	}
#endif /* NWSMOUSE > 0 */
}

/* Wakeup wsmoused(8), so that the mouse device can be reopened */
void
wsmoused_wakeup(struct wsdisplay_softc *sc)
{
#if NWSMOUSE > 0
	if (sc->wsmoused_dev) {
		sc->wsmoused_sleep = 0;
		wakeup(&sc->wsmoused_sleep);
	}
#endif /* NWSMOUSE > 0 */
}
#endif /* WSMOUSED_SUPPORT */
