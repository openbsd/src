#	$OpenBSD: Makefile,v 1.24 2005/06/15 16:13:27 mickey Exp $
#	$NetBSD: Makefile,v 1.13 1996/02/16 02:07:41 cgd Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
CFLAGS+=-DHASSETPROCTITLE -Dunix
SRCS=	ftpd.c ftpcmd.y logutmp.c logwtmp.c monitor.c monitor_fdpass.c popen.c
MAN=	ftpd.8
YFLAGS=
CLEANFILES+=y.tab.c

.include <bsd.own.mk>

# our internal version of ls.

LSDIR	= ${.CURDIR}/../../bin/ls
.PATH:	${LSDIR}
SRCS	+= ls.c cmp.c print.c util.c
CFLAGS	+= -I${.CURDIR} -I${LSDIR}

# not really used
CPPFLAGS+=-DINET6

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+=-DTCPWRAPPERS
LDADD+=	-lwrap
DPADD+=	${LIBWRAP}
.endif

.include <bsd.prog.mk>
