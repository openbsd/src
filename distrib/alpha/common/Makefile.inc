#	$OpenBSD: Makefile.inc,v 1.43 2019/04/29 15:56:25 deraadt Exp $

IMAGE=		ramdisk${OSrev}.fs

MOUNT_POINT=	/mnt

STRIPOPTS?=	-R .SUNW_ctf -R .shstrtab
INSTALLBOOT?=	/usr/mdec/installboot -v

XNAME?=		floppy
FLOPPY?=	${XNAME}${OSrev}.fs
FLOPPYBLKS?=	2880
FLOPPYTYPE?=	floppy
BOOT?=		/usr/mdec/boot
BOOTXX?=	/usr/mdec/bootxx

UTILS=		${.CURDIR}/../../miniroot

LISTS?=		${.CURDIR}/../common/list
CRUNCHCONF?=	instbin.conf
MTREE=		${UTILS}/mtree.conf
STRIPOPTS?=	-R .eh_frame -R .SUNW_ctf -R .shstrtab

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk ${LISTS} > instbin.conf

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all
	strip ${STRIPOPTS} instbin

all: ${FLOPPY}

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=8192

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS=	-t ffs -m 0 -o space
NEWFSARGS_FD=	-i 524288 -c ${FLOPPYBLKS}

CLEANFILES+=	bsd bsd.rd bsd.gz ${FLOPPY}

${FLOPPY}:	bsd.gz
	dd if=/dev/zero of=${FLOPPY} count=${FLOPPYBLKS}
	vnconfig -v ${FLOPPY} > vnd
	disklabel -w `cat vnd` ${FLOPPYTYPE}
	newfs ${NEWFSARGS} ${NEWFSARGS_FD} -s ${FLOPPYBLKS} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	dd if=${DESTDIR}/${BOOT} of=${MOUNT_POINT}/boot bs=512
	dd if=bsd.gz of=${MOUNT_POINT}/bsd bs=512
	${INSTALLBOOT} ${MOUNT_POINT}/boot ${BOOTXX} `cat vnd`
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

bsd.gz:	bsd.rd
	objcopy -Sg -R .comment -R .SUNW_ctf -R .shstrtab -R .shstrtab bsd.rd bsd.strip
	strip bsd.strip
	gzip -c -9nv bsd.strip > bsd.gz

bsd.rd: bsd ${IMAGE}
	cp bsd bsd.rd
	rdsetroot bsd.rd ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK}/obj/bsd bsd

${IMAGE}: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MAKEFSARGS_RD} $@ $@.d

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.ifdef RELEASEDIR
beforeinstall:
	cp ${FLOPPY} ${RELEASEDIR}
.endif

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    *.o *.lo *.c \
	    lib*.a lib*.olist instbin.map ${CLEANFILES}
	/bin/rm -rf ${IMAGE}.d

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
