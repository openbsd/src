#	$OpenBSD: Makefile,v 1.12 2003/05/09 00:07:09 millert Exp $

PROG=	routed
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=	routed.8
CFLAGS+=-D_ROUTED -D'min(a,b)=(a < b ? a : b)' -D'log=syslog'
CFLAGS+=-D'panic(s)={log(LOG_ERR, "%s", s); exit(1);}'
SUBDIR=	rtquery

# Do *NOT* turn this flag on unless you like security holes
#CFLAGS+=-DPERMIT_TRACE

.PATH: ${.CURDIR}/../../sys/net

.include <bsd.prog.mk>
