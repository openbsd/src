/*
 * Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the same terms as Perl itself.
 */

#define PERL_EXT_IO

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#define PERLIO_NOT_STDIO 1
#include "perl.h"
#include "XSUB.h"
#define NEED_eval_pv
#define NEED_newCONSTSUB
#define NEED_newSVpvn_flags
#include "ppport.h"
#include "poll.h"
#ifdef I_UNISTD
#  include <unistd.h>
#endif
#if defined(I_FCNTL) || defined(HAS_FCNTL)
#  include <fcntl.h>
#endif

#ifndef SIOCATMARK
#   ifdef I_SYS_SOCKIO
#       include <sys/sockio.h>
#   endif
#endif

#ifdef PerlIO
#if defined(MACOS_TRADITIONAL) && defined(USE_SFIO)
#define PERLIO_IS_STDIO 1
#undef setbuf
#undef setvbuf
#define setvbuf		_stdsetvbuf
#define setbuf(f,b)	( __sf_setbuf(f,b) )
#endif
typedef int SysRet;
typedef PerlIO * InputStream;
typedef PerlIO * OutputStream;
#else
#define PERLIO_IS_STDIO 1
typedef int SysRet;
typedef FILE * InputStream;
typedef FILE * OutputStream;
#endif

#define MY_start_subparse(fmt,flags) start_subparse(fmt,flags)

#ifndef gv_stashpvn
#define gv_stashpvn(str,len,flags) gv_stashpv(str,flags)
#endif

#ifndef __attribute__noreturn__
#  define __attribute__noreturn__
#endif

#ifndef NORETURN_FUNCTION_END
# define NORETURN_FUNCTION_END /* NOT REACHED */ return 0
#endif

#ifndef dVAR
#  define dVAR dNOOP
#endif

#ifndef OpSIBLING
#  define OpSIBLING(o) (o)->op_sibling
#endif

static int not_here(const char *s) __attribute__noreturn__;
static int
not_here(const char *s)
{
    croak("%s not implemented on this architecture", s);
    NORETURN_FUNCTION_END;
}

#ifndef UVCHR_IS_INVARIANT   /* For use with Perls without this macro */
#   if ('A' == 65)
#       define UVCHR_IS_INVARIANT(cp) ((cp) < 128)
#   elif (defined(NATIVE_IS_INVARIANT)) /* EBCDIC on old Perl */
#       define UVCHR_IS_INVARIANT(cp) ((cp) < 256 && NATIVE_IS_INVARIANT(cp))
#   elif defined(isASCII)    /* EBCDIC on very old Perl */
        /* In EBCDIC, the invariants are the code points corresponding to ASCII,
         * plus all the controls.  All but one EBCDIC control is below SPACE; it
         * varies depending on the code page, determined by the ord of '^' */
#       define UVCHR_IS_INVARIANT(cp) (isASCII(cp)                            \
                                       || (cp) < ' '                          \
                                       || (('^' == 106)    /* POSIX-BC */     \
                                          ? (cp) == 95                        \
                                          : (cp) == 0xFF)) /* 1047 or 037 */
#   else    /* EBCDIC on very very old Perl */
        /* This assumes isascii() is available, but that could be fixed by
         * having the macro test for each printable ASCII char */
#       define UVCHR_IS_INVARIANT(cp) (isascii(cp)                            \
                                       || (cp) < ' '                          \
                                       || (('^' == 106)    /* POSIX-BC */     \
                                          ? (cp) == 95                        \
                                          : (cp) == 0xFF)) /* 1047 or 037 */
#   endif
#endif


#ifndef PerlIO
#define PerlIO_fileno(f) fileno(f)
#endif

