/*	$OpenBSD: io.c,v 1.5 1998/06/25 19:02:00 millert Exp $	*/

/*
 * shell buffered IO and formatted output
 */

#include <ctype.h>
#include "sh.h"
#include "ksh_stat.h"

static int initio_done;

/*
 * formatted output functions
 */


/* A shell error occured (eg, syntax error, etc.) */
void
#ifdef HAVE_PROTOTYPES
errorf(const char *fmt, ...)
#else
errorf(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	shl_stdout_ok = 0;	/* debugging: note that stdout not valid */
	exstat = 1;
	if (*fmt) {
		error_prefix(TRUE);
		SH_VA_START(va, fmt);
		shf_vfprintf(shl_out, fmt, va);
		va_end(va);
		shf_putchar('\n', shl_out);
	}
	shf_flush(shl_out);
	unwind(LERROR);
}

/* like errorf(), but no unwind is done */
void
#ifdef HAVE_PROTOTYPES
warningf(int fileline, const char *fmt, ...)
#else
warningf(fileline, fmt, va_alist)
	int fileline;
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	error_prefix(fileline);
	SH_VA_START(va, fmt);
	shf_vfprintf(shl_out, fmt, va);
	va_end(va);
	shf_putchar('\n', shl_out);
	shf_flush(shl_out);
}

/* Used by built-in utilities to prefix shell and utility name to message
 * (also unwinds environments for special builtins).
 */
void
#ifdef HAVE_PROTOTYPES
bi_errorf(const char *fmt, ...)
#else
bi_errorf(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	shl_stdout_ok = 0;	/* debugging: note that stdout not valid */
	exstat = 1;
	if (*fmt) {
		error_prefix(TRUE);
		/* not set when main() calls parse_args() */
		if (builtin_argv0)
			shf_fprintf(shl_out, "%s: ", builtin_argv0);
		SH_VA_START(va, fmt);
		shf_vfprintf(shl_out, fmt, va);
		va_end(va);
		shf_putchar('\n', shl_out);
	}
	shf_flush(shl_out);
	/* POSIX special builtins and ksh special builtins cause
	 * non-interactive shells to exit.
	 * XXX odd use of KEEPASN; also may not want LERROR here
	 */
	if ((builtin_flag & SPEC_BI)
	    || (Flag(FPOSIX) && (builtin_flag & KEEPASN)))
	{
		builtin_argv0 = (char *) 0;
		unwind(LERROR);
	}
}

/* Called when something that shouldn't happen does */
void
#ifdef HAVE_PROTOTYPES
internal_errorf(int jump, const char *fmt, ...)
#else
internal_errorf(jump, fmt, va_alist)
	int jump;
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	error_prefix(TRUE);
	shf_fprintf(shl_out, "internal error: ");
	SH_VA_START(va, fmt);
	shf_vfprintf(shl_out, fmt, va);
	va_end(va);
	shf_putchar('\n', shl_out);
	shf_flush(shl_out);
	if (jump)
		unwind(LERROR);
}

/* used by error reporting functions to print "ksh: .kshrc[25]: " */
void
error_prefix(fileline)
	int fileline;
{
	/* Avoid foo: foo[2]: ... */
	if (!fileline || !source || !source->file
	    || strcmp(source->file, kshname) != 0)
		shf_fprintf(shl_out, "%s: ", kshname + (*kshname == '-'));
	if (fileline && source && source->file != NULL) {
		shf_fprintf(shl_out, "%s[%d]: ", source->file,
			source->errline > 0 ? source->errline : source->line);
		source->errline = 0;
	}
}

/* printf to shl_out (stderr) with flush */
void
#ifdef HAVE_PROTOTYPES
shellf(const char *fmt, ...)
#else
shellf(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	if (!initio_done) /* shl_out may not be set up yet... */
		return;
	SH_VA_START(va, fmt);
	shf_vfprintf(shl_out, fmt, va);
	va_end(va);
	shf_flush(shl_out);
}

