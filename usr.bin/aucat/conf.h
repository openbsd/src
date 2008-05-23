/*	$OpenBSD: conf.h,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef ACONF_H
#define ACONF_H

/*
 * debug trace levels:
 *
 * 0 - traces are off
 * 1 - init, free, stuff that's done only once
 * 2 - rare real-time events: eof / hup, etc...
 * 3 - poll(), block / unblock state changes
 * 4 - read()/write()
 */
#ifdef DEBUG

/* defined in main.c */
void debug_printf(int, char *, char *, ...);
extern int debug_level;

#define DPRINTF(...) DPRINTFN(1, __VA_ARGS__)
#define DPRINTFN(n, ...)					\
	do {							\
		if (debug_level >= (n))				\
			fprintf(stderr, __VA_ARGS__);		\
	} while(0)
#else
#define DPRINTF(...) do {} while(0)
#define DPRINTFN(n, ...) do {} while(0)
#endif


#define MIDI_MAXCTL		127
#define MIDI_TO_ADATA(m)	((ADATA_UNIT * (m) + 64) / 127)

#define DEFAULT_NFR	0x400		/* buf size in frames */
#define DEFAULT_NBLK	0x8		/* blocks per buffer */
#define DEFAULT_DEVICE	"/dev/audio"	/* defaul device */

#endif /* !defined(CONF_H) */
