#	$OpenBSD: Makefile,v 1.47 2020/04/02 19:30:00 gkoehler Exp $

CDROM=		cd${OSrev}.iso
MTREE=		${UTILS}/mtree.conf
RAMDISK=	RAMDISK

LISTS=		${.CURDIR}/list
UTILS=		${.CURDIR}/../../miniroot

all: ${CDROM}

${CDROM}: bsd.gz
	rm -rf ${.OBJDIR}/cd-dir/
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/macppc/
	cp bsd.gz ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	# On HFS, doing the following means COPIES of the files.. therefore
	# we skip doing so.
	# ln ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	# ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd
	cp ${DESTDIR}/usr/mdec/bsd.tbxi ${.OBJDIR}/cd-dir/${OSREV}/macppc
	cp ${DESTDIR}/usr/mdec/ofwboot ${.OBJDIR}/cd-dir/
	mkhybrid -r -part -hfs \
	    -hfs-bless ${.OBJDIR}/cd-dir/${OSREV}/macppc \
	    -map ${.CURDIR}/../../../gnu/usr.sbin/mkhybrid/src/more.mapping \
	    -A "OpenBSD ${OSREV} macppc bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/macppc  ${OSREV} boot-only CD" \
	    -o ${CDROM} ${.OBJDIR}/cd-dir

MRDISKTYPE=	rdroot
MRMAKEFSARGS=	-o disklabel=${MRDISKTYPE},minfree=0,density=4096

bsd.gz: bsd.rd
	gzip -9cn bsd.rd > bsd.gz

bsd.rd: mr.fs bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd mr.fs
	strip -R .SUNW_ctf bsd.rd

bsd:
	cd ${.CURDIR}/../../../sys/arch/${MACHINE}/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/${MACHINE}/compile/${RAMDISK}/obj/bsd bsd

mr.fs: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MRMAKEFSARGS} $@ $@.d

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -E -M -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib \
	    -c instbin.c -e instbin -m instbin.mk instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

.ifdef RELEASEDIR
install:
	cp bsd.rd ${RELEASEDIR}/bsd.rd
	chmod a+r ${RELEASEDIR}/bsd.rd
	cp ${CDROM} ${RELEASEDIR}
.endif

clean cleandir:
	rm -f *.core mr.fs instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map *.o *.lo *.c bsd bsd.rd cd${OSrev}.iso
	rm -rf cd-dir mr.fs.d

.include <bsd.obj.mk>
