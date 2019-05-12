# Define SMALL to disable command line editing
#CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	cmd.c file.c ftp.c http.c main.c progressmeter.c url.c util.c xmalloc.c

LDADD+=	-ledit -lcurses -lutil -ltls -lssl -lcrypto
DPADD+=	${LIBEDIT} ${LIBCURSES} ${LIBUTIL} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
