/*    doio.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *  Far below them they saw the white waters pour into a foaming bowl, and
 *  then swirl darkly about a deep oval basin in the rocks, until they found
 *  their way out again through a narrow gate, and flowed away, fuming and
 *  chattering, into calmer and more level reaches.
 *
 *     [p.684 of _The Lord of the Rings_, IV/vi: "The Forbidden Pool"]
 */

/* This file contains functions that do the actual I/O on behalf of ops.
 * For example, pp_print() calls the do_print() function in this file for
 * each argument needing printing.
 */

#include "EXTERN.h"
#define PERL_IN_DOIO_C
#include "perl.h"

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
#ifndef HAS_SEM
#include <sys/ipc.h>
#endif
#ifdef HAS_MSG
#include <sys/msg.h>
#endif
#ifdef HAS_SHM
#include <sys/shm.h>
# ifndef HAS_SHMAT_PROTOTYPE
    extern Shmat_t shmat (int, char *, int);
# endif
#endif
#endif

#ifdef I_UTIME
#  if defined(_MSC_VER) || defined(__MINGW32__)
#    include <sys/utime.h>
#  else
#    include <utime.h>
#  endif
#endif

#ifdef O_EXCL
#  define OPEN_EXCL O_EXCL
#else
#  define OPEN_EXCL 0
#endif

#define PERL_MODE_MAX 8
#define PERL_FLAGS_MAX 10

#include <signal.h>

static IO *
S_openn_setup(pTHX_ GV *gv, char *mode, PerlIO **saveifp, PerlIO **saveofp,
              int *savefd,  char *savetype)
{
    IO * const io = GvIOn(gv);

    PERL_ARGS_ASSERT_OPENN_SETUP;

    *saveifp = NULL;
    *saveofp = NULL;
    *savefd = -1;
    *savetype = IoTYPE_CLOSED;

    Zero(mode,sizeof(mode),char);
    PL_forkprocess = 1;		/* assume true if no fork */

    /* If currently open - close before we re-open */
    if (IoIFP(io)) {
	if (IoTYPE(io) == IoTYPE_STD) {
	    /* This is a clone of one of STD* handles */
	}
	else {
            const int old_fd = PerlIO_fileno(IoIFP(io));

            if (old_fd >= 0 && old_fd <= PL_maxsysfd) {
                /* This is one of the original STD* handles */
                *saveifp  = IoIFP(io);
                *saveofp  = IoOFP(io);
                *savetype = IoTYPE(io);
                *savefd   = old_fd;
            }
            else {
                int result;

                if (IoTYPE(io) == IoTYPE_PIPE)
                    result = PerlProc_pclose(IoIFP(io));
                else if (IoIFP(io) != IoOFP(io)) {
                    if (IoOFP(io)) {
                        result = PerlIO_close(IoOFP(io));
                        PerlIO_close(IoIFP(io)); /* clear stdio, fd already closed */
                    }
                    else
                        result = PerlIO_close(IoIFP(io));
                }
                else
                    result = PerlIO_close(IoIFP(io));

                if (result == EOF && old_fd > PL_maxsysfd) {
                    /* Why is this not Perl_warn*() call ? */
                    PerlIO_printf(Perl_error_log,
                                  "Warning: unable to close filehandle %"HEKf" properly.\n",
                                  HEKfARG(GvENAME_HEK(gv))
                        );
                }
            }
        }
	IoOFP(io) = IoIFP(io) = NULL;
    }
    return io;
}

bool
Perl_do_openn(pTHX_ GV *gv, const char *oname, I32 len, int as_raw,
	      int rawmode, int rawperm, PerlIO *supplied_fp, SV **svp,
	      I32 num_svs)
{
    PERL_ARGS_ASSERT_DO_OPENN;

    if (as_raw) {
        /* sysopen style args, i.e. integer mode and permissions */

	if (num_svs != 0) {
	    Perl_croak(aTHX_ "panic: sysopen with multiple args, num_svs=%ld",
		       (long) num_svs);
	}
        return do_open_raw(gv, oname, len, rawmode, rawperm);
    }
    return do_open6(gv, oname, len, supplied_fp, svp, num_svs);
}

bool
Perl_do_open_raw(pTHX_ GV *gv, const char *oname, STRLEN len,
                 int rawmode, int rawperm)
{
    PerlIO *saveifp;
    PerlIO *saveofp;
    int savefd;
    char savetype;
    char mode[PERL_MODE_MAX];	/* file mode ("r\0", "rb\0", "ab\0" etc.) */
    IO * const io = openn_setup(gv, mode, &saveifp, &saveofp, &savefd, &savetype);
    int writing = 0;
    PerlIO *fp;

    PERL_ARGS_ASSERT_DO_OPEN_RAW;

    /* For ease of blame back to 5.000, keep the existing indenting. */
    {
        /* sysopen style args, i.e. integer mode and permissions */
	STRLEN ix = 0;
	const int appendtrunc =
	     0
#ifdef O_APPEND	/* Not fully portable. */
	     |O_APPEND
#endif
#ifdef O_TRUNC	/* Not fully portable. */
	     |O_TRUNC
#endif
	     ;
	const int modifyingmode = O_WRONLY|O_RDWR|O_CREAT|appendtrunc;
	int ismodifying;
        SV *namesv;

	/* It's not always

	   O_RDONLY 0
	   O_WRONLY 1
	   O_RDWR   2

	   It might be (in OS/390 and Mac OS Classic it is)

	   O_WRONLY 1
	   O_RDONLY 2
	   O_RDWR   3

	   This means that simple & with O_RDWR would look
	   like O_RDONLY is present.  Therefore we have to
	   be more careful.
	*/
	if ((ismodifying = (rawmode & modifyingmode))) {
	     if ((ismodifying & O_WRONLY) == O_WRONLY ||
		 (ismodifying & O_RDWR)   == O_RDWR   ||
		 (ismodifying & (O_CREAT|appendtrunc)))
		  TAINT_PROPER("sysopen");
	}
	mode[ix++] = IoTYPE_NUMERIC; /* Marker to openn to use numeric "sysopen" */

#if defined(USE_64_BIT_RAWIO) && defined(O_LARGEFILE)
	rawmode |= O_LARGEFILE;	/* Transparently largefiley. */
#endif

        IoTYPE(io) = PerlIO_intmode2str(rawmode, &mode[ix], &writing);

	namesv = newSVpvn_flags(oname, len, SVs_TEMP);
	fp = PerlIO_openn(aTHX_ NULL, mode, -1, rawmode, rawperm, NULL, 1, &namesv);
    }
    return openn_cleanup(gv, io, fp, mode, oname, saveifp, saveofp, savefd,
                         savetype, writing, 0, NULL);
}

