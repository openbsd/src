#	$OpenBSD: Makefile,v 1.24 2009/01/05 18:51:49 martynas Exp $

# Define SMALL to disable command line editing and https support
#CFLAGS+=-DSMALL

# Uncomment the following to provide defaults for gate-ftp operation
#CFLAGS+=-DGATE_SERVER=\"ftp-gw.host\" # -DGATE_PORT=21

.if (defined(USE_SOCKS) && ${USE_SOCKS:L} == "yes")
CFLAGS += -DSOCKS -I/usr/local/include
LDADD += -L/usr/local/lib -lsocks
.endif

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c cookie.c domacro.c fetch.c ftp.c list.c \
	main.c ruserpass.c stringlist.c util.c

CPPFLAGS+= -DINET6

LDADD+=	-ledit -lcurses -lutil -lssl -lcrypto
DPADD+=	${LIBEDIT} ${LIBCURSES} ${LIBUTIL}
LDSTATIC= ${STATIC}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
