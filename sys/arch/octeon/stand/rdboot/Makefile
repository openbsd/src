#	$OpenBSD: Makefile,v 1.2 2019/07/28 15:45:30 visa Exp $

NOMAN=

.if ${MACHINE} == "octeon"
PROG=		rdboot
SRCS=		cmd.c disk.c rdboot.c vars.c
LDADD+=		-lutil
LDSTATIC+=	-static

realinstall:
	cd ${.CURDIR}/../../../../../distrib/octeon/boot && ${MAKE} bsd.rd
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/../../../../../distrib/octeon/boot/obj/bsd.rd \
	    ${DESTDIR}/usr/mdec/boot
.else
NOPROG=
.endif

.include <bsd.prog.mk>
