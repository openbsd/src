/*	$OpenBSD: wsfont.c,v 1.40 2015/03/14 03:38:50 jsg Exp $ */
/*	$NetBSD: wsfont.c,v 1.17 2001/02/07 13:59:24 ad Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>

#include "wsfont_glue.h"	/* NRASOPS_ROTATION */

#undef HAVE_FONT

#ifdef FONT_QVSS8x15
#define HAVE_FONT 1
#include <dev/wsfont/qvss8x15.h>
#endif

#ifdef FONT_LUCIDA16x29
#define HAVE_FONT 1
#include <dev/wsfont/lucida16x29.h>
#endif

#ifdef FONT_VT220L8x8
#define HAVE_FONT 1
#include <dev/wsfont/vt220l8x8.h>
#endif

#ifdef FONT_VT220L8x10
#define HAVE_FONT 1
#include <dev/wsfont/vt220l8x10.h>
#endif

#ifdef FONT_SONY8x16
#define HAVE_FONT 1
#include <dev/wsfont/sony8x16.h>
#endif

#ifdef FONT_SONY12x24
#define HAVE_FONT 1
#include <dev/wsfont/sony12x24.h>
#endif

#ifdef FONT_OMRON12x20
#define HAVE_FONT 1
#include <dev/wsfont/omron12x20.h>
#endif

#ifdef FONT_BOLD8x16
#define HAVE_FONT 1
#include <dev/wsfont/bold8x16.h>
#endif

#ifdef FONT_GALLANT12x22
#define HAVE_FONT 1
#endif

#ifdef FONT_BOLD8x16_ISO1
#define HAVE_FONT 1
#endif

/*
 * Make sure we always have at least one font.
 * Some platforms (currently luna88k, sgi, sparc and sparc64) always provide
 * a 8x16 font and a larger 12x22 font.
 * Other platforms also provide both, but the 12x22 font is omitted if
 * option SMALL_KERNEL.
 */
#ifndef HAVE_FONT
#define HAVE_FONT 1

#define	FONT_BOLD8x16_ISO1
#if defined(__luna88k__) || defined(__sgi__) || defined(__sparc__) || \
    defined(__sparc64__) || !defined(SMALL_KERNEL)
#define	FONT_GALLANT12x22
#endif

#endif	/* HAVE_FONT */

#ifdef FONT_BOLD8x16_ISO1
#include <dev/wsfont/bold8x16-iso1.h>
#endif

#ifdef FONT_GALLANT12x22
#include <dev/wsfont/gallant12x22.h>
#endif

struct font {
	TAILQ_ENTRY(font) chain;
	struct	wsdisplay_font *font;
	u_short	lockcount;
	u_short	cookie;
	u_short	flg;
};
TAILQ_HEAD(, font) fontlist;

/* Our list of built-in fonts */
static struct font builtin_fonts[] = {
#define BUILTIN_FONT(f, c) \
	{ .font = &(f), .cookie = (c), .lockcount = 0, \
	  .flg = WSFONT_STATIC | WSFONT_BUILTIN }
#ifdef FONT_BOLD8x16
	BUILTIN_FONT(bold8x16, 1),
#endif
#ifdef FONT_BOLD8x16_ISO1
	BUILTIN_FONT(bold8x16_iso1, 2),
#endif
#ifdef FONT_COURIER11x18
	BUILTIN_FONT(courier11x18, 3),
#endif
#ifdef FONT_GALLANT12x22
	BUILTIN_FONT(gallant12x22, 4),
#endif
#ifdef FONT_LUCIDA16x29
	BUILTIN_FONT(lucida16x29, 5),
#endif
#ifdef FONT_QVSS8x15
	BUILTIN_FONT(qvss8x15, 6),
#endif
#ifdef FONT_VT220L8x8
	BUILTIN_FONT(vt220l8x8, 7),
#endif
#ifdef FONT_VT220L8x10
	BUILTIN_FONT(vt220l8x10, 8),
#endif
#ifdef FONT_SONY8x16
	BUILTIN_FONT(sony8x16, 9),
#endif
#ifdef FONT_SONY12x24
	BUILTIN_FONT(sony12x24, 10),
#endif
#ifdef FONT_OMRON12x20
	BUILTIN_FONT(omron12x20, 11),
#endif
#undef BUILTIN_FONT
};

#if !defined(SMALL_KERNEL) || defined(__alpha__)

