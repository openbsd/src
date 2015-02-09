#	$OpenBSD: Makefile,v 1.28 2015/02/09 08:24:20 tedu Exp $

# Define SMALL to disable command line editing and https support
#CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c cookie.c domacro.c fetch.c ftp.c \
	list.c main.c ruserpass.c small.c stringlist.c util.c

CPPFLAGS+= -DINET6

LDADD+=	-ledit -lcurses -lutil -ltls -lssl -lcrypto
DPADD+=	${LIBEDIT} ${LIBCURSES} ${LIBUTIL} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}
LDSTATIC= ${STATIC}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
