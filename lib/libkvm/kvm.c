/*	$OpenBSD: kvm.c,v 1.17 1998/10/28 19:47:00 marc Exp $ */
/*	$NetBSD: kvm.c,v 1.43 1996/05/05 04:31:59 gwr Exp $	*/

/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm.c	8.2 (Berkeley) 2/13/94";
#else
static char *rcsid = "$OpenBSD: kvm.c,v 1.17 1998/10/28 19:47:00 marc Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <sys/core.h>
#include <sys/exec_aout.h>
#include <sys/kcore.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/swap_pager.h>

#include <ctype.h>
#include <db.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <kvm.h>

#include "kvm_private.h"

static int	kvm_dbopen __P((kvm_t *, const char *));
static int	_kvm_get_header __P((kvm_t *));
static kvm_t	*_kvm_open __P((kvm_t *, const char *, const char *,
		    const char *, int, char *));
static int	clear_gap __P((kvm_t *, FILE *, int));
static off_t	Lseek __P((kvm_t *, int, off_t, int));
static ssize_t	Read __P(( kvm_t *, int, void *, size_t));

char *
kvm_geterr(kd)
	kvm_t *kd;
{
	return (kd->errbuf);
}

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * Report an error using printf style arguments.  "program" is kd->program
 * on hard errors, and 0 on soft errors, so that under sun error emulation,
 * only hard errors are printed out (otherwise, programs like gdb will
 * generate tons of error messages when trying to access bogus pointers).
 */
void
#ifdef __STDC__
_kvm_err(kvm_t *kd, const char *program, const char *fmt, ...)
#else
_kvm_err(kd, program, fmt, va_alist)
	kvm_t *kd;
	char *program, *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fputc('\n', stderr);
	} else
		(void)vsnprintf(kd->errbuf,
		    sizeof(kd->errbuf), (char *)fmt, ap);

	va_end(ap);
}

void
#ifdef __STDC__
_kvm_syserr(kvm_t *kd, const char *program, const char *fmt, ...)
#else
_kvm_syserr(kd, program, fmt, va_alist)
	kvm_t *kd;
	char *program, *fmt;
	va_dcl
#endif
{
	va_list ap;
	register int n;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": %s\n", strerror(errno));
	} else {
		register char *cp = kd->errbuf;

		(void)vsnprintf(cp, sizeof(kd->errbuf), (char *)fmt, ap);
		n = strlen(cp);
		(void)snprintf(&cp[n], sizeof(kd->errbuf) - n, ": %s",
		    strerror(errno));
	}
	va_end(ap);
}

void *
_kvm_malloc(kd, n)
	register kvm_t *kd;
	register size_t n;
{
	void *p;

	if ((p = malloc(n)) == NULL)
		_kvm_err(kd, kd->program, strerror(errno));
	return (p);
}

/*
 * Wrappers for Lseek/Read system calls.  They check for errors and
 * call _kvm_syserr() if appropriate.
 */
static off_t
Lseek(kd, fd, offset, whence)
	kvm_t	*kd;
	int	fd, whence;
	off_t	offset;
{
	off_t	off;

	errno = 0;
	if ((off = lseek(fd, offset, whence)) == -1 && errno != 0) {
		_kvm_syserr(kd, kd->program, "Lseek");
		return (-1);
	}
	return (off);
}

static ssize_t
Read(kd, fd, buf, nbytes)
	kvm_t	*kd;
	int	fd;
	void	*buf;
	size_t	nbytes;
{
	ssize_t	rv;

	errno = 0;

	if ((rv = read(fd, buf, nbytes)) != nbytes && errno != 0)
		_kvm_syserr(kd, kd->program, "Read");
	return (rv);
}