static int
io_blocking(pTHX_ InputStream f, int block)
{
    int fd = -1;
#if defined(HAS_FCNTL)
    int RETVAL;
    if (!f) {
	errno = EBADF;
	return -1;
    }
    fd = PerlIO_fileno(f);
    if (fd < 0) {
      errno = EBADF;
      return -1;
    }
    RETVAL = fcntl(fd, F_GETFL, 0);
    if (RETVAL >= 0) {
	int mode = RETVAL;
	int newmode = mode;
#ifdef O_NONBLOCK
	/* POSIX style */

# ifndef O_NDELAY
#  define O_NDELAY O_NONBLOCK
# endif
	/* Note: UNICOS and UNICOS/mk a F_GETFL returns an O_NDELAY
	 * after a successful F_SETFL of an O_NONBLOCK. */
	RETVAL = RETVAL & (O_NONBLOCK | O_NDELAY) ? 0 : 1;

	if (block == 0) {
	    newmode &= ~O_NDELAY;
	    newmode |= O_NONBLOCK;
	} else if (block > 0) {
	    newmode &= ~(O_NDELAY|O_NONBLOCK);
	}
#else
	/* Not POSIX - better have O_NDELAY or we can't cope.
	 * for BSD-ish machines this is an acceptable alternative
	 * for SysV we can't tell "would block" from EOF but that is
	 * the way SysV is...
	 */
	RETVAL = RETVAL & O_NDELAY ? 0 : 1;

	if (block == 0) {
	    newmode |= O_NDELAY;
	} else if (block > 0) {
	    newmode &= ~O_NDELAY;
	}
#endif
	if (newmode != mode) {
            const int ret = fcntl(fd, F_SETFL, newmode);
	    if (ret < 0)
		RETVAL = ret;
	}
    }
    return RETVAL;
#else
#   ifdef WIN32
    if (block >= 0) {
	unsigned long flags = !block;
	/* ioctl claims to take char* but really needs a u_long sized buffer */
	const int ret = ioctl(fd, FIONBIO, (char*)&flags);
	if (ret != 0)
	    return -1;
	/* Win32 has no way to get the current blocking status of a socket.
	 * However, we don't want to just return undef, because there's no way
	 * to tell that the ioctl succeeded.
	 */
	return flags;
    }
    /* TODO: Perhaps set $! to ENOTSUP? */
    return -1;
#   else
    return -1;
#   endif
#endif
}

static OP *
io_pp_nextstate(pTHX)
{
    dVAR;
    COP *old_curcop = PL_curcop;
    OP *next = PL_ppaddr[PL_op->op_type](aTHX);
    PL_curcop = old_curcop;
    return next;
}

static OP *
io_ck_lineseq(pTHX_ OP *o)
{
    OP *kid = cBINOPo->op_first;
    for (; kid; kid = OpSIBLING(kid))
	if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE)
	    kid->op_ppaddr = io_pp_nextstate;
    return o;
}


MODULE = IO	PACKAGE = IO::Seekable	PREFIX = f

void
fgetpos(handle)
	InputStream	handle
    CODE:
	if (handle) {
#ifdef PerlIO
#if PERL_VERSION < 8
	    Fpos_t pos;
	    ST(0) = sv_newmortal();
	    if (PerlIO_getpos(handle, &pos) != 0) {
		ST(0) = &PL_sv_undef;
	    }
	    else {
		sv_setpvn(ST(0), (char *)&pos, sizeof(Fpos_t));
	    }
#else
	    ST(0) = sv_newmortal();
	    if (PerlIO_getpos(handle, ST(0)) != 0) {
		ST(0) = &PL_sv_undef;
	    }
#endif
#else
	    Fpos_t pos;
	    if (fgetpos(handle, &pos)) {
		ST(0) = &PL_sv_undef;
	    } else {
#  if PERL_VERSION >= 11
		ST(0) = newSVpvn_flags((char*)&pos, sizeof(Fpos_t), SVs_TEMP);
#  else
		ST(0) = sv_2mortal(newSVpvn((char*)&pos, sizeof(Fpos_t)));
#  endif
	    }
#endif
	}
	else {
	    errno = EINVAL;
	    ST(0) = &PL_sv_undef;
	}

