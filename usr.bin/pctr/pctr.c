/*	$OpenBSD: pctr.c,v 1.20 2008/07/08 21:39:52 sobrado Exp $	*/

/*
 * Copyright (c) 2007 Mike Belopuhov, Aleksey Lomovtsev
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

/*
 * Pentium performance counter control program for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project by leaving this copyright notice intact.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>

#include <machine/cpu.h>
#include <machine/pctr.h>
#include <machine/specialreg.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pctrvar.h"

static int	 cpu_type;
static int	 tsc_avail;

static int	 ctr, func, masku, thold;
static int	 cflag, eflag, iflag, kflag, uflag;
static int	 Mflag, Eflag, Sflag, Iflag, Aflag;

static void	 pctr_cpu_creds(void);
static char	*pctr_fn2str(u_int32_t);
static void	 pctr_printvals(struct pctrst *);
static int	 pctr_read(struct pctrst *);
static int	 pctr_write(int, u_int32_t);
static void	 pctr_list_fnct(void);
static int	 pctr_set_cntr(void);
static void	 usage(void);

int
main(int argc, char **argv)
{
	const char *errstr;
	struct pctrst st;
	int ch = -1;
	int list_mode = 0, set_mode = 0;

	pctr_cpu_creds();

	while ((ch = getopt(argc, argv, "AcEef:IiklMm:Ss:t:u")) != -1)
		switch (ch) {
		case 'A':
			Aflag++;
			break;
		case 'c':
			cflag++;
			break;
		case 'E':
			Eflag++;
			break;
		case 'e':
			eflag++;
			break;
		case 'f':
			if (sscanf(optarg, "%x", &func) <= 0 || func < 0 ||
			    func > PCTR_MAX_FUNCT)
				errx(1, "invalid function number");
			break;
		case 'I':
			Iflag++;
			break;
		case 'i':
			iflag++;
			break;
		case 'k':
			kflag++;
			break;
		case 'l':
			list_mode++;
			break;
		case 'M':
			Mflag++;
			break;
		case 'm':
			if (sscanf(optarg, "%x", &masku) <= 0 || masku < 0 ||
			    masku > PCTR_MAX_UMASK)
				errx(1, "invalid unit mask number");
			break;
		case 'S':
			Sflag++;
			break;
		case 's':
			set_mode++;
			ctr = strtonum(optarg, 0, PCTR_NUM-1, &errstr);
			if (errstr)
				errx(1, "counter number is %s: %s", errstr,
				    optarg);
			break;
		case 't':
			thold = strtonum(optarg, 0, 0xff, &errstr);
			if (errstr)
				errx(1, "threshold is %s: %s", errstr, optarg);
			break;
		case 'u':
			uflag++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	if (Aflag && (Mflag || Eflag || Sflag || Iflag))
		usage();

	if (list_mode)
		pctr_list_fnct();
	else if (set_mode) {
		if (pctr_set_cntr() < 0)
			err(1, "pctr_set_cntr");
	} else {
		bzero(&st, sizeof(st));
		if (pctr_read(&st) < 0)
			err(1, "pctr_read");
		pctr_printvals(&st);
	}
	return (0);
}

static void
pctr_cpu_creds(void)
{
	int atype;
	char arch[16], vendor[64];
	int mib[2], cpu_id, cpu_feature;
	size_t len;

	/* Get the architecture */
	mib[0] = CTL_HW;
	mib[1] = HW_MACHINE;
	len = sizeof(arch) - 1;
	bzero(arch, sizeof(arch));
	if (sysctl(mib, 2, arch, &len, NULL, 0) == -1)
		err(1, "HW_MACHINE");
	arch[len] = '\0';

	if (strcmp(arch, "i386") == 0)
		atype = ARCH_I386;
	else if (strcmp(arch, "amd64") == 0)
		atype = ARCH_AMD64;
	else
		errx(1, "architecture %s is not supported", arch);

	/* Get the CPU id */
	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_CPUID;
	len = sizeof(cpu_id);
	if (sysctl(mib, 2, &cpu_id, &len, NULL, 0) == -1)
		err(1, "CPU_CPUID");

	/* Get the CPU features */
	mib[1] = CPU_CPUFEATURE;
	len = sizeof(cpu_feature);
	if (sysctl(mib, 2, &cpu_feature, &len, NULL, 0) == -1)
		err(1, "CPU_CPUFEATURE");

	/* Get the processor vendor */
	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_CPUVENDOR;
	len = sizeof(vendor) - 1;
	bzero(vendor, sizeof(vendor));
	if (sysctl(mib, 2, vendor, &len, NULL, 0) == -1)
		err(1, "CPU_CPUVENDOR");
	vendor[len] = '\0';

	switch (atype) {
	case ARCH_I386:
		if (strcmp(vendor, "AuthenticAMD") == 0) {
			if (((cpu_id >> 8) & 15) >= 6)
				cpu_type = CPU_AMD;
			else
				cpu_type = CPU_UNDEF;	/* old AMD cpu */

		} else if (strcmp(vendor, "GenuineIntel") == 0) {
			if (((cpu_id >> 8) & 15) == 6 &&
			    ((cpu_id >> 4) & 15) > 14)
				cpu_type = CPU_CORE;
			else if (((cpu_id >> 8) & 15) >= 6)
				cpu_type = CPU_P6;
			else if (((cpu_id >> 4) & 15) > 0)
				cpu_type = CPU_P5;
			else
				cpu_type = CPU_UNDEF;	/* old Intel cpu */
		}
		if (cpu_feature & CPUID_TSC)
			tsc_avail = 1;
		break;
	case ARCH_AMD64:
		if (strcmp(vendor, "AuthenticAMD") == 0)
			cpu_type = CPU_AMD;
		else if (strcmp(vendor, "GenuineIntel") == 0)
			cpu_type = CPU_CORE;
		if (cpu_feature & CPUID_TSC)
			tsc_avail = 1;
		break;
	}
}

