/*	$NetBSD: boot.c,v 1.4 1995/11/23 02:39:27 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <sys/param.h>
#include <sys/exec.h>

#include <machine/prom.h>

#include "include/coff.h"
#define _KERNEL
#include "include/pte.h"

static int aout_exec __P((int, struct exec *, u_int64_t *));
static int coff_exec __P((int, struct exechdr *, u_int64_t *));
static int loadfile __P((char *, u_int64_t *));

char line[64] = "/netbsd";

char boot_file[128];
char boot_dev[128];
char boot_flags[128];
char boot_console[8];

extern char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

#define	KERNEL_ARGC	4
char *kernel_argv[KERNEL_ARGC+1] = {
	boot_file,
	boot_flags,
	boot_console,
	boot_dev,
	NULL
};

vm_offset_t ffp_save, ptbr_save;

void
main(argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
	u_int64_t entry;
	int ask;
	prom_return_t ret;

#ifdef notdef
	{
		extern char *_EDATA, *_end;
		bzero(_EDATA, _end - _EDATA);
	}
#endif

	/* Init prom callback vector. */
	init_prom_calls();

	/* print a banner */
	printf("\n\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	printf("\n");

	/* switch to OSF pal code. */
	OSFpal();

	printf("\n");

	prom_getenv(PROM_E_BOOTED_DEV, boot_dev, sizeof(boot_dev));
	prom_getenv(PROM_E_BOOTED_FILE, boot_file, sizeof(boot_file));
	prom_getenv(PROM_E_BOOTED_OSFLAGS, boot_flags, sizeof(boot_flags));
	prom_getenv(PROM_E_TTY_DEV, boot_console, sizeof(boot_console));

	printf("boot_dev = \"%s\"\n", boot_dev);
	printf("boot_file = \"%s\"\n", boot_file);
	printf("boot_flags = \"%s\"\n", boot_flags);
	printf("boot_console = \"%s\"\n", boot_console);

	if (boot_file[0] == '\0')
		bcopy(line, boot_file, strlen(line)+1);

#ifdef JUSTASK
	ask = 1;
#else
	ask = 0;
#endif
	for (;;) {
		if (ask) {
			(void)printf("Boot: ");
			gets(line);
			if (line[0] == '\0')
				continue;
			if (!strcmp(line, "halt"))
				halt();
/* XXX TURN LINE INTO BOOT FILE/FLAGS */
			bcopy(line, boot_file, strlen(line)+1);
		} else
			(void)printf("Boot: %s %s\n", boot_file, boot_flags);

		if (!loadfile(boot_file, &entry)) {

printf("calling %lx with %lx, %lx, %lx, %lx, %lx\n", entry,
ffp_save, ptbr_save, KERNEL_ARGC, kernel_argv, NULL);
			(*(void (*)())entry)(ffp_save, ptbr_save, KERNEL_ARGC,
			    kernel_argv, NULL);
		}

		ask = 1;
	}
	/* NOTREACHED */
}

/*
 * Open 'filename', read in program and return the entry point or -1 if error.
 */
static int
loadfile(fname, entryp)
	char *fname;
	u_int64_t *entryp;
{
	struct devices *dp;
	union {
		struct exec aout;
		struct exechdr coff;
	} hdr;
	ssize_t nr;
	int fd, rval;

	/* Open the file. */
	rval = 1;
	if ((fd = open(fname, 0)) < 0) {
		(void)printf("open error: %d\n", errno);
		goto err;
	}

	/* Read the exec header. */
	if ((nr = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
		(void)printf("read error: %d\n", errno);
		goto err;
	}

	/* Exec a.out or COFF. */
	rval = N_COFFBADMAG(hdr.coff.a) ?
	    aout_exec(fd, &hdr.aout, entryp) :
	    coff_exec(fd, &hdr.coff, entryp);

err:
#ifndef SMALL
	if (fd >= 0)
		(void)close(fd);
#endif
	if (rval)
		(void)printf("can't boot '%s'\n", fname);
	return (rval);
}

static int
aout_exec(fd, aout, entryp)
	int fd;
	struct exec *aout;
	u_int64_t *entryp;
{
	size_t sz;

	/* Check the magic number. */
	if (N_GETMAGIC(*aout) != OMAGIC) {
		(void)printf("bad magic: %o\n", N_GETMAGIC(*aout));
		return (1);
	}

	/* Read in text, data. */
	(void)printf("%lu+%lu", aout->a_text, aout->a_data);
	if (lseek(fd, (off_t)N_TXTOFF(*aout), SEEK_SET) < 0) {
		(void)printf("lseek: %d\n", errno);
		return (1);
	}
	sz = aout->a_text + aout->a_data;
	if (read(fd, (void *)aout->a_entry, sz) != sz) {
		(void)printf("read text/data: %d\n", errno);
		return (1);
	}

	/* Zero out bss. */
	if (aout->a_bss != 0) {
		(void)printf("+%lu", aout->a_bss);
		bzero(aout->a_entry + sz, aout->a_bss);
	}

	ffp_save = aout->a_entry + aout->a_text + aout->a_data + aout->a_bss;
	ffp_save = k0segtophys((ffp_save + PGOFSET & ~PGOFSET)) >> PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	(void)printf("\n");
	*entryp = aout->a_entry;
	return (0);
}

static int
coff_exec(fd, coff, entryp)
	int fd;
	struct exechdr *coff;
	u_int64_t *entryp;
{

	/* Read in text. */
	(void)printf("%lu", coff->a.tsize);
	(void)lseek(fd, N_COFFTXTOFF(coff->f, coff->a), 0);
	if (read(fd, (void *)coff->a.text_start, coff->a.tsize) !=
	    coff->a.tsize) {
		(void)printf("read text: %d\n", errno);
		return (1);
	}

	/* Read in data. */
	if (coff->a.dsize != 0) {
		(void)printf("+%lu", coff->a.dsize);
		if (read(fd, (void *)coff->a.data_start, coff->a.dsize) !=
		    coff->a.dsize) {
			(void)printf("read data: %d\n", errno);
			return (1);
		}
	}


	/* Zero out bss. */
	if (coff->a.bsize != 0) {
		(void)printf("+%lu", coff->a.bsize);
		bzero(coff->a.bss_start, coff->a.bsize);
	}

	ffp_save = coff->a.text_start + coff->a.tsize;
	if (ffp_save < coff->a.data_start + coff->a.dsize)
		ffp_save = coff->a.data_start + coff->a.dsize;
	if (ffp_save < coff->a.bss_start + coff->a.bsize)
		ffp_save = coff->a.bss_start + coff->a.bsize;
	ffp_save = k0segtophys((ffp_save + PGOFSET & ~PGOFSET)) >> PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	(void)printf("\n");
	*entryp = coff->a.entry;
	return (0);
}
