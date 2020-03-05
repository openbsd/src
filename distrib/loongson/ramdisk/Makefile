#	$OpenBSD: Makefile,v 1.26 2020/03/05 16:33:32 otto Exp $

FS=		miniroot${OSrev}.fs
FSSIZE=		18432
FSDISKTYPE=	miniroot
CDROM=		cd${OSrev}.iso
MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf
RAMDISK=	RAMDISK

LISTS=		${.CURDIR}/list
UTILS=		${.CURDIR}/../../miniroot

MRDISKTYPE=	rdroot
MRMAKEFSARGS=	-o disklabel=${MRDISKTYPE},minfree=0,density=4096

all: ${FS} ${CDROM}

# PMON cannot load a gzipped kernel
${FS}: bsd.strip
	dd if=/dev/zero of=${FS} bs=512 count=${FSSIZE}
	vnconfig -v -t ${FSDISKTYPE} ${FS} > vnd
	echo 'u\ne 0\n83\n\n63\n*\nw\nq\n' | fdisk -e `cat vnd` > /dev/null
	echo 'w\ny\nq\n' | disklabel -E `cat vnd` > /dev/null
	newfs_ext2fs /dev/r`cat vnd`i
	mount /dev/`cat vnd`i ${MOUNT_POINT}
	install -c -m 555 -o root -g wheel bsd.strip ${MOUNT_POINT}/bsd
	ln ${MOUNT_POINT}/bsd ${MOUNT_POINT}/bsd.rd
	df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

${CDROM}: bsd.rd
	rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/loongson
	mkdir -p ${.OBJDIR}/cd-dir/etc
	echo "set image /${OSREV}/loongson/bsd.rd" > ${.OBJDIR}/cd-dir/etc/boot.conf
	cp ${.OBJDIR}/bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/loongson
	mkhybrid -a -R -T -L -l -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "OpenBSD ${OSREV} loongson bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/loongson ${OSREV} boot CD" \
	    ${.OBJDIR}/cd-dir

bsd.strip: bsd.rd
	cp bsd.rd bsd.strip
	strip bsd.strip
	strip -R .comment -R .SUNW_ctf bsd.strip

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
	crunchgen -E -M -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib \
	    -c instbin.c -e instbin -m instbin.mk instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

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
	    lib*.a lib*.olist instbin.map *.o *.lo *.c bsd bsd.rd cd${OSrev}.iso
	rm -rf cd-dir mr.fs.d
	rm -f ${CDROM}

.include <bsd.obj.mk>