static __inline int
pctr_ctrfn_index(struct ctrfn *cfnp, u_int32_t func)
{
	int i;

	for (i = 0; cfnp[i].name != NULL; i++)
		if (cfnp[i].fn == func)
			return (i);
	return (-1);
}

static char *
pctr_fn2str(u_int32_t sel)
{
	static char buf[128];
	struct ctrfn *cfnp = NULL;
	char th[6], um[5], *msg;
	u_int32_t fn;
	int ind;

	bzero(buf, sizeof(buf));
	bzero(th, sizeof(th));
	bzero(um, sizeof(um));
	switch (cpu_type) {
	case CPU_P5:
		fn = sel & 0x3f;
		if ((ind = pctr_ctrfn_index(p5fn, fn)) < 0)
			msg = "unknown function";
		else
			msg = p5fn[ind].name;
		snprintf(buf, sizeof(buf), "%c%c%c %02x %s",
		    sel & P5CTR_C ? 'c' : '-',
		    sel & P5CTR_U ? 'u' : '-',
		    sel & P5CTR_K ? 'k' : '-',
		    fn, msg);
		break;
	case CPU_P6:
		cfnp = p6fn;
	case CPU_CORE:
		if (cpu_type == CPU_CORE)
			cfnp = corefn;
		fn = sel & 0xff;
		if ((ind = pctr_ctrfn_index(cfnp, fn)) < 0)
			msg = "unknown function";
		else
			msg = cfnp[ind].name;
		if (cfnp[ind].name && cfnp[ind].flags & CFL_MESI)
			snprintf(um, sizeof (um), "%c%c%c%c",
			    sel & PCTR_UM_M ? 'M' : '-',
			    sel & PCTR_UM_E ? 'E' : '-',
			    sel & PCTR_UM_S ? 'S' : '-',
			    sel & PCTR_UM_I ? 'I' : '-');
		else if (cfnp[ind].name && cfnp[ind].flags & CFL_SA)
			snprintf(um, sizeof(um), "%c",
			    sel & PCTR_UM_A ? 'A' : '-');
		if (sel >> PCTR_CM_SHIFT)
			snprintf(th, sizeof(th), "+%d",
			    sel >> PCTR_CM_SHIFT);
		snprintf(buf, sizeof(buf), "%c%c%c%c %02x %02x %s %s %s",
		    sel & PCTR_I ? 'i' : '-',
		    sel & PCTR_E ? 'e' : '-',
		    sel & PCTR_K ? 'k' : '-',
		    sel & PCTR_U ? 'u' : '-',
		    fn, (sel >> PCTR_UM_SHIFT) & 0xff, th, um, msg);
		break;
	case CPU_AMD:
		fn = sel & 0xff;
		if (sel >> PCTR_CM_SHIFT)
			snprintf(th, sizeof(th), "+%d",
			    sel >> PCTR_CM_SHIFT);
		snprintf(buf, sizeof(buf), "%c%c%c%c %02x %02x %s",
		    sel & PCTR_I ? 'i' : '-',
		    sel & PCTR_E ? 'e' : '-',
		    sel & PCTR_K ? 'k' : '-',
		    sel & PCTR_U ? 'u' : '-',
		    fn, (sel >> PCTR_UM_SHIFT) & 0xff, th);
		break;
	}
	return (buf);
}