static kvm_t *
_kvm_open(kd, uf, mf, sf, flag, errout)
	register kvm_t *kd;
	const char *uf;
	const char *mf;
	const char *sf;
	int flag;
	char *errout;
{
	struct stat st;

	kd->db = 0;
	kd->pmfd = -1;
	kd->vmfd = -1;
	kd->swfd = -1;
	kd->nlfd = -1;
	kd->procbase = 0;
	kd->nbpg = getpagesize();
	kd->swapspc = 0;
	kd->argspc = 0;
	kd->argbuf = 0;
	kd->argv = 0;
	kd->vmst = 0;
	kd->vm_page_buckets = 0;
	kd->kcore_hdr = 0;
	kd->cpu_dsize = 0;
	kd->cpu_data = 0;
	kd->dump_off = 0;

	if (uf && strlen(uf) >= MAXPATHLEN) {
		_kvm_err(kd, kd->program, "exec file name too long");
		goto failed;
	}
	if (flag & ~O_ACCMODE) {
		_kvm_err(kd, kd->program, "bad flags arg");
		goto failed;
	}
	if (mf == 0)
		mf = _PATH_MEM;
	if (sf == 0)
		sf = _PATH_DRUM;

	if ((kd->pmfd = open(mf, flag, 0)) < 0) {
		_kvm_syserr(kd, kd->program, "%s", mf);
		goto failed;
	}
	if (fstat(kd->pmfd, &st) < 0) {
		_kvm_syserr(kd, kd->program, "%s", mf);
		goto failed;
	}
	if (S_ISCHR(st.st_mode)) {
		/*
		 * If this is a character special device, then check that
		 * it's /dev/mem.  If so, open kmem too.  (Maybe we should
		 * make it work for either /dev/mem or /dev/kmem -- in either
		 * case you're working with a live kernel.)
		 */
		if (strcmp(mf, _PATH_MEM) != 0) {	/* XXX */
			_kvm_err(kd, kd->program,
				 "%s: not physical memory device", mf);
			goto failed;
		}
		if ((kd->vmfd = open(_PATH_KMEM, flag)) < 0) {
			_kvm_syserr(kd, kd->program, "%s", _PATH_KMEM);
			goto failed;
		}
		if ((kd->swfd = open(sf, flag, 0)) < 0) {
			_kvm_syserr(kd, kd->program, "%s", sf);
			goto failed;
		}
		/*
		 * Open kvm nlist database.  We only try to use
		 * the pre-built database if the namelist file name
		 * pointer is NULL.  If the database cannot or should
		 * not be opened, open the namelist argument so we
		 * revert to slow nlist() calls.
		 * If no file is specified, try opening _PATH_KSYMS and
		 * fall back to _PATH_UNIX.
		 */
		if (kvm_dbopen(kd, uf ? uf : _PATH_UNIX) == -1 &&
		    ((uf && (kd->nlfd = open(uf, O_RDONLY)) == -1) || (!uf &&
		    (kd->nlfd = open((uf = _PATH_KSYMS), O_RDONLY)) == -1 &&
		    (kd->nlfd = open((uf = _PATH_UNIX), O_RDONLY)) == -1))) {
			_kvm_syserr(kd, kd->program, "%s", uf);
			goto failed;
		}
	} else {
		/*
		 * This is a crash dump.
		 * Initalize the virtual address translation machinery,
		 * but first setup the namelist fd.
		 * If no file is specified, try opening _PATH_KSYMS and
		 * fall back to _PATH_UNIX.
		 */
		if ((uf && (kd->nlfd = open(uf, O_RDONLY)) == -1) || (!uf &&
		    (kd->nlfd = open((uf = _PATH_KSYMS), O_RDONLY)) == -1 &&
		    (kd->nlfd = open((uf = _PATH_UNIX), O_RDONLY)) == -1)) {
			_kvm_syserr(kd, kd->program, "%s", uf);
			goto failed;
		}

		/*
		 * If there is no valid core header, fail silently here.
		 * The address translations however will fail without 
		 * header. Things can be made to run by calling
		 * kvm_dump_mkheader() before doing any translation.
		 */
		if (_kvm_get_header(kd) == 0) {
			if (_kvm_initvtop(kd) < 0)
				goto failed;
		}
	}
	return (kd);
failed:
	/*
	 * Copy out the error if doing sane error semantics.
	 */
	if (errout != 0)
		(void)strncpy(errout, kd->errbuf, _POSIX2_LINE_MAX - 1);
	(void)kvm_close(kd);
	return (0);
}

/*
 * The kernel dump file (from savecore) contains:
 *    kcore_hdr_t kcore_hdr;
 *    kcore_seg_t cpu_hdr;
 *    (opaque)    cpu_data; (size is cpu_hdr.c_size)
 *	  kcore_seg_t mem_hdr;
 *    (memory)    mem_data; (size is mem_hdr.c_size)
 *    
 * Note: khdr is padded to khdr.c_hdrsize;
 * cpu_hdr and mem_hdr are padded to khdr.c_seghdrsize
 */
