/*	$OpenBSD: procfs_machdep.c,v 1.7 2007/11/28 17:05:09 tedu Exp $	*/
/*	$NetBSD: procfs_machdep.c,v 1.6 2001/02/21 21:39:59 jdolecek Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

extern int i386_fpu_present, i386_fpu_exception, i386_fpu_fdivbug;

static const char * const i386_features[] = {
	"fpu", "vme", "de", "pse", "tsc", "msr", "pae", "mce",
	"cx8", "apic", NULL, "sep", "mtrr", "pge", "mca", "cmov",
	"pat", "pse36", "pn", "clflush", NULL, "dts", "acpi", "mmx",
	"fxsr", "sse", "sse2", "ss", "ht", "tm", "ia64", "pbe"
};


/*
 * Linux-style /proc/cpuinfo.
 * Only used when procfs is mounted with -o linux.
 *
 * In the multiprocessor case, this should be a loop over all CPUs.
 */
int
procfs_getcpuinfstr(char *buf, int *len)
{
	int left, l, i;
	char featurebuf[256], *p;

	p = featurebuf;
	left = sizeof featurebuf;
	for (i = 0; i < 32; i++) {
		if ((cpu_feature & (1 << i)) && i386_features[i]) {
			l = snprintf(p, left, "%s ", i386_features[i]);
			if (l == -1)
				l = 0;
			else if (l >= left)
				l = left - 1;
			left -= l;
			p += l;
			if (left <= 0)
				break;
		}
	}

	p = buf;
	left = *len;
	l = snprintf(p, left,
		"processor\t: %d\n"
		"vendor_id\t: %s\n"
		"cpu family\t: %d\n"
		"model\t\t: %d\n"
		"model name\t: %s\n"
		"stepping\t: ",
		0,
		cpu_vendor,
		cpuid_level >= 0 ? ((cpu_id >> 8) & 15) : cpu_class + 3,
		cpuid_level >= 0 ? ((cpu_id >> 4) & 15) : 0,
		cpu_model
	    );
	if (l == -1)
		l = 0;
	else if (l >= left)
		l = left - 1;

	left -= l;
	p += l;
	if (left <= 0)
		return 0;

	if (cpuid_level >= 0)
		l = snprintf(p, left, "%d\n", cpu_id & 15);
	else
		l = snprintf(p, left, "unknown\n");

	if (l == -1)
		l = 0;
	else if (l >= left)
		l = left - 1;
	left -= l;
	p += l;
	if (left <= 0)
		return 0;

	if (cpuspeed != 0)
		l = snprintf(p, left, "cpu MHz\t\t: %d\n",
		    cpuspeed);
	else
		l = snprintf(p, left, "cpu MHz\t\t: unknown\n");

	if (l == -1)
		l = 0;
	else if (l >= left)
		l = left - 1;
	left -= l;
	p += l;
	if (left <= 0)
		return 0;

	l = snprintf(p, left,
		"fdiv_bug\t: %s\n"
		"fpu\t\t: %s\n"
		"fpu_exception:\t: %s\n"
		"cpuid level\t: %d\n"
		"wp\t\t: %s\n"
		"flags\t\t: %s\n",
		i386_fpu_fdivbug ? "yes" : "no",
		i386_fpu_present ? "yes" : "no",
		i386_fpu_exception ? "yes" : "no",
		cpuid_level,
		(rcr0() & CR0_WP) ? "yes" : "no",
		featurebuf);
	if (l == -1)
		l = 0;
	else if (l >= left)
		l = left - 1;

	*len = (p + l) - buf;

	return 0;
}
