#	$OpenBSD: Makefile,v 1.36 2019/04/29 01:48:39 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk${OSrev}.fs

STRIPOPTS?=	-R .SUNW_ctf -R .shstrtab

.include "${TOP}/inst-common/Makefile.inc"

LISTS+=		${.CURDIR}/list.local

all: bsd.rd

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=8192

bsd.rd:	bsd.rd_unz
	objcopy -Sg -R .comment bsd.rd_unz bsd.strip
	strip ${STRIPOPTS} bsd.strip
	gzip -c -9nv bsd.strip > bsd.rd

bsd.rd_unz: bsd ${IMAGE}
	cp bsd bsd.rd_unz
	rdsetroot bsd.rd_unz ${IMAGE}
	strip -R .SUNW_ctf -R .shstrtab bsd.rd_unz

bsd:
	cd ${.CURDIR}/../../../sys/arch/alpha/compile/RAMDISKBIG && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/alpha/compile/RAMDISKBIG/obj/bsd bsd

${IMAGE}: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MAKEFSARGS_RD} $@ $@.d

.ifdef RELEASEDIR
beforeinstall:
	cp bsd.rd ${RELEASEDIR}
	chmod a+r ${RELEASEDIR}/bsd.rd
.endif
