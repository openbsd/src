/*	$OpenBSD: diofb_mono.c,v 1.1 2006/04/14 21:05:43 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat.
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
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1991 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Mark Davies of the Department of Computer
 * Science, Victoria University of Wellington, New Zealand.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 * from: Utah $Hdr: grf_hy.c 1.2 93/08/13$
 *
 *	@(#)grf_hy.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics routines for real 1bpp frame buffers (i.e. pixels not being
 * byte-addressed)
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <hp300/dev/diofbreg.h>
#include <hp300/dev/diofbvar.h>

#include <hp300/dev/maskbits.h>

/* NOTE:
 * the first element in starttab could be 0xffffffff.  making it 0
 * lets us deal with a full first word in the middle loop, rather
 * than having to do the multiple reads and masks that we'd
 * have to do if we thought it was partial.
 */
const int starttab[32] = {
	0x00000000,
	0x7FFFFFFF,
	0x3FFFFFFF,
	0x1FFFFFFF,
	0x0FFFFFFF,
	0x07FFFFFF,
	0x03FFFFFF,
	0x01FFFFFF,
	0x00FFFFFF,
	0x007FFFFF,
	0x003FFFFF,
	0x001FFFFF,
	0x000FFFFF,
	0x0007FFFF,
	0x0003FFFF,
	0x0001FFFF,
	0x0000FFFF,
	0x00007FFF,
	0x00003FFF,
	0x00001FFF,
	0x00000FFF,
	0x000007FF,
	0x000003FF,
	0x000001FF,
	0x000000FF,
	0x0000007F,
	0x0000003F,
	0x0000001F,
	0x0000000F,
	0x00000007,
	0x00000003,
	0x00000001
};

const int endtab[32] = {
	0x00000000,
	0x80000000,
	0xC0000000,
	0xE0000000,
	0xF0000000,
	0xF8000000,
	0xFC000000,
	0xFE000000,
	0xFF000000,
	0xFF800000,
	0xFFC00000,
	0xFFE00000,
	0xFFF00000,
	0xFFF80000,
	0xFFFC0000,
	0xFFFE0000,
	0xFFFF0000,
	0xFFFF8000,
	0xFFFFC000,
	0xFFFFE000,
	0xFFFFF000,
	0xFFFFF800,
	0xFFFFFC00,
	0xFFFFFE00,
	0xFFFFFF00,
	0xFFFFFF80,
	0xFFFFFFC0,
	0xFFFFFFE0,
	0xFFFFFFF0,
	0xFFFFFFF8,
	0xFFFFFFFC,
	0xFFFFFFFE
};

void
diofb_mono_windowmove(struct diofb *fb, u_int16_t sx, u_int16_t sy,
    u_int16_t dx, u_int16_t dy, u_int16_t cx, u_int16_t cy, int rop)
{
	int width;		/* add to get to same position in next line */

	unsigned int *psrcLine, *pdstLine;
				/* pointers to line with current src and dst */
	unsigned int *psrc;	/* pointer to current src longword */
	unsigned int *pdst;	/* pointer to current dst longword */

				/* following used for looping through a line */
	unsigned int startmask, endmask;  /* masks for writing ends of dst */
	int nlMiddle;		/* whole longwords in dst */
	int nl;			/* temp copy of nlMiddle */
	unsigned int tmpSrc;
				/* place to store full source word */
	int xoffSrc;		/* offset (>= 0, < 32) from which to
				   fetch whole longwords fetched in src */
	int nstart;		/* number of ragged bits at start of dst */
	int nend;		/* number of ragged bits at end of dst */
	int srcStartOver;	/* pulling nstart bits from src
				   overflows into the next word? */

	width = fb->fbwidth >> 5;

	if (sy < dy) {	/* start at last scanline of rectangle */
		psrcLine = ((u_int *)fb->fbkva) + ((sy + cy - 1) * width);
		pdstLine = ((u_int *)fb->fbkva) + ((dy + cy - 1) * width);
		width = -width;
	} else {	/* start at first scanline */
		psrcLine = ((u_int *)fb->fbkva) + (sy * width);
		pdstLine = ((u_int *)fb->fbkva) + (dy * width);
	}

	/* x direction doesn't matter for < 1 longword */
	if (cx <= 32) {
		int srcBit, dstBit;	/* bit offset of src and dst */

		pdstLine += (dx >> 5);
		psrcLine += (sx >> 5);
		psrc = psrcLine;
		pdst = pdstLine;

		srcBit = sx & 0x1f;
		dstBit = dx & 0x1f;

		while (cy--) {
			getandputrop(psrc, srcBit, dstBit, cx, pdst, rop);
			pdst += width;
			psrc += width;
		}
	} else {
		maskbits(dx, cx, startmask, endmask, nlMiddle);
		if (startmask)
			nstart = 32 - (dx & 0x1f);
		else
			nstart = 0;
		if (endmask)
			nend = (dx + cx) & 0x1f;
		else
			nend = 0;

		xoffSrc = ((sx & 0x1f) + nstart) & 0x1f;
		srcStartOver = ((sx & 0x1f) + nstart) > 31;

		if (sx >= dx) {	/* move left to right */
			pdstLine += (dx >> 5);
			psrcLine += (sx >> 5);

			while (cy--) {
				psrc = psrcLine;
				pdst = pdstLine;

				if (startmask) {
					getandputrop(psrc, (sx & 0x1f),
					    (dx & 0x1f), nstart, pdst, rop);
					pdst++;
					if (srcStartOver)
						psrc++;
				}

				/* special case for aligned operations */
				if (xoffSrc == 0) {
					nl = nlMiddle;
					while (nl--) {
						DoRop(*pdst, rop, *psrc++,
						    *pdst);
						pdst++;
					}
				} else {
					nl = nlMiddle + 1;
					while (--nl) {
						getunalignedword(psrc, xoffSrc,
						    tmpSrc);
						DoRop(*pdst, rop, tmpSrc,
						    *pdst);
						pdst++;
						psrc++;
					}
				}

				if (endmask) {
					getandputrop0(psrc, xoffSrc, nend,
					    pdst, rop);
				}

				pdstLine += width;
				psrcLine += width;
			}
		} else {	/* move right to left */
			pdstLine += ((dx + cx) >> 5);
			psrcLine += ((sx + cx) >> 5);
			/*
			 * If fetch of last partial bits from source crosses
			 * a longword boundary, start at the previous longword
			 */
			if (xoffSrc + nend >= 32)
				--psrcLine;

			while (cy--) {
				psrc = psrcLine;
				pdst = pdstLine;

				if (endmask) {
					getandputrop0(psrc, xoffSrc, nend,
					    pdst, rop);
				}

				nl = nlMiddle + 1;
				while (--nl) {
					--psrc;
					--pdst;
					getunalignedword(psrc, xoffSrc, tmpSrc);
					DoRop(*pdst, rop, tmpSrc, *pdst);
				}

				if (startmask) {
					if (srcStartOver)
						--psrc;
					--pdst;
					getandputrop(psrc, (sx & 0x1f),
					    (dx & 0x1f), nstart, pdst, rop);
				}

				pdstLine += width;
				psrcLine += width;
			}
		}
	}
}
