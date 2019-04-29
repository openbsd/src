#	$OpenBSD: Makefile.inc,v 1.14 2019/04/29 15:56:25 deraadt Exp $

LISTS?=		${.CURDIR}/../common/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

FS?=		miniroot${OSrev}.fs
BOOT?=		${DESTDIR}/usr/mdec/boot
EFIBOOT?=	${DESTDIR}/usr/mdec/BOOTX64.EFI ${DESTDIR}/usr/mdec/BOOTIA32.EFI
FLOPPYSIZE?=	2880
FLOPPYTYPE?=	floppy3

all:	${FS}

${FS}:	bsd.gz
	dd if=/dev/zero of=${FS} bs=512 count=${FLOPPYSIZE}
	vnconfig -v ${FS} > vnd
.ifdef LBA
	fdisk -yi -l ${FLOPPYSIZE} -b 960 -f ${DESTDIR}/usr/mdec/mbr `cat vnd`
.endif
.ifdef TEMPLATE
	disklabel -wAT ${TEMPLATE} `cat vnd`
	newfs -t msdos /dev/r`cat vnd`i
	mount /dev/`cat vnd`i ${MOUNT_POINT}
	mkdir -p ${MOUNT_POINT}/efi/boot
	cp ${EFIBOOT} ${MOUNT_POINT}/efi/boot
	umount ${MOUNT_POINT}
.else
	disklabel -w `cat vnd` ${FLOPPYTYPE}
.endif
	newfs -m 0 -o space -i 524288 -c ${FLOPPYSIZE} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	cp ${BOOT} ${.OBJDIR}/boot
	strip ${.OBJDIR}/boot
	strip -R .comment -R .SUNW_ctf ${.OBJDIR}/boot
	installboot -v -r ${MOUNT_POINT} `cat vnd` \
	    ${DESTDIR}/usr/mdec/biosboot ${.OBJDIR}/boot
	dd if=bsd.gz of=${MOUNT_POINT}/bsd bs=512
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

DISKTYPE?=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

bsd.gz: bsd.rd
	cp bsd.rd bsd.strip
	strip bsd.strip
	strip -R .comment -R .SUNW_ctf bsd.strip
	gzip -c9n bsd.strip > bsd.gz

bsd.rd:	mr.fs bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd mr.fs

bsd:
	cd ${.CURDIR}/../../../sys/arch/amd64/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/amd64/compile/${RAMDISK}/obj/bsd bsd

mr.fs: instbin
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

.ifdef RELEASEDIR
install:
.ifndef NOBSDRD
	cp bsd.rd ${RELEASEDIR}/bsd.rd
	chmod a+r ${RELEASEDIR}/bsd.rd
.endif
.ifndef NOFS
	cp ${FS} ${RELEASEDIR}/${FS}
.endif
.endif	# RELEASEDIR

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib \
	-c instbin.c -e instbin -m instbin.mk instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all
	strip -R .comment -R .SUNW_ctf instbin

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

clean cleandir:
	/bin/rm -f *.core mr.fs instbin instbin.mk instbin*.cache \
	    *.o *.lo *.c bsd bsd.rd bsd.gz bsd.strip floppy*.fs \
	    lib*.a lib*.olist instbin.map boot instbin.conf ${FS}
	/bin/rm -rf mr.fs.d

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
