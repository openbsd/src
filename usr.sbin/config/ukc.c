/*	$OpenBSD: ukc.c,v 1.13 2004/06/08 20:59:29 mcbride Exp $ */

/*
 * Copyright (c) 1999-2001 Mats O Jansson.  All rights reserved.
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

#ifndef LINT
static char rcsid[] = "$OpenBSD: ukc.c,v 1.13 2004/06/08 20:59:29 mcbride Exp $";
#endif

#include <sys/types.h>
#include <sys/device.h>
#include <sys/ioctl.h>

#include <err.h>
#include <kvm.h>
#include <fcntl.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UKC_MAIN
#include "ukc.h"
#include "exec.h"

void	init(void);
void	usage(void);

int	ukc_mod_kernel = 0;

static void
check_int(int idx, const char *name)
{
	if (nl[idx].n_type == 0)
		printf("WARNING this kernel doesn't support modification "
		    "of %s.\n", name);
}

int
ukc(char *file, char *outfile, int uflag, int force)
{
	extern char *__progname;
	int ret, i;
	kvm_t *kd;
	char errbuf[_POSIX2_LINE_MAX];
	int histlen = 0, ok = 1;
	char history[1024], kversion[1024];

	if (file == NULL) {
		fprintf(stderr, "%s: no file specified\n", __progname);
		usage();
	}

	loadkernel(file);

	ret = nlist(file, nl);

	if (uflag) {
		if ((kd = kvm_openfiles(NULL,NULL,NULL,O_RDONLY, errbuf)) == 0)
			errx(1, "kvm_openfiles: %s", errbuf);

		if ((ret = kvm_nlist(kd, knl)) == -1)
			errx(1, "kvm_nlist: %s", kvm_geterr(kd));

		i = 0;
		while (i < NLENTRIES) {
			if (nl[i].n_type != knl[i].n_type ||
			    nl[i].n_desc != knl[i].n_desc ||
			    nl[i].n_value != knl[i].n_value)
				ok = 0;
			i++;
		}

		if (knl[I_HISTLEN].n_type != 0 && ok) {
			if (kvm_read(kd, knl[I_HISTLEN].n_value, &histlen,
			    sizeof(histlen)) != sizeof(histlen))
				warnx("cannot read %s: %s",
				    knl[I_HISTLEN].n_name,
				    kvm_geterr(kd));
		}
		if (knl[CA_HISTORY].n_type != 0 && ok) {
			if (kvm_read(kd, knl[CA_HISTORY].n_value, history,
			    sizeof(history)) != sizeof(history))
				warnx("cannot read %s: %s",
				    knl[CA_HISTORY].n_name,
				    kvm_geterr(kd));
		}
		if (knl[P_VERSION].n_type != 0 && ok) {
			if (kvm_read(kd, knl[P_VERSION].n_value, kversion,
			    sizeof(kversion)) != sizeof(kversion))
				warnx("cannot read %s: %s",
				    knl[P_VERSION].n_name,
				    kvm_geterr(kd));
		}
	}

	printf("%s", adjust((caddr_t)nl[P_VERSION].n_value));

	if (force == 0 && outfile == NULL)
		printf("warning: no output file specified\n");

	if (nl[IA_EXTRALOC].n_type == 0 || nl[I_NEXTRALOC].n_type == 0 ||
	    nl[I_UEXTRALOC].n_type == 0 || nl[I_HISTLEN].n_type == 0 ||
	    nl[CA_HISTORY].n_type == 0) {
		printf("\
WARNING this kernel doesn't contain all information needed!\n\
WARNING the commands add and change might not work.\n");
		oldkernel = 1;
	}

	if (nl[P_PDEVNAMES].n_type == 0 ||
	    nl[I_PDEVSIZE].n_type == 0 ||
	    nl[S_PDEVINIT].n_type == 0) {
		printf("\
WARNING this kernel doesn't support pseudo devices.\n");
		nopdev = 1;
	}

	check_int(I_BUFCACHEPCT, "BUFCACHEPERCENT");
	check_int(I_NKMEMPG, "NKMEMPAGES");
	check_int(I_SHMSEG, "SHMSEG");
	check_int(I_SHMMAXPGS, "SHMMAXPGS");

	init();

	if (uflag) {
		if (ok) {
			if (strcmp(adjust((caddr_t)nl[P_VERSION].n_value),
			    kversion) != 0)
				ok = 1;
		}
		if (!ok) {
			printf("WARNING kernel mismatch. -u ignored.\n");
			printf("WARNING the running kernel version:\n");
			printf("%s", kversion);
		} else
			process_history(histlen, history);
	}

	if (config()) {
		if (force == 0 && outfile == NULL) {
			fprintf(stderr, "not forced\n");
			exit(1);
		}
		if (outfile == NULL)
			outfile = file;
		if (ukc_mod_kernel == 0) {
			fprintf(stderr, "Kernel not modified\n");
			exit(1);
		} else {
			printf ("Saving modified kernel.\n");
			savekernel(outfile);
		}
	}
	return(0);
}

void
init(void)
{
	int i = 0, fd;
	struct cfdata *cd;
	short	*ln;
	int	*p;
#ifdef NOTDEF
	struct winsize w;
#endif

	cd = get_cfdata(0);			/* get first item */
	while (cd->cf_attach != 0) {
		maxdev = i;
		totdev = i;

		ln = get_locnamp(cd->cf_locnames);
		while (*ln != -1) {
			if (*ln > maxlocnames)
				maxlocnames = *ln;
			ln++;
		}
		i++;
		cd++;
	}

	while (cd->cf_attach == 0) {
		totdev = i;
		i++;
		cd++;
	}

	totdev = totdev - 1;

	if (nopdev == 0) {
		p = (int *)adjust((caddr_t)nl[I_PDEVSIZE].n_value);
		maxpseudo = *p;
	}

	if ((fd = open("/dev/tty", O_RDWR)) < 0)
		fd = 2;

#ifdef NOTDEF
	if (ioctl(fd, TIOCGWINSZ, &w) == 0)
		printf("row %d col %d\n", w.ws_row, w.ws_col);

	if ((s = getenv("LINES")) != NULL)
		sc_height = atoi(s);
	else
		sc_height = tgetnum("li");

	if (sc_height <= 0)
		sc_height = 24;

	if ((s = getenv("COLUMNS")) != NULL)
		sc_width = atoi(s);
	else
		sc_width = tgetnum("co");

	if (sc_width <= 0)
		sc_width = 80;
#endif
}