static int
_kvm_get_header(kd)
	kvm_t	*kd;
{
	kcore_hdr_t	kcore_hdr;
	kcore_seg_t	cpu_hdr;
	kcore_seg_t	mem_hdr;
	size_t		offset;
	ssize_t		sz;

	/*
	 * Read the kcore_hdr_t
	 */
	if (Lseek(kd, kd->pmfd, (off_t)0, SEEK_SET) == -1)
		return (-1);
	sz = Read(kd, kd->pmfd, &kcore_hdr, sizeof(kcore_hdr));
	if (sz != sizeof(kcore_hdr))
		return (-1);

	/*
	 * Currently, we only support dump-files made by the current
	 * architecture...
	 */
	if ((CORE_GETMAGIC(kcore_hdr) != KCORE_MAGIC) ||
	    (CORE_GETMID(kcore_hdr) != MID_MACHINE))
		return (-1);

	/*
	 * Currently, we only support exactly 2 segments: cpu-segment
	 * and data-segment in exactly that order.
	 */
	if (kcore_hdr.c_nseg != 2)
		return (-1);

	/*
	 * Save away the kcore_hdr.  All errors after this
	 * should do a to "goto fail" to deallocate things.
	 */
	kd->kcore_hdr = _kvm_malloc(kd, sizeof(kcore_hdr));
	memcpy(kd->kcore_hdr, &kcore_hdr, sizeof(kcore_hdr));
	offset = kcore_hdr.c_hdrsize;

	/*
	 * Read the CPU segment header
	 */
	if (Lseek(kd, kd->pmfd, (off_t)offset, SEEK_SET) == -1)
		goto fail;
	sz = Read(kd, kd->pmfd, &cpu_hdr, sizeof(cpu_hdr));
	if (sz != sizeof(cpu_hdr))
		goto fail;
	if ((CORE_GETMAGIC(cpu_hdr) != KCORESEG_MAGIC) ||
	    (CORE_GETFLAG(cpu_hdr) != CORE_CPU))
		goto fail;
	offset += kcore_hdr.c_seghdrsize;

	/*
	 * Read the CPU segment DATA.
	 */
	kd->cpu_dsize = cpu_hdr.c_size;
	kd->cpu_data = _kvm_malloc(kd, cpu_hdr.c_size);
	if (kd->cpu_data == NULL)
		goto fail;
	if (Lseek(kd, kd->pmfd, (off_t)offset, SEEK_SET) == -1)
		goto fail;
	sz = Read(kd, kd->pmfd, kd->cpu_data, cpu_hdr.c_size);
	if (sz != cpu_hdr.c_size)
		goto fail;
	offset += cpu_hdr.c_size;

	/*
	 * Read the next segment header: data segment
	 */
	if (Lseek(kd, kd->pmfd, (off_t)offset, SEEK_SET) == -1)
		goto fail;
	sz = Read(kd, kd->pmfd, &mem_hdr, sizeof(mem_hdr));
	if (sz != sizeof(mem_hdr))
		goto fail;
	offset += kcore_hdr.c_seghdrsize;

	if ((CORE_GETMAGIC(mem_hdr) != KCORESEG_MAGIC) ||
	    (CORE_GETFLAG(mem_hdr) != CORE_DATA))
		goto fail;

	kd->dump_off = offset;
	return (0);

fail:
	if (kd->kcore_hdr != NULL) {
		free(kd->kcore_hdr);
		kd->kcore_hdr = NULL;
	}
	if (kd->cpu_data != NULL) {
		free(kd->cpu_data);
		kd->cpu_data = NULL;
		kd->cpu_dsize = 0;
	}

}

/*
 * The format while on the dump device is: (new format)
 *    kcore_seg_t cpu_hdr;
 *    (opaque)    cpu_data; (size is cpu_hdr.c_size)
 *	  kcore_seg_t mem_hdr;
 *    (memory)    mem_data; (size is mem_hdr.c_size)
 */
