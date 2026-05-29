# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:30 robert Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

HDRS=	TargetLibraryInfo.inc

INCDIR=	/usr/include/llvm/Analysis

install includes: ${HDRS}
	${INSTALL} -d -m 755 ${DESTDIR}${INCDIR}
	@cd ${.OBJDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}${INCDIR}/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		-m 444 $$i ${DESTDIR}${INCDIR}"; \
	    echo $$j; \
	    eval "$$j"; \
	done

all: ${HDRS}

clean cleandir:
	rm -f ${HDRS}

TargetLibraryInfo.inc: ${LLVM_SRCS}/include/llvm/Analysis/TargetLibraryInfo.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-target-library-info \
		-I${LLVM_SRCS}/include -o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
