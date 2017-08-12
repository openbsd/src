#	$OpenBSD: Makefile,v 1.33 2017/08/12 19:41:16 jasper Exp $

CBIN=		instbin
ARCHDIR=	${.CURDIR}/..
SRCDIR=		${.CURDIR}/../../..

.include <bsd.own.mk>

.if exists(${ARCHDIR}/Makefile.inc)
.include "${ARCHDIR}/Makefile.inc"
.endif

IMAGE?=		ramdisk${REV}.fs
CRUNCHGENOPTS?=-E
KERNEL?=	RAMDISK
UTILS?=		${.CURDIR}/../../miniroot

LISTS=		${ARCHDIR}/list ${.CURDIR}/list.local
MTREE=		${UTILS}/mtree.conf

LIF?=		lif${REV}.fs
CDROM?=		cd${REV}.iso
MKBOOT?=	/usr/mdec/mkboot
BOOT?=		${DESTDIR}/usr/mdec/boot
CDBOOT?=	${DESTDIR}/usr/mdec/cdboot

STRIP?=		strip
STRIPOPTS?=	-R .SUNW_ctf
GZIPCMD?=	gzip
GZIPFLAGS?=	-9n

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

all:	bsd.rd ${LIF} ${CDROM}

${LIF}:	bsd ${BOOT}
	${MKBOOT} -v ${BOOT} bsd ${LIF}

${CDROM}: bsd ${CDBOOT}
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir ${.OBJDIR}/cd-dir
	cp bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	mkhybrid -A "OpenBSD ${OSREV} hppa bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/hppa ${OSREV} boot-only CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir
	dd if=${CDBOOT} of=${.OBJDIR}/${CDROM} bs=32k count=1 conv=notrunc
	dd if=/dev/zero bs=64k count=1 >> ${.OBJDIR}/${CDROM}

obsd:
	cd ${SRCDIR}/sys/arch/${MACHINE}/compile/${KERNEL} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${SRCDIR}/sys/arch/${MACHINE}/compile/${KERNEL}/obj/bsd obsd

bsd.rd:	obsd ${IMAGE} rdsetroot
	cp obsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd ${IMAGE}
	${STRIP} ${STRIPOPTS} bsd.rd

bsd:	bsd.rd
	(cat bsd.rd ; dd if=/dev/zero count=1) | ${GZIPCMD} ${GZIPFLAGS} > bsd

rdsetroot: ${.CURDIR}/../../common/elfrdsetroot.c \
	   ${.CURDIR}/../../common/elf32.c ${.CURDIR}/../../common/elf64.c
	${HOSTCC} ${HOSTCCFLAGS} -o rdsetroot \
	    ${.CURDIR}/../../common/elfrdsetroot.c \
	    ${.CURDIR}/../../common/elf32.c ${.CURDIR}/../../common/elf64.c

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} ARCHDIR=${ARCHDIR} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d
	vnconfig -v -c vnd0 $@
	df -i /dev/vnd0a
	vnconfig -u vnd0

${CBIN}.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen ${CRUNCHGENOPTS} -D ${SRCDIR} -L ${DESTDIR}/usr/lib \
	    ${CBIN}.conf

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map \
	    ${CBIN}.conf *.o *.lo *.c obsd bsd bsd.* ${LIF} ${CDROM}
	/bin/rm -rf ${IMAGE}.d

.ifdef RELEASEDIR
beforeinstall:
	cp bsd.rd ${LIF} ${CDROM} ${RELEASEDIR}
	chmod a+r ${RELEASEDIR}/bsd.rd
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