int
kvm_dump_mkheader(kd, dump_off)
kvm_t	*kd;
off_t	dump_off;
{
	kcore_seg_t	cpu_hdr;
	int	hdr_size, sz;

	if (kd->kcore_hdr != NULL) {
	    _kvm_err(kd, kd->program, "already has a dump header");
	    return (-1);
	}
	if (ISALIVE(kd)) {
		_kvm_err(kd, kd->program, "don't use on live kernel");
		return (-1);
	}

	/*
	 * Validate new format crash dump
	 */
	if (Lseek(kd, kd->pmfd, dump_off, SEEK_SET) == -1)
		return (-1);
	sz = Read(kd, kd->pmfd, &cpu_hdr, sizeof(cpu_hdr));
	if (sz != sizeof(cpu_hdr))
		return (-1);
	if ((CORE_GETMAGIC(cpu_hdr) != KCORE_MAGIC)
		|| (CORE_GETMID(cpu_hdr) != MID_MACHINE)) {
		_kvm_err(kd, 0, "invalid magic in cpu_hdr");
		return (0);
	}
	hdr_size = ALIGN(sizeof(cpu_hdr));

	/*
	 * Read the CPU segment.
	 */
	kd->cpu_dsize = cpu_hdr.c_size;
	kd->cpu_data = _kvm_malloc(kd, kd->cpu_dsize);
	if (kd->cpu_data == NULL)
		goto fail;
	if (Lseek(kd, kd->pmfd, dump_off+hdr_size, SEEK_SET) == -1)
		goto fail;
	sz = Read(kd, kd->pmfd, kd->cpu_data, cpu_hdr.c_size);
	if (sz != cpu_hdr.c_size) {
		_kvm_err(kd, 0, "invalid size in cpu_hdr");
		goto fail;
	}
	hdr_size += kd->cpu_dsize;

	/*
	 * Leave phys mem pointer at beginning of memory data
	 */
	kd->dump_off = dump_off + hdr_size;
	if (Lseek(kd, kd->pmfd, kd->dump_off, SEEK_SET) == -1) {
		_kvm_err(kd, 0, "invalid dump offset");
		goto fail;
	}

	/*
	 * Create a kcore_hdr.
	 */
	kd->kcore_hdr = _kvm_malloc(kd, sizeof(kcore_hdr_t));
	if (kd->kcore_hdr == NULL) {
		_kvm_err(kd, 0, "missing header");
		goto fail;
	}

	kd->kcore_hdr->c_hdrsize    = ALIGN(sizeof(kcore_hdr_t));
	kd->kcore_hdr->c_seghdrsize = ALIGN(sizeof(kcore_seg_t));
	kd->kcore_hdr->c_nseg       = 2;
	CORE_SETMAGIC(*(kd->kcore_hdr), KCORE_MAGIC, MID_MACHINE,0);

	/*
	 * Now that we have a valid header, enable translations.
	 */
	if (_kvm_initvtop(kd) == 0)
		/* Success */
		return (hdr_size);

fail:
	if (kd->kcore_hdr != NULL) {
		free(kd->kcore_hdr);
		kd->kcore_hdr = NULL;
	}
	if (kd->cpu_data != NULL) {
		free(kd->cpu_data);
		kd->cpu_data = NULL;
		kd->cpu_dsize = 0;
	}
	return (-1);
}

static int
clear_gap(kd, fp, size)
kvm_t	*kd;
FILE	*fp;
int	size;
{
	if (size <= 0) /* XXX - < 0 should never happen */
		return (0);
	while (size-- > 0) {
		if (fputc(0, fp) == EOF) {
			_kvm_syserr(kd, kd->program, "clear_gap");
			return (-1);
		}
	}
	return (0);
}

/*
 * Write the dump header info to 'fp'. Note that we can't use fseek(3) here
 * because 'fp' might be a file pointer obtained by zopen().
 */