/* printf to shl_stdout (stdout) */
void
#ifdef HAVE_PROTOTYPES
shprintf(const char *fmt, ...)
#else
shprintf(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	if (!shl_stdout_ok)
		internal_errorf(1, "shl_stdout not valid");
	SH_VA_START(va, fmt);
	shf_vfprintf(shl_stdout, fmt, va);
	va_end(va);
}

/* test if we can seek backwards fd (returns 0 or SHF_UNBUF) */
int
can_seek(fd)
	int fd;
{
	struct stat statb;

	return fstat(fd, &statb) == 0 && !S_ISREG(statb.st_mode) ?
		SHF_UNBUF : 0;
}

struct shf	shf_iob[3];

void
initio()
{
	shf_fdopen(1, SHF_WR, shl_stdout);	/* force buffer allocation */
	shf_fdopen(2, SHF_WR, shl_out);
	shf_fdopen(2, SHF_WR, shl_spare);	/* force buffer allocation */
	initio_done = 1;
}

/* A dup2() with error checking */
int
ksh_dup2(ofd, nfd, errok)
	int ofd;
	int nfd;
	int errok;
{
	int ret = dup2(ofd, nfd);

	if (ret < 0 && errno != EBADF && !errok)
		errorf("too many files open in shell");

#ifdef DUP2_BROKEN
	/* Ultrix systems like to preserve the close-on-exec flag */
	if (ret >= 0)
		(void) fcntl(nfd, F_SETFD, 0);
#endif /* DUP2_BROKEN */

	return ret;
}

/*
 * move fd from user space (0<=fd<10) to shell space (fd>=10),
 * set close-on-exec flag.
 */
int
savefd(fd, noclose)
	int fd;
	int noclose;
{
	int nfd;

	if (fd < FDBASE) {
		nfd = ksh_dupbase(fd, FDBASE);
		if (nfd < 0)
			if (errno == EBADF)
				return -1;
			else
				errorf("too many files open in shell");
		if (!noclose)
			close(fd);
	} else
		nfd = fd;
	fd_clexec(nfd);
	return nfd;
}

void
restfd(fd, ofd)
	int fd, ofd;
{
	if (fd == 2)
		shf_flush(&shf_iob[fd]);
	if (ofd < 0)		/* original fd closed */
		close(fd);
	else {
		ksh_dup2(ofd, fd, TRUE); /* XXX: what to do if this fails? */
		close(ofd);
	}
}

void
openpipe(pv)
	register int *pv;
{
	if (pipe(pv) < 0)
		errorf("can't create pipe - try again");
	pv[0] = savefd(pv[0], 0);
	pv[1] = savefd(pv[1], 0);
}

void
closepipe(pv)
	register int *pv;
{
	close(pv[0]);
	close(pv[1]);
}

/* Called by iosetup() (deals with 2>&4, etc.), c_read, c_print to turn
 * a string (the X in 2>&X, read -uX, print -uX) into a file descriptor.
 */
int
check_fd(name, mode, emsgp)
	char *name;
	int mode;
	const char **emsgp;
{
	int fd, fl;

	if (isdigit(name[0]) && !name[1]) {
		fd = name[0] - '0';
		if ((fl = fcntl(fd = name[0] - '0', F_GETFL, 0)) < 0) {
			if (emsgp)
				*emsgp = "bad file descriptor";
			return -1;
		}
		fl &= O_ACCMODE;
#ifdef OS2
		if (mode == W_OK ) { 
		       if (setmode(fd, O_TEXT) == -1) {
				if (emsgp)
					*emsgp = "couldn't set write mode";
				return -1;
			}
		 } else if (mode == R_OK)
	      		if (setmode(fd, O_BINARY) == -1) {
				if (emsgp)
					*emsgp = "couldn't set read mode";
				return -1; 
			}
#else /* OS2 */
		/* X_OK is a kludge to disable this check for dups (x<&1):
		 * historical shells never did this check (XXX don't know what
		 * posix has to say).
		 */
		if (!(mode & X_OK) && fl != O_RDWR
		    && (((mode & R_OK) && fl != O_RDONLY)
			|| ((mode & W_OK) && fl != O_WRONLY)))
		{
			if (emsgp)
				*emsgp = (fl == O_WRONLY) ?
						"fd not open for reading"
					      : "fd not open for writing";
			return -1;
		}
#endif /* OS2 */
		return fd;
	}
#ifdef KSH
	else if (name[0] == 'p' && !name[1])
		return coproc_getfd(mode, emsgp);
#endif /* KSH */
	if (emsgp)
		*emsgp = "illegal file descriptor name";
	return -1;
}

