#	$OpenBSD: Makefile,v 1.3 1997/09/22 05:25:48 millert Exp $

PROG=	mkisofs
MAN=	mkisofs.8

SRCS=	mkisofs.c tree.c write.c hash.c rock.c exclude.c multi.c \
	files.c match.c name.c eltorito.c

CFLAGS+=-DSYSTEM_ID_DEFAULT=\"OpenBSD\" -I${.CURDIR}

# XXX: alpha gcc bug
write.o: write.c
	${CC} ${CFLAGS} -O0 -c $<

.include <bsd.prog.mk>
