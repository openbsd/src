#	$OpenBSD: Makefile,v 1.9 1997/07/22 06:59:37 millert Exp $
#
#	from: @(#)Makefile	5.11 (Berkeley) 3/12/91

PROG=	rdist
CFLAGS+=-I. -DOS_H=\"os-openbsd.h\"
SRCS=	gram.y child.c client.c common.c distopt.c docmd.c expand.c \
	isexec.c lookup.c message.c rdist.c signal.c
CLEANFILES+=gram.c y.tab.h

LDADD=	-lcompat
DPADD=	${LIBCOMPAT}

.include <bsd.prog.mk>

.depend: gram.c