#ifdef KSH
/* Called once from main */
void
coproc_init()
{
	coproc.read = coproc.readw = coproc.write = -1;
	coproc.njobs = 0;
	coproc.id = 0;
}

/* Called by c_read() when eof is read - close fd if it is the co-process fd */
void
coproc_read_close(fd)
	int fd;
{
	if (coproc.read >= 0 && fd == coproc.read) {
		coproc_readw_close(fd);
		close(coproc.read);
		coproc.read = -1;
	}
}

/* Called by c_read() and by iosetup() to close the other side of the
 * read pipe, so reads will actually terminate.
 */
void
coproc_readw_close(fd)
	int fd;
{
	if (coproc.readw >= 0 && coproc.read >= 0 && fd == coproc.read) {
		close(coproc.readw);
		coproc.readw = -1;
	}
}

/* Called by c_print when a write to a fd fails with EPIPE and by iosetup
 * when co-process input is dup'd
 */
void
coproc_write_close(fd)
	int fd;
{
	if (coproc.write >= 0 && fd == coproc.write) {
		close(coproc.write);
		coproc.write = -1;
	}
}

/* Called to check for existance of/value of the co-process file descriptor.
 * (Used by check_fd() and by c_read/c_print to deal with -p option).
 */
int
coproc_getfd(mode, emsgp)
	int mode;
	const char **emsgp;
{
	int fd = (mode & R_OK) ? coproc.read : coproc.write;

	if (fd >= 0)
		return fd;
	if (emsgp)
		*emsgp = "no coprocess";
	return -1;
}

/* called to close file descriptors related to the coprocess (if any)
 * Should be called with SIGCHLD blocked.
 */
void
coproc_cleanup(reuse)
	int reuse;
{
	/* This to allow co-processes to share output pipe */
	if (!reuse || coproc.readw < 0 || coproc.read < 0) {
		if (coproc.read >= 0) {
			close(coproc.read);
			coproc.read = -1;
		}
		if (coproc.readw >= 0) {
			close(coproc.readw);
			coproc.readw = -1;
		}
	}
	if (coproc.write >= 0) {
		close(coproc.write);
		coproc.write = -1;
	}
}
#endif /* KSH */

/*
 * temporary files
 */

struct temp *
maketemp(ap)
	Area *ap;
{
	static unsigned int inc;
	struct temp *tp;
	int len;
	int fd;
	char *path;
	const char *tmp;

	tmp = tmpdir ? tmpdir : "/tmp";
	/* The 20 + 20 is a paranoid worst case for pid/inc */
	len = strlen(tmp) + 3 + 20 + 20 + 1;
	tp = (struct temp *) alloc(sizeof(struct temp) + len, ap);
	tp->name = path = (char *) &tp[1];
	tp->shf = (struct shf *) 0;
	while (1) {
		/* Note that temp files need to fit 8.3 DOS limits */
		shf_snprintf(path, len, "%s/sh%05u.%03x",
			tmp, (unsigned) procpid, inc++);
		/* Mode 0600 to be paranoid, O_TRUNC in case O_EXCL isn't
		 * really there.
		 */
		fd = open(path, O_RDWR|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd >= 0) {
			tp->shf = shf_fdopen(fd, SHF_WR, (struct shf *) 0);
			break;
		}
		if (errno != EINTR
#ifdef EEXIST
		    && errno != EEXIST
#endif /* EEXIST */
#ifdef EISDIR
		    && errno != EISDIR
#endif /* EISDIR */
			)
			/* Error must be printed by called: don't know here if
			 * errorf() or bi_errorf() should be used.
			 */
			break;
	}
	tp->next = NULL;
	tp->pid = procpid;
	return tp;
}
