#	$OpenBSD: Makefile,v 1.14 1999/02/26 00:15:54 art Exp $
#	$NetBSD: Makefile,v 1.13 1996/02/16 02:07:41 cgd Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
CFLAGS+=-DHASSETPROCTITLE -Dunix
SRCS=	ftpd.c ftpcmd.c logutmp.c logwtmp.c popen.c
MAN=	ftpd.8
CLEANFILES+=ftpcmd.c y.tab.h

.include <bsd.own.mk>

# our internal version of ls.

LSDIR	= ${.CURDIR}/../../bin/ls
.PATH:	${LSDIR}
SRCS	+= ls.c cmp.c print.c stat_flags.c util.c
CFLAGS	+= -I${LSDIR}

.if (${SKEY} == "yes")
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.if (${KERBEROS} == "yes")
SRCS+= klogin.c
.PATH:	${.CURDIR}/../../usr.bin/login
CFLAGS+= -DKERBEROS
LDADD+= -lkrb -ldes -lkafs
DPADD+= ${LIBKRB} ${LIBKRB} ${LIBKAFS}
.endif

.if (${TCP_WRAPPERS} == "yes")
CFLAGS+=-DTCPWRAPPERS
LDADD+=	-lwrap
DPADD+=	${LIBWRAP}
.endif

.include <bsd.prog.mk>
