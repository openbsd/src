#	$OpenBSD: Makefile,v 1.13 2004/04/25 15:50:35 markus Exp $

PROG=	routed
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=	routed.8
CFLAGS+=-D_ROUTED -D'min(a,b)=(a < b ? a : b)' -D'log=syslog'
CFLAGS+=-D'panic(s)={log(LOG_ERR, "%s", s); exit(1);}'
SUBDIR=	rtquery

# Do *NOT* turn this flag on unless you like security holes
#CFLAGS+=-DPERMIT_TRACE
CFLAGS+=-DSMALL_KERNEL

.PATH: ${.CURDIR}/../../sys/net

.include <bsd.prog.mk>
