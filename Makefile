#	$OpenBSD: Makefile,v 1.49 1999/03/02 21:47:32 niklas Exp $

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
# 3) It is strongly recommended that you build and install a new kernel
# before rebuilding your system. Some of the new programs may use new
# functionality or depend on API changes that your old kernel doesn't have.
#
# 4) If you are reasonably sure that things will compile OK, use the
# "make build" target supplied here. Good luck.
#
# 5) If you want to setup a cross-build environment, there is a "cross-tools"
# target available which upon completion of "make TARGET=<target> cross-tools" 
# (where <target> is one of the names in the /sys/arch directory) will produce
# a set of compilation tools along with the includes in the /usr/cross/<target>
# directory.
#

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
	(cd ${.CURDIR}/etc && ${MAKE} DESTDIR= distrib-dirs)
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
	(cd ${.CURDIR}/share/mk && ${SUDO} ${MAKE} install)
	(cd ${.CURDIR}/include; ${MAKE} prereq; ${SUDO} ${MAKE} includes)
	${SUDO} ${MAKE} cleandir
	(cd ${.CURDIR}/lib && ${MAKE} depend && ${MAKE} && \
	    ${SUDO} ${MAKE} install)
	(cd ${.CURDIR}/gnu/lib && ${MAKE} depend && ${MAKE} && \
	    ${SUDO} ${MAKE} install)
.if (${MACHINE_ARCH} == "mips")
	ldconfig
.endif
.if (${KERBEROS} == "yes")
	(cd ${.CURDIR}/kerberosIV && ${MAKE} SUDO=${SUDO} build)
.if (${MACHINE_ARCH} == "mips")
	ldconfig
.endif
.endif
	${MAKE} depend && ${MAKE} && ${SUDO} ${MAKE} install

.if !defined(TARGET)
cross-tools:
	echo "TARGET must be set"; exit 1
.else
cross-tools:	cross-helpers cross-dirs cross-includes cross-binutils \
	cross-gcc cross-lib

CROSSDIR=	${DESTDIR}/usr/cross/${TARGET}
CROSSENV=	AR=${CROSSDIR}/usr/bin/ar AS=${CROSSDIR}/usr/bin/as \
		CC=${CROSSDIR}/usr/bin/cc CPP=${CROSSDIR}/usr/bin/cpp \
		LD=${CROSSDIR}/usr/bin/ld NM=${CROSSDIR}/usr/bin/nm \
		LORDER=${CROSSDIR}/usr/bin/lorder \
		RANLIB=${CROSSDIR}/usr/bin/ranlib \
		SIZE=${CROSSDIR}/usr/bin/size STRIP=${CROSSDIR}/usr/bin/strip \
		HOSTCC=cc

cross-helpers:
	mkdir -p ${CROSSDIR}
	echo _MACHINE_ARCH | \
	    cat ${.CURDIR}/sys/arch/${TARGET}/include/param.h - | \
	    ${CPP} -E -I${.CURDIR}/sys/arch | \
	    sed -n '$$p' >${CROSSDIR}/TARGET_ARCH
# GROSS KLUDGE  MACHINE_ARCH is mips, but we use mipsel for gnu tools.
.if ${TARGET} == "arc" || ${TARGET} == "pmax"
	eval `grep '^osr=' sys/conf/newvers.sh`; \
	   sed "s/\$$/el-unknown-openbsd$$osr/" ${CROSSDIR}/TARGET_ARCH > \
	   ${CROSSDIR}/TARGET_CANON
.else
	eval `grep '^osr=' sys/conf/newvers.sh`; \
	   sed "s/\$$/-unknown-openbsd$$osr/" ${CROSSDIR}/TARGET_ARCH > \
	   ${CROSSDIR}/TARGET_CANON
.endif

cross-dirs:	${CROSSDIR}/stamp.dirs
	@-mkdir -p ${CROSSDIR}
	@-mkdir -p ${CROSSDIR}/usr/obj
	@-mkdir -p ${CROSSDIR}/usr/bin
	@-mkdir -p ${CROSSDIR}/usr/include
	@-mkdir -p ${CROSSDIR}/usr/lib
	@-mkdir -p ${CROSSDIR}/var/db
	@-mkdir -p ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`
	@ln -sf ${CROSSDIR}/usr/include \
		${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/include
	@ln -sf ${CROSSDIR}/usr/lib \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/lib
	@-mkdir -p ${CROSSDIR}/usr/obj
	@-mkdir -p ${CROSSDIR}/usr/bin
	@-mkdir -p ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin

${CROSSDIR}/stamp.dirs:
	@touch ${CROSSDIR}/stamp.dirs

cross-includes:	cross-dirs
	export MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` ;\
	    ${MAKE} DESTDIR=${CROSSDIR} includes

.if ${TARGET} == "powerpc" || ${TARGET} == "alpha" || ${TARGET} == "arc" || \
    ${TARGET} == "pmax" || ${TARGET} == "wgrisc" || ${TARGET} == "hppa"
cross-binutils: cross-binutils-new
.else
cross-binutils: cross-binutils-old
.endif

