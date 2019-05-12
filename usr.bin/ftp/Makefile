# $OpenBSD: Makefile,v 1.32 2019/05/12 20:58:19 jasper Exp $

# Define SMALL to disable command line editing
#CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	cmd.c file.c ftp.c http.c main.c progressmeter.c url.c util.c xmalloc.c

LDADD+=	-ledit -lcurses -lutil -ltls -lssl -lcrypto
DPADD+=	${LIBEDIT} ${LIBCURSES} ${LIBUTIL} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
