#	$OpenBSD: Makefile,v 1.10 2019/04/29 15:56:25 deraadt Exp $

IMAGE=		mr.fs
CRUNCHCONF?=	instbin.conf
CRUNCHCONFS?=	${.CURDIR}/../common/crunch.conf
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MTREE=		${UTILS}/mtree.conf

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

DIRS=\
	pine64 \
	rpi

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

all:	bsd.rd

bsd:
	cd ${.CURDIR}/../../../sys/arch/arm64/compile/RAMDISK && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/arm64/compile/RAMDISK/obj/bsd bsd

bsd.rd: ${IMAGE} bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd ${IMAGE}

${IMAGE}: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
.for DIR in ${DIRS}
	mkdir -p $@.d/usr/mdec/${DIR}
.endfor
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MAKEFSARGS_RD} $@ $@.d

.endif

instbin.mk instbin.cache instbin.c: ${CRUNCHCONF}
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map *.o *.lo *.c bsd.rd
	rm -f instbin.conf
	rm -rf cd-dir ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp bsd.rd ${RELEASEDIR}
	cd ${RELEASEDIR} && chmod a+r bsd.rd
.endif	# RELEASEDIR

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