int
kvm_dump_wrtheader(kd, fp, dumpsize)
kvm_t	*kd;
FILE	*fp;
int	dumpsize;
{
	kcore_seg_t	seghdr;
	long		offset;
	int		gap;

	if (kd->kcore_hdr == NULL || kd->cpu_data == NULL) {
		_kvm_err(kd, kd->program, "no valid dump header(s)");
		return (-1);
	}

	/*
	 * Write the generic header
	 */
	offset = 0;
	if (fwrite((void*)kd->kcore_hdr, sizeof(kcore_hdr_t), 1, fp) <= 0) {
		_kvm_syserr(kd, kd->program, "kvm_dump_wrtheader");
		return (-1);
	}
	offset += kd->kcore_hdr->c_hdrsize;
	gap     = kd->kcore_hdr->c_hdrsize - sizeof(kcore_hdr_t);
	if (clear_gap(kd, fp, gap) == -1)
		return (-1);

	/*
	 * Write the cpu header
	 */
	CORE_SETMAGIC(seghdr, KCORESEG_MAGIC, 0, CORE_CPU);
	seghdr.c_size = ALIGN(kd->cpu_dsize);
	if (fwrite((void*)&seghdr, sizeof(seghdr), 1, fp) <= 0) {
		_kvm_syserr(kd, kd->program, "kvm_dump_wrtheader");
		return (-1);
	}
	offset += kd->kcore_hdr->c_seghdrsize;
	gap     = kd->kcore_hdr->c_seghdrsize - sizeof(seghdr);
	if (clear_gap(kd, fp, gap) == -1)
		return (-1);

	if (fwrite((void*)kd->cpu_data, kd->cpu_dsize, 1, fp) <= 0) {
		_kvm_syserr(kd, kd->program, "kvm_dump_wrtheader");
		return (-1);
	}
	offset += seghdr.c_size;
	gap     = seghdr.c_size - kd->cpu_dsize;
	if (clear_gap(kd, fp, gap) == -1)
		return (-1);

	/*
	 * Write the actual dump data segment header
	 */
	CORE_SETMAGIC(seghdr, KCORESEG_MAGIC, 0, CORE_DATA);
	seghdr.c_size = dumpsize;
	if (fwrite((void*)&seghdr, sizeof(seghdr), 1, fp) <= 0) {
		_kvm_syserr(kd, kd->program, "kvm_dump_wrtheader");
		return (-1);
	}
	offset += kd->kcore_hdr->c_seghdrsize;
	gap     = kd->kcore_hdr->c_seghdrsize - sizeof(seghdr);
	if (clear_gap(kd, fp, gap) == -1)
		return (-1);

	return (offset);
}

kvm_t *
kvm_openfiles(uf, mf, sf, flag, errout)
	const char *uf;
	const char *mf;
	const char *sf;
	int flag;
	char *errout;
{
	register kvm_t *kd;

	if ((kd = malloc(sizeof(*kd))) == NULL) {
		(void)strncpy(errout, strerror(errno), _POSIX2_LINE_MAX - 1);
		return (0);
	}
	kd->program = 0;
	return (_kvm_open(kd, uf, mf, sf, flag, errout));
}

kvm_t *
kvm_open(uf, mf, sf, flag, program)
	const char *uf;
	const char *mf;
	const char *sf;
	int flag;
	const char *program;
{
	register kvm_t *kd;

	if ((kd = malloc(sizeof(*kd))) == NULL && program != NULL) {
		(void)fprintf(stderr, "%s: %s\n", strerror(errno));
		return (0);
	}
	kd->program = program;
	return (_kvm_open(kd, uf, mf, sf, flag, NULL));
}

int
kvm_close(kd)
	kvm_t *kd;
{
	register int error = 0;

	if (kd->pmfd >= 0)
		error |= close(kd->pmfd);
	if (kd->vmfd >= 0)
		error |= close(kd->vmfd);
	if (kd->nlfd >= 0)
		error |= close(kd->nlfd);
	if (kd->swfd >= 0)
		error |= close(kd->swfd);
	if (kd->db != 0)
		error |= (kd->db->close)(kd->db);
	if (kd->vmst)
		_kvm_freevtop(kd);
	kd->cpu_dsize = 0;
	if (kd->cpu_data != NULL)
		free((void *)kd->cpu_data);
	if (kd->kcore_hdr != NULL)
		free((void *)kd->kcore_hdr);
	if (kd->procbase != 0)
		free((void *)kd->procbase);
	if (kd->swapspc != 0)
		free((void *)kd->swapspc);
	if (kd->argspc != 0)
		free((void *)kd->argspc);
	if (kd->argbuf != 0)
		free((void *)kd->argbuf);
	if (kd->argv != 0)
		free((void *)kd->argv);
	free((void *)kd);

	return (0);
}

