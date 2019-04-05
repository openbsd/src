#	$OpenBSD: Makefile,v 1.35 2019/04/05 21:08:00 deraadt Exp $

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

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

# mix config is not needed.
all:	${BSD_RD} cd

${BSD_RD}: ${IMAGE} bsd
	cp bsd ${BSD_RD}
	rdsetroot ${BSD_RD} ${IMAGE}

cd: ${BSD_RD}
	-rm -rf ${.OBJDIR}/cd-dir/
	-@mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/macppc/
	cp bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	strip -R .SUNW_ctf ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	gzip -9n ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	mv ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd.gz ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	# On HFS, doing the following means COPIES of the files.. therefore
	# we skip doing so.
	# ln ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	# ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd
	cp ${.CURDIR}/../../../sys/arch/macppc/stand/tbxidata/bsd.tbxi \
	    ${.OBJDIR}/cd-dir/${OSREV}/macppc
	cp ${DESTDIR}/usr/mdec/ofwboot ${.OBJDIR}/cd-dir/
	mkhybrid -v -v -r -part -hfs \
	    -hfs-bless ${.OBJDIR}/cd-dir/${OSREV}/macppc \
	    -map ${.CURDIR}/../../../gnu/usr.sbin/mkhybrid/src/more.mapping \
	    -A "OpenBSD ${OSREV} macppc bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/macppc  ${OSREV} boot-only CD" \
	    -o ${.OBJDIR}/cd${OSrev}.iso ${.OBJDIR}/cd-dir

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d
	vnconfig -v -c vnd0 mr.fs
	df -i /dev/vnd0a
	vnconfig -u vnd0

.endif

bsd:
	cd ${TOP}/../../sys/arch/macppc/compile/RAMDISK && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${TOP}/../../sys/arch/macppc/compile/RAMDISK/obj/bsd bsd

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map *.o *.lo *.c bsd ${BSD_RD} cd${REV}.iso
	rm -rf cd-dir ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp ${BSD_RD} cd${REV}.iso ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
