#	$OpenBSD: Makefile,v 1.4 1997/08/19 08:05:16 niklas Exp $

PROG =	rpc.lockd
SRCS =	nlm_prot_svc.c lockd.c procs.c
MAN =	rpc.lockd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CFLAGS+= -I. -DSYSLOG

CLEANFILES= nlm_prot_svc.c nlm_prot.h

RPCSRC= ${DESTDIR}/usr/include/rpcsvc/nlm_prot.x
RPCGEN= rpcgen -L -C

nlm_prot_svc.c: ${RPCSRC} nlm_prot.h
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

nlm_prot.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

test: test.c
	cc -o test test.c -lrpcsvc

.include <bsd.prog.mk>
