#	$OpenBSD: Makefile,v 1.5 2000/09/03 18:41:22 espie Exp $

.include <bsd.own.mk>

.if (${YP:L} == "yes")
CFLAGS+=-DYP
.endif

PROG=	rpc.bootparamd
SRCS=	bootparamd.c bootparam_prot_svc.c
MAN=	bootparams.5 rpc.bootparamd.8
MLINKS=	rpc.bootparamd.8 bootparamd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

bootparam_prot_svc.c: ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x
	rm -f bootparam_prot.x
	ln -s ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x .
	rm -f bootparam_prot.h
	ln -s ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.h .
	rpcgen -m -o $@ bootparam_prot.x

CLEANFILES += bootparam_prot_svc.c bootparam_prot.x bootparam_prot.h

.include <bsd.prog.mk>