SysRet
fsetpos(handle, pos)
	InputStream	handle
	SV *		pos
    CODE:
	if (handle) {
#ifdef PerlIO
#if PERL_VERSION < 8
	    char *p;
	    STRLEN len;
	    if (SvOK(pos) && (p = SvPV(pos,len)) && len == sizeof(Fpos_t)) {
		RETVAL = PerlIO_setpos(handle, (Fpos_t*)p);
	    }
	    else {
		RETVAL = -1;
		errno = EINVAL;
	    }
#else
	    RETVAL = PerlIO_setpos(handle, pos);
#endif
#else
	    char *p;
	    STRLEN len;
	    if ((p = SvPV(pos,len)) && len == sizeof(Fpos_t)) {
		RETVAL = fsetpos(handle, (Fpos_t*)p);
	    }
	    else {
		RETVAL = -1;
		errno = EINVAL;
	    }
#endif
	}
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

MODULE = IO	PACKAGE = IO::File	PREFIX = f

void
new_tmpfile(packname = "IO::File")
    const char * packname
    PREINIT:
	OutputStream fp;
	GV *gv;
    CODE:
#ifdef PerlIO
	fp = PerlIO_tmpfile();
#else
	fp = tmpfile();
#endif
	gv = (GV*)SvREFCNT_inc(newGVgen(packname));
	if (gv)
	    (void) hv_delete(GvSTASH(gv), GvNAME(gv), GvNAMELEN(gv), G_DISCARD);
	if (gv && do_open(gv, "+>&", 3, FALSE, 0, 0, fp)) {
	    ST(0) = sv_2mortal(newRV((SV*)gv));
	    sv_bless(ST(0), gv_stashpv(packname, TRUE));
	    SvREFCNT_dec(gv);   /* undo increment in newRV() */
	}
	else {
	    ST(0) = &PL_sv_undef;
	    SvREFCNT_dec(gv);
	}

MODULE = IO	PACKAGE = IO::Poll

void
_poll(timeout,...)
	int timeout;
PPCODE:
{
#ifdef HAS_POLL
    const int nfd = (items - 1) / 2;
    SV *tmpsv = sv_2mortal(NEWSV(999,nfd * sizeof(struct pollfd)));
    /* We should pass _some_ valid pointer even if nfd is zero, but it
     * doesn't matter what it is, since we're telling it to not check any fds.
     */
    struct pollfd *fds = nfd ? (struct pollfd *)SvPVX(tmpsv) : (struct pollfd *)tmpsv;
    int i,j,ret;
    for(i=1, j=0  ; j < nfd ; j++) {
	fds[j].fd = SvIV(ST(i));
	i++;
	fds[j].events = (short)SvIV(ST(i));
	i++;
	fds[j].revents = 0;
    }
    if((ret = poll(fds,nfd,timeout)) >= 0) {
	for(i=1, j=0 ; j < nfd ; j++) {
	    sv_setiv(ST(i), fds[j].fd); i++;
	    sv_setiv(ST(i), fds[j].revents); i++;
	}
    }
    XSRETURN_IV(ret);
#else
	not_here("IO::Poll::poll");
#endif
}

MODULE = IO	PACKAGE = IO::Handle	PREFIX = io_

void
io_blocking(handle,blk=-1)
	InputStream	handle
	int		blk
PROTOTYPE: $;$
CODE:
{
    const int ret = io_blocking(aTHX_ handle, items == 1 ? -1 : blk ? 1 : 0);
    if(ret >= 0)
	XSRETURN_IV(ret);
    else
	XSRETURN_UNDEF;
}

MODULE = IO	PACKAGE = IO::Handle	PREFIX = f

