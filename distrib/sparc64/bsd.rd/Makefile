#	$OpenBSD: Makefile,v 1.37 2019/04/14 14:07:08 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk${REV}.fs
CBIN?=		instbin
LISTS?=		${.CURDIR}/list
UTILS?=		${TOP}/../miniroot

STRIPOPTS?=	-R .SUNW_ctf
GZIPFLAGS?=	-9fn
MTREE=		${UTILS}/mtree.conf

RAMDISK=	RAMDISK

all: bsd.rd

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

bsd.rd:	bsd.rd_unz
	objcopy -Sg -R .comment bsd.rd_unz bsd.strip
	strip ${STRIPOPTS} bsd.strip
	gzip -c ${GZIPFLAGS} bsd.strip > bsd.rd

bsd.rd_unz: bsd ${IMAGE}
	cp bsd bsd.rd_unz
	rdsetroot bsd.rd_unz ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../sys/arch/sparc64/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/sparc64/compile/${RAMDISK}/obj/bsd bsd

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

.ifdef RELEASEDIR
install:
.ifndef NOBSDRD
	cp bsd.rd ${RELEASEDIR}
	chmod a+r ${RELEASEDIR}/bsd.rd
.endif
.endif

${CBIN}.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib \
	-c ${CBIN}.c -e ${CBIN} -m ${CBIN}.mk ${CBIN}.conf

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}*.cache \
	    lib*.a lib*.olist ${CBIN}.map \
	    *.o *.lo *.c bsd bsd.rd{,_unz} bsd.gz bsd.strip floppy*.fs
	/bin/rm -rf ${IMAGE}.d

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
