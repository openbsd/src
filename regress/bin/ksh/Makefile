# $OpenBSD: Makefile,v 1.9 2016/01/12 09:00:39 schwarze Exp $

SUBDIR += vi

REGRESS_TARGETS=check
KSH=/bin/ksh
PERL=/usr/bin/perl
CATEGORIES=pdksh,sh,ksh,posix,posix-upu

# Avoid /tmp for test temp files if we have an obj dir
.if ${.CURDIR} != ${.OBJDIR}
TEMPDIR=${.OBJDIR}
.else
TEMPDIR=/tmp
.endif

check test:
	${PERL} ${.CURDIR}/th -s ${.CURDIR} -p ${KSH} -C ${CATEGORIES} -T ${TEMPDIR}

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
