#	$Id: Makefile,v 1.1 2007/12/05 09:22:44 reyk Exp $

.PATH:		${.CURDIR}/../snmpd

PROG=		snmpctl
SRCS=		buffer.c imsg.c log.c snmpctl.c parser.c

MAN=		snmpctl.8

CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../snmpd
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

.include <bsd.prog.mk>