int
ungetc(handle, c)
	InputStream	handle
	SV *	        c
    CODE:
	if (handle) {
#ifdef PerlIO
            UV v;

            if ((SvIOK_notUV(c) && SvIV(c) < 0) || (SvNOK(c) && SvNV(c) < 0.0))
                croak("Negative character number in ungetc()");

            v = SvUV(c);
            if (UVCHR_IS_INVARIANT(v) || (v <= 0xFF && !PerlIO_isutf8(handle)))
                RETVAL = PerlIO_ungetc(handle, (int)v);
            else {
                U8 buf[UTF8_MAXBYTES + 1], *end;
                Size_t len;

                if (!PerlIO_isutf8(handle))
                    croak("Wide character number in ungetc()");

                /* This doesn't warn for non-chars, surrogate, and
                 * above-Unicodes */
                end = uvchr_to_utf8_flags(buf, v, 0);
                len = end - buf;
                if ((Size_t)PerlIO_unread(handle, &buf, len) == len)
                    XSRETURN_UV(v);
                else
                    RETVAL = EOF;
            }
#else
            RETVAL = ungetc((int)SvIV(c), handle);
#endif
        }
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

int
ferror(handle)
	InputStream	handle
    CODE:
	if (handle)
#ifdef PerlIO
	    RETVAL = PerlIO_error(handle);
#else
	    RETVAL = ferror(handle);
#endif
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

int
clearerr(handle)
	InputStream	handle
    CODE:
	if (handle) {
#ifdef PerlIO
	    PerlIO_clearerr(handle);
#else
	    clearerr(handle);
#endif
	    RETVAL = 0;
	}
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

int
untaint(handle)
       SV *	handle
    CODE:
#ifdef IOf_UNTAINT
	IO * io;
	io = sv_2io(handle);
	if (io) {
	    IoFLAGS(io) |= IOf_UNTAINT;
	    RETVAL = 0;
	}
        else {
#endif
	    RETVAL = -1;
	    errno = EINVAL;
#ifdef IOf_UNTAINT
	}
#endif
    OUTPUT:
	RETVAL

SysRet
fflush(handle)
	OutputStream	handle
    CODE:
	if (handle)
#ifdef PerlIO
	    RETVAL = PerlIO_flush(handle);
#else
	    RETVAL = Fflush(handle);
#endif
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

void
setbuf(handle, ...)
	OutputStream	handle
    CODE:
	if (handle)
#ifdef PERLIO_IS_STDIO
        {
	    char *buf = items == 2 && SvPOK(ST(1)) ?
	      sv_grow(ST(1), BUFSIZ) : 0;
	    setbuf(handle, buf);
	}
#else
	    not_here("IO::Handle::setbuf");
#endif

SysRet
setvbuf(...)
    CODE:
	if (items != 4)
            Perl_croak(aTHX_ "Usage: IO::Handle::setvbuf(handle, buf, type, size)");
#if defined(PERLIO_IS_STDIO) && defined(_IOFBF) && defined(HAS_SETVBUF)
    {
        OutputStream	handle = 0;
	char *		buf = SvPOK(ST(1)) ? sv_grow(ST(1), SvIV(ST(3))) : 0;
	int		type;
	int		size;

	if (items == 4) {
	    handle = IoOFP(sv_2io(ST(0)));
	    buf    = SvPOK(ST(1)) ? sv_grow(ST(1), SvIV(ST(3))) : 0;
	    type   = (int)SvIV(ST(2));
	    size   = (int)SvIV(ST(3));
	}
	if (!handle)			/* Try input stream. */
	    handle = IoIFP(sv_2io(ST(0)));
	if (items == 4 && handle)
	    RETVAL = setvbuf(handle, buf, type, size);
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    }
#else
	RETVAL = (SysRet) not_here("IO::Handle::setvbuf");
#endif
    OUTPUT:
	RETVAL


SysRet
fsync(arg)
	SV * arg
    PREINIT:
	OutputStream handle = NULL;
    CODE:
#ifdef HAS_FSYNC
	handle = IoOFP(sv_2io(arg));
	if (!handle)
	    handle = IoIFP(sv_2io(arg));
	if (handle) {
	    int fd = PerlIO_fileno(handle);
	    if (fd >= 0) {
		RETVAL = fsync(fd);
	    } else {
		RETVAL = -1;
		errno = EBADF;
	    }
	} else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
#else
	RETVAL = (SysRet) not_here("IO::Handle::sync");
