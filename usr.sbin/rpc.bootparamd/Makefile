#	$OpenBSD: Makefile,v 1.10 2003/07/21 20:10:04 mickey Exp $

.include <bsd.own.mk>

.if (${YP:L} == "yes")
CFLAGS+=-DYP
.endif

.if (${MACHINE_ARCH} == vax)
bootparam_prot_svc.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -O0 -c $<
.endif

PROG=	rpc.bootparamd
SRCS=	bootparamd.c bootparam_prot_svc.c
MAN=	bootparams.5 rpc.bootparamd.8
MLINKS=	rpc.bootparamd.8 bootparamd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

bootparam_prot_svc.c: ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x
	ln -fs ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.h .
	rpcgen -C -m -o $@ $>

CLEANFILES += bootparam_prot_svc.c bootparam_prot.h

.include <bsd.prog.mk>