/* Reverse the bit order in a byte */
static const u_char reverse[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

#endif

static struct font *wsfont_find0(int);

#if !defined(SMALL_KERNEL) || defined(__alpha__)

/*
 * Reverse the bit order of a font
 */
static void	wsfont_revbit(struct wsdisplay_font *);
static void
wsfont_revbit(struct wsdisplay_font *font)
{
	u_char *p, *m;

	p = (u_char *)font->data;
	m = p + font->stride * font->numchars * font->fontheight;

	for (; p < m; p++)
		*p = reverse[*p];
}

#endif

#if !defined(SMALL_KERNEL)

/*
 * Reverse the byte order of a font
 */
static void	wsfont_revbyte(struct wsdisplay_font *);
static void
wsfont_revbyte(struct wsdisplay_font *font)
{
	int x, l, r, nr;
	u_char *rp;

	if (font->stride == 1)
		return;

	rp = (u_char *)font->data;
	nr = font->numchars * font->fontheight;

	while (nr--) {
		l = 0;
		r = font->stride - 1;

		while (l < r) {
			x = rp[l];
			rp[l] = rp[r];
			rp[r] = x;
			l++, r--;
		}

		rp += font->stride;
	}
}

#endif

/*
 * Enumerate the list of fonts
 */
void
wsfont_enum(int (*cb)(void *, struct wsdisplay_font *), void *cbarg)
{
	struct font *ent;
	int s;

	s = splhigh();

	TAILQ_FOREACH(ent, &fontlist, chain)
		if (cb(cbarg, ent->font) != 0)
			break;

	splx(s);
}

#if NRASOPS_ROTATION > 0

struct wsdisplay_font *wsfont_rotate_internal(struct wsdisplay_font *);

struct wsdisplay_font *
wsfont_rotate_internal(struct wsdisplay_font *font)
{
	int b, n, r, newstride;
	struct wsdisplay_font *newfont;
	char *newbits;

	/* Duplicate the existing font... */
	newfont = malloc(sizeof *font, M_DEVBUF, M_WAITOK);

	bcopy(font, newfont, sizeof *font);
	newfont->cookie = NULL;

	/* Allocate a buffer big enough for the rotated font. */
	newstride = (font->fontheight + 7) / 8;
	newbits = mallocarray(font->numchars, newstride * font->fontwidth,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	/* Rotate the font a bit at a time. */
	for (n = 0; n < font->numchars; n++) {
		char *ch = font->data + (n * font->stride * font->fontheight);

		for (r = 0; r < font->fontheight; r++) {
			for (b = 0; b < font->fontwidth; b++) {
				unsigned char *rb;

				rb = ch + (font->stride * r) + (b / 8);
				if (*rb & (0x80 >> (b % 8))) {
					unsigned char *rrb;

					rrb = newbits + newstride - 1 - (r / 8)
					    + (n * newstride * font->fontwidth)
					    + (newstride * b);
					*rrb |= (1 << (r % 8));
				}
			}
		}
	}

	newfont->data = newbits;

	/* Update font sizes. */
	newfont->stride = newstride;
	newfont->fontwidth = font->fontheight;
	newfont->fontheight = font->fontwidth;

	if (wsfont_add(newfont, 0) != 0) {
		/*
		 * If we seem to have rotated this font already, drop the
		 * new one...
		 */
		free(newbits, M_DEVBUF, 0);
		free(newfont, M_DEVBUF, 0);
		newfont = NULL;
	}

	return (newfont);
}

int
wsfont_rotate(int cookie)
{
	int s, ncookie;
	struct wsdisplay_font *font;
	struct font *origfont;

	s = splhigh();
	origfont = wsfont_find0(cookie);
	splx(s);

	font = wsfont_rotate_internal(origfont->font);
	if (font == NULL)
		return (-1);

	ncookie = wsfont_find(font->name, font->fontwidth, font->fontheight,
	    font->stride);

	return (ncookie);
}

#endif	/* NRASOPS_ROTATION */

/*
 * Initialize list with WSFONT_BUILTIN fonts
 */
void
wsfont_init(void)
{
	static int again;
	unsigned int i;

	if (again != 0)
		return;
	again = 1;

	TAILQ_INIT(&fontlist);

	for (i = 0; i < nitems(builtin_fonts); i++) {
		TAILQ_INSERT_TAIL(&fontlist, &builtin_fonts[i], chain);
	}
}

/*
 * Find a font by cookie. Called at splhigh.
 */
static struct font *
wsfont_find0(int cookie)
{
	struct font *ent;

	TAILQ_FOREACH(ent, &fontlist, chain)
		if (ent->cookie == cookie)
			return (ent);

	return (NULL);
}

/*
 * Find a font.
 */
int
wsfont_find(const char *name, int width, int height, int stride)
{
	struct font *ent;
	int s;

	s = splhigh();

	TAILQ_FOREACH(ent, &fontlist, chain) {
		if (height != 0 && ent->font->fontheight != height)
			continue;

		if (width != 0 && ent->font->fontwidth != width)
			continue;

		if (stride != 0 && ent->font->stride != stride)
			continue;

		if (name != NULL && strcmp(ent->font->name, name) != 0)
			continue;

		splx(s);
		return (ent->cookie);
	}

	splx(s);
	return (-1);
}

/*
 * Add a font to the list.
 */
int
wsfont_add(struct wsdisplay_font *font, int copy)
{
	static int cookiegen = 666;
	struct font *ent;
	int s;

	s = splhigh();

	/* Don't allow exact duplicates */
	if (wsfont_find(font->name, font->fontwidth, font->fontheight,
	    font->stride) >= 0) {
		splx(s);
		return (-1);
	}

	ent = (struct font *)malloc(sizeof *ent, M_DEVBUF, M_WAITOK);

	ent->lockcount = 0;
	ent->flg = 0;
	ent->cookie = cookiegen++;

	/*
	 * If we are coming from a WSDISPLAYIO_LDFONT ioctl, we need to
	 * make a copy of the wsdisplay_font struct, but not of font->bits.
	 */
	if (copy) {
		ent->font = (struct wsdisplay_font *)malloc(sizeof *ent->font,
		    M_DEVBUF, M_WAITOK);
		memcpy(ent->font, font, sizeof(*ent->font));
		ent->flg = 0;
	} else {
		ent->font = font;
		ent->flg = WSFONT_STATIC;
	}

	/* Now link into the list and return */
	TAILQ_INSERT_TAIL(&fontlist, ent, chain);
	splx(s);
	return (0);
}

/*
 * Remove a font.
 */
#ifdef notyet
int
wsfont_remove(int cookie)
{
	struct font *ent;
	int s;

	s = splhigh();

	if ((ent = wsfont_find0(cookie)) == NULL) {
		splx(s);
		return (-1);
	}

	if ((ent->flg & WSFONT_BUILTIN) != 0 || ent->lockcount != 0) {
		splx(s);
		return (-1);
	}

	/* Don't free statically allocated font data */
	if ((ent->flg & WSFONT_STATIC) != 0) {
		free(ent->font->data, M_DEVBUF, 0);
		free(ent->font, M_DEVBUF, 0);
	}

	/* Remove from list, free entry */
	TAILQ_REMOVE(&list, ent, chain);
	free(ent, M_DEVBUF, 0);
	splx(s);
	return (0);
}
#endif

/*
 * Lock a given font and return new lockcount. This fails if the cookie
 * is invalid, or if the font is already locked and the bit/byte order
 * requested by the caller differs.
 */
int
wsfont_lock(int cookie, struct wsdisplay_font **ptr, int bitorder,
    int byteorder)
{
	struct font *ent;
	int s, lc;

	s = splhigh();

	if ((ent = wsfont_find0(cookie)) != NULL) {
		if (bitorder && bitorder != ent->font->bitorder) {
#if !defined(SMALL_KERNEL) || defined(__alpha__)
			if (ent->lockcount) {
				splx(s);
				return (-1);
			}
			wsfont_revbit(ent->font);
			ent->font->bitorder = bitorder;
#else
			splx(s);
			return (-1);
#endif
		}

		if (byteorder && byteorder != ent->font->byteorder) {
#if !defined(SMALL_KERNEL)
			if (ent->lockcount) {
				splx(s);
				return (-1);
			}
			wsfont_revbyte(ent->font);
			ent->font->byteorder = byteorder;
#else
			splx(s);
			return (-1);
#endif
		}

		lc = ++ent->lockcount;
		*ptr = ent->font;
	} else
		lc = -1;

	splx(s);
	return (lc);
}

/*
 * Unlock a given font and return new lockcount.
 */
int
wsfont_unlock(int cookie)
{
	struct font *ent;
	int s, lc;

	s = splhigh();

	if ((ent = wsfont_find0(cookie)) != NULL) {
		if (ent->lockcount == 0)
			panic("wsfont_unlock: font not locked");
		lc = --ent->lockcount;
	} else
		lc = -1;

	splx(s);
	return (lc);
}

#if !defined(SMALL_KERNEL)

/*
 * Unicode to font encoding mappings
 */

/*
 * To save memory, font encoding tables use a two level lookup.
 * First the high byte of the Unicode is used to lookup the level 2
 * table, then the low byte indexes that table.  Level 2 tables that are
 * not needed are omitted (NULL), and both level 1 and level 2 tables
 * have base and size attributes to keep their size down.
 */

struct wsfont_level1_glyphmap {
	struct wsfont_level2_glyphmap **level2;
	int base;	/* High byte for first level2 entry	*/
	int size;	/* Number of level2 entries		*/
};

struct wsfont_level2_glyphmap {
	int base;	/* Low byte for first character		*/
	int size;	/* Number of characters			*/
	void *chars;	/* Pointer to character number entries  */
	int width;	/* Size of each entry in bytes (1,2,4)  */
};

/*
 * IBM 437 maps
 */

static u_int8_t
ibm437_chars_0[] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	255,173,155,156, 0, 157, 0,  0,  0,  0, 166,174,170, 0,  0,  0,
	 0, 241,253, 0,  0,  0,  0, 249, 0,  0, 167,175,172,171, 0, 168,
	 0,  0,  0,  0, 142,143,146,128, 0, 144, 0,  0,  0,  0,  0,  0,
	 0, 165, 0,  0,  0,  0, 153, 0,  0,  0,  0,  0, 154, 0,  0,  0,
	133,160,131, 0, 132,134,145,135,138,130,136,137,141,161,140,139,
	 0, 164,149,162,147, 0, 148,246, 0, 151,163,150,129, 0,  0, 152
},
ibm437_chars_1[] = {
	159
},
ibm437_chars_3[] = {
	226, 0,  0,  0,  0, 233, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	228, 0,  0, 232, 0,  0, 234, 0,  0,  0,  0,  0,  0,  0, 224,225,
	 0, 235,238, 0,  0,  0,  0,  0,  0, 230, 0,  0,  0, 227, 0,  0,
	229,231
},
ibm437_chars_32[] = {
	252, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0, 158
},
ibm437_chars_34[] = {
	237, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0, 248,250,251, 0,  0,  0, 236, 0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0, 239, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0, 247, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,240,  0,  0,243,
	242
},
ibm437_chars_35[] = {
	169, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	244,245
},
ibm437_chars_37[] = {
	196,205,179,186, 0,  0,  0,  0,  0,  0,  0,  0, 218,213,214,201,
	191,184,183,187,192,212,211,200,217,190,189,188,195,198, 0,  0,
	199, 0,  0, 204,180,181, 0,  0, 182, 0,  0, 185,194, 0,  0, 209,
	210, 0,  0, 203,193, 0,  0, 207,208, 0,  0, 202,197, 0,  0, 216,
	 0,  0, 215, 0,  0,  0,  0,  0,  0,  0,  0, 206, 0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	223, 0,  0,  0, 220, 0,  0,  0, 219, 0,  0,  0, 221, 0,  0,  0,
	222,176,177,178, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	254
};

static struct wsfont_level2_glyphmap
ibm437_level2_0 = { 0, 256, ibm437_chars_0, 1 },
ibm437_level2_1 = { 146, 1, ibm437_chars_1, 1 },
ibm437_level2_3 = { 147, 50, ibm437_chars_3, 1 },
ibm437_level2_32 = { 127, 41, ibm437_chars_32, 1 },
ibm437_level2_34 = { 5, 97, ibm437_chars_34, 1 },
ibm437_level2_35 = { 16, 18, ibm437_chars_35, 1 },
ibm437_level2_37 = { 0, 161, ibm437_chars_37, 1 };

static struct wsfont_level2_glyphmap *ibm437_level1[] = {
	&ibm437_level2_0, &ibm437_level2_1, NULL, &ibm437_level2_3,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	&ibm437_level2_32, NULL, &ibm437_level2_34, &ibm437_level2_35,
	NULL, &ibm437_level2_37
};

static struct wsfont_level1_glyphmap encodings[] = {
	/* WSDISPLAY_FONTENC_ISO */
	{ NULL, 0, 0 },
	/* WSDISPLAY_FONTENC_IBM */
	{ ibm437_level1, 0, nitems(ibm437_level1) }
};

#endif	/* !SMALL_KERNEL */

/*
 * Remap Unicode character to glyph
 */
int
wsfont_map_unichar(struct wsdisplay_font *font, int c)
{
	if (font->encoding == WSDISPLAY_FONTENC_ISO)
		return (c);

#if !defined(SMALL_KERNEL)
	if (font->encoding >= 0 && font->encoding < nitems(encodings)) {
		int hi = (c >> 8), lo = c & 255;
		struct wsfont_level1_glyphmap *map1 =
		    &encodings[font->encoding];
		struct wsfont_level2_glyphmap *map2;

		hi -= map1->base;

		if (hi >= 0 && hi < map1->size &&
		    (map2 = map1->level2[hi]) != NULL) {
			lo -= map2->base;

			if (lo >= 0 && lo < map2->size) {
				switch (map2->width) {
				case 1:
					c = (((u_int8_t *)map2->chars)[lo]);
					break;
				case 2:
					c = (((u_int16_t *)map2->chars)[lo]);
					break;
				case 4:
					c = (((u_int32_t *)map2->chars)[lo]);
					break;
				}

				if (c != 0 || lo == 0)
					return (c);
			}
		}
	}
#endif	/* !SMALL_KERNEL */

	return (-1);
}
