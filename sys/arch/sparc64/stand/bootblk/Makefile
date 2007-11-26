#	$OpenBSD: Makefile,v 1.7 2007/11/26 10:26:03 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 2000/08/20 14:58:45 mrg Exp $

CURDIR=	${.CURDIR}
S=	${CURDIR}/../../../..

#
# Override normal settings
#

CLEANFILES=	assym.fth.h assym.fth.h.tmp machine sparc \
		bootblk bootblk.text bootblk.text.tmp

NOMAN=
STRIPFLAG=

# deal with Solaris vs. NetBSD build environments for now ..
OS!=uname -s
.if (${OS} == "OpenBSD")
USE_GENASSYM?=	no
SVR4=
.else
USE_GENASSYM?=	yes
SVR4=-U__SVR4 -U__svr4__ -D__NetBSD__
.endif

INCLUDES=	-I. -I$S/arch -I$S -nostdinc
CPPFLAGS=	${INCLUDES} ${IDENT} ${PARAM} -D_LKM ${SVR4}

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
.endif

all: bootblk.text bootblk

sparc:
	ln -s ${.CURDIR}/../../../sparc/include sparc

assym.fth.h: ${.CURDIR}/genassym.sh genfth.cf
	sh ${.CURDIR}/genassym.sh ${CC} ${CFLAGS} \
	    ${CPPFLAGS} ${PROF} <${.CURDIR}/genfth.cf >assym.fth.h.tmp && \
	    mv -f assym.fth.h.tmp assym.fth.h

bootblk.text: bootblk.fth assym.fth.h
	awk '/fload/ { print "#include \"" $$2 "\"" }; !/fload/' \
	    ${.CURDIR}/bootblk.fth | /usr/bin/cpp -traditional -P >bootblk.text.tmp && \
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
