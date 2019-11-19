#	$OpenBSD: Makefile,v 1.15 2019/11/19 09:46:22 deraadt Exp $

FS=		miniroot${OSrev}.fs
FSSIZE=		5760
FSDISKTYPE=	fakeramdisk
CDROM=		cd${OSrev}.iso
MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf
RAMDISK=	RAMDISKBIG

LISTS=		${.CURDIR}/list
UTILS=		${.CURDIR}/../../miniroot

STRIPOPTS=	-R .eh_frame -R .SUNW_ctf -R .shstrtab

MRDISKTYPE=	rdroot
MRMAKEFSARGS=	-o disklabel=${MRDISKTYPE},minfree=0,density=8192

all: ${FS} ${CDROM}

${FS}: bsd.gz
	dd if=/dev/zero of=${FS} bs=512 count=${FSSIZE}
	vnconfig -v ${FS} > vnd
	disklabel -w `cat vnd` ${FSDISKTYPE}
	newfs -m 0 -o space -i 524288 -c ${FSSIZE} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	install -c -m 555 -o root -g wheel \
	    ${DESTDIR}/usr/mdec/boot ${MOUNT_POINT}/boot
	/usr/mdec/installboot -v \
	    ${MOUNT_POINT}/boot ${DESTDIR}/usr/mdec/bootxx `cat vnd`
	install -c -m 555 -o root -g wheel bsd.gz ${MOUNT_POINT}/bsd
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

${CDROM}: bsd.gz
	rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp bsd.gz ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	# make sure right kernel is in /
	ln ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.gz ${.OBJDIR}/cd-dir/bsd.rd
	ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd

	cp ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp ${DESTDIR}/usr/mdec/bootxx ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	(mkhybrid -a -R -T -L -d -D -N -o ${.OBJDIR}/${CDROM} -v -v -v \
	    -A "OpenBSD ${OSREV} ${MACHINE} bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/${MACHINE}   ${OSREV} boot-only CD" \
	    ${.OBJDIR}/cd-dir 2>&1) | tee log
	vnconfig -v ${CDROM} > vnd
	mount -t cd9660 /dev/`cat vnd`a ${MOUNT_POINT}
	/usr/mdec/installboot -v \
	    -s `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/${MACHINE}/boot$$" | cut -d' ' -f1` \
	    -e `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/${MACHINE}/boot$$" | cut -d' ' -f2` \
	    ${MOUNT_POINT}/${OSREV}/${MACHINE}/boot /usr/mdec/bootxx `cat vnd`
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

bsd.gz: bsd.rd
	objcopy -Sg -R .comment bsd.rd bsd.strip
	strip ${STRIPOPTS} bsd.strip
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
	strip ${STRIPOPTS} instbin

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
	rm -f *.core mr.fs instbin instbin.mk instbin.cache \
	    *.o *.lo *.c \
	    lib*.a lib*.olist instbin.map ${CLEANFILES}
	rm -rf mr.fs.d

.include <bsd.obj.mk>
