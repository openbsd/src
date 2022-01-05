#	$OpenBSD: Makefile,v 1.4 2022/01/05 16:24:29 deraadt Exp $

NOPROG=
MAN=	fw_update.8
CC=	${HOSTCC}
CFLAGS+=-I /usr/src/sys/dev/pci/drm/amd/amdgpu \
	-I /usr/src/sys/dev/pci/drm/i915 \
	-I /usr/src/sys/dev/pci/drm/radeon
ALL=	firmware_patterns

PROG=patterns

firmware_patterns: patterns
	$(.OBJDIR)/patterns | sort | uniq > $@

realinstall:

all:	firmware_patterns

afterinstall: firmware_patterns
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/fw_update.sh ${DESTDIR}${BINDIR}/fw_update
	${INSTALL} ${INSTALL_COPY} -o ${SHAREOWN} -g ${SHAREGRP} \
		-m ${SHAREMODE} ${.OBJDIR}/firmware_patterns \
		${DESTDIR}${SHAREDIR}/misc/firmware_patterns

CLEANFILES+=firmware_patterns
.include <bsd.prog.mk>
