#	$OpenBSD: Makefile,v 1.108 2004/11/30 15:46:01 mickey Exp $

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
# directory. The "cross-distrib" target will build cross-tools as well as
# binaries for a given <target>.
#

.include <bsd.own.mk>	# for NOMAN, if it's there.

SUBDIR+= lib include bin libexec sbin usr.bin usr.sbin share games
SUBDIR+= gnu

SUBDIR+= sys lkm

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
	${MAKE} depend && ${MAKE} && exec ${SUDO} ${MAKE} install

.if !defined(TARGET)
cross-tools cross-distrib:
	echo "TARGET must be set"; exit 1
.else
cross-tools:	cross-includes cross-binutils cross-gcc cross-lib
cross-distrib:	cross-tools cross-bin cross-etc-root-var

CROSSCPPFLAGS?=	-nostdinc -I${CROSSDIR}/usr/include
CROSSLDFLAGS?=	-nostdlib -L${CROSSDIR}/usr/lib -static
CROSSCFLAGS?=	${CROSSCPPFLAGS}
CROSSCXXFLAGS?=	${CROSSCPPFLAGS}
LDSTATIC?=	-static

CROSSDIR=	${DESTDIR}/usr/cross/${TARGET}
CROSSENV=	AR=${CROSSDIR}/usr/bin/ar AS=${CROSSDIR}/usr/bin/as \
		CC=${CROSSDIR}/usr/bin/cc CPP=${CROSSDIR}/usr/bin/cpp \
		CXX=${CROSSDIR}/usr/bin/c++ \
		LD=${CROSSDIR}/usr/bin/ld NM=${CROSSDIR}/usr/bin/nm \
		LORDER=/usr/bin/lorder RANLIB=${CROSSDIR}/usr/bin/ranlib \
		SIZE=${CROSSDIR}/usr/bin/size STRIP=${CROSSDIR}/usr/bin/strip \
		HOSTCC=\"${CC}\" HOSTCXX=\"${CXX}\" NOMAN= DESTDIR=${CROSSDIR} \
		HOSTCFLAGS=\"${CFLAGS}\" HOSTCXXFLAGS=\"${CXXFLAGS}\" \
		HOSTLDFLAGS=\"${LDFLAGS} \" \
		CFLAGS=\"${CROSSCFLAGS}\" CPPFLAGS=\"${CROSSCPPFLAGS}\" \
		CXXFLAGS=\"${CROSSCXXFLAGS}\" \
		LDFLAGS=\"${CROSSLDFLAGS}\"
CROSSPATH=	${PATH}:${CROSSDIR}/usr/bin
CROSSLANGS?=	c c++

CROSSDIRS=	${CROSSDIR}/.dirs_done
CROSSOBJ=	${CROSSDIR}/usr/obj/.obj_done
CROSSINCLUDES=	${CROSSDIR}/usr/include/.includes_done
CROSSBINUTILS=	${CROSSDIR}/usr/bin/.binutils_done
CROSSGCC=	${CROSSDIR}/usr/bin/.gcc_done
NO_CROSS=	isakmpd tn3270 less sudo openssl libkeynote libssl \
		photurisd keynote sectok ssh

cross-dirs:	${CROSSDIRS}
cross-obj:	${CROSSOBJ}
cross-includes:	${CROSSINCLUDES}
cross-binutils:	${CROSSBINUTILS}
cross-gcc:	${CROSSGCC}

