#	$OpenBSD: Makefile,v 1.15 2019/12/01 05:16:40 deraadt Exp $

FS=		miniroot${OSrev}.fs
FSSIZE=		8064
FSDISKTYPE=	mini34
CDROM=		cd${OSrev}.iso
MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf
RAMDISK=	RAMDISK_CD

LISTS=		${.CURDIR}/list
UTILS=		${.CURDIR}/../../miniroot

LBA=1

all: ${FS} ${CDROM}

${FS}: bsd.gz
	dd if=/dev/zero of=${FS} bs=512 count=${FSSIZE}
	vnconfig -v ${FS} > vnd
.ifdef LBA
	fdisk -yi -l ${FSSIZE} -f ${DESTDIR}/usr/mdec/mbr `cat vnd`
.endif
	disklabel -w `cat vnd` ${FSDISKTYPE}
	newfs -m 0 -o space -i 524288 -c ${FSSIZE} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	cp ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/boot
	strip ${.OBJDIR}/boot
	strip -R .comment -R .SUNW_ctf ${.OBJDIR}/boot
	installboot -v -r ${MOUNT_POINT} `cat vnd` \
	    ${DESTDIR}/usr/mdec/biosboot ${.OBJDIR}/boot
	install -c -m 555 -o root -g wheel bsd.gz ${MOUNT_POINT}/bsd
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

${CDROM}: bsd.rd
	rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/i386
	mkdir -p ${.OBJDIR}/cd-dir/etc
	echo "set image /${OSREV}/i386/bsd.rd" > ${.OBJDIR}/cd-dir/etc/boot.conf
	cp ${.OBJDIR}/bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp ${DESTDIR}/usr/mdec/cdbr ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp ${DESTDIR}/usr/mdec/cdboot ${.OBJDIR}/cd-dir/${OSREV}/i386/cdboot
	mkhybrid -a -R -T -L -l -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "OpenBSD ${OSREV} i386 bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/i386    ${OSREV} boot-only CD" \
	    -b ${OSREV}/i386/cdbr -c ${OSREV}/i386/boot.catalog \
	    ${.OBJDIR}/cd-dir

MRDISKTYPE=	rdrootb
MRMAKEFSARGS=	-o disklabel=${MRDISKTYPE},minfree=0,density=4096

bsd.gz: bsd.rd
	cp bsd.rd bsd.strip
	strip bsd.strip
	strip -R .comment -R .SUNW_ctf bsd.strip
	gzip -9cn bsd.strip > bsd.gz

bsd.rd: mr.fs bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd mr.fs

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
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib \
	    -c instbin.c -e instbin -m instbin.mk instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all
	strip -R .comment -R .SUNW_ctf instbin

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.ifdef RELEASEDIR
install:
	cp bsd.rd ${RELEASEDIR}/bsd.rd
	chmod a+r ${RELEASEDIR}/bsd.rd
	cp ${FS} ${RELEASEDIR}
	cp ${CDROM} ${RELEASEDIR}
.endif

clean cleandir:
	rm -f *.core mr.fs instbin instbin.mk instbin*.cache \
	    *.o *.lo *.c bsd bsd.rd bsd.gz bsd.strip floppy*.fs \
	    lib*.a lib*.olist instbin.map boot instbin.conf ${FS}
	rm -rf mr.fs.d
	rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
