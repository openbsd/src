#	$OpenBSD: Makefile,v 1.19 2002/11/08 03:30:17 fgsch Exp $

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

CPPFLAGS+= -DINET6

LDADD+=	-ledit -lcurses -lutil
DPADD+=	${LIBEDIT} ${LIBCURSES} ${LIBUTIL}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