cross-env:	.PHONY
	@echo ${CROSSENV} MACHINE=${TARGET} \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`

${CROSSDIRS}:
	@-mkdir -p ${CROSSDIR}
	@case ${TARGET} in \
		alpha|amd64|hppa|hppa64|i386|m68k|m88k|powerpc|sparc|sparc64|vax) \
			echo ${TARGET} ;;\
		amiga|hp300|mac68k|mvme68k) \
			echo m68k ;;\
		luna88k|mvme88k) \
			echo m88k ;;\
		macppc|mvmeppc) \
			echo powerpc ;;\
		sgi) \
			echo mips64 ;;\
		*) \
			(echo Unknown arch ${TARGET} >&2) ; exit 1;; \
	esac > ${CROSSDIR}/TARGET_ARCH
	@echo TARGET_ARCH is `cat ${CROSSDIR}/TARGET_ARCH`
	@eval `grep '^osr=' sys/conf/newvers.sh`; \
	   sed "s/\$$/-unknown-openbsd$$osr/" ${CROSSDIR}/TARGET_ARCH > \
	   ${CROSSDIR}/TARGET_CANON
	@-mkdir -p ${CROSSDIR}
	@-mkdir -p ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`
	@ln -sf ${CROSSDIR}/usr/include \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/include
	@ln -sf ${CROSSDIR}/usr/lib \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/lib
	@-mkdir -p ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin
	@(cd ${.CURDIR}/etc && DESTDIR=${CROSSDIR} ${MAKE} distrib-dirs)
	@touch ${CROSSDIRS}

${CROSSOBJ}:	${CROSSDIRS}
	@-mkdir -p ${CROSSDIR}/usr/obj
	@(cd ${.CURDIR} && \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    MACHINE=${TARGET} \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj)
	@touch ${CROSSOBJ}

${CROSSINCLUDES}:	${CROSSOBJ}
	@-mkdir -p ${CROSSDIR}/usr/include
	@(cd ${.CURDIR}/include && \
	    MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} prereq && \
	    MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} DESTDIR=${CROSSDIR} includes)
	@touch ${CROSSINCLUDES}

.if ${MACHINE_ARCH} == "m68k" || ${MACHINE_ARCH} == "m88k" || \
    ${MACHINE_ARCH} == "vax"
BINUTILS=	ar as ld nm ranlib objcopy objdump strings strip
NEW_BINUTILS?=	No
.else
BINUTILS=	ar as gasp ld nm objcopy objdump ranlib readelf size \
		strings strip
NEW_BINUTILS?=	Yes
.endif

${CROSSBINUTILS}:	${CROSSINCLUDES}
.if ${NEW_BINUTILS:L} == "yes"
	(cd ${.CURDIR}/gnu/usr.bin/binutils; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    TARGET_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    ${MAKE} -f Makefile.bsd-wrapper depend && \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    TARGET_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    ${MAKE} -f Makefile.bsd-wrapper all && \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} -f Makefile.bsd-wrapper install)
