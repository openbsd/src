#	$OpenBSD: Makefile,v 1.21 2019/04/28 15:15:39 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		mr.fs
CBIN?=		instbin
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot
MTREE=		${UTILS}/mtree.conf

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0

all:	bsd.rd

bsd.rd: ${IMAGE} bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd ${IMAGE}

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

bsd:
.ifndef(NOBUILD)
	cd ${.CURDIR}/../../../sys/arch/luna88k/compile/RAMDISK && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
.endif
	cp -p ${.CURDIR}/../../../sys/arch/luna88k/compile/RAMDISK/obj/bsd bsd

.ifdef RELEASEDIR
install:
	cp bsd.rd ${RELEASEDIR}
.endif

${CBIN}.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${CBIN}.conf

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.conf ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map *.o *.lo *.c bsd bsd.rd
	/bin/rm -rf ${IMAGE}.d

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
