#	$OpenBSD: Makefile,v 1.86 2002/08/11 22:48:05 art Exp $

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

.if (${KERBEROS:L} == "yes")
SUBDIR+= kerberosIV
.endif

.if (${KERBEROS5:L} == "yes")
SUBDIR+= kerberosV
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= distrib regress
.endif

.if exists(regress)
regression-tests:
	@echo Running regression tests...
	@cd ${.CURDIR}/regress && ${MAKE} depend && exec ${MAKE} regress
.endif

includes:
	cd ${.CURDIR}/include && ${MAKE} prereq && exec ${SUDO} ${MAKE} includes

beforeinstall:
	cd ${.CURDIR}/etc && exec ${MAKE} DESTDIR=${DESTDIR} distrib-dirs
	cd ${.CURDIR}/include && exec ${MAKE} includes

afterinstall:
.ifndef NOMAN
	cd ${.CURDIR}/share/man && exec ${MAKE} makedb
.endif

build:
.ifdef GLOBAL_AUTOCONF_CACHE
	cp /dev/null ${GLOBAL_AUTOCONF_CACHE}
.endif
	cd ${.CURDIR}/share/mk && exec ${SUDO} ${MAKE} install
	cd ${.CURDIR}/include && ${MAKE} prereq && exec ${SUDO} ${MAKE} includes
	${SUDO} ${MAKE} cleandir
	cd ${.CURDIR}/lib && ${MAKE} depend && ${MAKE} && \
	    NOMAN=1 exec ${SUDO} ${MAKE} install
	cd ${.CURDIR}/gnu/lib && ${MAKE} depend && ${MAKE} && \
	    NOMAN=1 exec ${SUDO} ${MAKE} install
.if (${KERBEROS:L} == "yes")
	cd ${.CURDIR}/kerberosIV/lib && ${MAKE} depend && ${MAKE} && \
	    NOMAN=1 exec ${SUDO} ${MAKE} install
.endif
.if (${KERBEROS5:L} == "yes")
	cd ${.CURDIR}/kerberosV/lib && ${MAKE} depend && ${MAKE} && \
	    NOMAN=1 exec ${SUDO} ${MAKE} install
.endif
	cd ${.CURDIR}/gnu/usr.bin/perl && \
	    ${MAKE} -f Makefile.bsd-wrapper depend && \
	    ${MAKE} -f Makefile.bsd-wrapper perl.lib && \
	    exec ${SUDO} ${MAKE} -f Makefile.bsd-wrapper install.lib
	${MAKE} depend && ${MAKE} && exec ${SUDO} ${MAKE} install

.if !defined(TARGET)
cross-tools:
	echo "TARGET must be set"; exit 1
.else
cross-tools:	cross-helpers cross-dirs cross-includes cross-binutils \
	cross-gcc cross-lib

CROSSDIR=	${DESTDIR}/usr/cross/${TARGET}
CROSSENV=	AR=${CROSSDIR}/usr/bin/ar AS=${CROSSDIR}/usr/bin/as \
		CC=${CROSSDIR}/usr/bin/cc CPP=${CROSSDIR}/usr/bin/cpp \
		CXX=${CROSSDIR}/usr/bin/c++ \
		LD=${CROSSDIR}/usr/bin/ld NM=${CROSSDIR}/usr/bin/nm \
		LORDER=/usr/bin/lorder RANLIB=${CROSSDIR}/usr/bin/ranlib \
		SIZE=${CROSSDIR}/usr/bin/size STRIP=${CROSSDIR}/usr/bin/strip \
		HOSTCC=cc
CROSSPATH=	${PATH}:${CROSSDIR}/usr/bin

