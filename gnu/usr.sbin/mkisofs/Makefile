#	$OpenBSD: Makefile,v 1.4 2001/11/22 21:04:14 espie Exp $

PROG=	mkisofs
MAN=	mkisofs.8

SRCS=	mkisofs.c tree.c write.c hash.c rock.c exclude.c multi.c \
	files.c match.c name.c eltorito.c

CFLAGS+=-DSYSTEM_ID_DEFAULT=\"OpenBSD\" -I${.CURDIR}

# XXX: alpha gcc bug
write.o: write.c
	${CC} ${CFLAGS} -O0 -c ${.CURDIR}/write.c

.include <bsd.prog.mk>
