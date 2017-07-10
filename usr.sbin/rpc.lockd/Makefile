#	$OpenBSD: Makefile,v 1.7 2017/07/10 21:30:37 espie Exp $

PROG =	rpc.lockd
SRCS =	nlm_prot_svc.c lockd.c lockd_lock.c procs.c
MAN =	rpc.lockd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CFLAGS+= -I. -DSYSLOG

CLEANFILES= nlm_prot_svc.c nlm_prot.h

RPCSRC= ${DESTDIR}/usr/include/rpcsvc/nlm_prot.x
RPCGEN= rpcgen -L -C

nlm_prot_svc.c: ${RPCSRC}
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

nlm_prot.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

BUILDFIRST = nlm_prot.h

test: test.c
	cc -o test test.c -lrpcsvc

.include <bsd.prog.mk>