/*
 * Set up state necessary to do queries on the kernel namelist
 * data base.  If the data base is out-of-data/incompatible with 
 * given executable, set up things so we revert to standard nlist call.
 * Only called for live kernels.  Return 0 on success, -1 on failure.
 */
static int
kvm_dbopen(kd, uf)
	kvm_t *kd;
	const char *uf;
{
	DBT rec;
	int dbversionlen;
	struct nlist nitem;
	char dbversion[_POSIX2_LINE_MAX];
	char kversion[_POSIX2_LINE_MAX];
	char dbname[MAXPATHLEN];

	uf = basename((char *)uf);

	(void)snprintf(dbname, sizeof(dbname), "%skvm_%s.db", _PATH_VARDB, uf);
	kd->db = dbopen(dbname, O_RDONLY, 0, DB_HASH, NULL);
	if (kd->db == NULL) {
		switch (errno) {
		case ENOENT:
			/* No kvm_bsd.db, fall back to /bsd silently */
			break;
		case EFTYPE:
			_kvm_err(kd, kd->program,
			    "file %s is incorrectly formatted", dbname);
			break;
		case EINVAL:
			_kvm_err(kd, kd->program,
			    "invalid argument to dbopen()");
			break;
		default:
			_kvm_err(kd, kd->program, "unknown dbopen() error");
			break;
		}
		return (-1);
	}

	/*
	 * read version out of database
	 */
	rec.data = VRS_KEY;
	rec.size = sizeof(VRS_KEY) - 1;
	if ((kd->db->get)(kd->db, (DBT *)&rec, (DBT *)&rec, 0))
		goto close;
	if (rec.data == 0 || rec.size > sizeof(dbversion))
		goto close;

	bcopy(rec.data, dbversion, rec.size);
	dbversionlen = rec.size;
	/*
	 * Read version string from kernel memory.
	 * Since we are dealing with a live kernel, we can call kvm_read()
	 * at this point.
	 */
	rec.data = VRS_SYM;
	rec.size = sizeof(VRS_SYM) - 1;
	if ((kd->db->get)(kd->db, (DBT *)&rec, (DBT *)&rec, 0))
		goto close;
	if (rec.data == 0 || rec.size != sizeof(struct nlist))
		goto close;
	bcopy((char *)rec.data, (char *)&nitem, sizeof(nitem));
	if (kvm_read(kd, (u_long)nitem.n_value, kversion, dbversionlen) != 
	    dbversionlen)
		goto close;
	/*
	 * If they match, we win - otherwise clear out kd->db so
	 * we revert to slow nlist().
	 */
	if (bcmp(dbversion, kversion, dbversionlen) == 0)
		return (0);
close:
	(void)(kd->db->close)(kd->db);
	kd->db = 0;

	return (-1);
}

int
kvm_nlist(kd, nl)
	kvm_t *kd;
	struct nlist *nl;
{
	register struct nlist *p;
	register int nvalid, rv;

	/*
	 * If we can't use the data base, revert to the 
	 * slow library call.
	 */
	if (kd->db == 0) {
		rv = __fdnlist(kd->nlfd, nl);
		if (rv == -1)
			_kvm_err(kd, 0, "bad namelist");
		return (rv);
	}

	/*
	 * We can use the kvm data base.  Go through each nlist entry
	 * and look it up with a db query.
	 */
	nvalid = 0;
	for (p = nl; p->n_name && p->n_name[0]; ++p) {
		register int len;
		DBT rec;

		if ((len = strlen(p->n_name)) > 4096) {
			/* sanity */
			_kvm_err(kd, kd->program, "symbol too large");
			return (-1);
		}
		rec.data = p->n_name;
		rec.size = len;

		/*
		 * Make sure that n_value = 0 when the symbol isn't found
		 */
		p->n_value = 0;

		if ((kd->db->get)(kd->db, (DBT *)&rec, (DBT *)&rec, 0))
			continue;
		if (rec.data == 0 || rec.size != sizeof(struct nlist))
			continue;
		++nvalid;
		/*
		 * Avoid alignment issues.
		 */
		bcopy((char *)&((struct nlist *)rec.data)->n_type,
		      (char *)&p->n_type, 
		      sizeof(p->n_type));
		bcopy((char *)&((struct nlist *)rec.data)->n_value,
		      (char *)&p->n_value, 
		      sizeof(p->n_value));
	}
	/*
	 * Return the number of entries that weren't found.
	 */
	return ((p - nl) - nvalid);
}

