#	$OpenBSD: Makefile,v 1.30 2017/09/18 20:13:52 jasper Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk${REV}.fs

STRIPOPTS?=	-R .SUNW_ctf -R .shstrtab
GZIPFLAGS?=	-9fn

RAMDISK=	RAMDISKBIG

.include "${TOP}/inst-common/Makefile.inc"

LISTS+=		${.CURDIR}/list.local

all: bsd.rd

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=8192

bsd.rd:	bsd.rd_unz
	objcopy -Sg -R .comment bsd.rd_unz bsd.strip
	strip ${STRIPOPTS} bsd.strip
	gzip -c ${GZIPFLAGS} bsd.strip > bsd.rd

bsd.rd_unz: bsd ${IMAGE} rdsetroot
	cp bsd bsd.rd_unz
	${.OBJDIR}/rdsetroot bsd.rd_unz ${IMAGE}
	strip -R .SUNW_ctf -R .shstrtab bsd.rd_unz

bsd:
	cd ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && env COPTS=-Oz ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK}/obj/bsd bsd

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

rdsetroot: ${TOP}/../common/elfrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o rdsetroot \
	    ${TOP}/../common/elfrdsetroot.c ${TOP}/../common/elf32.c \
	    ${TOP}/../common/elf64.c

.ifdef RELEASEDIR
beforeinstall:
	cp bsd.rd ${RELEASEDIR}
	chmod a+r ${RELEASEDIR}/bsd.rd
.endif
