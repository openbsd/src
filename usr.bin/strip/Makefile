#	$OpenBSD: Makefile,v 1.7 1998/05/11 07:41:23 niklas Exp $

TARGET_MACHINE_ARCH?=	${MACHINE_ARCH}

PROG=	strip

.if ${TARGET_MACHINE_ARCH} != ${MACHINE_ARCH}
# XXX should make this automatic
.if ${TARGET_MACHINE_ARCH} == "i386"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_I386
.elif ${TARGET_MACHINE_ARCH} == "m68k"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_M68K
.elif ${TARGET_MACHINE_ARCH} == "ns32k"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_NS32K
.elif ${TARGET_MACHINE_ARCH} == "sparc"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_SPARC
.elif ${TARGET_MACHINE_ARCH} == "vax"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_VAX
.endif
STRIP=	/usr/bin/strip

.elif ${HOSTCC} != ${CC}
STRIP=./hoststrip

realinstall: hoststrip

hoststrip: strip.c
	${HOSTCC} ${.CURDIR}/strip.c -o hoststrip
.else
STRIP=./strip
.endif

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 600 strip \
	    ${DESTDIR}${BINDIR}
	${STRIP} ${DESTDIR}${BINDIR}/strip
	chmod ${BINMODE} ${DESTDIR}${BINDIR}/strip
	-if [ "${INSTALL_COPY}" = "" ]; then rm -f ${STRIP}; fi

.include <bsd.prog.mk>
