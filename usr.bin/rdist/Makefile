#	from: @(#)Makefile	5.11 (Berkeley) 3/12/91
#	$Id: Makefile,v 1.4 1996/03/01 08:43:10 tholo Exp $

PROG=	rdist
CFLAGS+=-I. -DOS_H=\"os-openbsd.h\"
SRCS=	child.c client.c common.c distopt.c docmd.c expand.c gram.y \
	isexec.c lookup.c message.c rdist.c rshrcmd.c setargs.c signal.c
CLEANFILES+=gram.c y.tab.h

LDADD=	-lcompat
DPADD=	${LIBCOMPAT}

.include <bsd.prog.mk>

.depend: gram.c
