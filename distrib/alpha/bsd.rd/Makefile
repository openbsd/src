#	$OpenBSD: Makefile,v 1.37 2019/04/29 15:56:23 deraadt Exp $

IMAGE=		ramdisk${OSrev}.fs

STRIPOPTS?=	-R .SUNW_ctf -R .shstrtab

UTILS=		${.CURDIR}/../../miniroot
LISTS?=		${.CURDIR}/../common/list
MTREE=		${UTILS}/mtree.conf
STRIPOPTS?=	-R .eh_frame -R .SUNW_ctf -R .shstrtab

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
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MAKEFSARGS_RD} $@ $@.d

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all
	strip ${STRIPOPTS} instbin

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    *.o *.lo *.c \
	    lib*.a lib*.olist instbin.map ${CLEANFILES}
	/bin/rm -rf ${IMAGE}.d

.include <bsd.obj.mk>
.include <bsd.subdir.mk>

.ifdef RELEASEDIR
beforeinstall:
	cp bsd.rd ${RELEASEDIR}
	chmod a+r ${RELEASEDIR}/bsd.rd
.endif
