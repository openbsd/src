#	$OpenBSD: Makefile,v 1.11 2013/10/30 00:43:46 guenther Exp $
#	$NetBSD: Makefile,v 1.1 2000/08/20 14:58:45 mrg Exp $

CURDIR=	${.CURDIR}
S=	${CURDIR}/../../../..

#
# Override normal settings
#

CLEANFILES=	assym.fth.h assym.fth.h.tmp machine \
		bootblk bootblk.text bootblk.text.tmp

NOMAN=
STRIPFLAG=

INCLUDES=	-I. -I$S/arch -I$S -nostdinc
CPPFLAGS=	${INCLUDES} ${IDENT} ${PARAM}

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
.endif

all: bootblk.text bootblk

assym.fth.h: ${.CURDIR}/genassym.sh genfth.cf
	sh ${.CURDIR}/genassym.sh ${CC} ${CFLAGS} \
	    ${CPPFLAGS} ${PROF} <${.CURDIR}/genfth.cf >assym.fth.h.tmp && \
	    mv -f assym.fth.h.tmp assym.fth.h

bootblk.text: bootblk.fth assym.fth.h
	awk '/fload/ { file=$$2; while ((ret = getline <file) == 1) print; close(file); if (ret == -1) print "unable to read " file >"/dev/stderr"; next }; !/fload/' \
	    ${.CURDIR}/bootblk.fth >bootblk.text.tmp && \
	    mv -f bootblk.text.tmp bootblk.text

bootblk: bootblk.fth assym.fth.h
	fgen -o bootblk ${.CURDIR}/bootblk.fth

beforedepend:
	@touch .depend

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    bootblk ${DESTDIR}/usr/mdec

#
# The following are if you grab the fakeboot program from the Sun website
#

fake: bootblk bootblk.text
	../fakeboot/fakeboot -elf32 <bootblk >/bootblk
	../fakeboot/fakeboot -elf32 <bootblk.text >/bootblk.text

.include <bsd.prog.mk>
