#	$OpenBSD: Makefile,v 1.27 2015/12/01 22:04:12 deraadt Exp $
#	$NetBSD: Makefile,v 1.13 1996/02/16 02:07:41 cgd Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
SRCS=	ftpd.c ftpcmd.y logutmp.c logwtmp.c monitor.c monitor_fdpass.c popen.c
MAN=	ftpd.8
YFLAGS=
CLEANFILES+=y.tab.c

.include <bsd.own.mk>

# our internal version of ls.

LSDIR	= ${.CURDIR}/../../bin/ls
.PATH:	${LSDIR}
SRCS	+= ls.c cmp.c print.c util.c utf8.c
CFLAGS	+= -I${.CURDIR} -I${LSDIR}

# not really used
CPPFLAGS+=-DINET6

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
