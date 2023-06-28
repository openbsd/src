#	$OpenBSD: Makefile,v 1.37 2023/06/28 17:36:51 op Exp $

# Define SMALL to disable command line editing and some other features,
# NOSSL to disable https support.
#CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c cookie.c domacro.c fetch.c ftp.c \
	list.c main.c ruserpass.c small.c stringlist.c util.c

LDADD+=	-ledit -lcurses -ltls -lssl -lcrypto
DPADD+=	${LIBEDIT} ${LIBCURSES} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
