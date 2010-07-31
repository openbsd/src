/*	$OpenBSD: hidkbdsc.h,v 1.1 2010/07/31 16:04:50 miod Exp $	*/
/*      $NetBSD: ukbd.c,v 1.85 2003/03/11 16:44:00 augustss Exp $        */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

#define MAXKEYCODE 6
#define MAXMOD 8		/* max 32 */

#define MAXKEYS (MAXMOD+2*MAXKEYCODE)

struct hidkbd_data {
	u_int32_t	modifiers;
	u_int8_t	keycode[MAXKEYCODE];
};

struct hidkbd {
	/* stored data */
	struct hidkbd_data sc_ndata;
	struct hidkbd_data sc_odata;

	/* input reports */
	struct hid_location sc_modloc[MAXMOD];
	u_int sc_nmod;
	struct {
		u_int32_t mask;
		u_int8_t key;
	} sc_mods[MAXMOD];

	struct hid_location sc_keycodeloc;
	u_int sc_nkeycode;

	/* output reports */
	struct hid_location sc_numloc;
	struct hid_location sc_capsloc;
	struct hid_location sc_scroloc;
	int sc_leds;

	/* state information */
	struct device *sc_device;
	struct device *sc_wskbddev;
	char sc_enabled;

	char sc_console_keyboard;	/* we are the console keyboard */

	char sc_debounce;		/* for quirk handling */
	struct timeout sc_delay;	/* for quirk handling */
	struct hidkbd_data sc_data;	/* for quirk handling */

	/* key repeat logic */
	struct timeout sc_rawrepeat_ch;
#if defined(WSDISPLAY_COMPAT_RAWKBD)
#define REP_DELAY1 400
#define REP_DELAYN 100
	int sc_rawkbd;
	int sc_nrep;
	char sc_rep[MAXKEYS];
#endif /* defined(WSDISPLAY_COMPAT_RAWKBD) */

	int sc_polling;
	int sc_npollchar;
	u_int16_t sc_pollchars[MAXKEYS];
};

int	hidkbd_attach(struct device *, struct hidkbd *, int, uint32_t,
	    int, void *, int);
void	hidkbd_attach_wskbd(struct hidkbd *, kbd_t,
	    const struct wskbd_accessops *);
void	hidkbd_bell(u_int, u_int, u_int, int);
void	hidkbd_cngetc(struct hidkbd *, u_int *, int *);
int	hidkbd_detach(struct hidkbd *, int);
int	hidkbd_enable(struct hidkbd *, int);
void	hidkbd_input(struct hidkbd *, uint8_t *, u_int);
int	hidkbd_ioctl(struct hidkbd *, u_long, caddr_t, int, struct proc *);
int	hidkbd_set_leds(struct hidkbd *, int, uint8_t *);

extern int hidkbd_is_console;
