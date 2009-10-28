#	$OpenBSD: Makefile,v 1.13 2009/10/28 00:04:26 deraadt Exp $
#	$NetBSD: Makefile,v 1.15 1995/09/07 07:17:53 jtc Exp $

LIB=	compat
WANTLINT=
AINC=	-I${.CURDIR}/../libc/arch/${MACHINE_ARCH}
.if defined(DESTDIR)
AINC+=	-nostdinc -idirafter ${DESTDIR}/usr/include
.endif
NOPIC=	nopic

.PATH:	${.CURDIR}/4.1/${MACHINE_ARCH} ${.CURDIR}/4.1 \
	${.CURDIR}/4.3/${MACHINE_ARCH} ${.CURDIR}/4.3 \
	${.CURDIR}/4.4/${MACHINE_ARCH} ${.CURDIR}/4.4 \
	${.CURDIR}/regexp

# compat 4.1 sources
SRCS=	gtty.c ftime.c stty.c

.if (${MACHINE} == "vax")
# missing: (for vax) reset.s
.endif

MAN+=	ftime.3 stty.3

MLINKS+=stty.3 gtty.3

# compat 4.3 sources
# missing: sibuf.c sobuf.c strout.c
SRCS+=	regex.c rexec.c
SRCS+=	setrgid.c setruid.c

MAN+=	re_comp.3 rexec.3 setruid.3

MLINKS+=re_comp.3 re_exec.3
MLINKS+=setruid.3 setrgid.3

# compat 4.4 sources
SRCS+=	cuserid.c
MAN+=	cuserid.3


# regexp sources
SRCS+=	regerror.c regexp.c regsub.c
HDRS+=	regexp/regexp.h
MAN+=	regexp.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
