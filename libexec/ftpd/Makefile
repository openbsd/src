#	$OpenBSD: Makefile,v 1.19 2001/05/29 21:35:16 millert Exp $
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
SRCS	+= ls.c cmp.c print.c util.c
CFLAGS	+= -I${LSDIR}

# not really used
CPPFLAGS+=-DINET6

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+=-DTCPWRAPPERS
LDADD+=	-lwrap
DPADD+=	${LIBWRAP}
.endif

.include <bsd.prog.mk>
