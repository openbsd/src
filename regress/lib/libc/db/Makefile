#	$NetBSD: Makefile,v 1.11 1995/12/12 01:54:15 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/4/93

PROG=	dbtest

# add -DSTATISTICS to CFLAGS to get usage statistics.  Note that
# for this to work, libc must be compiled with -DSTATISTICS as well
CFLAGS+= -g -D__DBINTERFACE_PRIVATE -DDEBUG
NOMAN=  noman
CLEANFILES+= t1 t2 t3

install:

regress:
	sh ${.CURDIR}/run.test

.include <bsd.prog.mk>