bool
Perl_do_open6(pTHX_ GV *gv, const char *oname, STRLEN len,
              PerlIO *supplied_fp, SV **svp, U32 num_svs)
{
    PerlIO *saveifp;
    PerlIO *saveofp;
    int savefd;
    char savetype;
    char mode[PERL_MODE_MAX];	/* file mode ("r\0", "rb\0", "ab\0" etc.) */
    IO * const io = openn_setup(gv, mode, &saveifp, &saveofp, &savefd, &savetype);
    int writing = 0;
    PerlIO *fp;
    bool was_fdopen = FALSE;
    char *type  = NULL;

    PERL_ARGS_ASSERT_DO_OPEN6;

    /* For ease of blame back to 5.000, keep the existing indenting. */
    {
	/* Regular (non-sys) open */
	char *name;
	STRLEN olen = len;
	char *tend;
	int dodup = 0;
        bool in_raw = 0, in_crlf = 0, out_raw = 0, out_crlf = 0;

        /* Collect default raw/crlf info from the op */
        if (PL_op && PL_op->op_type == OP_OPEN) {
            /* set up IO layers */
            const U8 flags = PL_op->op_private;
            in_raw = (flags & OPpOPEN_IN_RAW);
            in_crlf = (flags & OPpOPEN_IN_CRLF);
            out_raw = (flags & OPpOPEN_OUT_RAW);
            out_crlf = (flags & OPpOPEN_OUT_CRLF);
        }

	type = savepvn(oname, len);
	tend = type+len;
	SAVEFREEPV(type);

        /* Lose leading and trailing white space */
	while (isSPACE(*type))
	    type++;
        while (tend > type && isSPACE(tend[-1]))
	    *--tend = '\0';

	if (num_svs) {
            const char *p;
            STRLEN nlen = 0;
	    /* New style explicit name, type is just mode and layer info */
#ifdef USE_STDIO
	    if (SvROK(*svp) && !strchr(oname,'&')) {
		if (ckWARN(WARN_IO))
		    Perl_warner(aTHX_ packWARN(WARN_IO),
			    "Can't open a reference");
		SETERRNO(EINVAL, LIB_INVARG);
                fp = NULL;
		goto say_false;
	    }
#endif /* USE_STDIO */
            p = (SvOK(*svp) || SvGMAGICAL(*svp)) ? SvPV(*svp, nlen) : NULL;

            if (p && !IS_SAFE_PATHNAME(p, nlen, "open")) {
                fp = NULL;
                goto say_false;
            }

	    name = p ? savepvn(p, nlen) : savepvs("");

	    SAVEFREEPV(name);
	}
	else {
	    name = type;
	    len  = tend-type;
	}
	IoTYPE(io) = *type;
	if ((*type == IoTYPE_RDWR) && /* scary */
           (*(type+1) == IoTYPE_RDONLY || *(type+1) == IoTYPE_WRONLY) &&
	    ((!num_svs || (tend > type+1 && tend[-1] != IoTYPE_PIPE)))) {
	    TAINT_PROPER("open");
	    mode[1] = *type++;
	    writing = 1;
	}

	if (*type == IoTYPE_PIPE) {
	    if (num_svs) {
		if (type[1] != IoTYPE_STD) {
	          unknown_open_mode:
		    Perl_croak(aTHX_ "Unknown open() mode '%.*s'", (int)olen, oname);
		}
		type++;
	    }
	    do {
		type++;
	    } while (isSPACE(*type));
	    if (!num_svs) {
		name = type;
		len = tend-type;
	    }
	    if (*name == '\0') {
		/* command is missing 19990114 */
		if (ckWARN(WARN_PIPE))
		    Perl_warner(aTHX_ packWARN(WARN_PIPE), "Missing command in piped open");
		errno = EPIPE;
                fp = NULL;
		goto say_false;
	    }
	    if (!(*name == '-' && name[1] == '\0') || num_svs)
		TAINT_ENV();
	    TAINT_PROPER("piped open");
	    if (!num_svs && name[len-1] == '|') {
		name[--len] = '\0' ;
		if (ckWARN(WARN_PIPE))
		    Perl_warner(aTHX_ packWARN(WARN_PIPE), "Can't open bidirectional pipe");
	    }
	    mode[0] = 'w';
	    writing = 1;
            if (out_raw)
		mode[1] = 'b';
            else if (out_crlf)
		mode[1] = 't';
	    if (num_svs > 1) {
		fp = PerlProc_popen_list(mode, num_svs, svp);
	    }
	    else {
		fp = PerlProc_popen(name,mode);
	    }
	    if (num_svs) {
		if (*type) {
		    if (PerlIO_apply_layers(aTHX_ fp, mode, type) != 0) {
                        fp = NULL;
			goto say_false;
		    }
		}
	    }
	} /* IoTYPE_PIPE */
	else if (*type == IoTYPE_WRONLY) {
	    TAINT_PROPER("open");
	    type++;
	    if (*type == IoTYPE_WRONLY) {
		/* Two IoTYPE_WRONLYs in a row make for an IoTYPE_APPEND. */
		mode[0] = IoTYPE(io) = IoTYPE_APPEND;
		type++;
	    }
	    else {
		mode[0] = 'w';
	    }
	    writing = 1;

            if (out_raw)
		mode[1] = 'b';
            else if (out_crlf)
		mode[1] = 't';
	    if (*type == '&') {
	      duplicity:
		dodup = PERLIO_DUP_FD;
		type++;
		if (*type == '=') {
		    dodup = 0;
		    type++;
		}
		if (!num_svs && !*type && supplied_fp) {
		    /* "<+&" etc. is used by typemaps */
		    fp = supplied_fp;
		}
		else {
		    PerlIO *that_fp = NULL;
                    int wanted_fd;
                    UV uv;
		    if (num_svs > 1) {
			/* diag_listed_as: More than one argument to '%s' open */
			Perl_croak(aTHX_ "More than one argument to '%c&' open",IoTYPE(io));
		    }
		    while (isSPACE(*type))
			type++;
		    if (num_svs && (
			     SvIOK(*svp)
			  || (SvPOKp(*svp) && looks_like_number(*svp))
		       )) {
                        wanted_fd = SvUV(*svp);
			num_svs = 0;
		    }
		    else if (isDIGIT(*type)
                        && grok_atoUV(type, &uv, NULL)
                        && uv <= INT_MAX
                    ) {
                        wanted_fd = (int)uv;
		    }
		    else {
			const IO* thatio;
			if (num_svs) {
			    thatio = sv_2io(*svp);
			}
			else {
			    GV * const thatgv = gv_fetchpvn_flags(type, tend - type,
						       0, SVt_PVIO);
			    thatio = GvIO(thatgv);
			}
			if (!thatio) {
#ifdef EINVAL
			    SETERRNO(EINVAL,SS_IVCHAN);
#endif
                            fp = NULL;
			    goto say_false;
			}
			if ((that_fp = IoIFP(thatio))) {
			    /* Flush stdio buffer before dup. --mjd
			     * Unfortunately SEEK_CURing 0 seems to
			     * be optimized away on most platforms;
			     * only Solaris and Linux seem to flush
			     * on that. --jhi */
			    /* On the other hand, do all platforms
			     * take gracefully to flushing a read-only
			     * filehandle?  Perhaps we should do
			     * fsetpos(src)+fgetpos(dst)?  --nik */
			    PerlIO_flush(that_fp);
			    wanted_fd = PerlIO_fileno(that_fp);
			    /* When dup()ing STDIN, STDOUT or STDERR
			     * explicitly set appropriate access mode */
			    if (that_fp == PerlIO_stdout()
				|| that_fp == PerlIO_stderr())
			        IoTYPE(io) = IoTYPE_WRONLY;
			    else if (that_fp == PerlIO_stdin())
                                IoTYPE(io) = IoTYPE_RDONLY;
			    /* When dup()ing a socket, say result is
			     * one as well */
			    else if (IoTYPE(thatio) == IoTYPE_SOCKET)
				IoTYPE(io) = IoTYPE_SOCKET;
			}
                        else {
                            SETERRNO(EBADF, RMS_IFI);
                            fp = NULL;
                            goto say_false;
                        }
		    }
		    if (!num_svs)
			type = NULL;
		    if (that_fp) {
			fp = PerlIO_fdupopen(aTHX_ that_fp, NULL, dodup);
		    }
		    else {
			if (dodup)
                            wanted_fd = PerlLIO_dup(wanted_fd);
			else
			    was_fdopen = TRUE;
                        if (!(fp = PerlIO_openn(aTHX_ type,mode,wanted_fd,0,0,NULL,num_svs,svp))) {
                            if (dodup && wanted_fd >= 0)
                                PerlLIO_close(wanted_fd);
			}
		    }
		}
	    } /* & */
	    else {
		while (isSPACE(*type))
		    type++;
		if (*type == IoTYPE_STD && (!type[1] || isSPACE(type[1]) || type[1] == ':')) {
		    type++;
		    fp = PerlIO_stdout();
		    IoTYPE(io) = IoTYPE_STD;
		    if (num_svs > 1) {
			/* diag_listed_as: More than one argument to '%s' open */
			Perl_croak(aTHX_ "More than one argument to '>%c' open",IoTYPE_STD);
		    }
		}
		else  {
		    if (num_svs) {
                        fp = PerlIO_openn(aTHX_ type,mode,-1,0,0,NULL,num_svs,svp);
                    }
                    else {
                        SV *namesv = newSVpvn_flags(type, tend - type, SVs_TEMP);
		        type = NULL;
                        fp = PerlIO_openn(aTHX_ type,mode,-1,0,0,NULL,1,&namesv);
		    }
		}
	    } /* !& */
	    if (!fp && type && *type && *type != ':' && !isIDFIRST(*type))
	       goto unknown_open_mode;
	} /* IoTYPE_WRONLY */
	else if (*type == IoTYPE_RDONLY) {
	    do {
		type++;
	    } while (isSPACE(*type));
	    mode[0] = 'r';
            if (in_raw)
		mode[1] = 'b';
            else if (in_crlf)
		mode[1] = 't';
	    if (*type == '&') {
		goto duplicity;
	    }
	    if (*type == IoTYPE_STD && (!type[1] || isSPACE(type[1]) || type[1] == ':')) {
		type++;
		fp = PerlIO_stdin();
		IoTYPE(io) = IoTYPE_STD;
		if (num_svs > 1) {
		    /* diag_listed_as: More than one argument to '%s' open */
		    Perl_croak(aTHX_ "More than one argument to '<%c' open",IoTYPE_STD);
		}
	    }
	    else {
		if (num_svs) {
                    fp = PerlIO_openn(aTHX_ type,mode,-1,0,0,NULL,num_svs,svp);
                }
                else {
                    SV *namesv  = newSVpvn_flags(type, tend - type, SVs_TEMP);
		    type = NULL;
                    fp = PerlIO_openn(aTHX_ type,mode,-1,0,0,NULL,1,&namesv);
		}
	    }
	    if (!fp && type && *type && *type != ':' && !isIDFIRST(*type))
	       goto unknown_open_mode;
	} /* IoTYPE_RDONLY */
	else if ((num_svs && /* '-|...' or '...|' */
		  type[0] == IoTYPE_STD && type[1] == IoTYPE_PIPE) ||
	         (!num_svs && tend > type+1 && tend[-1] == IoTYPE_PIPE)) {
	    if (num_svs) {
		type += 2;   /* skip over '-|' */
	    }
	    else {
		*--tend = '\0';
		while (tend > type && isSPACE(tend[-1]))
		    *--tend = '\0';
		for (; isSPACE(*type); type++)
		    ;
		name = type;
	        len  = tend-type;
	    }
	    if (*name == '\0') {
		/* command is missing 19990114 */
		if (ckWARN(WARN_PIPE))
		    Perl_warner(aTHX_ packWARN(WARN_PIPE), "Missing command in piped open");
		errno = EPIPE;
                fp = NULL;
		goto say_false;
	    }
	    if (!(*name == '-' && name[1] == '\0') || num_svs)
		TAINT_ENV();
	    TAINT_PROPER("piped open");
	    mode[0] = 'r';

            if (in_raw)
		mode[1] = 'b';
            else if (in_crlf)
		mode[1] = 't';

	    if (num_svs > 1) {
		fp = PerlProc_popen_list(mode,num_svs,svp);
	    }
	    else {
		fp = PerlProc_popen(name,mode);
	    }
	    IoTYPE(io) = IoTYPE_PIPE;
	    if (num_svs) {
		while (isSPACE(*type))
		    type++;
		if (*type) {
		    if (PerlIO_apply_layers(aTHX_ fp, mode, type) != 0) {
                        fp = NULL;
			goto say_false;
		    }
		}
	    }
	}
	else { /* layer(Args) */
	    if (num_svs)
		goto unknown_open_mode;
	    name = type;
	    IoTYPE(io) = IoTYPE_RDONLY;
	    for (; isSPACE(*name); name++)
		;
	    mode[0] = 'r';

            if (in_raw)
		mode[1] = 'b';
            else if (in_crlf)
		mode[1] = 't';

	    if (*name == '-' && name[1] == '\0') {
		fp = PerlIO_stdin();
		IoTYPE(io) = IoTYPE_STD;
	    }
	    else {
		if (num_svs) {
                    fp = PerlIO_openn(aTHX_ type,mode,-1,0,0,NULL,num_svs,svp);
                }
                else {
		    SV *namesv = newSVpvn_flags(type, tend - type, SVs_TEMP);
		    type = NULL;
                    fp = PerlIO_openn(aTHX_ type,mode,-1,0,0,NULL,1,&namesv);
		}
	    }
	}
    }

  say_false:
    return openn_cleanup(gv, io, fp, mode, oname, saveifp, saveofp, savefd,
                         savetype, writing, was_fdopen, type);
}

/* Yes, this is ugly, but it's private, and I don't see a cleaner way to
   simplify the two-headed public interface of do_openn. */