static void
pctr_printvals(struct pctrst *st)
{
	int i, n;

	switch (cpu_type) {
	case CPU_P5:
	case CPU_P6:
	case CPU_CORE:
		n = PCTR_INTEL_NUM;
	case CPU_AMD:
		if (cpu_type == CPU_AMD)
			n = PCTR_AMD_NUM;
		for (i = 0; i < n; i++)
			printf(" ctr%d = %16llu  [%s]\n", i, st->pctr_hwc[i],
			    pctr_fn2str(st->pctr_fn[i]));
		if (tsc_avail)
			printf("  tsc = %16llu\n", st->pctr_tsc);
		break;
	}
}

static int
pctr_read(struct pctrst *st)
{
	int fd, se;

	fd = open(_PATH_PCTR, O_RDONLY);
	if (fd < 0)
		return (-1);
	if (ioctl(fd, PCIOCRD, st) < 0) {
		se = errno;
		close(fd);
		errno = se;
		return (-1);
	}
	return (close(fd));
}

static int
pctr_write(int ctr, u_int32_t val)
{
	int fd, se;

	fd = open(_PATH_PCTR, O_WRONLY);
	if (fd < 0)
		return (-1);
	if (ioctl(fd, PCIOCS0 + ctr, &val) < 0) {
		se = errno;
		close(fd);
		errno = se;
		return (-1);
	}
	return (close(fd));
}

static __inline void
pctr_printdesc(char *desc)
{
	char *p;

	for (;;) {
		while (*desc == ' ')
			desc++;
		if (strlen(desc) < 70) {
			if (*desc)
				printf("      %s\n", desc);
			return;
		}
		p = desc + 72;
		while (*--p != ' ')
			;
		while (*--p == ' ')
			;
		p++;
		printf("      %.*s\n", (int)(p-desc), desc);
		desc = p;
	}
}

