#	$OpenBSD: Makefile,v 1.26 1998/03/18 15:51:04 mickey Exp $

#
# For more information on building in tricky environments, please see
# the list of possible environment variables described in
# /usr/share/mk/bsd.README.
# 
# Building recommendations:
# 
# 1) If at all possible, put this source tree in /usr/src.  If /usr/src
# must be a symbolic link, setenv BSDSRCDIR to point to the real location.
#
# 2) It is also recommended that you compile with objects outside the
# source tree. To do this, ensure /usr/obj exists or points to some
# area of disk of sufficient size.  Then do "cd /usr/src; make obj".
# This will make a symbolic link called "obj" in each directory, as
# well as populate the /usr/obj properly with directories for the
# objects.
#
# 3) If you are reasonably sure that things will compile OK, use the
# "make build" target supplied here. Good luck.

.include <bsd.own.mk>	# for NOMAN, if it's there.

SUBDIR+= lib include bin libexec sbin usr.bin usr.sbin share games
SUBDIR+= gnu

SUBDIR+= sys lkm

.if (${KERBEROS} == "yes")
SUBDIR+= kerberosIV
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= distrib
.endif

.if exists(regress)
.ifmake !(install)
SUBDIR+= regress
.endif

regression-tests:
	@echo Running regression tests...
	@(cd ${.CURDIR}/regress && ${MAKE} regress)
.endif

includes:
	(cd ${.CURDIR}/include; ${MAKE} includes)	

beforeinstall:
.ifndef DESTDIR
	(cd ${.CURDIR}/etc && ${MAKE} DESTDIR=/ distrib-dirs)
.else
	(cd ${.CURDIR}/etc && ${MAKE} distrib-dirs)
.endif
	(cd ${.CURDIR}/include; ${MAKE} includes)

afterinstall:
.ifndef NOMAN
	(cd ${.CURDIR}/share/man && ${MAKE} makedb)
.endif

build:
.ifdef GLOBAL_AUTOCONF_CACHE
	rm -f ${GLOBAL_AUTOCONF_CACHE}
.endif
	(cd ${.CURDIR}/share/mk && ${MAKE} install)
	(cd ${.CURDIR}/include; ${MAKE} includes)
	${MAKE} cleandir
	(cd ${.CURDIR}/lib && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/gnu/lib && ${MAKE} depend && ${MAKE} && ${MAKE} install)
.if (${MACHINE_ARCH} == "mips")
	ldconfig
.endif
.if (${KERBEROS} == "yes")
	(cd ${.CURDIR}/kerberosIV && ${MAKE} build)
.endif
.if (${MACHINE_ARCH} == "mips")
	ldconfig
.endif
	${MAKE} depend && ${MAKE} && ${MAKE} install

.if !defined(TARGET)
cross-tools:
	echo "TARGET must be set"; exit 1
.else
cross-tools:	cross-helpers cross-includes cross-binutils cross-gcc

CROSSDIR=	${DESTDIR}/usr/cross/${TARGET}

cross-helpers:
	-mkdir -p ${CROSSDIR}/usr/include
	echo _MACHINE_ARCH | \
	    cat ${.CURDIR}/sys/arch/${TARGET}/include/param.h - | \
	    ${CPP} -E -I${.CURDIR}/sys/arch | \
	    sed -n '$$p' >${CROSSDIR}/TARGET_ARCH
	eval `grep '^osr=' sys/conf/newvers.sh`; \
	   sed "s/\$$/-unknown-openbsd$$osr/" ${CROSSDIR}/TARGET_ARCH > \
	   ${CROSSDIR}/TARGET_CANON

cross-includes:
	${MAKE} MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    DESTDIR=${CROSSDIR} includes

cross-binutils:
	-mkdir -p ${CROSSDIR}/usr/obj
	export BSDSRCDIR=`pwd`; \
	    (cd ${.CURDIR}/gnu/usr.bin/binutils; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} -f Makefile.bsd-wrapper obj); \
	    (cd ${CROSSDIR}/usr/obj/gnu/usr.bin/binutils; \
	    /bin/sh ${BSDSRCDIR}/gnu/usr.bin/binutils/configure \
	    --prefix ${CROSSDIR}/usr \
	    --target `cat ${CROSSDIR}/TARGET_CANON` && \
	    ${MAKE} && ${MAKE} install)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/usr.bin/lorder/lorder.sh.gnm \
	    ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-lorder

cross-gas:
	-mkdir -p ${CROSSDIR}/usr/obj
	-mkdir -p ${CROSSDIR}/usr/bin
	(cd gnu/usr.bin/gas; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj)
	(cd gnu/usr.bin/gas; \
	    TARGET_MACHINE_ARCH=${TARGET} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE})
	(cd gnu/usr.bin/gas; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)

cross-gcc:
	-mkdir -p ${CROSSDIR}/usr/obj
	(cd gnu/usr.bin/gcc; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj BSDSRCDIR=${.CURDIR} \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} -f Makefile.bsd-wrapper obj)
	(cd ${CROSSDIR}/usr/obj/gnu/usr.bin/gcc; \
	    /bin/sh ${.CURDIR}/gnu/usr.bin/gcc/configure \
	    --prefix ${CROSSDIR}/usr \
	    --target `cat ${CROSSDIR}/TARGET_CANON` && \
	    ${MAKE} BISON=yacc LANGUAGES=c LDFLAGS=${LDSTATIC} \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" && \
	    ${MAKE} BISON=yacc LANGUAGES=c LDFLAGS=${LDSTATIC} \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" install)
.endif

.include <bsd.subdir.mk>
