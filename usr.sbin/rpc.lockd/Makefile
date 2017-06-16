#	$OpenBSD: Makefile,v 1.6 2017/06/16 10:25:54 espie Exp $

PROG =	rpc.lockd
SRCS =	nlm_prot_svc.c lockd.c lockd_lock.c procs.c
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

BUILDFIRST = nlm_prot.h

test: test.c
	cc -o test test.c -lrpcsvc

.include <bsd.prog.mk>