static bool
S_openn_cleanup(pTHX_ GV *gv, IO *io, PerlIO *fp, char *mode, const char *oname,
                PerlIO *saveifp, PerlIO *saveofp, int savefd, char savetype,
                int writing, bool was_fdopen, const char *type)
{
    int fd;

    PERL_ARGS_ASSERT_OPENN_CLEANUP;

    if (!fp) {
	if (IoTYPE(io) == IoTYPE_RDONLY && ckWARN(WARN_NEWLINE)
	    && should_warn_nl(oname)
	    
	)
        {
            GCC_DIAG_IGNORE(-Wformat-nonliteral); /* PL_warn_nl is constant */
	    Perl_warner(aTHX_ packWARN(WARN_NEWLINE), PL_warn_nl, "open");
            GCC_DIAG_RESTORE;
        }
	goto say_false;
    }

    if (ckWARN(WARN_IO)) {
	if ((IoTYPE(io) == IoTYPE_RDONLY) &&
	    (fp == PerlIO_stdout() || fp == PerlIO_stderr())) {
		Perl_warner(aTHX_ packWARN(WARN_IO),
			    "Filehandle STD%s reopened as %"HEKf
			    " only for input",
			    ((fp == PerlIO_stdout()) ? "OUT" : "ERR"),
			    HEKfARG(GvENAME_HEK(gv)));
	}
	else if ((IoTYPE(io) == IoTYPE_WRONLY) && fp == PerlIO_stdin()) {
		Perl_warner(aTHX_ packWARN(WARN_IO),
		    "Filehandle STDIN reopened as %"HEKf" only for output",
		     HEKfARG(GvENAME_HEK(gv))
		);
	}
    }

    fd = PerlIO_fileno(fp);
    /* Do NOT do: "if (fd < 0) goto say_false;" here.  If there is no
     * fd assume it isn't a socket - this covers PerlIO::scalar -
     * otherwise unless we "know" the type probe for socket-ness.
     */
    if (IoTYPE(io) && IoTYPE(io) != IoTYPE_PIPE && IoTYPE(io) != IoTYPE_STD && fd >= 0) {
	if (PerlLIO_fstat(fd,&PL_statbuf) < 0) {
	    /* If PerlIO claims to have fd we had better be able to fstat() it. */
	    (void) PerlIO_close(fp);
	    goto say_false;
	}
#ifndef PERL_MICRO
	if (S_ISSOCK(PL_statbuf.st_mode))
	    IoTYPE(io) = IoTYPE_SOCKET;	/* in case a socket was passed in to us */
#ifdef HAS_SOCKET
	else if (
	    !(PL_statbuf.st_mode & S_IFMT)
	    && IoTYPE(io) != IoTYPE_WRONLY  /* Dups of STD* filehandles already have */
	    && IoTYPE(io) != IoTYPE_RDONLY  /* type so they aren't marked as sockets */
	) {				    /* on OS's that return 0 on fstat()ed pipe */
	     char tmpbuf[256];
	     Sock_size_t buflen = sizeof tmpbuf;
	     if (PerlSock_getsockname(fd, (struct sockaddr *)tmpbuf, &buflen) >= 0
		      || errno != ENOTSOCK)
		    IoTYPE(io) = IoTYPE_SOCKET; /* some OS's return 0 on fstat()ed socket */
				                /* but some return 0 for streams too, sigh */
	}
#endif /* HAS_SOCKET */
#endif /* !PERL_MICRO */
    }

    /* Eeek - FIXME !!!
     * If this is a standard handle we discard all the layer stuff
     * and just dup the fd into whatever was on the handle before !
     */

    if (saveifp) {		/* must use old fp? */
        /* If fd is less that PL_maxsysfd i.e. STDIN..STDERR
           then dup the new fileno down
         */
	if (saveofp) {
	    PerlIO_flush(saveofp);	/* emulate PerlIO_close() */
	    if (saveofp != saveifp) {	/* was a socket? */
		PerlIO_close(saveofp);
	    }
	}
	if (savefd != fd) {
	    /* Still a small can-of-worms here if (say) PerlIO::scalar
	       is assigned to (say) STDOUT - for now let dup2() fail
	       and provide the error
	     */
	    if (fd < 0) {
                SETERRNO(EBADF,RMS_IFI);
		goto say_false;
            } else if (PerlLIO_dup2(fd, savefd) < 0) {
		(void)PerlIO_close(fp);
		goto say_false;
	    }
#ifdef VMS
	    if (savefd != PerlIO_fileno(PerlIO_stdin())) {
                char newname[FILENAME_MAX+1];
                if (PerlIO_getname(fp, newname)) {
                    if (fd == PerlIO_fileno(PerlIO_stdout()))
                        vmssetuserlnm("SYS$OUTPUT", newname);
                    if (fd == PerlIO_fileno(PerlIO_stderr()))
                        vmssetuserlnm("SYS$ERROR", newname);
                }
	    }
#endif

#if !defined(WIN32)
           /* PL_fdpid isn't used on Windows, so avoid this useless work.
            * XXX Probably the same for a lot of other places. */
            {
                Pid_t pid;
                SV *sv;

                sv = *av_fetch(PL_fdpid,fd,TRUE);
                SvUPGRADE(sv, SVt_IV);
                pid = SvIVX(sv);
                SvIV_set(sv, 0);
                sv = *av_fetch(PL_fdpid,savefd,TRUE);
                SvUPGRADE(sv, SVt_IV);
                SvIV_set(sv, pid);
            }
#endif

	    if (was_fdopen) {
                /* need to close fp without closing underlying fd */
                int ofd = PerlIO_fileno(fp);
                int dupfd = ofd >= 0 ? PerlLIO_dup(ofd) : -1;
#if defined(HAS_FCNTL) && defined(F_SETFD)
		/* Assume if we have F_SETFD we have F_GETFD. */
                /* Get a copy of all the fd flags. */
                int fd_flags = ofd >= 0 ? fcntl(ofd, F_GETFD) : -1;
                if (fd_flags < 0) {
                    if (dupfd >= 0)
                        PerlLIO_close(dupfd);
                    goto say_false;
                }
#endif
                if (ofd < 0 || dupfd < 0) {
                    if (dupfd >= 0)
                        PerlLIO_close(dupfd);
                    goto say_false;
                }
                PerlIO_close(fp);
                PerlLIO_dup2(dupfd, ofd);
#if defined(HAS_FCNTL) && defined(F_SETFD)
		/* The dup trick has lost close-on-exec on ofd,
                 * and possibly any other flags, so restore them. */
		fcntl(ofd,F_SETFD, fd_flags);
#endif
                PerlLIO_close(dupfd);
	    }
            else
		PerlIO_close(fp);
	}
	fp = saveifp;
	PerlIO_clearerr(fp);
	fd = PerlIO_fileno(fp);
    }
#if defined(HAS_FCNTL) && defined(F_SETFD) && defined(FD_CLOEXEC)
    if (fd >= 0 && fd > PL_maxsysfd && fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
        PerlLIO_close(fd);
        goto say_false;
    }
#endif
    IoIFP(io) = fp;

    IoFLAGS(io) &= ~IOf_NOLINE;
    if (writing) {
	if (IoTYPE(io) == IoTYPE_SOCKET
	    || (IoTYPE(io) == IoTYPE_WRONLY && fd >= 0 && S_ISCHR(PL_statbuf.st_mode)) ) {
	    char *s = mode;
	    if (*s == IoTYPE_IMPLICIT || *s == IoTYPE_NUMERIC)
	      s++;
	    *s = 'w';
	    if (!(IoOFP(io) = PerlIO_openn(aTHX_ type,s,fd,0,0,NULL,0,NULL))) {
		PerlIO_close(fp);
		goto say_false;
	    }
	}
	else
	    IoOFP(io) = fp;
    }
    return TRUE;

  say_false:
    IoIFP(io) = saveifp;
    IoOFP(io) = saveofp;
    IoTYPE(io) = savetype;
    return FALSE;
}

PerlIO *
Perl_nextargv(pTHX_ GV *gv, bool nomagicopen)
{
    IO * const io = GvIOp(gv);
    SV *const old_out_name = PL_inplace ? newSVsv(GvSV(gv)) : NULL;

    PERL_ARGS_ASSERT_NEXTARGV;

    if (old_out_name)
        SAVEFREESV(old_out_name);

    if (!PL_argvoutgv)
	PL_argvoutgv = gv_fetchpvs("ARGVOUT", GV_ADD|GV_NOTQUAL, SVt_PVIO);
    if (io && (IoFLAGS(io) & (IOf_ARGV|IOf_START)) == (IOf_ARGV|IOf_START)) {
	IoFLAGS(io) &= ~IOf_START;
	if (PL_inplace) {
	    assert(PL_defoutgv);
	    Perl_av_create_and_push(aTHX_ &PL_argvout_stack,
				    SvREFCNT_inc_simple_NN(PL_defoutgv));
	}
    }
    if (PL_filemode & (S_ISUID|S_ISGID)) {
	PerlIO_flush(IoIFP(GvIOn(PL_argvoutgv)));  /* chmod must follow last write */
#ifdef HAS_FCHMOD
	if (PL_lastfd != -1)
	    (void)fchmod(PL_lastfd,PL_filemode);
#else
	(void)PerlLIO_chmod(PL_oldname,PL_filemode);
#endif
    }
    PL_lastfd = -1;
    PL_filemode = 0;
    if (!GvAV(gv))
	return NULL;
    while (av_tindex(GvAV(gv)) >= 0) {
	Stat_t statbuf;
	STRLEN oldlen;
        SV *const sv = av_shift(GvAV(gv));
	SAVEFREESV(sv);
	SvTAINTED_off(GvSVn(gv)); /* previous tainting irrelevant */
	sv_setsv(GvSVn(gv),sv);
	SvSETMAGIC(GvSV(gv));
	PL_oldname = SvPVx(GvSV(gv), oldlen);
        if (LIKELY(!PL_inplace)) {
            if (nomagicopen
                    ? do_open6(gv, "<", 1, NULL, &GvSV(gv), 1)
                    : do_open6(gv, PL_oldname, oldlen, NULL, NULL, 0)
               ) {
                return IoIFP(GvIOp(gv));
            }
        }
        else {
            {
                IO * const io = GvIOp(PL_argvoutgv);
                if (io && IoIFP(io) && old_out_name && !io_close(io, PL_argvoutgv, FALSE, FALSE)) {
                    Perl_croak(aTHX_ "Failed to close in-place edit file %"SVf": %s\n",
                               old_out_name, Strerror(errno));
                }
            }
            /* This very long block ends with return IoIFP(GvIOp(gv));
               Both this block and the block above fall through on open
               failure to the warning code, and then the while loop above tries
               the next entry. */
            if (do_open_raw(gv, PL_oldname, oldlen, O_RDONLY, 0)) {
#ifndef FLEXFILENAMES
                int filedev;
                int fileino;
#endif
                Uid_t fileuid;
                Gid_t filegid;

		TAINT_PROPER("inplace open");
		if (oldlen == 1 && *PL_oldname == '-') {
		    setdefout(gv_fetchpvs("STDOUT", GV_ADD|GV_NOTQUAL,
					  SVt_PVIO));
		    return IoIFP(GvIOp(gv));
		}
#ifndef FLEXFILENAMES
		filedev = PL_statbuf.st_dev;
		fileino = PL_statbuf.st_ino;
#endif
		PL_filemode = PL_statbuf.st_mode;
		fileuid = PL_statbuf.st_uid;
		filegid = PL_statbuf.st_gid;
		if (!S_ISREG(PL_filemode)) {
		    Perl_ck_warner_d(aTHX_ packWARN(WARN_INPLACE),
				     "Can't do inplace edit: %s is not a regular file",
				     PL_oldname );
		    do_close(gv,FALSE);
		    continue;
		}
		if (*PL_inplace && strNE(PL_inplace, "*")) {
		    const char *star = strchr(PL_inplace, '*');
		    if (star) {
			const char *begin = PL_inplace;
			sv_setpvs(sv, "");
			do {
			    sv_catpvn(sv, begin, star - begin);
			    sv_catpvn(sv, PL_oldname, oldlen);
			    begin = ++star;
			} while ((star = strchr(begin, '*')));
			if (*begin)
			    sv_catpv(sv,begin);
		    }
		    else {
			sv_catpv(sv,PL_inplace);
		    }
#ifndef FLEXFILENAMES
		    if ((PerlLIO_stat(SvPVX_const(sv),&PL_statbuf) >= 0
			 && PL_statbuf.st_dev == filedev
			 && PL_statbuf.st_ino == fileino)
#ifdef DJGPP
			|| ((_djstat_fail_bits & _STFAIL_TRUENAME)!=0)
#endif
                      )
		    {
			Perl_ck_warner_d(aTHX_ packWARN(WARN_INPLACE),
					 "Can't do inplace edit: %"SVf" would not be unique",
					 SVfARG(sv));
			do_close(gv,FALSE);
			continue;
		    }
#endif
#ifdef HAS_RENAME
#if !defined(DOSISH) && !defined(__CYGWIN__)
		    if (PerlLIO_rename(PL_oldname,SvPVX_const(sv)) < 0) {
			Perl_ck_warner_d(aTHX_ packWARN(WARN_INPLACE),
					 "Can't rename %s to %"SVf": %s, skipping file",
					 PL_oldname, SVfARG(sv), Strerror(errno));
			do_close(gv,FALSE);
			continue;
		    }
#else
		    do_close(gv,FALSE);
		    (void)PerlLIO_unlink(SvPVX_const(sv));
		    (void)PerlLIO_rename(PL_oldname,SvPVX_const(sv));
		    do_open_raw(gv, SvPVX_const(sv), SvCUR(sv), O_RDONLY, 0);
#endif /* DOSISH */
#else
		    (void)UNLINK(SvPVX_const(sv));
		    if (link(PL_oldname,SvPVX_const(sv)) < 0) {
			Perl_ck_warner_d(aTHX_ packWARN(WARN_INPLACE),
					 "Can't rename %s to %"SVf": %s, skipping file",
					 PL_oldname, SVfARG(sv), Strerror(errno) );
			do_close(gv,FALSE);
			continue;
		    }
		    (void)UNLINK(PL_oldname);
#endif
		}
		else {
#if !defined(DOSISH) && !defined(__amigaos4__)
#  ifndef VMS  /* Don't delete; use automatic file versioning */
		    if (UNLINK(PL_oldname) < 0) {
			Perl_ck_warner_d(aTHX_ packWARN(WARN_INPLACE),
					 "Can't remove %s: %s, skipping file",
					 PL_oldname, Strerror(errno) );
			do_close(gv,FALSE);
			continue;
		    }
#  endif
#else
		    Perl_croak(aTHX_ "Can't do inplace edit without backup");
#endif
		}

		sv_setpvn(sv,PL_oldname,oldlen);
		SETERRNO(0,0);		/* in case sprintf set errno */
		if (!Perl_do_open_raw(aTHX_ PL_argvoutgv, SvPVX_const(sv),
                                      SvCUR(sv),
#ifdef VMS
                                      O_WRONLY|O_CREAT|O_TRUNC, 0
#else
                                      O_WRONLY|O_CREAT|OPEN_EXCL, 0600
#endif
                        )) {
		    Perl_ck_warner_d(aTHX_ packWARN(WARN_INPLACE), "Can't do inplace edit on %s: %s",
				     PL_oldname, Strerror(errno) );
		    do_close(gv,FALSE);
		    continue;
		}
		setdefout(PL_argvoutgv);
		PL_lastfd = PerlIO_fileno(IoIFP(GvIOp(PL_argvoutgv)));
                if (PL_lastfd >= 0) {
                    (void)PerlLIO_fstat(PL_lastfd,&statbuf);
#ifdef HAS_FCHMOD
                    (void)fchmod(PL_lastfd,PL_filemode);
#else
                    (void)PerlLIO_chmod(PL_oldname,PL_filemode);
#endif
                    if (fileuid != statbuf.st_uid || filegid != statbuf.st_gid) {
                        /* XXX silently ignore failures */
#ifdef HAS_FCHOWN
                        PERL_UNUSED_RESULT(fchown(PL_lastfd,fileuid,filegid));
#else
#ifdef HAS_CHOWN
                        PERL_UNUSED_RESULT(PerlLIO_chown(PL_oldname,fileuid,filegid));
#endif
#endif
                    }
		}
                return IoIFP(GvIOp(gv));
	    }
	} /* successful do_open_raw(), PL_inplace non-NULL */

        if (ckWARN_d(WARN_INPLACE)) {
            const int eno = errno;
            if (PerlLIO_stat(PL_oldname, &statbuf) >= 0
                && !S_ISREG(statbuf.st_mode)) {
                Perl_warner(aTHX_ packWARN(WARN_INPLACE),
                            "Can't do inplace edit: %s is not a regular file",
                            PL_oldname);
            }
            else {
                Perl_warner(aTHX_ packWARN(WARN_INPLACE), "Can't open %s: %s",
                            PL_oldname, Strerror(eno));
            }
	}
    }
    if (io && (IoFLAGS(io) & IOf_ARGV))
	IoFLAGS(io) |= IOf_START;
    if (PL_inplace) {
        if (old_out_name) {
            IO * const io = GvIOp(PL_argvoutgv);
            if (io && IoIFP(io) && !io_close(io, PL_argvoutgv, FALSE, FALSE)) {
                Perl_croak(aTHX_ "Failed to close in-place edit file %"SVf": %s\n",
                           old_out_name, Strerror(errno));
            }
        }
        else {
            /* maybe this is no longer wanted */
            (void)do_close(PL_argvoutgv,FALSE);
        }
	if (io && (IoFLAGS(io) & IOf_ARGV)
	    && PL_argvout_stack && AvFILLp(PL_argvout_stack) >= 0)
	{
	    GV * const oldout = MUTABLE_GV(av_pop(PL_argvout_stack));
	    setdefout(oldout);
	    SvREFCNT_dec_NN(oldout);
	    return NULL;
	}
	setdefout(gv_fetchpvs("STDOUT", GV_ADD|GV_NOTQUAL, SVt_PVIO));
    }
    return NULL;
}

