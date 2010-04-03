/*	$OpenBSD: conf.h,v 1.14 2010/04/03 17:59:17 ratchov Exp $	*/
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
#ifndef CONF_H
#define CONF_H

#ifdef DEBUG
/*
 * Debug trace levels:
 *
 * 0 - fatal errors: bugs, asserts, internal errors.
 * 1 - warnings: bugs in clients, failed allocations, non-fatal errors.
 * 2 - misc information (hardware parameters, incoming clients)
 * 3 - structural changes (new aproc structures and files stream params changes)
 * 4 - data blocks and messages
 */
extern int debug_level;
#endif

/*
 * Number of blocks in the device play/record buffers.  Because Sun API
 * cannot notify apps of the current positions, we have to use all N
 * buffers devices blocks plus one extra block, to make write() block,
 * so that poll() can return the exact postition.
 */
#define DEV_NBLK 2

/*
 * Number of blocks in the wav-file i/o buffers.
 */
#define WAV_NBLK 6

/*
 * socket and option names
 */
#define DEFAULT_MIDITHRU	"midithru"
#define DEFAULT_SOFTAUDIO	"softaudio"
#define DEFAULT_OPT		"default"

/*
 * MIDI buffer size
 */
#define MIDI_BUFSZ		3125	/* 1 second at 31.25kbit/s */

/*
 * units used for MTC clock.
 */
#define MTC_SEC			2400	/* 1 second is 2400 ticks */

#endif /* !defined(CONF_H) */
