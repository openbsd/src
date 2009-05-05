#	$OpenBSD: Makefile,v 1.5 2009/05/05 19:37:40 martynas Exp $
#	$NetBSD: Makefile,v 1.13 1997/08/18 10:20:13 lukem Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

# Define SMALL to disable command line editing
COPTS+=-Os
CFLAGS+=-DSMALL

# Uncomment the following to provide defaults for gate-ftp operation
#CFLAGS+=-DGATE_SERVER=\"ftp-gw.host\" # -DGATE_PORT=21

# Uncomment the following for socks5 support.
#CFLAGS += -DSOCKS -include /usr/local/include/socks.h
#LDADD += -lsocks5

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
MAN=
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lcurses -lutil
DPADD+=	${LIBCURSES} ${LIBUTIL}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