/* explicit renamed to avoid C++ conflict    -- kja */
bool
Perl_do_close(pTHX_ GV *gv, bool not_implicit)
{
    bool retval;
    IO *io;

    if (!gv)
	gv = PL_argvgv;
    if (!gv || !isGV_with_GP(gv)) {
	if (not_implicit)
	    SETERRNO(EBADF,SS_IVCHAN);
	return FALSE;
    }
    io = GvIO(gv);
    if (!io) {		/* never opened */
	if (not_implicit) {
	    report_evil_fh(gv);
	    SETERRNO(EBADF,SS_IVCHAN);
	}
	return FALSE;
    }
    retval = io_close(io, NULL, not_implicit, FALSE);
    if (not_implicit) {
	IoLINES(io) = 0;
	IoPAGE(io) = 0;
	IoLINES_LEFT(io) = IoPAGE_LEN(io);
    }
    IoTYPE(io) = IoTYPE_CLOSED;
    return retval;
}

bool
Perl_io_close(pTHX_ IO *io, GV *gv, bool not_implicit, bool warn_on_fail)
{
    bool retval = FALSE;

    PERL_ARGS_ASSERT_IO_CLOSE;

    if (IoIFP(io)) {
	if (IoTYPE(io) == IoTYPE_PIPE) {
	    const int status = PerlProc_pclose(IoIFP(io));
	    if (not_implicit) {
		STATUS_NATIVE_CHILD_SET(status);
		retval = (STATUS_UNIX == 0);
	    }
	    else {
		retval = (status != -1);
	    }
	}
	else if (IoTYPE(io) == IoTYPE_STD)
	    retval = TRUE;
	else {
	    if (IoOFP(io) && IoOFP(io) != IoIFP(io)) {		/* a socket */
		const bool prev_err = PerlIO_error(IoOFP(io));
#ifdef USE_PERLIO
		if (prev_err)
		    PerlIO_restore_errno(IoOFP(io));
#endif
		retval = (PerlIO_close(IoOFP(io)) != EOF && !prev_err);
		PerlIO_close(IoIFP(io));	/* clear stdio, fd already closed */
	    }
	    else {
		const bool prev_err = PerlIO_error(IoIFP(io));
#ifdef USE_PERLIO
		if (prev_err)
		    PerlIO_restore_errno(IoIFP(io));
#endif
		retval = (PerlIO_close(IoIFP(io)) != EOF && !prev_err);
	    }
	}
	IoOFP(io) = IoIFP(io) = NULL;

	if (warn_on_fail && !retval) {
	    if (gv)
		Perl_ck_warner_d(aTHX_ packWARN(WARN_IO),
				"Warning: unable to close filehandle %"
				 HEKf" properly: %"SVf,
				 HEKfARG(GvNAME_HEK(gv)),
                                 SVfARG(get_sv("!",GV_ADD)));
	    else
		Perl_ck_warner_d(aTHX_ packWARN(WARN_IO),
				"Warning: unable to close filehandle "
				"properly: %"SVf,
				 SVfARG(get_sv("!",GV_ADD)));
	}
    }
    else if (not_implicit) {
	SETERRNO(EBADF,SS_IVCHAN);
    }

    return retval;
}

bool
Perl_do_eof(pTHX_ GV *gv)
{
    IO * const io = GvIO(gv);

    PERL_ARGS_ASSERT_DO_EOF;

    if (!io)
	return TRUE;
    else if (IoTYPE(io) == IoTYPE_WRONLY)
	report_wrongway_fh(gv, '>');

    while (IoIFP(io)) {
        if (PerlIO_has_cntptr(IoIFP(io))) {	/* (the code works without this) */
	    if (PerlIO_get_cnt(IoIFP(io)) > 0)	/* cheat a little, since */
		return FALSE;			/* this is the most usual case */
        }

	{
	     /* getc and ungetc can stomp on errno */
	    dSAVE_ERRNO;
	    const int ch = PerlIO_getc(IoIFP(io));
	    if (ch != EOF) {
		(void)PerlIO_ungetc(IoIFP(io),ch);
		RESTORE_ERRNO;
		return FALSE;
	    }
	    RESTORE_ERRNO;
	}

        if (PerlIO_has_cntptr(IoIFP(io)) && PerlIO_canset_cnt(IoIFP(io))) {
	    if (PerlIO_get_cnt(IoIFP(io)) < -1)
		PerlIO_set_cnt(IoIFP(io),-1);
	}
	if (PL_op->op_flags & OPf_SPECIAL) { /* not necessarily a real EOF yet? */
	    if (gv != PL_argvgv || !nextargv(gv, FALSE))	/* get another fp handy */
		return TRUE;
	}
	else
	    return TRUE;		/* normal fp, definitely end of file */
    }
    return TRUE;
}

Off_t
Perl_do_tell(pTHX_ GV *gv)
{
    IO *const io = GvIO(gv);
    PerlIO *fp;

    PERL_ARGS_ASSERT_DO_TELL;

    if (io && (fp = IoIFP(io))) {
	return PerlIO_tell(fp);
    }
    report_evil_fh(gv);
    SETERRNO(EBADF,RMS_IFI);
    return (Off_t)-1;
}

bool
Perl_do_seek(pTHX_ GV *gv, Off_t pos, int whence)
{
    IO *const io = GvIO(gv);
    PerlIO *fp;

    if (io && (fp = IoIFP(io))) {
	return PerlIO_seek(fp, pos, whence) >= 0;
    }
    report_evil_fh(gv);
    SETERRNO(EBADF,RMS_IFI);
    return FALSE;
}

Off_t
Perl_do_sysseek(pTHX_ GV *gv, Off_t pos, int whence)
{
    IO *const io = GvIO(gv);
    PerlIO *fp;

    PERL_ARGS_ASSERT_DO_SYSSEEK;

    if (io && (fp = IoIFP(io))) {
        int fd = PerlIO_fileno(fp);
        if (fd < 0 || (whence == SEEK_SET && pos < 0)) {
            SETERRNO(EINVAL,LIB_INVARG);
            return -1;
        } else {
            return PerlLIO_lseek(fd, pos, whence);
        }
    }
    report_evil_fh(gv);
    SETERRNO(EBADF,RMS_IFI);
    return (Off_t)-1;
}

