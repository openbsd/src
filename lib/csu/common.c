/*	$OpenBSD: common.c,v 1.14 2008/01/29 13:02:31 krw Exp $	*/
/*	$NetBSD: common.c,v 1.4 1995/09/23 22:34:20 pk Exp $	*/
/*
 * Copyright (c) 1993,1995 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#ifdef DYNAMIC

typedef int (*rtld_entry_fn)(int, struct crt_ldso *);
static struct ld_entry	**ld_entry;

static void
__load_rtld(dp)
	struct _dynamic *dp;
{
	static struct crt_ldso	crt;
	struct exec	hdr;
	rtld_entry_fn	entry;

#ifdef DEBUG
	/* Provision for alternate ld.so - security risk! */
	if ((crt.crt_ldso = _getenv("LDSO")) == NULL)
#endif
		crt.crt_ldso = LDSO;

	crt.crt_ldfd = open(crt.crt_ldso, 0, 0);
	if (crt.crt_ldfd == -1) {
		/* If we don't need ld.so then just return instead bail out. */
		if (!LD_NEED(dp)) {
			ld_entry = 0;
			return;
		}
		_FATAL("No ld.so\n");
	}

	/* Read LDSO exec header */
	if (read(crt.crt_ldfd, &hdr, sizeof hdr) != sizeof hdr) {
		_FATAL("Failure reading ld.so\n");
	}
	if (N_GETMAGIC(hdr) != ZMAGIC && N_GETMAGIC(hdr) != QMAGIC) {
		_FATAL("Bad magic: ld.so\n");
	}

	/* We use MAP_ANON */
	crt.crt_dzfd = -1;

	/* Map in ld.so */
	crt.crt_ba = mmap(0, hdr.a_text+hdr.a_data+hdr.a_bss,
			PROT_READ|PROT_EXEC,
			MAP_PRIVATE,
			crt.crt_ldfd, N_TXTOFF(hdr));
	if (crt.crt_ba == -1) {
		_FATAL("Cannot map ld.so\n");
	}

/* !!!
 * This is gross, ld.so is a ZMAGIC a.out, but has `sizeof(hdr)' for
 * an entry point and not at PAGSIZ as the N_*ADDR macros assume.
 */
#undef N_DATADDR
#undef N_BSSADDR
#define N_DATADDR(x)	((x).a_text)
#define N_BSSADDR(x)	((x).a_text + (x).a_data)

	/*
	 * Map in data segment of ld.so writable
	 * The segment must be PROT_EXEC too because the
	 * GOT is located there.
	 */
	if (mmap(crt.crt_ba+N_DATADDR(hdr), hdr.a_data,
			PROT_READ|PROT_WRITE|PROT_EXEC,
			MAP_FIXED|MAP_PRIVATE,
			crt.crt_ldfd, N_DATOFF(hdr)) == -1) {
		_FATAL("Cannot map ld.so\n");
	}

	/* Map bss segment of ld.so zero */
	if (hdr.a_bss && mmap(crt.crt_ba+N_BSSADDR(hdr), hdr.a_bss,
			PROT_READ|PROT_WRITE,
			MAP_FIXED|MAP_ANON|MAP_PRIVATE,
			crt.crt_dzfd, 0) == -1) {
		_FATAL("Cannot map ld.so\n");
	}

	crt.crt_dp = dp;
	crt.crt_ep = environ;
	crt.crt_bp = (caddr_t)_callmain;
	crt.crt_prog = __progname;

	ld_entry = &crt.crt_ldentry;
	entry = (rtld_entry_fn)(crt.crt_ba + sizeof hdr);
	if ((*entry)(CRT_VERSION_BSD_4, &crt) == -1) {
		/* Feeble attempt to deal with out-dated ld.so */
#		define str "crt0: update /usr/libexec/ld.so\n"
		(void)write(2, str, sizeof(str));
#		undef str
		if ((*entry)(CRT_VERSION_BSD_3, &crt) == -1) {
			_FATAL("ld.so failed\n");
		}
		ld_entry = &dp->d_entry;
		return;
	}
	atexit((*ld_entry)->dlexit);
}

/*
 * DL stubs
 */

void *
dlopen(name, mode)
	const char	*name;
	int		mode;
{
	if ((*ld_entry) == NULL)
		return NULL;

	return ((*ld_entry)->dlopen)(name, mode);
}

int
dlclose(fd)
	void	*fd;
{
	if ((*ld_entry) == NULL)
		return -1;

	return ((*ld_entry)->dlclose)(fd);
}

void *
dlsym(fd, name)
	void		*fd;
	const char	*name;
{
	if ((*ld_entry) == NULL)
		return NULL;

	return ((*ld_entry)->dlsym)(fd, name);
}

int
dlctl(fd, cmd, arg)
void	*fd, *arg;
int	cmd;
{
	if ((*ld_entry) == NULL)
		return -1;

	return ((*ld_entry)->dlctl)(fd, cmd, arg);
}

const char *
dlerror()
{
	int     error;

	if ((*ld_entry) == NULL ||
	    ((*ld_entry)->dlctl)(NULL, DL_GETERRNO, &error) == -1)
		return "Service unavailable";

	if (error == 0)
		return NULL;
	return (char *)strerror(error);
}

/*
 * Support routines
 */

#ifdef DEBUG
static int
_strncmp(s1, s2, n)
	register char *s1, *s2;
	register int n;
{

	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

static char *
_getenv(name)
	register char *name;
{
	extern char **environ;
	register int len;
	register char **P, *C;

	for (C = name, len = 0; *C && *C != '='; ++C, ++len);
	for (P = environ; *P; ++P)
		if (!_strncmp(*P, name, len))
			if (*(C = *P + len) == '=') {
				return(++C);
			}
	return (char *)0;
}
#endif

#endif /* DYNAMIC */

static char *
_strrchr(p, ch)
register char *p, ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}
