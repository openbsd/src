#	from: @(#)Makefile	5.11 (Berkeley) 3/12/91
#	$Id: Makefile,v 1.2 1996/02/03 12:12:07 dm Exp $

PROG=	rdist
CFLAGS+=-I. -DOS_H=\"os-openbsd.h\"
SRCS=	child.c client.c common.c distopt.c docmd.c expand.c isexec.c \
		lookup.c message.c rdist.c rshrcmd.c setargs.c signal.c
OBJS+=	gram.o
CLEANFILES=y.tab.h

LDADD=	-lcompat
DPADD=	${LIBCOMPAT}

.include <bsd.prog.mk>
