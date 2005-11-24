#	$OpenBSD: Makefile,v 1.9 2005/11/24 20:49:20 deraadt Exp $


RPCSRCS=	bootparam_prot.x klm_prot.x mount.x nfs_prot.x \
		nlm_prot.x rex.x rnusers.x rusers.x rquota.x rstat.x rwall.x \
		sm_inter.x spray.x yp.x yppasswd.x
SRCS=		${RPCSRCS:R:S/$/.c/g}
HDRS=           ${RPCSRCS:R:S/$/.h/g}
CLEANFILES+=	${SRCS} ${HDRS}

LIB=		rpcsvc
WANTLINT=
NOMAN=

all:		${HDRS}
beforedepend:	${HDRS}

includes:	${HDRS}
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		${DESTDIR}/usr/include/rpcsvc
	@echo installing rpc .h and .x files
	@(cd ${.CURDIR}; for i in $(RPCSRCS); do \
	    cmp -s $(.CURDIR)/$$i ${DESTDIR}/usr/include/rpcsvc/$$i || \
		{ j="${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include/rpcsvc"; \
		echo $$j; $$j; } \
	done)
	@sleep 1; for i in $(HDRS); do \
	    cmp -s $$i ${DESTDIR}/usr/include/rpcsvc/$$i || \
		{ j="${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include/rpcsvc"; \
		echo $$j; $$j; } \
	done

# new suffixes have to go afterwards, because bsd.lib.mk clears them
.include <bsd.lib.mk>
.SUFFIXES: .x .c .h

.x.c:
	rpcgen -C -c ${.IMPSRC} -o ${.TARGET}

.x.h:
	rpcgen -C -h ${.IMPSRC} -o ${.TARGET}
