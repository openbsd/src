#	$OpenBSD: Makefile,v 1.6 2002/02/18 19:12:50 deraadt Exp $

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
	rm -f bootparam_prot.x
	ln -s ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x .
	rm -f bootparam_prot.h
	ln -s ${DESTDIR}/usr/include/rpcsvc/bootparam_prot.h .
	rpcgen -m -o $@ bootparam_prot.x

CLEANFILES += bootparam_prot_svc.c bootparam_prot.x bootparam_prot.h

.include <bsd.prog.mk>
