#	$NetBSD: Makefile,v 1.3 1995/07/25 22:20:13 gwr Exp $

.include <bsd.own.mk>

PROG=	rpc.pcnfsd
MAN=	pcnfsd.8
MLINKS= pcnfsd.8 rpc.pcnfsd.8
CFLAGS += -DUSER_CACHE -DWTMP -I${.OBJDIR}

.if (${YP} == "yes")
CFLAGS+=-DUSE_YP
.endif

SRCS=	pcnfsd_v1.c pcnfsd_v2.c pcnfsd_misc.c \
	pcnfsd_cache.c pcnfsd_print.c pcnfsd_svc.c pcnfsd_xdr.c

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

pcnfsd_svc.c: pcnfsd.x
	rpcgen -s udp -s tcp -I ${.CURDIR}/pcnfsd.x -o $@

pcnfsd_xdr.c: pcnfsd.x
	rpcgen -c ${.CURDIR}/pcnfsd.x -o $@

pcnfsd.h: pcnfsd.x
	rpcgen -h ${.CURDIR}/pcnfsd.x -o $@

CLEANFILES += pcnfsd_svc.c pcnfsd_xdr.c pcnfsd.h
CLEANFILES += clnt.pcnfsd pcnfsd_test.o pcnfsd_test.ln pcnfsd_clnt.*

.include <bsd.prog.mk>

.depend ${OBJS}: pcnfsd.h
