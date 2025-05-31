/*	$OpenBSD: gmon.c,v 1.35 2025/05/31 14:06:26 deraadt Exp $ */
/*-
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/time.h>
#include <sys/gmon.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

struct gmonparam _gmonparam = { GMON_PROF_OFF };

static int	s_scale;
/* see profil(2) where this is describe (incorrectly) */
#define		SCALE_1_TO_1	0x10000L

#define ERR(s) write(STDERR_FILENO, s, sizeof(s))

PROTO_NORMAL(moncontrol);
PROTO_DEPRECATED(monstartup);

/* XXX remove after May 2025 */
void
monstartup(u_long lowpc, u_long highpc)
{
	abort();
}

void
_monstartup(u_long lowpc, u_long highpc)
{
	int o;
	struct gmonparam *p = &_gmonparam;
	char *profdir = NULL;
	void *addr;

	/*
	 * round lowpc and highpc to multiples of the density we're using
	 * so the rest of the scaling (here and in gprof) stays in ints.
	 */
	p->lowpc = ROUNDDOWN(lowpc, HISTFRACTION * sizeof(HISTCOUNTER));
	p->highpc = ROUNDUP(highpc, HISTFRACTION * sizeof(HISTCOUNTER));
	p->textsize = p->highpc - p->lowpc;
	p->kcountsize = p->textsize / HISTFRACTION;
	p->hashfraction = HASHFRACTION;
	p->fromssize = p->textsize / p->hashfraction;
	p->tolimit = p->textsize * ARCDENSITY / 100;
	if (p->tolimit < MINARCS)
		p->tolimit = MINARCS;
	else if (p->tolimit > MAXARCS)
		p->tolimit = MAXARCS;
	p->tossize = p->tolimit * sizeof(struct tostruct);

	p->outbuflen = sizeof(struct gmonhdr) + p->kcountsize +
	    MAXARCS * sizeof(struct rawarc);

	/* Create a contig output buffer */
	addr = mmap(NULL, p->outbuflen,  PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED)
		goto mapfailed;
	p->outbuf = addr;
	p->kcount = (void *)((char *)addr + sizeof(struct gmonhdr));
	p->rawarcs = (void *)((char *)addr + sizeof(struct gmonhdr) + p->kcountsize);

	addr = mmap(NULL, p->fromssize,  PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED)
		goto mapfailed;
	p->froms = addr;

	addr = mmap(NULL, p->tossize,  PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED)
		goto mapfailed;
	p->tos = addr;
	p->tos[0].link = 0;

	o = p->highpc - p->lowpc;
	if (p->kcountsize < o) {
#ifndef notdef
		s_scale = ((float)p->kcountsize / o ) * SCALE_1_TO_1;
#else /* avoid floating point */
		int quot = o / p->kcountsize;

		if (quot >= 0x10000)
			s_scale = 1;
		else if (quot >= 0x100)
			s_scale = 0x10000 / quot;
		else if (o >= 0x800000)
			s_scale = 0x1000000 / (o / (p->kcountsize >> 8));
		else
			s_scale = 0x1000000 / ((o << 8) / p->kcountsize);
#endif
	} else
		s_scale = SCALE_1_TO_1;

	if (issetugid() == 0)
		profdir = getenv("PROFDIR");
	if (profdir)
		p->dirfd = open(profdir, O_DIRECTORY, 0);
	else
		p->dirfd = -1;

	moncontrol(1);

	if (p->dirfd != -1)
		close(p->dirfd);
	return;

mapfailed:
	if (p->froms != NULL) {
		munmap(p->froms, p->fromssize);
		p->froms = NULL;
	}
	if (p->tos != NULL) {
		munmap(p->tos, p->tossize);
		p->tos = NULL;
	}
	ERR("_monstartup: out of memory\n");
}

void
_mcleanup(void)
{
	int fromindex, endfrom, totarc = 0, toindex;
	u_long frompc;
	struct rawarc rawarc;
	struct gmonparam *p = &_gmonparam;
	struct gmonhdr *hdr;
	struct clockinfo clockinfo;
	const int mib[2] = { CTL_KERN, KERN_CLOCKRATE };
	size_t size;
#ifdef DEBUG
	int log, len;
	char dbuf[200];
#endif

	if (p->state == GMON_PROF_ERROR)
		ERR("_mcleanup: tos overflow\n");

	/*
	 * There is nothing we can do if sysctl(2) fails or if
	 * clockinfo.hz is unset.
	 */
	size = sizeof(clockinfo);
	if (sysctl(mib, 2, &clockinfo, &size, NULL, 0) == -1) {
		clockinfo.profhz = 0;
	} else if (clockinfo.profhz == 0) {
		clockinfo.profhz = clockinfo.hz;	/* best guess */
	}

	moncontrol(0);

#ifdef DEBUG
	log = open("gmon.log", O_CREAT|O_TRUNC|O_WRONLY, 0664);
	if (log == -1) {
		perror("mcount: gmon.log");
		close(fd);
		return;
	}
	snprintf(dbuf, sizeof dbuf, "[mcleanup1] kcount 0x%x ssiz %d\n",
	    p->kcount, p->kcountsize);
	write(log, dbuf, strlen(dbuf));
#endif
	hdr = (struct gmonhdr *)p->outbuf;
	hdr->lpc = p->lowpc;
	hdr->hpc = p->highpc;
	hdr->ncnt = p->kcountsize + sizeof(*hdr);
	hdr->version = GMONVERSION;
	hdr->profrate = clockinfo.profhz;
	endfrom = p->fromssize / sizeof(*p->froms);

	for (fromindex = 0; fromindex < endfrom; fromindex++) {
		if (p->froms[fromindex] == 0)
			continue;
		frompc = p->lowpc;
		frompc += fromindex * p->hashfraction * sizeof(*p->froms);
		for (toindex = p->froms[fromindex]; toindex != 0;
		     toindex = p->tos[toindex].link) {
#ifdef DEBUG
			(void) snprintf(dbuf, sizeof dbuf,
			"[mcleanup2] frompc 0x%x selfpc 0x%x count %d\n" ,
				frompc, p->tos[toindex].selfpc,
				p->tos[toindex].count);
			write(log, dbuf, strlen(dbuf));
#endif
			rawarc.raw_frompc = frompc;
			rawarc.raw_selfpc = p->tos[toindex].selfpc;
			rawarc.raw_count = p->tos[toindex].count;
			memcpy(&p->rawarcs[totarc * sizeof(struct rawarc)],
			    &rawarc, sizeof rawarc);
			totarc++;
			if (totarc >= MAXARCS)
				goto donearcs;
		}
	}
donearcs:
	/*
	 * Update field in header.  Kernel will use this to write
	 * out a smaller amount of arcs than originally allocated
	 */
	hdr->totarc = totarc * sizeof(struct rawarc);
}

/*
 * Control profiling
 *	profiling is what mcount checks to see if
 *	all the data structures are ready.
 */
void
moncontrol(int mode)
{
	struct gmonparam *p = &_gmonparam;

	if (mode) {
		/* start */
		profil(p->outbuf, p->outbuflen, p->kcountsize, p->lowpc,
		    s_scale, p->dirfd);
		p->state = GMON_PROF_ON;
	} else {
		/* stop */
		profil(NULL, 0, 0, 0, 0, -1);
		p->state = GMON_PROF_OFF;
	}
}
DEF_WEAK(moncontrol);
