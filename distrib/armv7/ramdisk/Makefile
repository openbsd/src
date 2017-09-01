#	$OpenBSD: Makefile,v 1.17 2017/09/01 04:40:40 jsg Exp $

REV=	${OSrev}

TOP=		${.CURDIR}/..

BSD_RD=		bsd.rd
IMAGE=		mr.fs
CBIN?=		instbin
CRUNCHCONF?=	${CBIN}.conf
CRUNCHCONFS?=	${.CURDIR}/../common/crunch.conf
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MTREE=		${UTILS}/mtree.conf

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

DIRS=\
	am335x \
	cubie \
	cubox \
	nitrogen \
	panda \
	wandboard

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

ALLBSD=		bsd
ALLBSDRD=	${BSD_RD}
all:	${ALLBSDRD}

bsd:
	cd ${TOP}/../../sys/arch/armv7/compile/RAMDISK && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${TOP}/../../sys/arch/armv7/compile/RAMDISK/obj/bsd bsd

${BSD_RD}: ${IMAGE} bsd rdsetroot
	cp bsd ${BSD_RD}
	${.OBJDIR}/rdsetroot ${BSD_RD} ${IMAGE}

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
.for DIR in ${DIRS}
	mkdir -p $@.d/usr/mdec/${DIR}
.endfor
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

.endif

rdsetroot:	${TOP}/../common/elfrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o rdsetroot \
	    ${TOP}/../common/elfrdsetroot.c ${TOP}/../common/elf32.c \
	    ${TOP}/../common/elf64.c

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map \
	    *.o *.lo *.c ${ALLBSD} ${ALLBSDRD} rdsetroot
	rm -f instbin.conf
	rm -rf cd-dir ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp ${ALLBSDRD} ${RELEASEDIR}
.endif	# RELEASEDIR

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