cross-env:
	@echo ${CROSSENV} DESTDIR=${CROSSDIR} MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`

cross-helpers:
	@-mkdir -p ${CROSSDIR}
	@case ${TARGET} in \
		sparc|i386|m68k|alpha|hppa|powerpc|sparc64|m88k|vax) \
			echo ${TARGET} ;;\
		amiga|sun3|mac68k|hp300|mvme68k) \
			echo m68k ;;\
		mvme88k) \
			echo m88k ;;\
		mvmeppc|macppc) \
			echo powerpc ;;\
		*) \
			(echo Unknown arch ${TARGET} >&2) ; exit 1;; \
	esac > ${CROSSDIR}/TARGET_ARCH
	@echo TARGET_ARCH is `cat ${CROSSDIR}/TARGET_ARCH`
	@eval `grep '^osr=' sys/conf/newvers.sh`; \
	   sed "s/\$$/-unknown-openbsd$$osr/" ${CROSSDIR}/TARGET_ARCH > \
	   ${CROSSDIR}/TARGET_CANON

cross-dirs:	${CROSSDIR}/stamp.dirs
	@-mkdir -p ${CROSSDIR}
	@-mkdir -p ${CROSSDIR}/usr/obj
	@-mkdir -p ${CROSSDIR}/usr/bin
	@-mkdir -p ${CROSSDIR}/usr/include/kerberosIV
	@-mkdir -p ${CROSSDIR}/usr/include/kerberosV
	@-mkdir -p ${CROSSDIR}/usr/lib/apache/include/xml
	@-mkdir -p ${CROSSDIR}/usr/libexec
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
	cd include; \
	    MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    ${MAKE} prereq && \
	    MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    ${MAKE} DESTDIR=${CROSSDIR} includes

.if ${TARGET} == "macppc" || ${TARGET} == "alpha" || ${TARGET} == "hppa" || \
    ${TARGET} == "sparc64"|| ${TARGET} == "mvmeppc" || ${TARGET} == "sparc"
cross-binutils: cross-binutils-new cross-binutils-links
.else
cross-binutils: cross-binutils-old cross-binutils-links
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
	    --disable-nls --disable-gdbtk --disable-commonbfdlib \
	    --target `cat ${CROSSDIR}/TARGET_CANON` && \
	    ${MAKE} CFLAGS="${CFLAGS}" && ${MAKE} install )

cross-binutils-old: cross-gas cross-ar cross-ld cross-strip cross-size \
	cross-ranlib cross-nm

cross-binutils-links: cross-dirs
	for cmd in ar as ld nm ranlib objcopy objdump size strings strip; do \
	    if [ ! -e ${CROSSDIR}/usr/bin/$$cmd -a -e ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd ]; then \
		ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd \
		${CROSSDIR}/usr/bin/$$cmd ;\
	    elif [ -e ${CROSSDIR}/usr/bin/$$cmd -a ! -e ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd ]; then \
		ln -sf ${CROSSDIR}/usr/bin/$$cmd \
		${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd; \
	    fi ;\
	done

cross-gas:	cross-dirs
	(cd ${.CURDIR}/gnu/usr.bin/gas; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} depend all; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/as \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/as

cross-ld:	cross-dirs
	(cd ${.CURDIR}/gnu/usr.bin/ld; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN= depend all; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ld \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ld

cross-ar:	cross-dirs
	(cd ${.CURDIR}/usr.bin/ar; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ar \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ar

cross-ranlib:	cross-dirs
	(cd ${.CURDIR}/usr.bin/ranlib; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN= depend all; \
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
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/strip \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/strip

cross-size:	cross-dirs
	(cd ${.CURDIR}/usr.bin/size; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= depend all; \
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
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/nm \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/nm

cross-gcc:	cross-dirs
	cd ${.CURDIR}/gnu/egcs/gcc; \
	    BSDOBJDIR=${CROSSDIR}/usr/obj BSDSRCDIR=${.CURDIR} \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} -f Makefile.bsd-wrapper obj
	(cd ${CROSSDIR}/usr/obj/gnu/egcs/gcc; \
	    /bin/sh ${.CURDIR}/gnu/egcs/gcc/configure \
	    --prefix ${CROSSDIR}/usr \
	    --target `cat ${CROSSDIR}/TARGET_CANON` \
	    --with-gxx-include-dir=${CROSSDIR}/usr/include/g++ && \
	    PATH=${CROSSPATH} ${MAKE} BISON=yacc LANGUAGES="c c++" \
	    LDFLAGS=${LDSTATIC} build_infodir=. \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" && \
	    ${MAKE} BISON=yacc LANGUAGES="c c++" LDFLAGS=${LDSTATIC} \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" \
	    build_infodir=. INSTALL_MAN= INSTALL_HEADERS_DIR= install)
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-gcc \
	    ${CROSSDIR}/usr/bin/cc
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-g++ \
	    ${CROSSDIR}/usr/bin/c++
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${CROSSDIR}/usr/obj/gnu/egcs/gcc/cpp \
	    ${CROSSDIR}/usr/libexec/cpp
	sed -e 's#/usr/libexec/cpp#${CROSSDIR}/usr/libexec/cpp#' \
	    -e 's#/usr/include#${CROSSDIR}/usr/include#' \
	    ${.CURDIR}/usr.bin/cpp/cpp.sh > ${CROSSDIR}/usr/bin/cpp
	chmod ${BINMODE} ${CROSSDIR}/usr/bin/cpp
	chown ${BINOWN}:${BINGRP} ${CROSSDIR}/usr/bin/cpp

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
.if (${KERBEROS:L} == "yes")
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	cd kerberosIV/lib; \
	BSDOBJDIR=${CROSSDIR}/usr/obj BSDSRCDIR=${.CURDIR} \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} obj; \
	${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= depend; \
	${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN=; \
	${CROSSENV} DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install
.endif
.if (${KERBEROS5:L} == "yes")
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	cd kerberosV/lib; \
	BSDOBJDIR=${CROSSDIR}/usr/obj BSDSRCDIR=${.CURDIR} \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} obj; \
	${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= depend; \
	${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN=; \
	${CROSSENV} DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install
.endif
.endif

.include <bsd.subdir.mk>