int
Perl_mode_from_discipline(pTHX_ const char *s, STRLEN len)
{
    int mode = O_BINARY;
    PERL_UNUSED_CONTEXT;
    if (s) {
	while (*s) {
	    if (*s == ':') {
		switch (s[1]) {
		case 'r':
		    if (s[2] == 'a' && s[3] == 'w'
			&& (!s[4] || s[4] == ':' || isSPACE(s[4])))
		    {
			mode = O_BINARY;
			s += 4;
			len -= 4;
			break;
		    }
		    /* FALLTHROUGH */
		case 'c':
		    if (s[2] == 'r' && s[3] == 'l' && s[4] == 'f'
			&& (!s[5] || s[5] == ':' || isSPACE(s[5])))
		    {
			mode = O_TEXT;
			s += 5;
			len -= 5;
			break;
		    }
		    /* FALLTHROUGH */
		default:
		    goto fail_discipline;
		}
	    }
	    else if (isSPACE(*s)) {
		++s;
		--len;
	    }
	    else {
		const char *end;
  fail_discipline:
		end = strchr(s+1, ':');
		if (!end)
		    end = s+len;
#ifndef PERLIO_LAYERS
		Perl_croak(aTHX_ "IO layers (like '%.*s') unavailable", end-s, s);
#else
		len -= end-s;
		s = end;
#endif
	    }
	}
    }
    return mode;
}

#if !defined(HAS_TRUNCATE) && !defined(HAS_CHSIZE)
I32
my_chsize(int fd, Off_t length)
{
#ifdef F_FREESP
	/* code courtesy of William Kucharski */
#define HAS_CHSIZE

    Stat_t filebuf;

    if (PerlLIO_fstat(fd, &filebuf) < 0)
	return -1;

    if (filebuf.st_size < length) {

	/* extend file length */

	if ((PerlLIO_lseek(fd, (length - 1), 0)) < 0)
	    return -1;

	/* write a "0" byte */

	if ((PerlLIO_write(fd, "", 1)) != 1)
	    return -1;
    }
    else {
	/* truncate length */
	struct flock fl;
	fl.l_whence = 0;
	fl.l_len = 0;
	fl.l_start = length;
	fl.l_type = F_WRLCK;    /* write lock on file space */

	/*
	* This relies on the UNDOCUMENTED F_FREESP argument to
	* fcntl(2), which truncates the file so that it ends at the
	* position indicated by fl.l_start.
	*
	* Will minor miracles never cease?
	*/

	if (fcntl(fd, F_FREESP, &fl) < 0)
	    return -1;

    }
    return 0;
#else
    Perl_croak_nocontext("truncate not implemented");
#endif /* F_FREESP */
    return -1;
}
#endif /* !HAS_TRUNCATE && !HAS_CHSIZE */

bool
Perl_do_print(pTHX_ SV *sv, PerlIO *fp)
{
    PERL_ARGS_ASSERT_DO_PRINT;

    /* assuming fp is checked earlier */
    if (!sv)
	return TRUE;
    if (SvTYPE(sv) == SVt_IV && SvIOK(sv)) {
	assert(!SvGMAGICAL(sv));
	if (SvIsUV(sv))
	    PerlIO_printf(fp, "%"UVuf, (UV)SvUVX(sv));
	else
	    PerlIO_printf(fp, "%"IVdf, (IV)SvIVX(sv));
	return !PerlIO_error(fp);
    }
    else {
	STRLEN len;
	/* Do this first to trigger any overloading.  */
	const char *tmps = SvPV_const(sv, len);
	U8 *tmpbuf = NULL;
	bool happy = TRUE;

	if (PerlIO_isutf8(fp)) { /* If the stream is utf8 ... */
	    if (!SvUTF8(sv)) {	/* Convert to utf8 if necessary */
		/* We don't modify the original scalar.  */
		tmpbuf = bytes_to_utf8((const U8*) tmps, &len);
		tmps = (char *) tmpbuf;
	    }
	    else if (ckWARN4_d(WARN_UTF8, WARN_SURROGATE, WARN_NON_UNICODE, WARN_NONCHAR)) {
		(void) check_utf8_print((const U8*) tmps, len);
	    }
	} /* else stream isn't utf8 */
	else if (DO_UTF8(sv)) { /* But if is utf8 internally, attempt to
				   convert to bytes */
	    STRLEN tmplen = len;
	    bool utf8 = TRUE;
	    U8 * const result = bytes_from_utf8((const U8*) tmps, &tmplen, &utf8);
	    if (!utf8) {

		/* Here, succeeded in downgrading from utf8.  Set up to below
		 * output the converted value */
		tmpbuf = result;
		tmps = (char *) tmpbuf;
		len = tmplen;
	    }
	    else {  /* Non-utf8 output stream, but string only representable in
		       utf8 */
		assert((char *)result == tmps);
		Perl_ck_warner_d(aTHX_ packWARN(WARN_UTF8),
				 "Wide character in %s",
				   PL_op ? OP_DESC(PL_op) : "print"
				);
		    /* Could also check that isn't one of the things to avoid
		     * in utf8 by using check_utf8_print(), but not doing so,
		     * since the stream isn't a UTF8 stream */
	    }
	}
	/* To detect whether the process is about to overstep its
	 * filesize limit we would need getrlimit().  We could then
	 * also transparently raise the limit with setrlimit() --
	 * but only until the system hard limit/the filesystem limit,
	 * at which we would get EPERM.  Note that when using buffered
	 * io the write failure can be delayed until the flush/close. --jhi */
	if (len && (PerlIO_write(fp,tmps,len) == 0))
	    happy = FALSE;
	Safefree(tmpbuf);
	return happy ? !PerlIO_error(fp) : FALSE;
    }
}

I32
Perl_my_stat_flags(pTHX_ const U32 flags)
{
    dSP;
    IO *io;
    GV* gv;

    if (PL_op->op_flags & OPf_REF) {
	gv = cGVOP_gv;
      do_fstat:
        if (gv == PL_defgv)
            return PL_laststatval;
	io = GvIO(gv);
        do_fstat_have_io:
        PL_laststype = OP_STAT;
        PL_statgv = gv ? gv : (GV *)io;
        sv_setpvs(PL_statname, "");
        if (io) {
	    if (IoIFP(io)) {
                int fd = PerlIO_fileno(IoIFP(io));
                if (fd < 0) {
                    /* E.g. PerlIO::scalar has no real fd. */
                    return (PL_laststatval = -1);
                } else {
                    return (PL_laststatval = PerlLIO_fstat(fd, &PL_statcache));
                }
            } else if (IoDIRP(io)) {
                return (PL_laststatval = PerlLIO_fstat(my_dirfd(IoDIRP(io)), &PL_statcache));
            }
        }
	PL_laststatval = -1;
	report_evil_fh(gv);
	return -1;
    }
    else if ((PL_op->op_private & (OPpFT_STACKED|OPpFT_AFTER_t))
	     == OPpFT_STACKED)
	return PL_laststatval;
    else {
	SV* const sv = TOPs;
	const char *s;
	STRLEN len;
	if ((gv = MAYBE_DEREF_GV_flags(sv,flags))) {
	    goto do_fstat;
	}
        else if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVIO) {
            io = MUTABLE_IO(SvRV(sv));
	    gv = NULL;
            goto do_fstat_have_io;
        }

	s = SvPV_flags_const(sv, len, flags);
	PL_statgv = NULL;
	sv_setpvn(PL_statname, s, len);
	s = SvPVX_const(PL_statname);		/* s now NUL-terminated */
	PL_laststype = OP_STAT;
	PL_laststatval = PerlLIO_stat(s, &PL_statcache);
	if (PL_laststatval < 0 && ckWARN(WARN_NEWLINE) && should_warn_nl(s)) {
            GCC_DIAG_IGNORE(-Wformat-nonliteral); /* PL_warn_nl is constant */
	    Perl_warner(aTHX_ packWARN(WARN_NEWLINE), PL_warn_nl, "stat");
            GCC_DIAG_RESTORE;
        }
	return PL_laststatval;
    }
}


I32
Perl_my_lstat_flags(pTHX_ const U32 flags)
{
    static const char* const no_prev_lstat = "The stat preceding -l _ wasn't an lstat";
    dSP;
    const char *file;
    SV* const sv = TOPs;
    bool isio = FALSE;
    if (PL_op->op_flags & OPf_REF) {
	if (cGVOP_gv == PL_defgv) {
	    if (PL_laststype != OP_LSTAT)
		Perl_croak(aTHX_ "%s", no_prev_lstat);
	    return PL_laststatval;
	}
	PL_laststatval = -1;
	if (ckWARN(WARN_IO)) {
	    /* diag_listed_as: Use of -l on filehandle%s */
	    Perl_warner(aTHX_ packWARN(WARN_IO),
		 	     "Use of -l on filehandle %"HEKf,
			      HEKfARG(GvENAME_HEK(cGVOP_gv)));
	}
	return -1;
    }
    if ((PL_op->op_private & (OPpFT_STACKED|OPpFT_AFTER_t))
	     == OPpFT_STACKED) {
      if (PL_laststype != OP_LSTAT)
	Perl_croak(aTHX_ "%s", no_prev_lstat);
      return PL_laststatval;
    }

    PL_laststype = OP_LSTAT;
    PL_statgv = NULL;
    if ( (  (SvROK(sv) && (  isGV_with_GP(SvRV(sv))
                          || (isio = SvTYPE(SvRV(sv)) == SVt_PVIO)  )
            )
         || isGV_with_GP(sv)
         )
      && ckWARN(WARN_IO)) {
        if (isio)
	    /* diag_listed_as: Use of -l on filehandle%s */
            Perl_warner(aTHX_ packWARN(WARN_IO),
                             "Use of -l on filehandle");
        else
	    /* diag_listed_as: Use of -l on filehandle%s */
            Perl_warner(aTHX_ packWARN(WARN_IO),
                             "Use of -l on filehandle %"HEKf,
                              HEKfARG(GvENAME_HEK((const GV *)
                                          (SvROK(sv) ? SvRV(sv) : sv))));
    }
    file = SvPV_flags_const_nolen(sv, flags);
    sv_setpv(PL_statname,file);
    PL_laststatval = PerlLIO_lstat(file,&PL_statcache);
    if (PL_laststatval < 0 && ckWARN(WARN_NEWLINE) && should_warn_nl(file)) {
        GCC_DIAG_IGNORE(-Wformat-nonliteral); /* PL_warn_nl is constant */
        Perl_warner(aTHX_ packWARN(WARN_NEWLINE), PL_warn_nl, "lstat");
        GCC_DIAG_RESTORE;
    }
    return PL_laststatval;
}

static void
S_exec_failed(pTHX_ const char *cmd, int fd, int do_report)
{
    const int e = errno;
    PERL_ARGS_ASSERT_EXEC_FAILED;

    if (ckWARN(WARN_EXEC))
        Perl_warner(aTHX_ packWARN(WARN_EXEC), "Can't exec \"%s\": %s",
                    cmd, Strerror(e));
    if (do_report) {
        /* XXX silently ignore failures */
        PERL_UNUSED_RESULT(PerlLIO_write(fd, (void*)&e, sizeof(int)));
	PerlLIO_close(fd);
    }
}