cross-binutils-new:	cross-dirs
	export BSDSRCDIR=`pwd`; \
	    (cd ${.CURDIR}/gnu/usr.bin/binutils; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} -f Makefile.bsd-wrapper obj); \
	    (cd ${CROSSDIR}/usr/obj/gnu/usr.bin/binutils; \
	    /bin/sh ${BSDSRCDIR}/gnu/usr.bin/binutils/configure \
	    --prefix ${CROSSDIR}/usr \
	    --target `cat ${CROSSDIR}/TARGET_CANON` && \
	    ${MAKE} CFLAGS=${CFLAGS} && ${MAKE} DESTDIR=${CROSSDIR} install)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/usr.bin/lorder/lorder.sh.gnm \
	    ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-lorder
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-as \
	    ${CROSSDIR}/usr/bin/as
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-ar \
	    ${CROSSDIR}/usr/bin/ar
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-ld \
	    ${CROSSDIR}/usr/bin/ld
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-strip \
	    ${CROSSDIR}/usr/bin/strip
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-size \
	    ${CROSSDIR}/usr/bin/size
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-ranlib \
	    ${CROSSDIR}/usr/bin/ranlib
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-nm \
	    ${CROSSDIR}/usr/bin/nm
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-strings \
	    ${CROSSDIR}/usr/bin/strings
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-lorder \
	    ${CROSSDIR}/usr/bin/lorder
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-objcopy \
	    ${CROSSDIR}/usr/bin/objcopy
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-objdump \
	    ${CROSSDIR}/usr/bin/objdump
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-gdb \
	    ${CROSSDIR}/usr/bin/gdb

cross-binutils-old: cross-gas cross-ar cross-ld cross-strip cross-size \
	cross-ranlib cross-nm

cross-gas:	cross-dirs
	(cd ${.CURDIR}/gnu/usr.bin/gas; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    TARGET_MACHINE_ARCH=${TARGET} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE}; \
	    TARGET_MACHINE_ARCH=${TARGET} \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/as \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/as

cross-ld:	cross-dirs
	(cd ${.CURDIR}/gnu/usr.bin/ld; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    TARGET_MACHINE_ARCH=${TARGET} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN=; \
	    TARGET_MACHINE_ARCH=${TARGET} \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ld \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ld

cross-ar:	cross-dirs
	(cd ${.CURDIR}/usr.bin/ar; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN=; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ar \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ar

cross-ranlib:	cross-dirs
	(cd ${.CURDIR}/usr.bin/ranlib; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN=; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ranlib \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ranlib

cross-strip:	cross-dirs
	(cd ${.CURDIR}/usr.bin/strip; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=${TARGET} NOMAN=; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=${TARGET} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/strip \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/strip

cross-size:	cross-dirs
	(cd ${.CURDIR}/usr.bin/size; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=${TARGET} NOMAN=; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/size \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/size

cross-nm:	cross-dirs
	(cd ${.CURDIR}/usr.bin/nm; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=${TARGET} NOMAN=; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/nm \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/nm

cross-gcc:	cross-dirs
	cd ${.CURDIR}/gnu/usr.bin/gcc; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj BSDSRCDIR=${.CURDIR} \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} -f Makefile.bsd-wrapper obj
	(cd ${CROSSDIR}/usr/obj/gnu/usr.bin/gcc; \
	    /bin/sh ${.CURDIR}/gnu/usr.bin/gcc/configure \
	    --prefix ${CROSSDIR}/usr \
	    --target `cat ${CROSSDIR}/TARGET_CANON` && \
	    ${MAKE} BISON=yacc LANGUAGES=c LDFLAGS=${LDSTATIC} \
	    build_infodir=. \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" && \
	    ${MAKE} BISON=yacc LANGUAGES=c LDFLAGS=${LDSTATIC} \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" \
	    build_infodir=. INSTALL_MAN= INSTALL_HEADERS_DIR= install)
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-gcc \
	    ${CROSSDIR}/usr/bin/cc
	CPP=`${CROSSDIR}/usr/bin/cc -print-libgcc-file-name | \
	    sed 's/libgcc\.a/cpp/'`; \
	    sed -e 's#/usr/libexec/cpp#'$$CPP'#' \
	    -e 's#/usr/include#${CROSSDIR}/usr/include#' \
	    ${.CURDIR}/usr.bin/cpp/cpp.sh > ${CROSSDIR}/usr/bin/cpp
	chmod ${BINMODE} ${CROSSDIR}/usr/bin/cpp
	chown ${BINOWN}.${BINGRP} ${CROSSDIR}/usr/bin/cpp

# XXX MAKEOBJDIR maybe should be obj.${TARGET} here, revisit later
cross-lib:	cross-dirs
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	(cd ${.CURDIR}/lib; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    for lib in csu libc; do \
		(cd $$lib; \
		    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
		    ${MAKE} NOMAN= depend; \
		    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
		    ${MAKE} NOMAN=; \
		    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
		    DESTDIR=${CROSSDIR} ${MAKE} NOMAN= install); \
	    done; \
	    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= depend; \
	    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN=; \
	    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    DESTDIR=${CROSSDIR} SKIPDIR=libocurses/PSD.doc \
	    ${MAKE} NOMAN= install)
.if (${KERBEROS} == "yes")
	(cd kerberosIV; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    for lib in acl krb kadm kafs kdb; do \
		(cd $$lib; \
		    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
		    ${MAKE} NOMAN= depend; \
		    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
		    ${MAKE} NOMAN=; \
		    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
		    ${MAKE} NOMAN= install); \
	    done)
.endif
.endif

.include <bsd.subdir.mk>
