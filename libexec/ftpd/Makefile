#	$NetBSD: Makefile,v 1.12 1995/04/11 02:44:45 cgd Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
CFLAGS+=-DHASSETPROCTITLE
SRCS=	ftpd.c ftpcmd.c logwtmp.c popen.c
MAN=	ftpd.8
CLEANFILES+=ftpcmd.c y.tab.h
.PATH:	${.CURDIR}/../../usr.bin/ftp

#LDADD+= -lcrypt
#DPADD+= ${LIBCRYPT}

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
