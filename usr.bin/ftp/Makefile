#	$OpenBSD: Makefile,v 1.17 2000/09/03 18:41:23 espie Exp $

# Define SMALL to disable command line editing
#CFLAGS+=-DSMALL

# Uncomment the following to provide defaults for gate-ftp operation
#CFLAGS+=-DGATE_SERVER=\"ftp-gw.host\" # -DGATE_PORT=21

.if (defined(USE_SOCKS) && ${USE_SOCKS:L} == "yes")
CFLAGS += -DSOCKS -I/usr/local/include
LDADD += -lsocks5
.endif

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c ruserpass.c \
	stringlist.c util.c

# not really used at this moment
CPPFLAGS+= -DINET6

LDADD+=	-ledit -lcurses
DPADD+=	${LIBEDIT} ${LIBCURSES}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
