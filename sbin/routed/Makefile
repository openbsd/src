#	$OpenBSD: Makefile,v 1.9 1997/11/18 21:31:50 kstailey Exp $

PROG=	routed
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=	routed.8
CFLAGS+=-D_ROUTED -D'min(a,b)=(a < b ? a : b)' -D'log=syslog'
CFLAGS+=-D'panic(s)={log(LOG_ERR,s); exit(1);}'
CFLAGS+=-D'random()=arc4random()'
SUBDIR=	rtquery
DPADD=	${LIBCOMPAT}
LDADD=	-lcompat

# Do *NOT* turn this flag on unless you like security holes
#CFLAGS+=-DPERMIT_TRACE

.PATH: ${.CURDIR}/../../sys/net

.include <bsd.prog.mk>
