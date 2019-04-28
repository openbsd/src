#	$OpenBSD: Makefile,v 1.20 2019/04/28 15:15:40 deraadt Exp $

REV=	${OSrev}

TOP=		${.CURDIR}/..

BSD_BIN=	bsd.bin
IMAGE=		mr.fs
CBIN?=		instbin
CRUNCHCONF?=	${CBIN}.conf
CRUNCHCONFS?=	${.CURDIR}/../common/crunch.conf
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MTREE=		${UTILS}/mtree.conf

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

# mix config is not needed.
all:	bsd.rd ${BSD_BIN}

bsd.rd: ${IMAGE} bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd ${IMAGE}

${BSD_BIN}: bsd.rd
	objcopy -O binary bsd.rd ${BSD_BIN}

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

.endif

bsd:
	cd ${TOP}/../../sys/arch/socppc/compile/RAMDISK && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && ${MAKE}'
	cp -p ${TOP}/../../sys/arch/socppc/compile/RAMDISK/obj/bsd bsd

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map *.o *.lo *.c bsd bsd.rd cd${REV}.iso
	rm -rf cd-dir ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp bsd.rd ${BSD_BIN} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
