#	$OpenBSD: Makefile,v 1.6 1996/07/19 22:03:56 millert Exp $
#
#	from: @(#)Makefile	5.11 (Berkeley) 3/12/91

PROG=	rdist
CFLAGS+=-I${.CURDIR} -I. -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c rshrcmd.c setargs.c signal.c
CLEANFILES+=gram.c y.tab.h

LDADD=	-lcompat
DPADD=	${LIBCOMPAT}

.include <bsd.prog.mk>

.depend: gram.c