static void
pctr_list_fnct(void)
{
	struct ctrfn *cfnp = NULL;

	if (cpu_type == CPU_P5)
		cfnp = p5fn;
	else if (cpu_type == CPU_P6)
		cfnp = p6fn;
	else if (cpu_type == CPU_CORE)
		cfnp = corefn;
	else if (cpu_type == CPU_AMD)
		cfnp = amdfn;
	else
		return;

	for (; cfnp->name; cfnp++) {
		printf("%02x  %s", cfnp->fn, cfnp->name);
		if (cfnp->flags & CFL_MESI)
			printf("  (MESI)");
		else if (cfnp->flags & CFL_SA)
			printf("  (A)");
		if (cfnp->flags & CFL_C0)
			printf("  (ctr0 only)");
		else if (cfnp->flags & CFL_C1)
			printf("  (ctr1 only)");
		if (cfnp->flags & CFL_UM)
			printf("  (needs unit mask)");
		printf("\n");
		if (cfnp->desc)
			pctr_printdesc(cfnp->desc);
	}
}

static int
pctr_set_cntr(void)
{
	struct ctrfn *cfnp = NULL;
	u_int32_t val = func;
	int ind = 0;

	switch (cpu_type) {
	case CPU_P5:
		if (ctr >= PCTR_INTEL_NUM)
			errx(1, "only %d counters are supported",
			    PCTR_INTEL_NUM);
		if (cflag)
			val |= P5CTR_C;
		if (kflag)
			val |= P5CTR_K;
		if (uflag)
			val |= P5CTR_U;
		if (func && (!kflag && !uflag))
			val |= P5CTR_K | P5CTR_U;
		break;
	case CPU_P6:
		cfnp = p6fn;
	case CPU_CORE:
		if (cpu_type == CPU_CORE)
			cfnp = corefn;
		if (ctr >= PCTR_INTEL_NUM)
			errx(1, "only %d counters are supported",
			    PCTR_INTEL_NUM);
		if (func && (ind = pctr_ctrfn_index(cfnp, func)) < 0)
			errx(1, "function %02x is not supported", func);
		if (func && (cfnp[ind].flags & CFL_SA))
			val |= PCTR_UM_A;
		if (func && (cfnp[ind].flags & CFL_MESI)) {
			if (Mflag)
				val |= PCTR_UM_M;
			if (Eflag)
				val |= PCTR_UM_E;
			if (Sflag)
				val |= PCTR_UM_S;
			if (Iflag)
				val |= PCTR_UM_I;
			if (!Mflag || !Eflag || !Sflag || !Iflag)
				val |= PCTR_UM_MESI;
		}
		if (func && (cfnp[ind].flags & CFL_ED))
			val |= PCTR_E;
		if (func && (cfnp[ind].flags & CFL_UM) && !masku)
			errx(1, "function %02x needs unit mask specification",
			    func);
	case CPU_AMD:
		if (cpu_type == CPU_AMD && func &&
		    ((ind = pctr_ctrfn_index(amdfn, func)) < 0))
			errx(1, "function %02x is not supported", func);
		if (ctr >= PCTR_AMD_NUM)
			errx(1, "only %d counters are supported",
			    PCTR_AMD_NUM);
		if (eflag)
			val |= PCTR_E;
		if (iflag)
			val |= PCTR_I;
		if (kflag)
			val |= PCTR_K;
		if (uflag)
			val |= PCTR_U;
		if (func && (!kflag && !uflag))
			val |= PCTR_K | PCTR_U;
		val |= masku << PCTR_UM_SHIFT;
		val |= thold << PCTR_CM_SHIFT;
		if (func)
			val |= PCTR_EN;
		break;
	}

	return (pctr_write(ctr, val));
}

static void
usage(void)
{
	extern char *__progname;
	char *usg = NULL;

	switch (cpu_type) {
	case CPU_P5:
		usg = "[-cklu] [-f funct] [-s ctr]";
		break;
	case CPU_P6:
	case CPU_CORE:
		usg = "[-AEeIiklMSu] [-f funct] [-m umask] [-s ctr] "
		    "[-t thold]";
		break;
	case CPU_AMD:
		usg = "[-eilku] [-f funct] [-m umask] [-s ctr] "
		    "[-t thold]";
		break;
	}

	fprintf(stderr, "usage: %s %s\n", __progname, usg);
	exit(1);
}
