#	from @(#)Makefile	8.1 (Berkeley) 6/1/93

PROG=	kdestroy
SRCS=	kdestroy.c
.PATH:  ${.CURDIR}/../roken
SRCS+=	getarg.c get_window_size.c
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes -lkafs
BINDIR=	/usr/bin
CFLAGS+= -I${.CURDIR}/../roken

.include <bsd.prog.mk>
