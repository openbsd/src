/*	$OpenBSD: build.c,v 1.12 2004/10/09 20:36:05 mickey Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
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

#ifndef lint
/*static char sccsid[] = "from: @(#)build.c	5.3 (Berkeley) 3/12/91";*/
static char rcsid[] = "$OpenBSD: build.c,v 1.12 2004/10/09 20:36:05 mickey Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <a.out.h>
#include <dirent.h>
#include <unistd.h>
#include <ar.h>
#include <limits.h>
#include <ranlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <archive.h>
#include <err.h>
#include "byte.c"
#include "extern.h"


extern CHDR chdr;			/* converted header */

typedef struct _rlib {
	struct _rlib *next;		/* next structure */
	off_t pos;			/* offset of defining archive file */
	char *sym;			/* symbol */
	int symlen;			/* strlen(sym) */
} RLIB;
RLIB *rhead, **pnext;

static FILE	*fp;
static long	symcnt;			/* symbol count */
static long	tsymlen;		/* total string length */

static int rexec();
static void symobj();

int
build(void)
{
	CF cf;
	int afd, tfd;
	int current_mid;
	off_t size;

	current_mid = -1;
	afd = open_archive(O_RDWR);
	fp = fdopen(afd, "r+");
	tfd = tmp();

	SETCF(afd, archive, tfd, tname, RPAD|WPAD);

	/* Read through the archive, creating list of symbols. */
	symcnt = tsymlen = 0;
	pnext = &rhead;
	while(get_arobj(afd)) {
		int new_mid;

		if (!strcmp(chdr.name, RANLIBMAG)) {
			skip_arobj(afd);
			continue;
		}
		new_mid = rexec(afd, tfd);
		if (new_mid != -1) {
			if (current_mid == -1)
				current_mid = new_mid;
			else if (new_mid != current_mid)
				errx(1, "Mixed object format archive: %d / %d", 
					new_mid, current_mid);
		}
		put_arobj(&cf, (struct stat *)NULL);
	}
	*pnext = NULL;

	/* Create the symbol table.  Endianess the same as last mid seen */
	symobj(current_mid);

	/* Copy the saved objects into the archive. */
	size = lseek(tfd, (off_t)0, SEEK_CUR);
	(void)lseek(tfd, (off_t)0, SEEK_SET);
	SETCF(tfd, tname, afd, archive, NOPAD);
	copy_ar(&cf, size);
	(void)ftruncate(afd, lseek(afd, (off_t)0, SEEK_CUR));
	(void)close(tfd);

	/* Set the time. */
	settime(afd);
	close_archive(afd);
	return(0);
}

/*
 * rexec
 *	Read the exec structure; ignore any files that don't look
 *	exactly right. Return MID.
 * 	return -1 for files that don't look right.
 *	XXX it's hard to be sure when to ignore files, and when to error
 *	out.
 */
static int
rexec(int rfd, int wfd)
{
	RLIB *rp;
	long nsyms;
	int nr, symlen;
	char *strtab = 0;
	char *sym;
	struct exec ebuf;
	struct nlist nl;
	off_t r_off, w_off;
	long strsize;
	int result = -1;

	/* Get current offsets for original and tmp files. */
	r_off = lseek(rfd, (off_t)0, SEEK_CUR);
	w_off = lseek(wfd, (off_t)0, SEEK_CUR);

	/* Read in exec structure. */
	nr = read(rfd, (char *)&ebuf, sizeof(struct exec));
	if (nr != sizeof(struct exec))
		goto bad;

	/* Check magic number and symbol count. */
	if (BAD_OBJECT(ebuf) || ebuf.a_syms == 0)
		goto bad;
	fix_header_order(&ebuf);

	/* Seek to string table. */
	if (lseek(rfd, N_STROFF(ebuf) + r_off, SEEK_SET) == (off_t)-1) {
		if (errno == EINVAL)
			goto bad;
		else
			error(archive);
	}

	/* Read in size of the string table. */
	nr = read(rfd, (char *)&strsize, sizeof(strsize));
	if (nr != sizeof(strsize))
		goto bad;

	strsize = fix_32_order(strsize, N_GETMID(ebuf));

	/* Read in the string table. */
	strsize -= sizeof(strsize);
	strtab = (char *)emalloc(strsize);
	nr = read(rfd, strtab, strsize);
	if (nr != strsize) 
		goto bad;

	/* Seek to symbol table. */
	if (fseek(fp, N_SYMOFF(ebuf) + r_off, SEEK_SET) == (off_t)-1)
		goto bad;

	result = N_GETMID(ebuf);
	/* For each symbol read the nlist entry and save it as necessary. */
	nsyms = ebuf.a_syms / sizeof(struct nlist);
	while (nsyms--) {
		if (!fread((char *)&nl, sizeof(struct nlist), 1, fp)) {
			if (feof(fp))
				badfmt();
			error(archive);
		}
		fix_nlist_order(&nl, N_GETMID(ebuf));

		/* Ignore if no name or local. */
		if (!nl.n_un.n_strx || !(nl.n_type & N_EXT))
			continue;

		/*
		 * If the symbol is an undefined external and the n_value
		 * field is non-zero, keep it.
		 */
		if ((nl.n_type & N_TYPE) == N_UNDF && !nl.n_value)
			continue;

		/* First four bytes are the table size. */
		sym = strtab + nl.n_un.n_strx - sizeof(long);
		symlen = strlen(sym) + 1;

		rp = (RLIB *)emalloc(sizeof(RLIB));
		rp->sym = (char *)emalloc(symlen);
		bcopy(sym, rp->sym, symlen);
		rp->symlen = symlen;
		rp->pos = w_off;

		/* Build in forward order for "ar -m" command. */
		*pnext = rp;
		pnext = &rp->next;

		++symcnt;
		tsymlen += symlen;
	}

bad: 	if (nr < 0)
		error(archive);
	free(strtab);
	(void)lseek(rfd, (off_t)r_off, SEEK_SET);
	return result;
}

