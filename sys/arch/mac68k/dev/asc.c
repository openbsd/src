/*	$NetBSD: asc.c,v 1.9 1995/11/01 04:58:21 briggs Exp $	*/

/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ASC driver code and asc_ringbell() support
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/device.h>
#include <machine/cpu.h>


/* Global ASC location */
volatile unsigned char *ASCBase = (unsigned char *) 0x14000;


/* bell support data */
static int bell_freq = 1880;
static int bell_length = 10;
static int bell_volume = 100;
static int bell_ringing = 0;

static int ascprobe __P((struct device *, struct cfdata *, void *));
static void ascattach __P((struct device *, struct device *, void *));
extern int matchbyname __P((struct device *, void *, void *));

struct cfdriver asccd =
{NULL, "asc", matchbyname, ascattach,
DV_DULL, sizeof(struct device), NULL, 0};

static int
ascprobe(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void   *aux;
{
	if (strcmp(*((char **) aux), asccd.cd_name))
		return 0;

	return 1;
}


static void
ascattach(parent, dev, aux)
	struct device *parent, *dev;
	void   *aux;
{
	printf(" Apple sound chip.\n");
}

int 
asc_setbellparams(
    int freq,
    int length,
    int volume)
{
	/* I only perform these checks for sanity. */
	/* I suppose someone might want a bell that rings */
	/* all day, but then the can make kernel mods themselves. */

	if (freq < 10 || freq > 40000)
		return (EINVAL);
	if (length < 0 || length > 3600)
		return (EINVAL);
	if (volume < 0 || volume > 100)
		return (EINVAL);

	bell_freq = freq;
	bell_length = length;
	bell_volume = volume;

	return (0);
}


int 
asc_getbellparams(
    int *freq,
    int *length,
    int *volume)
{
	*freq = bell_freq;
	*length = bell_length;
	*volume = bell_volume;

	return (0);
}


void 
asc_bellstop(
    int param)
{
	if (bell_ringing > 1000 || bell_ringing < 0)
		panic("bell got out of synch?????");
	if (--bell_ringing == 0) {
		ASCBase[0x801] = 0;
	}
	/* disable ASC */
}


int 
asc_ringbell()
{
	int     i;
	unsigned long freq;

	if (bell_ringing == 0) {
		for (i = 0; i < 0x800; i++)
			ASCBase[i] = 0;

		for (i = 0; i < 256; i++) {
			ASCBase[i] = i / 4;
			ASCBase[i + 512] = i / 4;
			ASCBase[i + 1024] = i / 4;
			ASCBase[i + 1536] = i / 4;
		}		/* up part of wave, four voices ? */
		for (i = 0; i < 256; i++) {
			ASCBase[i + 256] = 0x3f - (i / 4);
			ASCBase[i + 768] = 0x3f - (i / 4);
			ASCBase[i + 1280] = 0x3f - (i / 4);
			ASCBase[i + 1792] = 0x3f - (i / 4);
		}		/* down part of wave, four voices ? */

		/* Fix this.  Need to find exact ASC sampling freq */
		freq = 65536 * bell_freq / 466;

		/* printf("beep: from %d, %02x %02x %02x %02x\n",
		 * cur_beep.freq, (freq >> 24) & 0xff, (freq >> 16) & 0xff,
		 * (freq >> 8) & 0xff, (freq) & 0xff); */
		for (i = 0; i < 8; i++) {
			ASCBase[0x814 + 8 * i] = (freq >> 24) & 0xff;
			ASCBase[0x815 + 8 * i] = (freq >> 16) & 0xff;
			ASCBase[0x816 + 8 * i] = (freq >> 8) & 0xff;
			ASCBase[0x817 + 8 * i] = (freq) & 0xff;
		}		/* frequency; should put cur_beep.freq in here
				 * somewhere. */

		ASCBase[0x807] = 3;	/* 44 ? */
		ASCBase[0x806] = 255 * bell_volume / 100;
		ASCBase[0x805] = 0;
		ASCBase[0x80f] = 0;
		ASCBase[0x802] = 2;	/* sampled */
		ASCBase[0x801] = 2;	/* enable sampled */
	}
	bell_ringing++;
	timeout((void *) asc_bellstop, 0, bell_length);
}