bool
Perl_do_aexec5(pTHX_ SV *really, SV **mark, SV **sp,
	       int fd, int do_report)
{
    dVAR;
    PERL_ARGS_ASSERT_DO_AEXEC5;
#if defined(__SYMBIAN32__) || defined(__LIBCATAMOUNT__)
    Perl_croak(aTHX_ "exec? I'm not *that* kind of operating system");
#else
    if (sp > mark) {
	const char **a;
	const char *tmps = NULL;
	Newx(PL_Argv, sp - mark + 1, const char*);
	a = PL_Argv;

	while (++mark <= sp) {
	    if (*mark)
		*a++ = SvPV_nolen_const(*mark);
	    else
		*a++ = "";
	}
	*a = NULL;
	if (really)
	    tmps = SvPV_nolen_const(really);
	if ((!really && *PL_Argv[0] != '/') ||
	    (really && *tmps != '/'))		/* will execvp use PATH? */
	    TAINT_ENV();		/* testing IFS here is overkill, probably */
	PERL_FPU_PRE_EXEC
	if (really && *tmps) {
            PerlProc_execvp(tmps,EXEC_ARGV_CAST(PL_Argv));
	} else {
            PerlProc_execvp(PL_Argv[0],EXEC_ARGV_CAST(PL_Argv));
	}
	PERL_FPU_POST_EXEC
 	S_exec_failed(aTHX_ (really ? tmps : PL_Argv[0]), fd, do_report);
    }
    do_execfree();
#endif
    return FALSE;
}

void
Perl_do_execfree(pTHX)
{
    Safefree(PL_Argv);
    PL_Argv = NULL;
    Safefree(PL_Cmd);
    PL_Cmd = NULL;
}

#ifdef PERL_DEFAULT_DO_EXEC3_IMPLEMENTATION

bool
Perl_do_exec3(pTHX_ const char *incmd, int fd, int do_report)
{
    dVAR;
    const char **a;
    char *s;
    char *buf;
    char *cmd;
    /* Make a copy so we can change it */
    const Size_t cmdlen = strlen(incmd) + 1;

    PERL_ARGS_ASSERT_DO_EXEC3;

    Newx(buf, cmdlen, char);
    cmd = buf;
    memcpy(cmd, incmd, cmdlen);

    while (*cmd && isSPACE(*cmd))
	cmd++;

    /* save an extra exec if possible */

#ifdef CSH
    {
        char flags[PERL_FLAGS_MAX];
	if (strnEQ(cmd,PL_cshname,PL_cshlen) &&
	    strnEQ(cmd+PL_cshlen," -c",3)) {
          my_strlcpy(flags, "-c", PERL_FLAGS_MAX);
	  s = cmd+PL_cshlen+3;
	  if (*s == 'f') {
	      s++;
              my_strlcat(flags, "f", PERL_FLAGS_MAX - 2);
	  }
	  if (*s == ' ')
	      s++;
	  if (*s++ == '\'') {
	      char * const ncmd = s;

	      while (*s)
		  s++;
	      if (s[-1] == '\n')
		  *--s = '\0';
	      if (s[-1] == '\'') {
		  *--s = '\0';
		  PERL_FPU_PRE_EXEC
		  PerlProc_execl(PL_cshname, "csh", flags, ncmd, (char*)NULL);
		  PERL_FPU_POST_EXEC
		  *s = '\'';
 		  S_exec_failed(aTHX_ PL_cshname, fd, do_report);
		  Safefree(buf);
		  return FALSE;
	      }
	  }
	}
    }
#endif /* CSH */

    /* see if there are shell metacharacters in it */

    if (*cmd == '.' && isSPACE(cmd[1]))
	goto doshell;

    if (strnEQ(cmd,"exec",4) && isSPACE(cmd[4]))
	goto doshell;

    s = cmd;
    while (isWORDCHAR(*s))
	s++;	/* catch VAR=val gizmo */
    if (*s == '=')
	goto doshell;

    for (s = cmd; *s; s++) {
	if (*s != ' ' && !isALPHA(*s) &&
	    strchr("$&*(){}[]'\";\\|?<>~`\n",*s)) {
	    if (*s == '\n' && !s[1]) {
		*s = '\0';
		break;
	    }
	    /* handle the 2>&1 construct at the end */
	    if (*s == '>' && s[1] == '&' && s[2] == '1'
		&& s > cmd + 1 && s[-1] == '2' && isSPACE(s[-2])
		&& (!s[3] || isSPACE(s[3])))
	    {
                const char *t = s + 3;

		while (*t && isSPACE(*t))
		    ++t;
		if (!*t && (PerlLIO_dup2(1,2) != -1)) {
		    s[-2] = '\0';
		    break;
		}
	    }
	  doshell:
	    PERL_FPU_PRE_EXEC
            PerlProc_execl(PL_sh_path, "sh", "-c", cmd, (char *)NULL);
	    PERL_FPU_POST_EXEC
 	    S_exec_failed(aTHX_ PL_sh_path, fd, do_report);
	    Safefree(buf);
	    return FALSE;
	}
    }

    Newx(PL_Argv, (s - cmd) / 2 + 2, const char*);
    PL_Cmd = savepvn(cmd, s-cmd);
    a = PL_Argv;
    for (s = PL_Cmd; *s;) {
	while (isSPACE(*s))
	    s++;
	if (*s)
	    *(a++) = s;
	while (*s && !isSPACE(*s))
	    s++;
	if (*s)
	    *s++ = '\0';
    }
    *a = NULL;
    if (PL_Argv[0]) {
	PERL_FPU_PRE_EXEC
        PerlProc_execvp(PL_Argv[0],EXEC_ARGV_CAST(PL_Argv));
	PERL_FPU_POST_EXEC
	if (errno == ENOEXEC) {		/* for system V NIH syndrome */
	    do_execfree();
	    goto doshell;
	}
 	S_exec_failed(aTHX_ PL_Argv[0], fd, do_report);
    }
    do_execfree();
    Safefree(buf);
    return FALSE;
}

#endif /* OS2 || WIN32 */

I32
Perl_apply(pTHX_ I32 type, SV **mark, SV **sp)
{
    I32 val;
    I32 tot = 0;
    const char *const what = PL_op_name[type];
    const char *s;
    STRLEN len;
    SV ** const oldmark = mark;
    bool killgp = FALSE;

    PERL_ARGS_ASSERT_APPLY;

    PERL_UNUSED_VAR(what); /* may not be used depending on compile options */

    /* Doing this ahead of the switch statement preserves the old behaviour,
       where attempting to use kill as a taint test test would fail on
       platforms where kill was not defined.  */
#ifndef HAS_KILL
    if (type == OP_KILL)
	Perl_die(aTHX_ PL_no_func, what);
#endif
#ifndef HAS_CHOWN
    if (type == OP_CHOWN)
	Perl_die(aTHX_ PL_no_func, what);
#endif


#define APPLY_TAINT_PROPER() \
    STMT_START {							\
	if (TAINT_get) { TAINT_PROPER(what); }				\
    } STMT_END

    /* This is a first heuristic; it doesn't catch tainting magic. */
    if (TAINTING_get) {
	while (++mark <= sp) {
	    if (SvTAINTED(*mark)) {
		TAINT;
		break;
	    }
	}
	mark = oldmark;
    }
    switch (type) {
    case OP_CHMOD:
	APPLY_TAINT_PROPER();
	if (++mark <= sp) {
	    val = SvIV(*mark);
	    APPLY_TAINT_PROPER();
	    tot = sp - mark;
	    while (++mark <= sp) {
                GV* gv;
                if ((gv = MAYBE_DEREF_GV(*mark))) {
		    if (GvIO(gv) && IoIFP(GvIOp(gv))) {
#ifdef HAS_FCHMOD
                        int fd = PerlIO_fileno(IoIFP(GvIOn(gv)));
			APPLY_TAINT_PROPER();
                        if (fd < 0) {
                            SETERRNO(EBADF,RMS_IFI);
                            tot--;
                        } else if (fchmod(fd, val))
                            tot--;
#else
			Perl_die(aTHX_ PL_no_func, "fchmod");
#endif
		    }
		    else {
                        SETERRNO(EBADF,RMS_IFI);
			tot--;
		    }
		}
		else {
		    const char *name = SvPV_nomg_const(*mark, len);
		    APPLY_TAINT_PROPER();
                    if (!IS_SAFE_PATHNAME(name, len, "chmod") ||
                        PerlLIO_chmod(name, val)) {
                        tot--;
                    }
		}
	    }
	}
	break;
#ifdef HAS_CHOWN
    case OP_CHOWN:
	APPLY_TAINT_PROPER();
	if (sp - mark > 2) {
            I32 val2;
	    val = SvIVx(*++mark);
	    val2 = SvIVx(*++mark);
	    APPLY_TAINT_PROPER();
	    tot = sp - mark;
	    while (++mark <= sp) {
                GV* gv;
		if ((gv = MAYBE_DEREF_GV(*mark))) {
		    if (GvIO(gv) && IoIFP(GvIOp(gv))) {
#ifdef HAS_FCHOWN
                        int fd = PerlIO_fileno(IoIFP(GvIOn(gv)));
			APPLY_TAINT_PROPER();
                        if (fd < 0) {
                            SETERRNO(EBADF,RMS_IFI);
			    tot--;
                        } else if (fchown(fd, val, val2))
			    tot--;
#else
			Perl_die(aTHX_ PL_no_func, "fchown");
#endif
		    }
		    else {
                        SETERRNO(EBADF,RMS_IFI);
			tot--;
		    }
		}
		else {
		    const char *name = SvPV_nomg_const(*mark, len);
		    APPLY_TAINT_PROPER();
                    if (!IS_SAFE_PATHNAME(name, len, "chown") ||
                        PerlLIO_chown(name, val, val2)) {
			tot--;
                    }
		}
	    }
	}
	break;
#endif
/*
XXX Should we make lchown() directly available from perl?
For now, we'll let Configure test for HAS_LCHOWN, but do
nothing in the core.
    --AD  5/1998
*/
#ifdef HAS_KILL
    case OP_KILL:
	APPLY_TAINT_PROPER();
	if (mark == sp)
	    break;
	s = SvPVx_const(*++mark, len);
	if (*s == '-' && isALPHA(s[1]))
	{
	    s++;
	    len--;
            killgp = TRUE;
	}
	if (isALPHA(*s)) {
	    if (*s == 'S' && s[1] == 'I' && s[2] == 'G') {
		s += 3;
                len -= 3;
            }
           if ((val = whichsig_pvn(s, len)) < 0)
               Perl_croak(aTHX_ "Unrecognized signal name \"%"SVf"\"", SVfARG(*mark));
	}
	else
	{
	    val = SvIV(*mark);
	    if (val < 0)
	    {
		killgp = TRUE;
                val = -val;
	    }
	}
	APPLY_TAINT_PROPER();
	tot = sp - mark;

	while (++mark <= sp) {
	    Pid_t proc;
	    SvGETMAGIC(*mark);
	    if (!(SvNIOK(*mark) || looks_like_number(*mark)))
		Perl_croak(aTHX_ "Can't kill a non-numeric process ID");
	    proc = SvIV_nomg(*mark);
	    APPLY_TAINT_PROPER();
#ifdef HAS_KILLPG
            /* use killpg in preference, as the killpg() wrapper for Win32
             * understands process groups, but the kill() wrapper doesn't */
            if (killgp ? PerlProc_killpg(proc, val)
                       : PerlProc_kill(proc, val))
#else
            if (PerlProc_kill(killgp ? -proc: proc, val))
#endif
		tot--;
	}
	PERL_ASYNC_CHECK();
	break;
#endif
    case OP_UNLINK:
	APPLY_TAINT_PROPER();
	tot = sp - mark;
	while (++mark <= sp) {
	    s = SvPV_const(*mark, len);
	    APPLY_TAINT_PROPER();
	    if (!IS_SAFE_PATHNAME(s, len, "unlink")) {
                tot--;
            }
	    else if (PL_unsafe) {
		if (UNLINK(s))
		{
		    tot--;
		}
#if defined(__amigaos4__) && defined(NEWLIB)
		else
		{
                  /* Under AmigaOS4 unlink only 'fails' if the
                   * filename is invalid.  It may not remove the file
                   * if it's locked, so check if it's still around. */
                  if ((access(s,F_OK) != -1))
                  {
                    tot--;
                  }
		}
#endif
	    }
	    else {	/* don't let root wipe out directories without -U */
		Stat_t statbuf;
		if (PerlLIO_lstat(s, &statbuf) < 0)
		    tot--;
		else if (S_ISDIR(statbuf.st_mode)) {
		    SETERRNO(EISDIR, SS_NOPRIV);
		    tot--;
		}
		else {
		    if (UNLINK(s))
		    {
				tot--;
			}
#if defined(__amigaos4__) && defined(NEWLIB)
			else
			{
				/* Under AmigaOS4 unlink only 'fails' if the filename is invalid */
				/* It may not remove the file if it's Locked, so check if it's still */
				/* arround */
				if((access(s,F_OK) != -1))
				{
					tot--;
				}
			}	
#endif
		}
	    }
	}
	break;
#if defined(HAS_UTIME) || defined(HAS_FUTIMES)
    case OP_UTIME:
	APPLY_TAINT_PROPER();
	if (sp - mark > 2) {
#if defined(HAS_FUTIMES)
	    struct timeval utbuf[2];
	    void *utbufp = utbuf;
#elif defined(I_UTIME) || defined(VMS)
	    struct utimbuf utbuf;
	    struct utimbuf *utbufp = &utbuf;
#else
	    struct {
		Time_t	actime;
		Time_t	modtime;
	    } utbuf;
	    void *utbufp = &utbuf;
#endif

	   SV* const accessed = *++mark;
	   SV* const modified = *++mark;

           /* Be like C, and if both times are undefined, let the C
            * library figure out what to do.  This usually means
            * "current time". */

           if ( accessed == &PL_sv_undef && modified == &PL_sv_undef )
                utbufp = NULL;
           else {
                Zero(&utbuf, sizeof utbuf, char);
#ifdef HAS_FUTIMES
		utbuf[0].tv_sec = (long)SvIV(accessed);  /* time accessed */
		utbuf[0].tv_usec = 0;
		utbuf[1].tv_sec = (long)SvIV(modified);  /* time modified */
		utbuf[1].tv_usec = 0;
#elif defined(BIG_TIME)
                utbuf.actime = (Time_t)SvNV(accessed);  /* time accessed */
                utbuf.modtime = (Time_t)SvNV(modified); /* time modified */
#else
                utbuf.actime = (Time_t)SvIV(accessed);  /* time accessed */
                utbuf.modtime = (Time_t)SvIV(modified); /* time modified */
#endif
            }
	    APPLY_TAINT_PROPER();
	    tot = sp - mark;
	    while (++mark <= sp) {
                GV* gv;
                if ((gv = MAYBE_DEREF_GV(*mark))) {
		    if (GvIO(gv) && IoIFP(GvIOp(gv))) {
#ifdef HAS_FUTIMES
                        int fd =  PerlIO_fileno(IoIFP(GvIOn(gv)));
			APPLY_TAINT_PROPER();
                        if (fd < 0) {
                            SETERRNO(EBADF,RMS_IFI);
                            tot--;
			} else if (futimes(fd, (struct timeval *) utbufp))
			    tot--;
#else
			Perl_die(aTHX_ PL_no_func, "futimes");
#endif
		    }
		    else {
			tot--;
		    }
		}
		else {
		    const char * const name = SvPV_nomg_const(*mark, len);
		    APPLY_TAINT_PROPER();
		    if (!IS_SAFE_PATHNAME(name, len, "utime")) {
                        tot--;
                    }
                    else
#ifdef HAS_FUTIMES
		    if (utimes(name, (struct timeval *)utbufp))
#else
		    if (PerlLIO_utime(name, utbufp))
#endif
			tot--;
		}

	    }
	}
	else
	    tot = 0;
	break;
#endif
    }
    return tot;

#undef APPLY_TAINT_PROPER
}

