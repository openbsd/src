#	$OpenBSD: Makefile,v 1.7 2009/06/04 05:46:49 halex Exp $
#	$NetBSD: Makefile,v 1.13 1997/08/18 10:20:13 lukem Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

# Define SMALL to disable command line editing
COPTS+=-Os
CFLAGS+=-DSMALL -DINET6

# Uncomment the following to provide defaults for gate-ftp operation
#CFLAGS+=-DGATE_SERVER=\"ftp-gw.host\" # -DGATE_PORT=21

# Uncomment the following for socks5 support.
#CFLAGS += -DSOCKS -I/usr/local/include
#LDADD += -L/usr/local/lib -lsocks

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
MAN=
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lcurses -lutil
DPADD+=	${LIBCURSES} ${LIBUTIL}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