#endif
    OUTPUT:
	RETVAL

SV *
_create_getline_subs(const char *code)
    CODE:
	OP *(*io_old_ck_lineseq)(pTHX_ OP *) = PL_check[OP_LINESEQ];
	PL_check[OP_LINESEQ] = io_ck_lineseq;
	RETVAL = SvREFCNT_inc(eval_pv(code,FALSE));
	PL_check[OP_LINESEQ] = io_old_ck_lineseq;
    OUTPUT:
	RETVAL


MODULE = IO	PACKAGE = IO::Socket

SysRet
sockatmark (sock)
   InputStream sock
   PROTOTYPE: $
   PREINIT:
     int fd;
   CODE:
     fd = PerlIO_fileno(sock);
     if (fd < 0) {
       errno = EBADF;
       RETVAL = -1;
     }
#ifdef HAS_SOCKATMARK
     else {
       RETVAL = sockatmark(fd);
     }
#else
     else {
       int flag = 0;
#   ifdef SIOCATMARK
#     if defined(NETWARE) || defined(WIN32)
       if (ioctl(fd, SIOCATMARK, (char*)&flag) != 0)
#     else
       if (ioctl(fd, SIOCATMARK, &flag) != 0)
#     endif
	 XSRETURN_UNDEF;
#   else
       not_here("IO::Socket::atmark");
#   endif
       RETVAL = flag;
     }
#endif
   OUTPUT:
     RETVAL

BOOT:
{
    HV *stash;
    /*
     * constant subs for IO::Poll
     */
    stash = gv_stashpvn("IO::Poll", 8, TRUE);
#ifdef	POLLIN
	newCONSTSUB(stash,"POLLIN",newSViv(POLLIN));
#endif
#ifdef	POLLPRI
        newCONSTSUB(stash,"POLLPRI", newSViv(POLLPRI));
#endif
#ifdef	POLLOUT
        newCONSTSUB(stash,"POLLOUT", newSViv(POLLOUT));
#endif
#ifdef	POLLRDNORM
        newCONSTSUB(stash,"POLLRDNORM", newSViv(POLLRDNORM));
#endif
#ifdef	POLLWRNORM
        newCONSTSUB(stash,"POLLWRNORM", newSViv(POLLWRNORM));
#endif
#ifdef	POLLRDBAND
        newCONSTSUB(stash,"POLLRDBAND", newSViv(POLLRDBAND));
#endif
#ifdef	POLLWRBAND
        newCONSTSUB(stash,"POLLWRBAND", newSViv(POLLWRBAND));
#endif
#ifdef	POLLNORM
        newCONSTSUB(stash,"POLLNORM", newSViv(POLLNORM));
#endif
#ifdef	POLLERR
        newCONSTSUB(stash,"POLLERR", newSViv(POLLERR));
#endif
#ifdef	POLLHUP
        newCONSTSUB(stash,"POLLHUP", newSViv(POLLHUP));
#endif
#ifdef	POLLNVAL
        newCONSTSUB(stash,"POLLNVAL", newSViv(POLLNVAL));
#endif
    /*
     * constant subs for IO::Handle
     */
    stash = gv_stashpvn("IO::Handle", 10, TRUE);
#ifdef _IOFBF
        newCONSTSUB(stash,"_IOFBF", newSViv(_IOFBF));
#endif
#ifdef _IOLBF
        newCONSTSUB(stash,"_IOLBF", newSViv(_IOLBF));
#endif
#ifdef _IONBF
        newCONSTSUB(stash,"_IONBF", newSViv(_IONBF));
#endif
#ifdef SEEK_SET
        newCONSTSUB(stash,"SEEK_SET", newSViv(SEEK_SET));
#endif
#ifdef SEEK_CUR
        newCONSTSUB(stash,"SEEK_CUR", newSViv(SEEK_CUR));
#endif
#ifdef SEEK_END
        newCONSTSUB(stash,"SEEK_END", newSViv(SEEK_END));
#endif
}