/* Do the permissions allow some operation?  Assumes statcache already set. */
#ifndef VMS /* VMS' cando is in vms.c */
bool
Perl_cando(pTHX_ Mode_t mode, bool effective, const Stat_t *statbufp)
/* effective is a flag, true for EUID, or for checking if the effective gid
 *  is in the list of groups returned from getgroups().
 */
{
    PERL_ARGS_ASSERT_CANDO;
    PERL_UNUSED_CONTEXT;

#ifdef DOSISH
    /* [Comments and code from Len Reed]
     * MS-DOS "user" is similar to UNIX's "superuser," but can't write
     * to write-protected files.  The execute permission bit is set
     * by the Microsoft C library stat() function for the following:
     *		.exe files
     *		.com files
     *		.bat files
     *		directories
     * All files and directories are readable.
     * Directories and special files, e.g. "CON", cannot be
     * write-protected.
     * [Comment by Tom Dinger -- a directory can have the write-protect
     *		bit set in the file system, but DOS permits changes to
     *		the directory anyway.  In addition, all bets are off
     *		here for networked software, such as Novell and
     *		Sun's PC-NFS.]
     */

     /* Atari stat() does pretty much the same thing. we set x_bit_set_in_stat
      * too so it will actually look into the files for magic numbers
      */
     return (mode & statbufp->st_mode) ? TRUE : FALSE;

#else /* ! DOSISH */
# ifdef __CYGWIN__
    if (ingroup(544,effective)) {     /* member of Administrators */
# else
    if ((effective ? PerlProc_geteuid() : PerlProc_getuid()) == 0) {	/* root is special */
# endif
	if (mode == S_IXUSR) {
	    if (statbufp->st_mode & 0111 || S_ISDIR(statbufp->st_mode))
		return TRUE;
	}
	else
	    return TRUE;		/* root reads and writes anything */
	return FALSE;
    }
    if (statbufp->st_uid == (effective ? PerlProc_geteuid() : PerlProc_getuid()) ) {
	if (statbufp->st_mode & mode)
	    return TRUE;	/* ok as "user" */
    }
    else if (ingroup(statbufp->st_gid,effective)) {
	if (statbufp->st_mode & mode >> 3)
	    return TRUE;	/* ok as "group" */
    }
    else if (statbufp->st_mode & mode >> 6)
	return TRUE;	/* ok as "other" */
    return FALSE;
#endif /* ! DOSISH */
}
#endif /* ! VMS */

static bool
S_ingroup(pTHX_ Gid_t testgid, bool effective)
{
#ifndef PERL_IMPLICIT_SYS
    /* PERL_IMPLICIT_SYS like Win32: getegid() etc. require the context. */
    PERL_UNUSED_CONTEXT;
#endif
    if (testgid == (effective ? PerlProc_getegid() : PerlProc_getgid()))
	return TRUE;
#ifdef HAS_GETGROUPS
    {
	Groups_t *gary = NULL;
	I32 anum;
        bool rc = FALSE;

	anum = getgroups(0, gary);
        if (anum > 0) {
            Newx(gary, anum, Groups_t);
            anum = getgroups(anum, gary);
            while (--anum >= 0)
                if (gary[anum] == testgid) {
                    rc = TRUE;
                    break;
                }

            Safefree(gary);
        }
        return rc;
    }
#else
    return FALSE;
#endif
}

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)

I32
Perl_do_ipcget(pTHX_ I32 optype, SV **mark, SV **sp)
{
    const key_t key = (key_t)SvNVx(*++mark);
    SV *nsv = optype == OP_MSGGET ? NULL : *++mark;
    const I32 flags = SvIVx(*++mark);

    PERL_ARGS_ASSERT_DO_IPCGET;
    PERL_UNUSED_ARG(sp);

    SETERRNO(0,0);
    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGGET:
	return msgget(key, flags);
#endif
#ifdef HAS_SEM
    case OP_SEMGET:
	return semget(key, (int) SvIV(nsv), flags);
#endif
#ifdef HAS_SHM
    case OP_SHMGET:
	return shmget(key, (size_t) SvUV(nsv), flags);
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
        /* diag_listed_as: msg%s not implemented */
	Perl_croak(aTHX_ "%s not implemented", PL_op_desc[optype]);
#endif
    }
    return -1;			/* should never happen */
}

I32
Perl_do_ipcctl(pTHX_ I32 optype, SV **mark, SV **sp)
{
    char *a;
    I32 ret = -1;
    const I32 id  = SvIVx(*++mark);
#ifdef Semctl
    const I32 n   = (optype == OP_SEMCTL) ? SvIVx(*++mark) : 0;
#endif
    const I32 cmd = SvIVx(*++mark);
    SV * const astr = *++mark;
    STRLEN infosize = 0;
    I32 getinfo = (cmd == IPC_STAT);

    PERL_ARGS_ASSERT_DO_IPCCTL;
    PERL_UNUSED_ARG(sp);

    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct msqid_ds);
	break;
#endif
#ifdef HAS_SHM
    case OP_SHMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct shmid_ds);
	break;
#endif
#ifdef HAS_SEM
    case OP_SEMCTL:
#ifdef Semctl
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct semid_ds);
	else if (cmd == GETALL || cmd == SETALL)
	{
	    struct semid_ds semds;
	    union semun semun;
#ifdef EXTRA_F_IN_SEMUN_BUF
            semun.buff = &semds;
#else
            semun.buf = &semds;
#endif
	    getinfo = (cmd == GETALL);
	    if (Semctl(id, 0, IPC_STAT, semun) == -1)
		return -1;
	    infosize = semds.sem_nsems * sizeof(short);
		/* "short" is technically wrong but much more portable
		   than guessing about u_?short(_t)? */
	}
#else
        /* diag_listed_as: sem%s not implemented */
	Perl_croak(aTHX_ "%s not implemented", PL_op_desc[optype]);
#endif
	break;
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
        /* diag_listed_as: shm%s not implemented */
	Perl_croak(aTHX_ "%s not implemented", PL_op_desc[optype]);
