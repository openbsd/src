#	$NetBSD: Makefile,v 1.10 1995/10/09 23:25:54 pk Exp $

SUBDIR= boot bootxx installboot

### find out what to use for libkern and libsa
SAREL=
KERNREL=
.include "Makefile.inc"
.include "$S/lib/libkern/Makefile.inc"
.include "$S/lib/libsa/Makefile.inc"

all: ${KERNLIB} ${SALIB} _SUBDIRUSE

libdep:
	@echo  ${KERNLIB} ${.OBJDIR}/${SALIB} 

sadep:
	@echo  ${.OBJDIR}/${SALIB}

kernlibdir:
	@echo  ${KERNDST}

salibdir:
	@echo  ${.OBJDIR}/${SADST}

clean:: _SUBDIRUSE

cleandir: _SUBDIRUSE

BINDIR=/usr/mdec

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/binstall.sh ${DESTDIR}${BINDIR}/binstall

.include <bsd.subdir.mk>