int kvm_dump_inval(kd)
kvm_t	*kd;
{
	struct nlist	nlist[2];
	u_long		pa;

	if (ISALIVE(kd)) {
		_kvm_err(kd, kd->program, "clearing dump on live kernel");
		return (-1);
	}
	nlist[0].n_name = "_dumpmag";
	nlist[1].n_name = NULL;

	if (kvm_nlist(kd, nlist) == -1) {
		_kvm_err(kd, 0, "bad namelist");
		return (-1);
	}
	if (_kvm_kvatop(kd, (u_long)nlist[0].n_value, &pa) == 0)
		return (-1);

	errno = 0;
	if (lseek(kd->pmfd, _kvm_pa2off(kd, pa), SEEK_SET) == -1
		&& errno != 0) {
		_kvm_err(kd, 0, "cannot invalidate dump - lseek");
		return (-1);
	}
	pa = 0;
	if (write(kd->pmfd, &pa, sizeof(pa)) != sizeof(pa)) {
		_kvm_err(kd, 0, "cannot invalidate dump - write");
		return (-1);
	}
	return (0);
}

ssize_t
kvm_read(kd, kva, buf, len)
	kvm_t *kd;
	register u_long kva;
	register void *buf;
	register size_t len;
{
	register int cc;
	register void *cp;

	if (ISALIVE(kd)) {
		/*
		 * We're using /dev/kmem.  Just read straight from the
		 * device and let the active kernel do the address translation.
		 */
		errno = 0;
		if (lseek(kd->vmfd, (off_t)kva, SEEK_SET) == -1
			&& errno != 0) {
			_kvm_err(kd, 0, "invalid address (%lx)", kva);
			return (-1);
		}
		cc = read(kd->vmfd, buf, len);
		if (cc < 0) {
			_kvm_syserr(kd, 0, "kvm_read");
			return (-1);
		} else if (cc < len)
			_kvm_err(kd, kd->program, "short read");
		return (cc);
	} else {
		if ((kd->kcore_hdr == NULL) || (kd->cpu_data == NULL)) {
			_kvm_err(kd, kd->program, "no valid dump header");
			return (-1);
		}
		cp = buf;
		while (len > 0) {
			u_long	pa;
			off_t	foff;
		
			/* In case of error, _kvm_kvatop sets the err string */
			cc = _kvm_kvatop(kd, kva, &pa);
			if (cc == 0)
				return (-1);
			if (cc > len)
				cc = len;
			foff = _kvm_pa2off(kd, pa);
			errno = 0;
			if (lseek(kd->pmfd, foff, SEEK_SET) == -1
				&& errno != 0) {
				_kvm_syserr(kd, 0, _PATH_MEM);
				break;
			}
			cc = read(kd->pmfd, cp, cc);
			if (cc < 0) {
				_kvm_syserr(kd, kd->program, "kvm_read");
				break;
			}
			/*
			 * If kvm_kvatop returns a bogus value or our core
			 * file is truncated, we might wind up seeking beyond
			 * the end of the core file in which case the read will
			 * return 0 (EOF).
			 */
			if (cc == 0)
				break;
			cp = (char *)cp + cc;
			kva += cc;
			len -= cc;
		}
		return ((char *)cp - (char *)buf);
	}
	/* NOTREACHED */
}

ssize_t
kvm_write(kd, kva, buf, len)
	kvm_t *kd;
	register u_long kva;
	register const void *buf;
	register size_t len;
{
	register int cc;

	if (ISALIVE(kd)) {
		/*
		 * Just like kvm_read, only we write.
		 */
		errno = 0;
		if (lseek(kd->vmfd, (off_t)kva, SEEK_SET) == -1
			&& errno != 0) {
			_kvm_err(kd, 0, "invalid address (%lx)", kva);
			return (-1);
		}
		cc = write(kd->vmfd, buf, len);
		if (cc < 0) {
			_kvm_syserr(kd, 0, "kvm_write");
			return (-1);
		} else if (cc < len)
			_kvm_err(kd, kd->program, "short write");
		return (cc);
	} else {
		_kvm_err(kd, kd->program,
		    "kvm_write not implemented for dead kernels");
		return (-1);
	}
	/* NOTREACHED */
}
