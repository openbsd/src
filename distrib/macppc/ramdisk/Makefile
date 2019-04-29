#	$OpenBSD: Makefile,v 1.40 2019/04/29 15:56:26 deraadt Exp $

IMAGE=		mr.fs
CRUNCHCONF?=	instbin.conf
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
all:	bsd.rd cd

bsd.rd: ${IMAGE} bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd ${IMAGE}

cd: bsd.rd
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

${IMAGE}: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MAKEFSARGS_RD} $@ $@.d
	vnconfig -v mr.fs > vnd
	df -i /dev/`cat vnd`a
	vnconfig -u `cat vnd`
	rm -f vnd

.endif

bsd:
	cd ${.CURDIR}/../../../sys/arch/macppc/compile/RAMDISK && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/macppc/compile/RAMDISK/obj/bsd bsd

instbin.mk instbin.cache instbin.c: ${CRUNCHCONF}
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map *.o *.lo *.c bsd bsd.rd cd${OSrev}.iso
	rm -rf cd-dir ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp bsd.rd cd${OSrev}.iso ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
