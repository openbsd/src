#	$OpenBSD: Makefile,v 1.7 1996/09/07 03:14:26 millert Exp $
#
#	from: @(#)Makefile	5.11 (Berkeley) 3/12/91

PROG=	rdist
CFLAGS+=-I. -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c rshrcmd.c setargs.c signal.c
CLEANFILES+=gram.c y.tab.h

LDADD=	-lcompat
DPADD=	${LIBCOMPAT}

.include <bsd.prog.mk>

.depend: gram.c
