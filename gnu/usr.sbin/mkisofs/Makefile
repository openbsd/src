#	$OpenBSD: Makefile,v 1.2 1997/09/17 03:29:31 downsj Exp $

PROG=	mkisofs
MAN=	mkisofs.8

SRCS=	mkisofs.c tree.c write.c hash.c rock.c exclude.c multi.c \
	files.c fnmatch.c match.c name.c eltorito.c

CFLAGS+=-DSYSTEM_ID_DEFAULT=\"OpenBSD\" -I${.CURDIR}

# XXX: alpha gcc bug
write.o: write.c
	${CC} ${CFLAGS} -O0 -c $<

.include <bsd.prog.mk>
