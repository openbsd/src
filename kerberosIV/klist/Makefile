#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.3 1998/05/18 01:24:53 art Exp $

PROG=	klist
SRCS=	klist.c
.PATH:	${.CURDIR}/../roken
SRCS+=	parse_time.c parse_units.c
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes -lkafs
BINDIR=	/usr/bin
CFLAGS+= -I${.CURDIR}/../roken

.include <bsd.prog.mk>