.else
	(cd ${.CURDIR}/gnu/usr.bin/gas; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} depend all; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/as \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/as
	(cd ${.CURDIR}/gnu/usr.bin/ld; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOPIC= NOMAN= depend all; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOPIC= NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ld \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ld
	(cd ${.CURDIR}/usr.bin/ar; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ar \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ar
	(cd ${.CURDIR}/usr.bin/ranlib; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ranlib \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ranlib
	(cd ${.CURDIR}/usr.bin/strip; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/strip \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/strip
.endif
	(cd ${.CURDIR}/usr.bin/nm; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/nm \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/nm
	ln -sf ${CROSSDIR}/usr/bin/size \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/size
	@for cmd in ${BINUTILS}; do \
	 if [ ! -e ${CROSSDIR}/usr/bin/$$cmd -a \
	 -e ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd ]; then \
	    ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd \
	        ${CROSSDIR}/usr/bin/$$cmd ;\
	 elif [ -e ${CROSSDIR}/usr/bin/$$cmd -a \
	 ! -e ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd ]; then \
	    ln -sf ${CROSSDIR}/usr/bin/$$cmd \
	        ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd; \
	 fi ;\
	 if [ -e ${CROSSDIR}/usr/bin/$$cmd -a \
	 ! -e ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/$$cmd ]; then \
	    ln -sf ${CROSSDIR}/usr/bin/$$cmd \
	        ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/$$cmd; \
	 fi ;\
	done
	@touch ${CROSSBINUTILS}

# bsd.own.mk can't do it for us
.if ${TARGET} == "amd64" || ${TARGET} == "cats" || \
    ${TARGET} == "hppa" || ${TARGET} == "hppa64" || \
    ${TARGET} == "sparc64" || ${TARGET} == "sgi"
USE_GCC3=yes
.endif

${CROSSGCC}:		${CROSSBINUTILS}
.if ${USE_GCC3:L} == "yes"
	(cd ${.CURDIR}/gnu/usr.bin/gcc; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    TARGET_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` CROSSDIR=${CROSSDIR} \
	    ${MAKE} -f Makefile.bsd-wrapper depend && \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    TARGET_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` CROSSDIR=${CROSSDIR} \
	    ${MAKE} -f Makefile.bsd-wrapper all && \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} -f Makefile.bsd-wrapper install)
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-g++ \
	    ${CROSSDIR}/usr/bin/c++
	ln -sf ${CROSSDIR}/usr/libexec/cpp \
	    ${CROSSDIR}/usr/bin/cpp
.else
	(cd ${CROSSDIR}/usr/obj/gnu/egcs/gcc; \
	    /bin/sh ${.CURDIR}/gnu/egcs/gcc/configure \
	    --with-gnu-as --with-gnu-ld --prefix ${CROSSDIR}/usr \
	    --target `cat ${CROSSDIR}/TARGET_CANON` \
	    --enable-languages="c,c++" --enable-cpp --disable-nls \
	    --with-gxx-include-dir=${CROSSDIR}/usr/include/g++ && \
	    PATH=${CROSSPATH} ${MAKE} BISON=yacc LANGUAGES="${CROSSLANGS}" \
	    CFLAGS="${CFLAGS} -I${.CURDIR}/gnu/lib/libiberty/include" \
	    LIBIBERTY_INCLUDES=${.CURDIR}/gnu/lib/libiberty/include \
	    DEMANGLER_PROG= DEMANGLE_H= LDFLAGS="${LDSTATIC}" build_infodir=. \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" && \
	    ${MAKE} BISON=yacc LANGUAGES="${CROSSLANGS}" LDFLAGS="${LDSTATIC}" \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" \
	    CFLAGS="${CFLAGS} -I${.CURDIR}/gnu/lib/libiberty/include" \
	    LIBIBERTY_INCLUDES=${.CURDIR}/gnu/lib/libiberty/include \
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
.endif
	@touch ${CROSSGCC}

# XXX MAKEOBJDIR maybe should be obj.${TARGET} here, revisit later
cross-lib:	${CROSSGCC}
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	(cd ${.CURDIR}/lib; \
	    for lib in csu libc; do \
	    (cd $$lib; \
	        eval ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
		    ${MAKE} depend all install); \
	    done; \
	    eval ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	        SKIPDIR=\"${NO_CROSS} libocurses/PSD.doc\" \
	        ${MAKE} depend all install)

cross-bin:	${CROSSOBJ}
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	for i in libexec bin sbin usr.bin usr.sbin; do \
	(cd ${.CURDIR}/$$i; \
	    eval ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	        SKIPDIR=\"${BINUTILS} ${NO_CROSS}\" \
	        ${MAKE} depend all install); \
	done

cross-etc-root-var:	${CROSSOBJ}
	(cd ${.CURDIR}/etc && \
	    DESTDIR=${CROSSDIR} ${MAKE} distribution-etc-root-var)

cross-depend:	.PHONY
	@(cd ${.CURDIR} && \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    SKIPDIR="${NO_CROSS}" \
	    ${MAKE} depend)

cross-clean:	.PHONY
	@(cd ${.CURDIR} && \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    SKIPDIR="${NO_CROSS}" \
	    ${MAKE} clean)

cross-cleandir:	.PHONY
	@(cd ${.CURDIR} && \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    SKIPDIR="${NO_CROSS}" \
	    ${MAKE} cleandir)

.endif # defined(TARGET)
 
.include <bsd.subdir.mk>
