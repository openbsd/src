#	$NetBSD: Makefile,v 1.3 1998/06/16 19:06:31 is Exp $

NOMAN=notyet
PROG=dotest
SRCS=dotest.c dotestsubs.S
AFLAGS+=-I.
CPPFLAGS+=-I.
SPDIR=${.CURDIR}/../../../../../sys/arch/m68k/060sp
CLEANFILES+=itest.S ftest.S

.SUFFIXES: .S .sa

.sa.S: ; sh ${SPDIR}/asm2gas ${.IMPSRC} >${.TARGET}

.PATH: ${SPDIR}

depend dotestsubs.o: itest.S ftest.S

regress: ${PROG}
	@/usr/sbin/sysctl hw.model | grep -v -q 68060 || \
		./${PROG} | tee /dev/tty | cmp -s - ${.CURDIR}/goodoutput


.include <bsd.prog.mk>