#endif
    }

    if (infosize)
    {
	if (getinfo)
	{
	    SvPV_force_nolen(astr);
	    a = SvGROW(astr, infosize+1);
	}
	else
	{
	    STRLEN len;
	    a = SvPV(astr, len);
	    if (len != infosize)
		Perl_croak(aTHX_ "Bad arg length for %s, is %lu, should be %ld",
		      PL_op_desc[optype],
		      (unsigned long)len,
		      (long)infosize);
	}
    }
    else
    {
	const IV i = SvIV(astr);
	a = INT2PTR(char *,i);		/* ouch */
    }
    SETERRNO(0,0);
    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGCTL:
	ret = msgctl(id, cmd, (struct msqid_ds *)a);
	break;
#endif
#ifdef HAS_SEM
    case OP_SEMCTL: {
#ifdef Semctl
            union semun unsemds;

            if(cmd == SETVAL) {
                unsemds.val = PTR2nat(a);
            }
            else {
#ifdef EXTRA_F_IN_SEMUN_BUF
                unsemds.buff = (struct semid_ds *)a;
#else
                unsemds.buf = (struct semid_ds *)a;
#endif
            }
	    ret = Semctl(id, n, cmd, unsemds);
#else
	    /* diag_listed_as: sem%s not implemented */
	    Perl_croak(aTHX_ "%s not implemented", PL_op_desc[optype]);
#endif
        }
	break;
#endif
#ifdef HAS_SHM
    case OP_SHMCTL:
	ret = shmctl(id, cmd, (struct shmid_ds *)a);
	break;
#endif
    }
    if (getinfo && ret >= 0) {
	SvCUR_set(astr, infosize);
	*SvEND(astr) = '\0';
	SvSETMAGIC(astr);
    }
    return ret;
}

I32
Perl_do_msgsnd(pTHX_ SV **mark, SV **sp)
{
#ifdef HAS_MSG
    STRLEN len;
    const I32 id = SvIVx(*++mark);
    SV * const mstr = *++mark;
    const I32 flags = SvIVx(*++mark);
    const char * const mbuf = SvPV_const(mstr, len);
    const I32 msize = len - sizeof(long);

    PERL_ARGS_ASSERT_DO_MSGSND;
    PERL_UNUSED_ARG(sp);

    if (msize < 0)
	Perl_croak(aTHX_ "Arg too short for msgsnd");
    SETERRNO(0,0);
    if (id >= 0 && flags >= 0) {
      return msgsnd(id, (struct msgbuf *)mbuf, msize, flags);
    } else {
      SETERRNO(EINVAL,LIB_INVARG);
      return -1;
    }
#else
    PERL_UNUSED_ARG(sp);
    PERL_UNUSED_ARG(mark);
    /* diag_listed_as: msg%s not implemented */
    Perl_croak(aTHX_ "msgsnd not implemented");
    return -1;
#endif
}

I32
Perl_do_msgrcv(pTHX_ SV **mark, SV **sp)
{
#ifdef HAS_MSG
    char *mbuf;
    long mtype;
    I32 msize, flags, ret;
    const I32 id = SvIVx(*++mark);
    SV * const mstr = *++mark;

    PERL_ARGS_ASSERT_DO_MSGRCV;
    PERL_UNUSED_ARG(sp);

    /* suppress warning when reading into undef var --jhi */
    if (! SvOK(mstr))
	sv_setpvs(mstr, "");
    msize = SvIVx(*++mark);
    mtype = (long)SvIVx(*++mark);
    flags = SvIVx(*++mark);
    SvPV_force_nolen(mstr);
    mbuf = SvGROW(mstr, sizeof(long)+msize+1);

    SETERRNO(0,0);
    if (id >= 0 && msize >= 0 && flags >= 0) {
        ret = msgrcv(id, (struct msgbuf *)mbuf, msize, mtype, flags);
    } else {
        SETERRNO(EINVAL,LIB_INVARG);
        ret = -1;
    }
    if (ret >= 0) {
	SvCUR_set(mstr, sizeof(long)+ret);
	*SvEND(mstr) = '\0';
	/* who knows who has been playing with this message? */
	SvTAINTED_on(mstr);
    }
    return ret;
#else
    PERL_UNUSED_ARG(sp);
    PERL_UNUSED_ARG(mark);
    /* diag_listed_as: msg%s not implemented */
    Perl_croak(aTHX_ "msgrcv not implemented");
    return -1;
#endif
}

I32
Perl_do_semop(pTHX_ SV **mark, SV **sp)
{
#ifdef HAS_SEM
    STRLEN opsize;
    const I32 id = SvIVx(*++mark);
    SV * const opstr = *++mark;
    const char * const opbuf = SvPV_const(opstr, opsize);

    PERL_ARGS_ASSERT_DO_SEMOP;
    PERL_UNUSED_ARG(sp);

    if (opsize < 3 * SHORTSIZE
	|| (opsize % (3 * SHORTSIZE))) {
	SETERRNO(EINVAL,LIB_INVARG);
	return -1;
    }
    SETERRNO(0,0);
    /* We can't assume that sizeof(struct sembuf) == 3 * sizeof(short). */
    {
        const int nsops  = opsize / (3 * sizeof (short));
        int i      = nsops;
        short * const ops = (short *) opbuf;
        short *o   = ops;
        struct sembuf *temps, *t;
        I32 result;

        Newx (temps, nsops, struct sembuf);
        t = temps;
        while (i--) {
            t->sem_num = *o++;
            t->sem_op  = *o++;
            t->sem_flg = *o++;
            t++;
        }
        result = semop(id, temps, nsops);
        Safefree(temps);
        return result;
    }
#else
    /* diag_listed_as: sem%s not implemented */
    Perl_croak(aTHX_ "semop not implemented");
#endif
}

I32
Perl_do_shmio(pTHX_ I32 optype, SV **mark, SV **sp)
{
#ifdef HAS_SHM
    char *shm;
    struct shmid_ds shmds;
    const I32 id = SvIVx(*++mark);
    SV * const mstr = *++mark;
    const I32 mpos = SvIVx(*++mark);
    const I32 msize = SvIVx(*++mark);

    PERL_ARGS_ASSERT_DO_SHMIO;
    PERL_UNUSED_ARG(sp);

    SETERRNO(0,0);
    if (shmctl(id, IPC_STAT, &shmds) == -1)
	return -1;
    if (mpos < 0 || msize < 0
	|| (size_t)mpos + msize > (size_t)shmds.shm_segsz) {
	SETERRNO(EFAULT,SS_ACCVIO);		/* can't do as caller requested */
	return -1;
    }
    if (id >= 0) {
        shm = (char *)shmat(id, NULL, (optype == OP_SHMREAD) ? SHM_RDONLY : 0);
    } else {
        SETERRNO(EINVAL,LIB_INVARG);
        return -1;
    }
    if (shm == (char *)-1)	/* I hate System V IPC, I really do */
	return -1;
    if (optype == OP_SHMREAD) {
	char *mbuf;
	/* suppress warning when reading into undef var (tchrist 3/Mar/00) */
	SvGETMAGIC(mstr);
	SvUPGRADE(mstr, SVt_PV);
	if (! SvOK(mstr))
	    sv_setpvs(mstr, "");
	SvPOK_only(mstr);
	mbuf = SvGROW(mstr, (STRLEN)msize+1);

	Copy(shm + mpos, mbuf, msize, char);
	SvCUR_set(mstr, msize);
	*SvEND(mstr) = '\0';
	SvSETMAGIC(mstr);
	/* who knows who has been playing with this shared memory? */
	SvTAINTED_on(mstr);
    }
    else {
	STRLEN len;

	const char *mbuf = SvPV_const(mstr, len);
	const I32 n = ((I32)len > msize) ? msize : (I32)len;
	Copy(mbuf, shm + mpos, n, char);
	if (n < msize)
	    memzero(shm + mpos + n, msize - n);
    }
    return shmdt(shm);
#else
    /* diag_listed_as: shm%s not implemented */
    Perl_croak(aTHX_ "shm I/O not implemented");
    return -1;
#endif
}

#endif /* SYSV IPC */

/*
=head1 IO Functions

=for apidoc start_glob

Function called by C<do_readline> to spawn a glob (or do the glob inside
perl on VMS).  This code used to be inline, but now perl uses C<File::Glob>
this glob starter is only used by miniperl during the build process,
or when PERL_EXTERNAL_GLOB is defined.
Moving it away shrinks F<pp_hot.c>; shrinking F<pp_hot.c> helps speed perl up.

=cut
*/

PerlIO *
Perl_start_glob (pTHX_ SV *tmpglob, IO *io)
{
    SV * const tmpcmd = newSV(0);
    PerlIO *fp;
    STRLEN len;
    const char *s = SvPV(tmpglob, len);

    PERL_ARGS_ASSERT_START_GLOB;

    if (!IS_SAFE_SYSCALL(s, len, "pattern", "glob"))
        return NULL;

    ENTER;
    SAVEFREESV(tmpcmd);
#ifdef VMS /* expand the wildcards right here, rather than opening a pipe, */
           /* since spawning off a process is a real performance hit */

PerlIO * 
Perl_vms_start_glob
   (pTHX_ SV *tmpglob,
    IO *io);

    fp = Perl_vms_start_glob(aTHX_ tmpglob, io);

#else /* !VMS */
#ifdef DOSISH
#ifdef OS2
    sv_setpv(tmpcmd, "for a in ");
    sv_catsv(tmpcmd, tmpglob);
    sv_catpv(tmpcmd, "; do echo \"$a\\0\\c\"; done |");
#else
#ifdef DJGPP
    sv_setpv(tmpcmd, "/dev/dosglob/"); /* File System Extension */
    sv_catsv(tmpcmd, tmpglob);
#else
    sv_setpv(tmpcmd, "perlglob ");
    sv_catsv(tmpcmd, tmpglob);
    sv_catpv(tmpcmd, " |");
#endif /* !DJGPP */
#endif /* !OS2 */
#else /* !DOSISH */
#if defined(CSH)
    sv_setpvn(tmpcmd, PL_cshname, PL_cshlen);
    sv_catpv(tmpcmd, " -cf 'set nonomatch; glob ");
    sv_catsv(tmpcmd, tmpglob);
    sv_catpv(tmpcmd, "' 2>/dev/null |");
#else
    sv_setpv(tmpcmd, "echo ");
    sv_catsv(tmpcmd, tmpglob);
    sv_catpv(tmpcmd, "|tr -s ' \t\f\r' '\\n\\n\\n\\n'|");
#endif /* !CSH */
#endif /* !DOSISH */
    {
	GV * const envgv = gv_fetchpvs("ENV", 0, SVt_PVHV);
	SV ** const home = hv_fetchs(GvHV(envgv), "HOME", 0);
	SV ** const path = hv_fetchs(GvHV(envgv), "PATH", 0);
	if (home && *home) SvGETMAGIC(*home);
	if (path && *path) SvGETMAGIC(*path);
	save_hash(gv_fetchpvs("ENV", 0, SVt_PVHV));
	if (home && *home) SvSETMAGIC(*home);
	if (path && *path) SvSETMAGIC(*path);
    }
    (void)do_open6(PL_last_in_gv, SvPVX_const(tmpcmd), SvCUR(tmpcmd),
                   NULL, NULL, 0);
    fp = IoIFP(io);
#endif /* !VMS */
    LEAVE;

    if (!fp && ckWARN(WARN_GLOB)) {
        Perl_warner(aTHX_ packWARN(WARN_GLOB), "glob failed (can't start child: %s)",
                    Strerror(errno));
    }

    return fp;
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
