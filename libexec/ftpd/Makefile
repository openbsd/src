#	$OpenBSD: Makefile,v 1.9 1997/03/26 00:34:34 deraadt Exp $
#	$NetBSD: Makefile,v 1.13 1996/02/16 02:07:41 cgd Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
.ifdef (${MACHINE} == "powerpc")
# bug in compiler causes ftpd.o to fail to compile with optimization.
CFLAGS= ${DEBUG}
.endif
CFLAGS+=-DHASSETPROCTITLE -Dunix
SRCS=	ftpd.c ftpcmd.c logutmp.c logwtmp.c popen.c
MAN=	ftpd.8
CLEANFILES+=ftpcmd.c y.tab.h
.PATH:	${.CURDIR}/../../usr.bin/ftp

.include <bsd.own.mk>

.if defined(SKEY)
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.if defined(KERBEROS)
SRCS+= klogin.c
.PATH:	${.CURDIR}/../../usr.bin/login
CFLAGS+= -DKERBEROS
LDADD+= -lkrb -ldes
DPADD+= ${LIBKRB} ${LIBKRB}
.endif

.include <bsd.prog.mk>