/*
 * symobj --
 *	Write the symbol table into the archive, computing offsets as
 *	writing.  Use the right format depending on mid.
 */
static void
symobj(int mid)
{
	RLIB *rp, *rnext;
	struct ranlib rn;
	char hb[sizeof(struct ar_hdr) + 1], pad;
	long ransize, size, stroff;
	uid_t uid;
	gid_t gid;

	/* Rewind the archive, leaving the magic number. */
	if (fseek(fp, (off_t)SARMAG, SEEK_SET) == (off_t)-1)
		error(archive);

	/* Size of the ranlib archive file, pad if necessary. */
	ransize = sizeof(long) +
	    symcnt * sizeof(struct ranlib) + sizeof(long) + tsymlen;
	if (ransize & 01) {
		++ransize;
		pad = '\n';
	} else
		pad = '\0';

	uid = getuid();
	if (uid > USHRT_MAX) {
		warnx("warning: uid %u truncated to %u", uid, USHRT_MAX);
		uid = USHRT_MAX;
	}
	gid = getgid();
	if (gid > USHRT_MAX) {
		warnx("warning: gid %u truncated to %u", gid, USHRT_MAX);
		gid = USHRT_MAX;
	}

	/* Put out the ranlib archive file header. */
#define	DEFMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
	(void)snprintf(hb, sizeof hb, HDR2, RANLIBMAG, 0L, uid, gid,
	    DEFMODE & ~umask(0), (off_t)ransize, ARFMAG);
	if (!fwrite(hb, sizeof(struct ar_hdr), 1, fp))
		error(tname);

	/* First long is the size of the ranlib structure section. */
	size = fix_32_order(symcnt * sizeof(struct ranlib), mid);
	if (!fwrite((char *)&size, sizeof(size), 1, fp))
		error(tname);

	/* Offset of the first archive file. */
	size = SARMAG + sizeof(struct ar_hdr) + ransize;

	/*
	 * Write out the ranlib structures.  The offset into the string
	 * table is cumulative, the offset into the archive is the value
	 * set in rexec() plus the offset to the first archive file.
	 */
	for (rp = rhead, stroff = 0; rp; rp = rp->next) {
		rn.ran_un.ran_strx = stroff;
		stroff += rp->symlen;
		rn.ran_off = size + rp->pos;
		fix_ranlib_order(&rn, mid);
		if (!fwrite((char *)&rn, sizeof(struct ranlib), 1, fp))
			error(archive);
	}

	/* Second long is the size of the string table. */

	size = fix_32_order(tsymlen, mid);
	if (!fwrite((char *)&size, sizeof(size), 1, fp))
		error(tname);

	/* Write out the string table. */
	for (rp = rhead; rp; rp = rnext) {
		if (!fwrite(rp->sym, rp->symlen, 1, fp))
			error(tname);
		rnext = rp->next;
		free(rp);
	}
	rhead = NULL;

	if (pad && !fwrite(&pad, sizeof(pad), 1, fp))
		error(tname);

	(void)fflush(fp);
}
