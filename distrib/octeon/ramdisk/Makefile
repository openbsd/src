#	$OpenBSD: Makefile,v 1.10 2019/04/29 15:56:26 deraadt Exp $

IMAGE=		mr.fs
CRUNCHCONF?=	instbin.conf
CRUNCHCONFS?=	${.CURDIR}/../common/crunch.conf
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MTREE=		${UTILS}/mtree.conf

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

# mix config is not needed.
all:	bsd.rd

bsd.rd: ${IMAGE} bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd ${IMAGE}

${IMAGE}: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MAKEFSARGS_RD} $@ $@.d

bsd:
	cd ${.CURDIR}/../../../sys/arch/octeon/compile/RAMDISK && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/octeon/compile/RAMDISK/obj/bsd bsd

instbin.mk instbin.cache instbin.c: ${CRUNCHCONF}
	crunchgen -M -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map *.o *.lo *.c bsd bsd.rd
	rm -rf cd-dir ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp bsd.rd ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
